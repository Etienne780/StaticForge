#include "Internal/LZ4Compression.h"
#include <cassert>
#include <cstdio>

namespace StaticForge::Internal {

    LZ4Compression::LZ4Compression(bool isLittleEndian, bool storeHeader)
        : m_isLittleEndian(isLittleEndian), m_storeHeader(storeHeader){
    }

    std::vector<std::byte> LZ4Compression::Compress(const char* data, size_t size) {
        Reset();

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
            size_t idx = hash & (TABLE_SIZE - 1);

            Entry& e = m_table[idx];

            uint32_t matchStart = e.offset;

            bool valid =
                e.hash == hash &&
                offset > matchStart &&
                (offset - matchStart) <= MAX_DISTANCE &&
                std::memcmp(ctx.data + matchStart, ctx.data + offset, MIN_MATCH) == 0;

            if (!valid) {
                e.hash = hash;
                e.offset = static_cast<uint32_t>(offset);
                offset++;
                continue;
            }

            uint64_t matchLen = MIN_MATCH;
            while (
                offset + matchLen < ctx.size &&
                matchLen < MAX_MATCH &&
                ctx.data[matchStart + matchLen] == ctx.data[offset + matchLen])
            {
                matchLen++;
            }

            uint64_t literalLen = offset - literalStart;
            if (literalLen > 0)
                EmitLiterals(result, ctx.data + literalStart, literalLen);

            uint64_t distance = offset - matchStart;
            EmitMatch(result, distance, matchLen);

            e.hash = hash;
            e.offset = static_cast<uint32_t>(offset);

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
        Reset();

        if (data == nullptr && size > 0) {
            AddError("Decompress: data pointer is null with non-zero size");
            return {};
        }

        size_t off = 0;

        uint64_t originalSize = 0;
        if (m_storeHeader) {
            if (!ReadU64(data, size, off, originalSize))
                return {};
        }
        else {
            originalSize = oriSize;
        }

        if (originalSize > (1ULL << 32)) {
            AddError("Decompress: implausibly large original size (" +
                std::to_string(originalSize) + " bytes)");
            return {};
        }

        std::vector<std::byte> result;
        result.reserve(static_cast<size_t>(originalSize));

        while (off < size) {
            uint8_t blockType = static_cast<uint8_t>(data[off++]);

            switch (blockType) {
            case BLOCK_END:
                off = size;// end loop
                break;
            case BLOCK_LITERAL: {
                uint16_t len = 0;
                if (!ReadU16(data, size, off, len))
                    return {};

                if (off + len > size) {
                    AddError("Decompress: literal block overflows input (need " +
                        std::to_string(len) + " bytes, " +
                        std::to_string(size - off) + " available)");
                    return {};
                }

                for (uint16_t i = 0; i < len; ++i)
                    result.push_back(data[off++]);
                
                break;
            }
            case BLOCK_MATCH: {
                uint16_t distance = 0;
                uint16_t matchLen = 0;

                if (!ReadU16(data, size, off, distance)) 
                    return {};
                if (!ReadU16(data, size, off, matchLen))  
                    return {};

                if (distance == 0) {
                    AddError("Decompress: zero-distance match (invalid back-reference)");
                    return {};
                }

                if (static_cast<uint64_t>(distance) > result.size()) {
                    AddError("Decompress: match distance (" + std::to_string(distance) +
                        ") exceeds output so far (" +
                        std::to_string(result.size()) + ")");
                    return {};
                }
                
                if (matchLen < MIN_MATCH) {
                    AddError("Decompress: match length " + std::to_string(matchLen) +
                        " is below minimum (" + std::to_string(MIN_MATCH) + ")");
                    return {};
                }
                uint64_t copyStart = result.size() - distance;
                for (uint16_t i = 0; i < matchLen; ++i)
                    result.push_back(result[copyStart + i]);
                break;
            }
            default: {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "0x%02X", blockType);
                AddError(std::string("Decompress: unknown block type ") + buf);
                return {};
            }
            }
        }
   
        if (result.size() != originalSize) {
            AddError("Decompress: output size mismatch (expected " +
                std::to_string(originalSize) + ", got " +
                std::to_string(result.size()) + ")");
            return {};
        }

        return result;
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

    void LZ4Compression::Reset() {
        m_table.fill({ 0xFFFFFFFFu, 0 });
        m_errors.clear();
    }

    void LZ4Compression::AddError(const std::string& message) const {
        constexpr const char* header = "[Error]: ";
        constexpr const char* indent = ">        ";
        const char* prefix = m_errors.empty() ? header : indent;
        m_errors.emplace_back(std::string(prefix) + message);
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
        if (off + 2 > size) {
            AddError("Decompress: truncated stream reading uint16 at offset " +
                std::to_string(off));
            return false;
        }
        uint16_t lo = static_cast<uint8_t>(data[off]);
        uint16_t hi = static_cast<uint8_t>(data[off + 1]);

        off += 2;
        out = static_cast<uint16_t>(lo | (hi << 8));
        return true;
    }

    bool LZ4Compression::ReadU64(const std::byte* data, size_t size,
        size_t& off, uint64_t& out) const
    {
        if (off + 8 > size) {
            AddError("Decompress: truncated stream reading uint64 at offset " +
                std::to_string(off));
            return false;
        }

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

}