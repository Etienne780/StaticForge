#include "Internal/LZ4Compression.h"
#include <algorithm>
#include <cassert>
#include <cstdio>

namespace StaticForge::Internal {

    LZ4Compression::LZ4Compression(bool isLittleEndian, bool storeHeader, uint32_t tableSizeLog2, uint32_t minMatchLength)
        : m_isLittleEndian(isLittleEndian)
        , m_storeHeader(storeHeader) {
        SetTableSizeLog2(tableSizeLog2);
        SetMinMatchLength(minMatchLength);
    }

    void LZ4Compression::SetTableSizeLog2(uint32_t tableSizeLog2) {
        uint32_t value = std::clamp(tableSizeLog2, MIN_TABLE_SIZE_LOG2, MAX_TABLE_SIZE_LOG2);
        if (value == m_tableSizeLog2)
            return;

        m_tableSizeLog2 = value;
        // gets reallocated with the new size on the next Compress call
        m_table.clear();
        m_table.shrink_to_fit();
    }

    void LZ4Compression::SetMinMatchLength(uint32_t minMatchLength) {
        m_minMatchLength = std::clamp(minMatchLength, MIN_MATCH, MAX_MATCH);
    }

    uint32_t LZ4Compression::GetTableSizeLog2() const {
        return m_tableSizeLog2;
    }

    uint32_t LZ4Compression::GetTableSize() const {
        return 1u << m_tableSizeLog2;
    }

    uint32_t LZ4Compression::GetMinMatchLength() const {
        return m_minMatchLength;
    }

    std::vector<std::byte> LZ4Compression::Compress(const char* data, size_t size) {
        m_errors.clear();
        ResetTable();

        if (data == nullptr && size > 0) {
            AddError("Compress: data pointer is null with non-zero size");
            return {};
        }

        Context ctx{ data, size };
        std::vector<std::byte> result;
        result.reserve(size + 16);

        if (m_storeHeader)
            WriteU64(result, static_cast<uint64_t>(size));

        if (ctx.size <= MIN_MATCH) {
            if (ctx.size > 0)
                EmitLiterals(result, ctx.data, ctx.size);

            result.push_back(std::byte(BLOCK_END));
            return result;
        }

        uint64_t offset = 0;
        uint64_t literalStart = 0;
        while (offset <= ctx.size - MIN_MATCH) {
            uint32_t hash = Read4ByteSequence(ctx.data + offset);
            uint32_t idx = GetTableIndex(hash);

            Entry& e = m_table[idx];

            uint32_t matchStart = e.offset;

            bool valid =
                e.generation == m_generation &&
                e.hash == hash &&
                offset > matchStart &&
                (offset - matchStart) <= MAX_DISTANCE &&
                std::memcmp(ctx.data + matchStart, ctx.data + offset, MIN_MATCH) == 0;

            uint64_t matchLen = MIN_MATCH;
            if (valid) {
                while (
                    offset + matchLen < ctx.size &&
                    matchLen < MAX_MATCH &&
                    ctx.data[matchStart + matchLen] == ctx.data[offset + matchLen])
                {
                    matchLen++;
                }

                // short matches cost more than storing the bytes as literals
                if (matchLen < m_minMatchLength)
                    valid = false;
            }

            if (!valid) {
                e.hash = hash;
                e.offset = static_cast<uint32_t>(offset);
                e.generation = m_generation;
                offset++;
                continue;
            }

            uint64_t literalLen = offset - literalStart;
            if (literalLen > 0)
                EmitLiterals(result, ctx.data + literalStart, literalLen);

            uint64_t distance = offset - matchStart;
            EmitMatch(result, distance, matchLen);

            e.hash = hash;
            e.offset = static_cast<uint32_t>(offset);
            e.generation = m_generation;

            offset += matchLen;
            literalStart = offset;
        }

        uint64_t remaining = ctx.size - literalStart;
        if (remaining > 0)
            EmitLiterals(result, ctx.data + literalStart, remaining);

        result.push_back(std::byte(BLOCK_END));
        return result;
    }

    std::vector<std::byte> LZ4Compression::Decompress(const std::byte* data, size_t size, size_t oriSize) {
        m_errors.clear();

        if (data == nullptr && size > 0) {
            AddError("Decompress: data pointer is null with non-zero size");
            return {};
        }

        uint64_t originalSize = oriSize;
        if (m_storeHeader) {
            size_t off = 0;
            if (!ReadU64(data, size, off, originalSize)) {
                AddError("Decompress: truncated stream reading the original size");
                return {};
            }
        }

        if (originalSize > (1ULL << 32)) {
            AddError("Decompress: implausibly large original size (" +
                std::to_string(originalSize) + " bytes)");
            return {};
        }

        std::vector<std::byte> result(static_cast<size_t>(originalSize));
        std::string error;
        if (!DecompressTo(data, size, result.data(), result.size(), &error)) {
            AddError(error);
            return {};
        }

        return result;
    }

    bool LZ4Compression::DecompressTo(const std::byte* data, size_t size, std::byte* out, size_t outSize, std::string* errorOut) const {
        auto fail = [errorOut](const std::string& message) {
            if (errorOut)
                *errorOut = "Decompression failed: " + message;
            return false;
        };

        if ((data == nullptr && size > 0) || (out == nullptr && outSize > 0))
            return fail("pointer is null with non-zero size");

        size_t off = 0;
        if (m_storeHeader) {
            uint64_t storedSize = 0;
            if (!ReadU64(data, size, off, storedSize))
                return fail("truncated stream reading the original size");

            if (storedSize != outSize) {
                return fail("stored size (" + std::to_string(storedSize) +
                    ") does not match output size (" + std::to_string(outSize) + ")");
            }
        }

        size_t written = 0;
        bool end = false;
        while (off < size && !end) {
            uint8_t blockType = static_cast<uint8_t>(data[off++]);

            switch (blockType) {
            case BLOCK_END:
                end = true;
                break;
            case BLOCK_LITERAL: {
                uint16_t len = 0;
                if (!ReadU16(data, size, off, len))
                    return fail("truncated stream reading uint16 at offset " + std::to_string(off));

                if (off + len > size) {
                    return fail("literal block overflows input (need " +
                        std::to_string(len) + " bytes, " +
                        std::to_string(size - off) + " available)");
                }

                if (len > outSize - written)
                    return fail("output is larger than expected (" + std::to_string(outSize) + " bytes)");

                std::memcpy(out + written, data + off, len);
                off += len;
                written += len;
                break;
            }
            case BLOCK_MATCH: {
                uint16_t distance = 0;
                uint16_t matchLen = 0;

                if (!ReadU16(data, size, off, distance) || !ReadU16(data, size, off, matchLen))
                    return fail("truncated stream reading uint16 at offset " + std::to_string(off));

                if (distance == 0)
                    return fail("zero-distance match (invalid back-reference)");

                if (distance > written) {
                    return fail("match distance (" + std::to_string(distance) +
                        ") exceeds output so far (" + std::to_string(written) + ")");
                }

                if (matchLen < MIN_MATCH) {
                    return fail("match length " + std::to_string(matchLen) +
                        " is below minimum (" + std::to_string(MIN_MATCH) + ")");
                }

                if (matchLen > outSize - written)
                    return fail("output is larger than expected (" + std::to_string(outSize) + " bytes)");

                // byte by byte, source and destination can overlap
                const size_t copyStart = written - distance;
                for (uint16_t i = 0; i < matchLen; ++i)
                    out[written + i] = out[copyStart + i];
                written += matchLen;
                break;
            }
            default: {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "0x%02X", blockType);
                return fail(std::string("unknown block type ") + buf);
            }
            }
        }

        if (written != outSize) {
            return fail("output size mismatch (expected " +
                std::to_string(outSize) + ", got " + std::to_string(written) + ")");
        }

        return true;
    }

    bool LZ4Compression::IsValid() const {
        return m_errors.empty();
    }

    std::string LZ4Compression::GetError() const {
        std::string result;
        for (size_t i = 0; i < m_errors.size(); ++i) {
            result += m_errors[i];
            if (i + 1 < m_errors.size())
                result += '\n';
        }
        return result;
    }

    void LZ4Compression::ResetTable() {
        if (m_table.empty()) {
            m_table.assign(GetTableSize(), Entry{});
            m_generation = 1;
            return;
        }

        m_generation++;

        // only clear the table when the generation counter wraps around
        if (m_generation == 0) {
            std::fill(m_table.begin(), m_table.end(), Entry{});
            m_generation = 1;
        }
    }

    void LZ4Compression::AddError(const std::string& message) const {
        m_errors.emplace_back(message);
    }

    void LZ4Compression::EmitLiterals(std::vector<std::byte>& out,
        const char* src, uint64_t len) const
    {
        uint64_t emitted = 0;
        while (emitted < len) {
            uint64_t chunk = std::min<uint64_t>(len - emitted, MAX_LITERAL_CHUNK);

            out.push_back(std::byte(BLOCK_LITERAL));
            WriteU16(out, static_cast<uint16_t>(chunk));

            for (uint64_t i = 0; i < chunk; ++i)
                out.push_back(std::byte(src[emitted + i]));

            emitted += chunk;
        }
    }

    void LZ4Compression::EmitMatch(std::vector<std::byte>& out,
        uint64_t distance, uint64_t matchLen) const
    {
        assert(distance <= MAX_DISTANCE);
        assert(matchLen <= MAX_MATCH);
        assert(matchLen >= MIN_MATCH);

        out.push_back(std::byte(BLOCK_MATCH));
        WriteU16(out, static_cast<uint16_t>(distance));
        WriteU16(out, static_cast<uint16_t>(matchLen));
    }

    void LZ4Compression::WriteU16(std::vector<std::byte>& out, uint16_t v) const {
        out.push_back(std::byte(v & 0xFF));
        out.push_back(std::byte((v >> 8) & 0xFF));
    }

    void LZ4Compression::WriteU64(std::vector<std::byte>& out, uint64_t v) const {
        for (int i = 0; i < 8; ++i)
            out.push_back(std::byte((v >> (i * 8)) & 0xFF));
    }

    bool LZ4Compression::ReadU16(const std::byte* data, size_t size,
        size_t& off, uint16_t& out) const
    {
        if (off + 2 > size)
            return false;

        uint16_t lo = static_cast<uint8_t>(data[off]);
        uint16_t hi = static_cast<uint8_t>(data[off + 1]);

        off += 2;
        out = static_cast<uint16_t>(lo | (hi << 8));
        return true;
    }

    bool LZ4Compression::ReadU64(const std::byte* data, size_t size,
        size_t& off, uint64_t& out) const
    {
        if (off + 8 > size)
            return false;

        uint64_t v = 0;
        for (int i = 0; i < 8; ++i)
            v |= static_cast<uint64_t>(static_cast<uint8_t>(data[off + i])) << (i * 8);
        
        off += 8;
        out = v;
        return true;
    }

    uint32_t LZ4Compression::SwapEndian(uint32_t v) const {
        return ((v >> 24) & 0x000000FFu) |
            ((v >> 8) & 0x0000FF00u) |
            ((v << 8) & 0x00FF0000u) |
            ((v << 24) & 0xFF000000u);
    }

    uint32_t LZ4Compression::Read4ByteSequence(const char* p) const {
        uint32_t seq;
        std::memcpy(&seq, p, sizeof(seq));
        return m_isLittleEndian ? seq : SwapEndian(seq);
    }

    uint32_t LZ4Compression::GetTableIndex(uint32_t sequence) const {
        // spreads all 4 bytes over the index bits, so small tables also work well
        return (sequence * 2654435761u) >> (32 - m_tableSizeLog2);
    }

}