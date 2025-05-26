#include "RLE.hpp"
#include "LZ77.hpp"
#include "Delta.hpp"
// #include "FSE.hpp"
// #include "Huffman.hpp"
#include "Algorithms.hpp"
#include <stdexcept>
#include <unordered_set>

namespace wisent::algorithms
{
    std::vector<std::vector<uint8_t>> encodeIntColumn(
        const std::vector<int64_t>& column,
        ColumnChunkMetaData& columnChunkMetaData
    ) {
        std::vector<std::vector<uint8_t>> pages;
        size_t totalValues = 0;
        size_t totalUncompressedSize = 0;
        size_t totalCompressedSize = 0;

        columnChunkMetaData.physicalType = PhysicalType::INT64;
        columnChunkMetaData.compressionType = CompressionType::NONE;
        columnChunkMetaData.encodingType.push_back(EncodingType::PLAIN);

        size_t startIndex = 0;
        while (startIndex < column.size()) 
        {
            size_t bytesInPage = 0;
            std::vector<uint8_t> pageBuffer;
            size_t endIndex = startIndex;

            int64_t minVal = column[startIndex];
            int64_t maxVal = column[startIndex];

            while (endIndex < column.size() && bytesInPage + SIZE_OF_INT64 <= DEFAULT_PAGE_SIZE) 
            {
                int64_t value = column[endIndex];
                minVal = std::min(minVal, value);
                maxVal = std::max(maxVal, value);

                for (size_t b = 0; b < SIZE_OF_INT64; ++b) 
                {
                    pageBuffer.push_back(static_cast<uint8_t>((value >> (8 * b)) & 0xFF));
                }

                bytesInPage += SIZE_OF_INT64;
                ++endIndex;
            }

            size_t numValues = endIndex - startIndex;

            PageStatistics pageStats;
            pageStats.minInt = minVal;
            pageStats.maxInt = maxVal;
            pageStats.distinctCount = std::unordered_set<int64_t>(
                column.begin() + startIndex, column.begin() + endIndex).size();

            PageHeader pageHeader;
            pageHeader.pageType = PageType::DATA_PAGE;
            pageHeader.numberOfValues = static_cast<uint32_t>(numValues);
            pageHeader.firstRowIndex = static_cast<int64_t>(startIndex);
            pageHeader.uncompressedPageSize = static_cast<uint32_t>(pageBuffer.size());
            pageHeader.compressedPageSize = pageHeader.uncompressedPageSize;
            pageHeader.statistics = pageStats;

            pages.push_back(std::move(pageBuffer));
            columnChunkMetaData.pageHeaders.push_back(std::move(pageHeader));

            totalValues += numValues;
            totalUncompressedSize += bytesInPage;
            totalCompressedSize += bytesInPage;

            startIndex = endIndex;
        }

        columnChunkMetaData.numerOfValues = totalValues;
        columnChunkMetaData.totalUncompressedSize = totalUncompressedSize;
        columnChunkMetaData.totalCompressedSize = totalCompressedSize;

        if (!column.empty()) 
        {
            int64_t minVal = *std::min_element(column.begin(), column.end());
            int64_t maxVal = *std::max_element(column.begin(), column.end());

            columnChunkMetaData.statistics.minInt = minVal;
            columnChunkMetaData.statistics.maxInt = maxVal;
            columnChunkMetaData.statistics.distinctCount = std::unordered_set<int64_t>(
                column.begin(), column.end()).size();
        }

        return pages;
    };

    std::vector<std::vector<uint8_t>> encodeDoubleColumn(
        const std::vector<double>& column,
        ColumnChunkMetaData& columnChunkMetaData
    ) {
        std::vector<std::vector<uint8_t>> pages;
        size_t startIndex = 0;
        size_t totalValues = 0;
        size_t totalUncompressedSize = 0;

        columnChunkMetaData.physicalType = PhysicalType::DOUBLE;
        columnChunkMetaData.compressionType = CompressionType::NONE;
        columnChunkMetaData.encodingType.push_back(EncodingType::PLAIN);

        while (startIndex < column.size()) 
        {
            std::vector<uint8_t> pageBuffer;
            size_t bytesInPage = 0;
            size_t endIndex = startIndex;

            double minVal = column[startIndex];
            double maxVal = column[startIndex];

            while (endIndex < column.size() && bytesInPage + SIZE_OF_DOUBLE <= DEFAULT_PAGE_SIZE) 
            {
                double value = column[endIndex];
                minVal = std::min(minVal, value);
                maxVal = std::max(maxVal, value);

                uint8_t* bytePtr = reinterpret_cast<uint8_t*>(&value);
                for (size_t i = 0; i < SIZE_OF_DOUBLE; ++i) 
                {
                    pageBuffer.push_back(bytePtr[i]);
                }

                bytesInPage += SIZE_OF_DOUBLE;
                ++endIndex;
            }

            size_t numValues = endIndex - startIndex;

            PageStatistics stats;
            stats.minDouble = minVal;
            stats.maxDouble = maxVal;
            stats.distinctCount = std::unordered_set<double>(
                column.begin() + startIndex, column.begin() + endIndex).size();

            PageHeader header;
            header.pageType = PageType::DATA_PAGE;
            header.numberOfValues = static_cast<uint32_t>(numValues);
            header.firstRowIndex = static_cast<int64_t>(startIndex);
            header.uncompressedPageSize = static_cast<uint32_t>(bytesInPage);
            header.compressedPageSize = static_cast<uint32_t>(bytesInPage);
            header.statistics = stats;

            columnChunkMetaData.pageHeaders.push_back(header);
            pages.push_back(std::move(pageBuffer));

            totalValues += numValues;
            totalUncompressedSize += bytesInPage;
            startIndex = endIndex;
        }

        columnChunkMetaData.numerOfValues = totalValues;
        columnChunkMetaData.totalUncompressedSize = totalUncompressedSize;
        columnChunkMetaData.totalCompressedSize = totalUncompressedSize;

        if (!column.empty()) 
        {
            double minVal = *std::min_element(column.begin(), column.end());
            double maxVal = *std::max_element(column.begin(), column.end());
            columnChunkMetaData.statistics.minDouble = minVal;
            columnChunkMetaData.statistics.maxDouble = maxVal;
            columnChunkMetaData.statistics.distinctCount = std::unordered_set<double>(column.begin(), column.end()).size();
        }

        return pages;
    }; 


    std::vector<std::vector<uint8_t>> encodeStringColumn(
        const std::vector<std::string>& column,
        ColumnChunkMetaData& columnChunkMetaData
    ) {
        std::vector<std::vector<uint8_t>> pages;
        size_t startIndex = 0;
        size_t totalValues = 0;
        size_t totalUncompressedSize = 0;

        columnChunkMetaData.physicalType = PhysicalType::BYTE_ARRAY;
        columnChunkMetaData.compressionType = CompressionType::NONE;
        columnChunkMetaData.encodingType.push_back(EncodingType::PLAIN);

        while (startIndex < column.size()) 
        {
            std::vector<uint8_t> pageBuffer;
            size_t bytesInPage = 0;
            size_t endIndex = startIndex;

            std::string minStr = column[startIndex];
            std::string maxStr = column[startIndex];

            while (endIndex < column.size()) 
            {
                const std::string& str = column[endIndex];
                size_t strLen = str.size();
                size_t encodedLen = 4 + strLen;

                if (bytesInPage + encodedLen > DEFAULT_PAGE_SIZE) break;

                minStr = std::min(minStr, str);
                maxStr = std::max(maxStr, str);

                uint32_t len = static_cast<uint32_t>(strLen);
                for (int i = 0; i < 4; ++i)
                    pageBuffer.push_back((len >> (8 * i)) & 0xFF);

                pageBuffer.insert(pageBuffer.end(), str.begin(), str.end());

                bytesInPage += encodedLen;
                ++endIndex;
            }

            size_t numValues = endIndex - startIndex;

            PageStatistics stats;
            stats.distinctCount = std::unordered_set<std::string>(
                column.begin() + startIndex, column.begin() + endIndex).size();

            PageHeader header;
            header.pageType = PageType::DATA_PAGE;
            header.numberOfValues = static_cast<uint32_t>(numValues);
            header.firstRowIndex = static_cast<int64_t>(startIndex);
            header.uncompressedPageSize = static_cast<uint32_t>(bytesInPage);
            header.compressedPageSize = static_cast<uint32_t>(bytesInPage);
            header.statistics = stats;

            columnChunkMetaData.pageHeaders.push_back(header);
            pages.push_back(std::move(pageBuffer));

            totalValues += numValues;
            totalUncompressedSize += bytesInPage;
            startIndex = endIndex;
        }

        columnChunkMetaData.numerOfValues = totalValues;
        columnChunkMetaData.totalUncompressedSize = totalUncompressedSize;
        columnChunkMetaData.totalCompressedSize = totalUncompressedSize;

        return pages;
    }; 


    std::vector<uint8_t> performCompression(
        CompressionType type,
        const std::vector<uint8_t>& buffer
    ) {
        switch (type) 
        {
            case CompressionType::DELTA:
                return compressWith<DELTA>(
                    reinterpret_cast<const char*>(buffer.data()),
                    buffer.size()
                );
            case CompressionType::RLE:
                return compressWith<RLE>(
                    reinterpret_cast<const char*>(buffer.data()),
                    buffer.size()
                );
            case CompressionType::LZ77:
                return compressWith<LZ77>(
                    reinterpret_cast<const char*>(buffer.data()),
                    buffer.size()
                );
            default:
                throw std::invalid_argument("Unsupported compression type");
        }
    }; 
}