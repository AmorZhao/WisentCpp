#include <cstddef>
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <optional>

namespace wisent::algorithms
{
    constexpr size_t DEFAULT_PAGE_SIZE = 1024 * 1024;   // 1 MB

    // constexpr size_t MIN_PAGE_SIZE = 64 * 1024;         // 64 KB
    // constexpr size_t MAX_PAGE_SIZE = 4 * 1024 * 1024;   // 4 MB

    constexpr size_t SIZE_OF_INT64 = sizeof(int64_t);      // 8 bytes
    constexpr size_t SIZE_OF_DOUBLE = sizeof(double);      // 8 bytes

    enum class PageType {
        DATA_PAGE,
        // DATA_PAGE_V2,
        DICTIONARY_PAGE
        // INDEX_PAGE,
        // BLOOM_FILTER_PAGE
    };

    struct PageStatistics 
    {
        std::optional<int64_t> nullCount;
        std::optional<int64_t> distinctCount;

        // std::optional<std::string> minString;
        // std::optional<std::string> maxString;

        std::optional<int64_t> minInt;
        std::optional<int64_t> maxInt;

        std::optional<double> minDouble;
        std::optional<double> maxDouble;
    };

    struct PageHeader 
    {
        PageType pageType;

        uint32_t numberOfValues;
        std::optional<int64_t> firstRowIndex;

        uint32_t uncompressedPageSize;
        uint32_t compressedPageSize; 

        std::optional<PageStatistics> statistics;

        bool isDictionaryPage = false;
        std::optional<uint32_t> dictionaryPageSize;
    };

    enum class EncodingType {
        PLAIN,                      // 0
        RLE,                        // 1    
        BIT_PACKED,                 // 2
        DICTIONARY,                 // 3
        DELTA_BINARY_PACKED,        // 4
        DELTA_LENGTH_BYTE_ARRAY,    // 5
        DELTA_BYTE_ARRAY,           // 6 
    };

    enum class PhysicalType {
        // INT32,
        INT64,
        // FLOAT,
        DOUBLE,
        BYTE_ARRAY,
        // FIXED_LEN_BYTE_ARRAY,
        BOOLEAN
    };

    enum class CompressionType {
        NONE,
        DELTA,
        RLE,
        LZ77,
        HUFFMAN,
        FSE
        // TODO: add type builder for compression pipelines
    };

    struct ColumnMetaData 
    {
        std::string columnName;
        uint64_t numerOfValues;

        uint64_t totalUncompressedSize;
        uint64_t totalCompressedSize;

        PhysicalType physicalType;
        std::vector<EncodingType> encodingType;
        CompressionType compressionType;
      
        PageStatistics statistics;

        std::optional<uint32_t> dictionary_page_offset;

        std::vector<PageHeader> pageHeaders;

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
        {"de", CompressionType::DELTA}
    };

    static const std::unordered_map<CompressionType, std::string> compressionTypeNames = 
    {
        {CompressionType::NONE, "none"},
        {CompressionType::RLE, "rle"},
        {CompressionType::HUFFMAN, "huffman"},
        {CompressionType::LZ77, "lz77"},
        {CompressionType::FSE, "fse"},
        {CompressionType::DELTA, "delta"}
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
        const char* data, 
        const size_t size
    ) {
        const std::byte* input = reinterpret_cast<const std::byte*>(data);

        std::vector<std::byte> output(size * 2); 
        auto result = codec::compress(input, size, output.data());
        if (!result.success()) {
            throw std::runtime_error("Compression failed");
        }

        output.resize(result.value.value());

        return std::vector<uint8_t>(
            reinterpret_cast<const uint8_t*>(output.data()),
            reinterpret_cast<const uint8_t*>(output.data() + output.size())
        );
    }

    std::vector<uint8_t> performCompression(
        CompressionType type,
        const std::vector<uint8_t>& buffer
    ); 

}
