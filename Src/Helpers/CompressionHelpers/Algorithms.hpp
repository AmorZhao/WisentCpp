#pragma once
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <optional>

namespace wisent::algorithms
{
    // ===================== Encoding algorithms ==================
    
    constexpr size_t DEFAULT_PAGE_SIZE = 1024 * 1024;   // 1 MB

    // constexpr size_t MIN_PAGE_SIZE = 64 * 1024;         // 64 KB
    // constexpr size_t MAX_PAGE_SIZE = 4 * 1024 * 1024;   // 4 MB

    constexpr size_t SIZE_OF_INT64 = sizeof(int64_t);      // 8 bytes
    constexpr size_t SIZE_OF_DOUBLE = sizeof(double);      // 8 bytes

    enum class PageType : size_t {
        DATA_PAGE,
        // DATA_PAGE_V2,
        DICTIONARY_PAGE
        // INDEX_PAGE,
        // BLOOM_FILTER_PAGE
    };

    struct Statistics   // size = 4 x 8 byte values
    {
        int64_t nullCount;
        int64_t distinctCount;

        std::optional<std::string> minString;
        std::optional<std::string> maxString;

        std::optional<int64_t> minInt;
        std::optional<int64_t> maxInt;

        std::optional<double> minDouble;
        std::optional<double> maxDouble;
    };

    constexpr size_t EXPRESSION_COUNT_PER_PAGE_HEADER = 11; // or 12 for dictionary pages
    struct PageHeader 
    {
        PageType pageType;  

        uint64_t numberOfValues;
        uint64_t firstRowIndex;

        uint64_t uncompressedPageSize;
        uint64_t compressedPageSize; 

        Statistics pageStatistics;  // 4 arguments

        bool isDictionaryPage = false;
        std::optional<uint64_t> dictionaryPageSize;

        // uint64_t *byteArrayOffset; 
        std::vector<uint8_t> byteArray; 
    };

    enum class EncodingType : size_t {
        PLAIN,                      // 0
        RLE,                        // 1    
        BIT_PACKED,                 // 2
        DICTIONARY,                 // 3
        DELTA_BINARY_PACKED,        // 4
        DELTA_LENGTH_BYTE_ARRAY,    // 5
        DELTA_BYTE_ARRAY,           // 6 
    };

    enum class PhysicalType : size_t {
        // INT32,
        INT64,
        // FLOAT,
        DOUBLE,
        BYTE_ARRAY,
        // FIXED_LEN_BYTE_ARRAY,
        BOOLEAN
    };

    enum class CompressionType : size_t {
        NONE,
        DELTA,
        RLE,
        LZ77,
        HUFFMAN,
        FSE, 
        CUSTOM
    };

    // (excluding page header expressions)
    constexpr size_t KEY_VALUE_PAIR_PER_COLUMNMETADATA = 7;
    struct ColumnMetaData 
    {
        std::string columnName;  // head

        uint64_t numberOfValues;
        uint64_t totalUncompressedSize;
        uint64_t totalCompressedSize;

        PhysicalType physicalType;
        EncodingType encodingType;
        std::vector<CompressionType> compressionTypes;

        std::vector<PageHeader> pageHeaders; 

        // optional: columnar statistics
        // optional: dictionary offset
        // optional: bloom filtering
    };

    template <typename T>
    std::vector<std::vector<T>> splitPages(
        const std::vector<T>& column, 
        size_t pageSize = DEFAULT_PAGE_SIZE
    ) {
        std::vector<std::vector<T>> pages;
        size_t total = column.size();

        for (size_t i = 0; i < total; i += pageSize) 
        {
            size_t end = std::min(i + pageSize, total);
            pages.emplace_back(column.begin() + i, column.begin() + end);
        }
        return pages;
    }; 

    std::vector<std::vector<uint8_t>> encodeIntColumn(
        const std::vector<int64_t>& column,
        ColumnMetaData& columnChunkMetaData
    ); 

    std::vector<std::vector<uint8_t>> encodeDoubleColumn(
        const std::vector<double>& column,
        ColumnMetaData& columnChunkMetaData
    ); 

    std::vector<std::vector<uint8_t>> encodeStringColumn(
        const std::vector<std::string>& column,
        ColumnMetaData& columnChunkMetaData
    ); 

    // =================== Compression algorithms ===================
    
    static const std::unordered_map<std::string, CompressionType> compressionAliases = 
    {
        {"none", CompressionType::NONE},
        {"rle", CompressionType::RLE},
        {"runlengthencoding", CompressionType::RLE},
        {"huffman", CompressionType::HUFFMAN},
        {"lz77", CompressionType::LZ77},
        {"fse", CompressionType::FSE},
        {"finitestateentropy", CompressionType::FSE},
        {"delta", CompressionType::DELTA},
        {"de", CompressionType::DELTA}, 
        {"custom", CompressionType::CUSTOM}
    };

    static const std::unordered_map<CompressionType, std::string> compressionTypeNames = 
    {
        {CompressionType::NONE, "none"},
        {CompressionType::RLE, "rle"},
        {CompressionType::HUFFMAN, "huffman"},
        {CompressionType::LZ77, "lz77"},
        {CompressionType::FSE, "fse"},
        {CompressionType::DELTA, "delta"}, 
        {CompressionType::CUSTOM, "custom"}
    };

    static std::string compressionTypeToString(CompressionType type)
    {
        return compressionTypeNames.at(type); 
    }

    static CompressionType stringToCompressionType(std::string type)
    {
        std::transform(type.begin(), type.end(), type.begin(), ::tolower);
        auto it = compressionAliases.find(type);
        if (it == compressionAliases.end()) 
        {
            throw std::invalid_argument("Unknown compression type: " + type);
        }
        return it->second;
    }; 

    template <typename codec>
    std::vector<uint8_t> compressWith(
        std::vector<uint8_t> const &data
    ) {
        auto result = codec::compress(data);

        if (!result.success()) 
        {
            throw std::runtime_error("Compression failed");
        }

        return result.getValue();
    }; 

    std::vector<uint8_t> performCompression(
        CompressionType type,
        const std::vector<uint8_t>& buffer
    ); 

    template <typename codec>
    std::vector<uint8_t> decompressWith(
        std::vector<uint8_t> const &data
    ) {
        auto result = codec::decompress(data);

        if (!result.success()) 
        {
            throw std::runtime_error("Decompression failed");
        }

        return result.getValue();
    }; 

    std::vector<uint8_t> performDecompression(
        CompressionType type,
        const std::vector<uint8_t>& buffer
    ); 
}
