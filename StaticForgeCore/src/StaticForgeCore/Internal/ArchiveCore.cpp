#include <algorithm>
#include <cstring>
#include <sstream>
#include <iomanip>

#include "Internal/ArchiveCore.h"
#include "Internal/InternalHelpers.h"
#include "Internal/MmapFile.h"
#include "Internal/LZ4Compression.h"

namespace StaticForge::Internal {

	// decompression doesn't change the object, so one instance is shared by all threads
	static const LZ4Compression& GetDecompressor() {
		static const LZ4Compression decompressor(IsLittleEndian(), !LZ4_STORE_HEADER);
		return decompressor;
	}

	static bool VerifyChecksum(const std::byte* data, uint64_t size, uint32_t expected, std::string* errorOut) {
		uint32_t computed = FNV1a(data, static_cast<size_t>(size), 2166136261u);
		if (computed != expected) {
			*errorOut = "Checksum mismatch for entry (expected " + std::to_string(expected) + ", got " + std::to_string(computed) + ")";
			return false;
		}
		return true;
	}

	static bool ReadStream(std::ifstream& stream, uint64_t absOffset, uint64_t size, std::byte* dst, std::string* errorOut) {
		// prevents a previous failed load from breaking all following loads
		stream.clear();
		stream.seekg(static_cast<std::streamoff>(absOffset), std::ios::beg);
		if (!stream) {
			*errorOut = "Failed to seek at offset " + std::to_string(absOffset);
			return false;
		}

		if (size == 0)
			return true;

		stream.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(size));
		if (!stream) {
			*errorOut = "Failed to read " + std::to_string(size) + " bytes at offset " + std::to_string(absOffset);
			return false;
		}
		return true;
	}

	// returns a pointer to size stored bytes of an entry, starting at pos
	// mapped: points into the mapping, stream: the bytes are read into temp
	static const std::byte* ReadStored(const ArchiveData& data, const StaticForgeIndexEntry& entry, EntrySource source,
		uint64_t pos, uint64_t size, std::vector<std::byte>* temp, std::string* errorOut) {
		const uint64_t absOffset = data.header.dataOffset + entry.fileOffset + pos;

		if (source.mmap) {
			const auto* src = reinterpret_cast<const std::byte*>(source.mmap->At(absOffset, size));
			if (!src) {
				*errorOut = "Entry is out of bounds"
					+ std::string(" (offset=") + std::to_string(absOffset)
					+ ", size=" + std::to_string(size)
					+ ", map size=" + std::to_string(source.mmap->Size()) + ")";
			}
			return src;
		}

		temp->resize(static_cast<size_t>(size));
		if (!ReadStream(*source.stream, absOffset, size, temp->data(), errorOut))
			return nullptr;

		// never null on success, also if size is 0
		static const std::byte empty{};
		return temp->empty() ? &empty : temp->data();
	}

	// walks the compressed blocks of an entry and decompresses the blocks inside of the range
	// stored: all stored bytes in memory, or nullptr to read each block with ReadStored()
	// dst: receives the range, or nullptr to only validate the block sizes
	static bool DecompressBlocks(const ArchiveData& data, const StaticForgeIndexEntry& entry, EntrySource source,
		const std::byte* stored, uint64_t rangeStart, uint64_t rangeSize, std::byte* dst, std::string* errorOut) {
		constexpr uint64_t frameSize = sizeof(uint32_t) * 2;
		const uint64_t storedSize = entry.compressedFileSize;
		const uint64_t rangeEnd = rangeStart + rangeSize;

		std::vector<std::byte> temp;
		std::vector<std::byte> blockBuffer;

		uint64_t pos = 0;    // position in the stored data
		uint64_t outPos = 0; // position in the original data
		while (outPos < rangeEnd) {
			if (storedSize - pos < frameSize) {
				*errorOut = "Compressed block framing error at pos " + std::to_string(pos);
				return false;
			}

			// frame: [u32 compressed size][u32 original size]
			const std::byte* frame = stored ? stored + pos : ReadStored(data, entry, source, pos, frameSize, &temp, errorOut);
			if (!frame)
				return false;

			uint32_t compSize32 = 0;
			uint32_t origSize32 = 0;
			std::memcpy(&compSize32, frame, sizeof(uint32_t));
			std::memcpy(&origSize32, frame + sizeof(uint32_t), sizeof(uint32_t));
			if (!IsLittleEndian()) {
				compSize32 = SwapEndian(compSize32);
				origSize32 = SwapEndian(origSize32);
			}
			pos += frameSize;

			if (compSize32 > storedSize - pos) {
				*errorOut = "Compressed block overflows data at pos " + std::to_string(pos);
				return false;
			}

			if (origSize32 > MAX_COMPRESSION_BLOCK_SIZE || origSize32 > entry.fileSize - outPos) {
				*errorOut = "Compressed block at pos " + std::to_string(pos) + " has an invalid size (archive is corrupted)";
				return false;
			}

			const uint64_t blockEnd = outPos + origSize32;

			// only the blocks inside of the range are decompressed
			if (dst && blockEnd > rangeStart) {
				const std::byte* payload = stored ? stored + pos : ReadStored(data, entry, source, pos, compSize32, &temp, errorOut);
				if (!payload)
					return false;

				const uint64_t copyStart = std::max(rangeStart, outPos);
				const uint64_t copyEnd = std::min(rangeEnd, blockEnd);

				// a block that is only partly inside of the range is decompressed into blockBuffer first
				const bool wholeBlock = copyStart == outPos && copyEnd == blockEnd;
				std::byte* target = nullptr;
				if (wholeBlock) {
					target = dst + (outPos - rangeStart);
				}
				else {
					blockBuffer.resize(origSize32);
					target = blockBuffer.data();
				}

				// compressed size == original size marks a block that is stored raw
				std::string error;
				if (compSize32 == origSize32) {
					if (origSize32 > 0)
						std::memcpy(target, payload, origSize32);
				}
				else if (!GetDecompressor().DecompressTo(payload, compSize32, target, origSize32, &error)) {
					*errorOut = "Decompression failed at block (pos=" + std::to_string(pos) + "): " + error;
					return false;
				}

				if (!wholeBlock) {
					std::memcpy(dst + (copyStart - rangeStart), blockBuffer.data() + (copyStart - outPos),
						static_cast<size_t>(copyEnd - copyStart));
				}
			}

			pos += compSize32;
			outPos = blockEnd;
		}

		return true;
	}

	const StaticForgeIndexEntry* ArchiveData::Find(const std::string& key) const {
		auto it = hashNameToEntry.find(HashFilename(NormalizeFilepathSlashes(key)));
		if (it == hashNameToEntry.end() || it->second >= indexEntries.size())
			return nullptr;
		return &indexEntries[it->second];
	}

	std::string ArchiveData::GetDisplayName(size_t index) const {
		auto it = indexToName.find(index);
		if (it != indexToName.end())
			return it->second;

		std::ostringstream hash;
		hash << std::hex << std::setw(16) << std::setfill('0') << (index < indexEntries.size() ? indexEntries[index].hashName : 0);
		return "hash " + hash.str();
	}

	bool LoadEntry(const ArchiveData& data, const StaticForgeIndexEntry& entry, EntrySource source,
		std::vector<std::byte>* outVector, std::byte* outBuffer, uint64_t bufferSize, std::string* errorOut) {
		const bool isCompressed = entry.compressedFileSize != 0;

		if (!outVector) {
			if (bufferSize < entry.fileSize) {
				*errorOut = "Buffer is too small (asset size " + std::to_string(entry.fileSize)
					+ ", buffer size " + std::to_string(bufferSize) + ")";
				return false;
			}
			if (!outBuffer && entry.fileSize > 0) {
				*errorOut = "Buffer is null";
				return false;
			}
		}

		const uint64_t storedSize = isCompressed ? entry.compressedFileSize : entry.fileSize;

		// outVector is only changed on success
		std::vector<std::byte> local;

		// uncompressed data from the stream is read directly into the target
		if (!isCompressed && source.stream) {
			std::byte* dst = outBuffer;
			if (outVector) {
				local.resize(static_cast<size_t>(storedSize));
				dst = local.data();
			}

			if (!ReadStream(*source.stream, data.header.dataOffset + entry.fileOffset, storedSize, dst, errorOut))
				return false;
			if (!VerifyChecksum(dst, storedSize, entry.checksum, errorOut))
				return false;

			if (outVector)
				*outVector = std::move(local);
			return true;
		}

		std::vector<std::byte> temp;
		const std::byte* stored = ReadStored(data, entry, source, 0, storedSize, &temp, errorOut);
		if (!stored)
			return false;

		if (!VerifyChecksum(stored, storedSize, entry.checksum, errorOut))
			return false;

		// fileSize comes from the archive, so check the blocks before allocating it
		if (isCompressed && outVector && !DecompressBlocks(data, entry, source, stored, 0, entry.fileSize, nullptr, errorOut))
			return false;

		std::byte* dst = outBuffer;
		if (outVector) {
			local.resize(static_cast<size_t>(entry.fileSize));
			dst = local.data();
		}

		if (isCompressed) {
			if (!DecompressBlocks(data, entry, source, stored, 0, entry.fileSize, dst, errorOut))
				return false;
		}
		else if (entry.fileSize > 0) {
			std::memcpy(dst, stored, static_cast<size_t>(entry.fileSize));
		}

		if (outVector)
			*outVector = std::move(local);
		return true;
	}

	bool LoadEntryRange(const ArchiveData& data, const StaticForgeIndexEntry& entry, EntrySource source,
		uint64_t offset, uint64_t size, std::byte* outBuffer, std::string* errorOut) {
		const bool isCompressed = entry.compressedFileSize != 0;

		if (offset > entry.fileSize || size > entry.fileSize - offset) {
			*errorOut = "Range (offset " + std::to_string(offset) + ", size " + std::to_string(size)
				+ ") is outside of the asset (size " + std::to_string(entry.fileSize) + ")";
			return false;
		}

		if (size == 0)
			return true;

		if (!outBuffer) {
			*errorOut = "Buffer is null";
			return false;
		}

		if (isCompressed)
			return DecompressBlocks(data, entry, source, nullptr, offset, size, outBuffer, errorOut);

		if (source.stream)
			return ReadStream(*source.stream, data.header.dataOffset + entry.fileOffset + offset, size, outBuffer, errorOut);

		std::vector<std::byte> unused;
		const std::byte* src = ReadStored(data, entry, source, offset, size, &unused, errorOut);
		if (!src)
			return false;

		std::memcpy(outBuffer, src, static_cast<size_t>(size));
		return true;
	}

	const std::byte* GetEntryMapped(const ArchiveData& data, const StaticForgeIndexEntry& entry, const MmapFile& mmap,
		std::string* errorOut) {
		if (entry.compressedFileSize != 0) {
			*errorOut = "asset is compressed - use LoadAssetMapped() instead";
			return nullptr;
		}

		const uint64_t absOffset = data.header.dataOffset + entry.fileOffset;
		const auto* src = reinterpret_cast<const std::byte*>(mmap.At(absOffset, entry.fileSize));
		if (!src) {
			*errorOut = "entry is out of bounds (offset=" + std::to_string(absOffset)
				+ ", size=" + std::to_string(entry.fileSize)
				+ ", map size=" + std::to_string(mmap.Size()) + ")";
		}
		return src;
	}

	bool VerifyEntry(const ArchiveData& data, const StaticForgeIndexEntry& entry, std::ifstream& stream,
		std::vector<char>* chunkBuffer, std::string* reasonOut) {
		const uint64_t storedSize = entry.compressedFileSize != 0 ? entry.compressedFileSize : entry.fileSize;

		stream.clear();
		stream.seekg(static_cast<std::streamoff>(data.header.dataOffset + entry.fileOffset), std::ios::beg);

		uint32_t checksum = 2166136261u;
		uint64_t remaining = storedSize;
		while (remaining > 0 && stream) {
			size_t readSize = static_cast<size_t>(std::min<uint64_t>(remaining, chunkBuffer->size()));
			stream.read(chunkBuffer->data(), static_cast<std::streamsize>(readSize));
			checksum = FNV1a(chunkBuffer->data(), static_cast<size_t>(stream.gcount()), checksum);
			remaining -= static_cast<uint64_t>(stream.gcount());
		}

		if (remaining != 0) {
			*reasonOut = "data is missing";
			return false;
		}
		if (checksum != entry.checksum) {
			*reasonOut = "checksum mismatch";
			return false;
		}
		return true;
	}

}
