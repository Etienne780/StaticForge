#pragma once
#include <vector>
#include <string>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "StaticForgeTypes.h"

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
     * - minimum match length = 4 (the encoder only emits matches >= GetMinMatchLength())
     */
    class LZ4Compression {
    public:
        static constexpr uint32_t MIN_TABLE_SIZE_LOG2 = MIN_COMPRESSION_TABLE_SIZE_LOG2;
        static constexpr uint32_t MAX_TABLE_SIZE_LOG2 = MAX_COMPRESSION_TABLE_SIZE_LOG2;
        static constexpr uint32_t DEFAULT_TABLE_SIZE_LOG2 = DEFAULT_COMPRESSION_TABLE_SIZE_LOG2;

        static constexpr uint32_t MIN_MATCH = MIN_COMPRESSION_MIN_MATCH_LENGTH;
        static constexpr uint32_t MAX_MATCH = MAX_COMPRESSION_MIN_MATCH_LENGTH;
        static constexpr uint32_t DEFAULT_MIN_MATCH_LENGTH = DEFAULT_COMPRESSION_MIN_MATCH_LENGTH;

        explicit LZ4Compression() = default;

        /**
         * @brief Constructs compressor instance.
         * @param isLittleEndian Input byte order flag. (LZ4_IS_LITTLE_ENDIAN)
         * @param storeHeader Write/read original size header. (LZ4_STORE_HEADER)
         * @param tableSizeLog2 Match table size as power of two, see SetTableSizeLog2()
         * @param minMatchLength Shortest match the encoder emits, see SetMinMatchLength()
         */
        explicit LZ4Compression(
            bool isLittleEndian,
            bool storeHeader,
            uint32_t tableSizeLog2 = DEFAULT_TABLE_SIZE_LOG2,
            uint32_t minMatchLength = DEFAULT_MIN_MATCH_LENGTH
        );

        /**
         * @brief Sets the match table size (entries = 1 << tableSizeLog2).
         *
         * Clamped to [MIN_TABLE_SIZE_LOG2, MAX_TABLE_SIZE_LOG2].
         */
        void SetTableSizeLog2(uint32_t tableSizeLog2);

        /**
         * @brief Sets the shortest match the encoder emits, shorter matches are stored as literals.
         *
         * Clamped to [MIN_MATCH, MAX_MATCH]. Does not affect decompression.
         */
        void SetMinMatchLength(uint32_t minMatchLength);

        /**
         * @brief Returns the match table size as power of two.
         */
        uint32_t GetTableSizeLog2() const;

        /**
         * @brief Returns the number of entries in the match table.
         */
        uint32_t GetTableSize() const;

        /**
         * @brief Returns the shortest match the encoder emits.
         */
        uint32_t GetMinMatchLength() const;

        /**
         * @brief Compresses a memory buffer.
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
         * @brief Decompresses directly into an existing buffer.
         *
         * Does not change the object, so it can be used from several threads at once.
         *
         * @param data Input compressed data
         * @param size Input size in bytes
         * @param out Output buffer
         * @param outSize Original size, the output must have exactly this size
         * @param errorOut Receives the error on failure (optional)
         * @return true on success
         */
        bool DecompressTo(const std::byte* data, size_t size, std::byte* out, size_t outSize, std::string* errorOut) const;

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
            uint32_t generation = 0; /*< entry is only valid if it matches m_generation */
        };

        static constexpr uint8_t  BLOCK_END = 0x00;
        static constexpr uint8_t  BLOCK_LITERAL = 0x01;
        static constexpr uint8_t  BLOCK_MATCH = 0x02;

        static constexpr uint32_t MAX_DISTANCE = 0xFFFF;
        static constexpr uint32_t MAX_LITERAL_CHUNK = 0xFFFF;

        const bool m_isLittleEndian = false;
        const bool m_storeHeader = false;
        uint32_t m_tableSizeLog2 = DEFAULT_TABLE_SIZE_LOG2;
        uint32_t m_minMatchLength = DEFAULT_MIN_MATCH_LENGTH;

        // only allocated on the first Compress call, decompression does not need it
        std::vector<Entry> m_table;
        uint32_t m_generation = 0;
        mutable std::vector<std::string> m_errors;

        /**
        * @brief Invalidates all match table entries without clearing the whole table.
        */
        void ResetTable();

        void AddError(const std::string& message) const;

        void EmitLiterals(std::vector<std::byte>& out,
            const char* src, uint64_t len) const;
        void EmitMatch(std::vector<std::byte>& out,
            uint64_t distance, uint64_t matchLen) const;

        /// Writes 16-bit value (LE).
        void WriteU16(std::vector<std::byte>& out, uint16_t v) const;

        /// Writes 64-bit value (LE).
        void WriteU64(std::vector<std::byte>& out, uint64_t v) const;

        /// Read values (LE), false if the data is too short.
        bool ReadU16(const std::byte* data, size_t size, size_t& off,
            uint16_t& out) const;
        bool ReadU64(const std::byte* data, size_t size, size_t& off,
            uint64_t& out) const;

        uint32_t SwapEndian(uint32_t v) const;
        uint32_t Read4ByteSequence(const char* p) const;

        /// Maps a 4 byte sequence to an index in the match table.
        uint32_t GetTableIndex(uint32_t sequence) const;
    };

}