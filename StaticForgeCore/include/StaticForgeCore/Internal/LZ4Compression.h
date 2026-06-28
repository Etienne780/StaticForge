#pragma once
#include <vector>
#include <string>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace StaticForge::Internal {

    constexpr bool LZ4_IS_LITTLE_ENDIAN = true;
    constexpr bool LZ4_STORE_HEADER = true;

    /**
     * @brief Lightweight LZ4-inspired block compressor.
     *
     * Binary format:
     * - Optional header: original size (uint64, little-endian)
     * - Blocks:
     *   - 0x01: literal [len:uint16][data]
     *   - 0x02: match   [distance:uint16][len:uint16]
     *   - 0x00: end-of-stream
     *
     * Constraints:
     * - distance <= 65535
     * - matchLen <= 65535
     * - minimum match length = 4
     */
    class LZ4Compression {
    public:
        explicit LZ4Compression() = default;

        /**
         * @brief Constructs compressor instance.
         * @param isLittleEndian Input byte order flag. (LZ4_IS_LITTLE_ENDIAN)
         * @param storeHeader Write/read original size header. (LZ4_STORE_HEADER)
         */
        explicit LZ4Compression(bool isLittleEndian, bool storeHeader);

        /**
         * @brief Compresses a memory buffer.
         *
         * The internal match map is not reset automatically.
         * Call Reset() manually if a clean state is required.
         *
         * @param data Input buffer
         * @param size Input size in bytes
         * @return Compressed byte stream or empty on error
         */
        std::vector<std::byte> Compress(const char* data, size_t size);

        /**
         * @brief Decompresses a previously compressed buffer.
         * @param data Input compressed data
         * @param size Input size in bytes
         * @param originalSize Fallback size if header is disabled
         * @return Decompressed buffer or empty on error
         */
        std::vector<std::byte> Decompress(const std::byte* data, size_t size, size_t originalSize = 0);

        /**
         * @brief Checks whether no errors occurred.
         */
        bool IsValid() const;

        /**
         * @brief Returns formatted error log.
         */
        std::string GetError() const;

    private:
        struct Context {
            const char* data;
            size_t size;

            Context(const char* d, size_t s) : data(d), size(s) {}
        };

        struct Entry {
            uint32_t offset = 0;
            uint32_t hash = 0;

            Entry() = default;
        };

        static constexpr uint8_t  BLOCK_END = 0x00;
        static constexpr uint8_t  BLOCK_LITERAL = 0x01;
        static constexpr uint8_t  BLOCK_MATCH = 0x02;

        static constexpr uint32_t MIN_MATCH = 4;
        static constexpr uint32_t MAX_DISTANCE = 0xFFFF;
        static constexpr uint32_t MAX_MATCH = 0xFFFF;
        static constexpr uint32_t MAX_LITERAL_CHUNK = 0xFFFF;

        static constexpr uint32_t TABLE_SIZE = 1 << 20; // 1M entries (adjust)
        static constexpr uint32_t HASH_MASK = TABLE_SIZE - 1;

        static constexpr uint64_t WINDOW_SIZE = 64 * 1024;

        const bool m_isLittleEndian = false;
        const bool m_storeHeader = false;

        std::array<Entry, TABLE_SIZE> m_table{};
        mutable std::vector<std::string> m_errors;

        /**
        * @brief Resets internal state and error log.
        */
        void Reset();

        void AddError(const std::string& message) const;

        void EmitLiterals(std::vector<std::byte>& out,
            const char* src, uint64_t len) const;
        void EmitMatch(std::vector<std::byte>& out,
            uint64_t distance, uint64_t matchLen) const;

        /// Writes 16-bit value (LE).
        void WriteU16(std::vector<std::byte>& out, uint16_t v) const;

        /// Writes 64-bit value (LE).
        void WriteU64(std::vector<std::byte>& out, uint64_t v) const;

        bool ReadU16(const std::byte* data, size_t size, size_t& off,
            uint16_t& out) const;
        bool ReadU64(const std::byte* data, size_t size, size_t& off,
            uint64_t& out) const;

        uint32_t SwapEndian(uint32_t v) const;
        uint32_t Read4ByteSequence(const char* p) const;
    };

}