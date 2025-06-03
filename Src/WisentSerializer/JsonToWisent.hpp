#pragma once
#include "WisentHelpers.hpp"
#include "../Helpers/CsvLoading.hpp"
#include "../Helpers/ISharedMemorySegment.hpp"
#include "../WisentCompressor/CompressionHelpers/Algorithms.hpp"
#include <cstdint>
#include <string>
#include <cassert>
#include <vector>
#include <sys/resource.h>
#include "../../Include/json.h"

using json = nlohmann::json;
using namespace wisent::algorithms; 

class JsonToWisent : public json::json_sax_t 
{
  private:
    // memory storage
    WisentRootExpression *root;
    ISharedMemorySegment *sharedMemory;

    // flags & configs
    bool disableRLE;
    bool disableCsvHandling;
    std::string const &csvPrefix;
    bool enableColumnCompression; 
    std::unordered_map<std::string, ColumnMetaData> processedColumns; 

    /* counters & stacks
     *
     *  newExpressionIndex
     *      index of the next expression to be added
     *
     *  layerIndex
     *      +1 when a new expression starts
     *      -1 when an expression ends
     *
     *  cumulArgCountPerLayer: {s0, s1, s2, s3, ..., sn}
     *      partial_sum'ed from argumentCountPerLayer
     *      - The (i-1)-th element gives the first child argument index for the i-th expression
     *          (e.g. the 3rd expression has children from index s2 from this list)
     *         and gets updated to the last child index offset when ending an expression
     *         (keeps track of the *processed* argument count for each layer)
     *      - The last element gives the total number of arguments
     *
     *  wasKeyValue: {false, false, true, false, ...}
     *      - when a key is handled: the corresponding layer set to true ("is handling a key-value pair")
     *      - when (any other) instance method is handled: 
     *          - if true: ends the (key-value pair) expression, set wasKeyValue to false again
     *          - if false: does not end the expression
     *
     *  expressionIndexStack: {0, offset1, ...} 
     *      - push_back (first child index offset) when a new expression starts
     *      - pop_back when ending the expression
     *
     *  argumentIteratorStack: {it0, it1, ...}
     *      - push_back 0 when a new expression starts
     *      - increment by 1 everytime an child argument is added
     *      - pop_back when the expression ends
     *
     *  repeatedArgumentTypeCount
     *      number of repeated argument types in a row
    */
    uint64_t newExpressionIndex{0};
    uint64_t layerIndex{0};
    std::vector<uint64_t> cumulArgCountPerLayer;
    std::vector<bool> wasKeyValue;
    std::vector<uint64_t> expressionIndexStack{0};
    std::vector<uint64_t> argumentIteratorStack{0};
    uint64_t repeatedArgumentTypeCount; 

  public:
    // Constructor for serializer
    JsonToWisent(
        uint64_t expressionCount,
        std::vector<uint64_t> &&argumentCountPerLayer,
        ISharedMemorySegment *sharedMemory,
        std::string const &csvPrefix, 
        bool disableRLE, 
        bool disableCsvHandling
    ): 
        root(nullptr),
        cumulArgCountPerLayer(std::move(argumentCountPerLayer)),
        sharedMemory(sharedMemory), 
        csvPrefix(csvPrefix),
        disableRLE(disableRLE), 
        disableCsvHandling(disableCsvHandling),
        repeatedArgumentTypeCount(0), 
        enableColumnCompression(false)
    {
        std::partial_sum(
            cumulArgCountPerLayer.begin(),
            cumulArgCountPerLayer.end(),
            cumulArgCountPerLayer.begin()
        );
        root = allocateExpressionTree(
            cumulArgCountPerLayer.back(),  // sum of all argument counts
            expressionCount, 
            SharedMemorySegments::sharedMemoryMalloc
        );
        wasKeyValue.resize(cumulArgCountPerLayer.size(), false);
    }

    // Constructor for compressor, includes pipeline map
    JsonToWisent(
        uint64_t expressionCount,
        std::vector<uint64_t> &&argumentCountPerLayer,
        ISharedMemorySegment *sharedMemory,
        std::string const &csvPrefix, 
        bool disableRLE, 
        bool disableCsvHandling,
        std::unordered_map<std::string, ColumnMetaData> processedColumns
    ): 
        root(nullptr),
        cumulArgCountPerLayer(std::move(argumentCountPerLayer)),
        sharedMemory(sharedMemory), 
        csvPrefix(csvPrefix),
        disableRLE(disableRLE), 
        disableCsvHandling(disableCsvHandling),
        repeatedArgumentTypeCount(0), 
        enableColumnCompression(true),
        processedColumns(processedColumns)
    {
        std::partial_sum(
            cumulArgCountPerLayer.begin(),
            cumulArgCountPerLayer.end(),
            cumulArgCountPerLayer.begin()
        );
        root = allocateExpressionTree(
            cumulArgCountPerLayer.back(),
            expressionCount, 
            SharedMemorySegments::sharedMemoryMalloc
        );
        wasKeyValue.resize(cumulArgCountPerLayer.size(), false);
    }

    WisentRootExpression *getRoot() { return root; }

    bool null() override
    {
        addSymbol("Null");
        handleKeyValueEnd();
        return true;
    }

    bool boolean(bool val) override
    {
        addSymbol(val ? "True" : "False");
        handleKeyValueEnd();
        return true;
    }

    bool number_integer(number_integer_t val) override
    {
        addLong(val);
        handleKeyValueEnd();
        return true;
    }

    bool number_unsigned(number_unsigned_t val) override
    {
        addLong(val);
        handleKeyValueEnd();
        return true;
    }

    bool number_float(number_float_t val, const string_t & /*s*/) override
    {
        addDouble(val);
        handleKeyValueEnd();
        return true;
    }

    bool string(string_t &val) override
    {
        if (!handleCsvFile(val)) {
            addString(val);
        }
        handleKeyValueEnd();
        return true;
    }

    bool start_object(std::size_t /*elements*/) override
    {
        startExpression("Object");
        return true;
    }

    bool end_object() override
    {
        endExpression();
        handleKeyValueEnd();
        return true;
    }

    bool start_array(std::size_t /*elements*/) override
    {
        startExpression("List");
        return true;
    }

    bool end_array() override
    {
        endExpression();
        handleKeyValueEnd();
        return true;
    }

    bool key(string_t &val) override
    {
        startExpression(val);
        wasKeyValue[layerIndex] = true;
        return true;
    }

    void handleKeyValueEnd()
    {
        if (wasKeyValue[layerIndex]) {
            wasKeyValue[layerIndex] = false;
            endExpression();
        }
    }

    bool binary(json::binary_t &val) override
    {
        throw std::runtime_error("binary value not implemented");
    }

    bool parse_error(
        std::size_t position, 
        const std::string &last_token,
        const json::exception &ex) override
    {
        throw std::runtime_error(
            "parse_error(position=" + std::to_string(position)
            + ", last_token=" + last_token + ",\n            "
            + "ex=" + std::string(ex.what()) + ")");
    }

  private:
    uint64_t getNextArgumentIndex()
    {
        // next index = current expression's child index offset + the i-th (currently handling) child argument
        return getSubexpressionsBuffer(root)[expressionIndexStack.back()].firstChildOffset 
                + argumentIteratorStack.back()++;
    }

    void applyTypeRLE(std::uint64_t argIndex)
    {
        if (disableRLE) {
            return;
        }
        if (repeatedArgumentTypeCount == 0) {
            repeatedArgumentTypeCount = 1;
            return;
        }
        if (getArgumentTypesBuffer(root)[argIndex - 1] != getArgumentTypesBuffer(root)[argIndex]) 
        {
            resetTypeRLE(argIndex);
            repeatedArgumentTypeCount = 1;
            return;
        }
        ++repeatedArgumentTypeCount;
    }

    void resetTypeRLE(std::uint64_t endIndex)
    {
        if (repeatedArgumentTypeCount >= WisentArgumentType_RLE_MINIMUM_SIZE) 
        {
            setRLEArgumentFlagOrPropagateTypes(
                root, 
                endIndex - repeatedArgumentTypeCount,
                repeatedArgumentTypeCount
            );
        }
        repeatedArgumentTypeCount = 0;
    }

    void addLong(std::int64_t input)
    {
        uint64_t argIndex = getNextArgumentIndex();
        *makeLongArgument(root, argIndex) = input;  // store value directly
        applyTypeRLE(argIndex);
    }

    void addDouble(double_t input)
    {
        uint64_t argIndex = getNextArgumentIndex();
        *makeDoubleArgument(root, argIndex) = input;   // store value directly
        applyTypeRLE(argIndex);
    }

    void addString(std::string const &input)
    {
        // index offset from the start of the string buffer
        size_t storedStringOffset = storeString(
            &root, 
            input.c_str(), 
            SharedMemorySegments::sharedMemoryRealloc
        );

        uint64_t argIndex = getNextArgumentIndex();
        *makeStringArgument(root, argIndex) = storedStringOffset;
        applyTypeRLE(argIndex);
    }

    void addSymbol(std::string const &symbol)
    {
        // index offset from the start of the string buffer
        size_t storedStringOffset = storeString(
            &root, 
            symbol.c_str(), 
            SharedMemorySegments::sharedMemoryRealloc
        );

        uint64_t argIndex = getNextArgumentIndex();
        *makeSymbolArgument(root, argIndex) = storedStringOffset;
        applyTypeRLE(argIndex);
    }

    void addByteArray(const std::vector<uint8_t> byteArray)  
    {
        // stores the byte array in the string buffer
        // returns the index offset (relative to the start of the string buffer)
        auto storedBytesOffset = storeBytes(
            &root, 
            byteArray,
            SharedMemorySegments::sharedMemoryRealloc
        );

        uint64_t argIndex = getNextArgumentIndex();
        *makeByteArrayArgument(root, argIndex) = storedBytesOffset;
        applyTypeRLE(argIndex);
    }

    void addExpression(size_t newExpressionIndex)
    {
        uint64_t argIndex = getNextArgumentIndex();
        *makeExpressionArgument(root, argIndex) = newExpressionIndex;
        resetTypeRLE(argIndex);
    }

    void startExpression(std::string const &head)
    {
        // make argument & type
        addExpression(newExpressionIndex);

        // store head name in the string buffer
        size_t storedStringOffset = storeString(
            &root, 
            head.c_str(), 
            SharedMemorySegments::sharedMemoryRealloc
        );

        // make subexpression 
        unsigned long startChildOffset = cumulArgCountPerLayer[layerIndex++];
        WisentExpression newSubexpression = WisentExpression{
            storedStringOffset,     // name in the string buffer
            startChildOffset,       // first child index in arguments buffer
            0                        // not known yet; set during endExpression()
        };
        *makeExpression(root, newExpressionIndex) = newSubexpression; 

        // update stacks
        argumentIteratorStack.push_back(0);
        expressionIndexStack.push_back(newExpressionIndex);
        newExpressionIndex++; 
    }

    void endExpression()
    {
        // update last child index
        WisentExpression &expression = getSubexpressionsBuffer(root)[expressionIndexStack.back()];
        expression.lastChildOffset = expression.firstChildOffset + argumentIteratorStack.back();

        resetTypeRLE(expression.lastChildOffset);

        // layer finished, pop stacks
        argumentIteratorStack.pop_back();
        expressionIndexStack.pop_back();
        cumulArgCountPerLayer[--layerIndex] = expression.lastChildOffset;
    }

    bool handleCsvFile(std::string const &filename)
    {
        if (disableCsvHandling) 
        {
            return false;
        }
        auto extPos = filename.find_last_of(".");
        if (extPos == std::string::npos || filename.substr(extPos) != ".csv") 
        {
            return false;
        }
        startExpression("Table");
        auto doc = openCsvFile(csvPrefix + filename);
        for (auto const &columnName : doc.GetColumnNames()) 
        {
            if (enableColumnCompression) 
            {
                if (processedColumns.find(columnName) != processedColumns.end())
                {
                    handleCsvColumnWithCompression(
                        doc, 
                        columnName, 
                        processedColumns[columnName]
                    );
                    continue;
                }
            }

            if (!handleCsvColumn<int64_t>(
                    doc, 
                    columnName, 
                    [this](auto val) { addLong(val); })
            ) {
                if (!handleCsvColumn<double_t>(
                    doc, 
                    columnName, 
                    [this](auto val) { addDouble(val); })
                ) {
                    if (!handleCsvColumn<std::string>(
                        doc, 
                        columnName, 
                        [this](auto const &val) { addString(val); })
                    ) {
                        throw std::runtime_error("failed to handle csv column: '" + columnName + "'");
                    }
                }
            }
        }
        endExpression();
        return true;
    }

    template <typename T, typename Func>
    bool handleCsvColumn(
        rapidcsv::Document const &doc,
        std::string const &columnName, 
        Func &&addValueFunc)
    {
        auto column = loadCsvData<T>(doc, columnName);
        if (column.empty()) {
            return false;
        }
        startExpression(columnName);
        for (auto const &val : column) 
        {
            val ? addValueFunc(*val) : addSymbol("Missing");
        }
        endExpression();
        return true;
    }

    /*
     * Column metadata is handled as if it was a subexpression
     *
     * "ColumnName": {
     *     "numberOfValues": "<columnMetadata.numerOfValues>",
     *     "totalUncompressedSize": "<columnMetadata.totalUncompressedSize>",
     *     "totalCompressedSize": "<columnMetadata.totalCompressedSize>",
     *     "physicalType": "<columnMetadata.physicalType>",
     *     "encodingType": "<columnMetadata.encodingType>",
     *     "compressionType": "<columnMetadata.compressionType>",
     *     "pages": [
     *       {
     *         "pageType": "<pageHeader.pageType>",
     *         "numberOfValues": "<pageHeader.numberOfValues>",
     *         "firstRowIndex": "<pageHeader.firstRowIndex>",
     *         "uncompressedPageSize": "<pageHeader.uncompressedPageSize>",
     *         "compressedPageSize": "<pageHeader.compressedPageSize>",
     *         "nullCount": "<pageHeader.pageStatistics.nullCount>"
     *         "distinctCount": "<pageHeader.pageStatistics.distinctCount>"
     *         "minValue": "<pageHeader.pageStatistics.minInt/minDouble/minString>",
     *         "maxValue": "<pageHeader.pageStatistics.maxInt/maxDouble/maxString>",
     *         "isDictionaryPage": "<pageHeader.isDictionaryPage>",
     *         "dictionaryPageSize": "<pageHeader.dictionaryPageSize>" / "missing",
     *         "byteArrayOffset": "<pageHeader.byteArrayOffset>"   // (compressed data pointer)
     *       }, 
     *       ...
     *     ]
     *   }
    */

    void addColumnMetaDataExpressions(ColumnMetaData const &columnMetaData)
    {
        // KEY_VALUE_PAIR_PER_COLUMNMETADATA
        startExpression("numberOfValues");
        addLong(columnMetaData.numberOfValues);
        endExpression();

        startExpression("totalUncompressedSize");
        addLong(columnMetaData.totalUncompressedSize);
        endExpression();

        startExpression("totalCompressedSize");
        addLong(columnMetaData.totalCompressedSize);
        endExpression();

        startExpression("physicalType");
        addLong(static_cast<std::int64_t>(columnMetaData.physicalType));
        endExpression();

        startExpression("encodingType");
        addLong(static_cast<std::int64_t>(columnMetaData.encodingType)); 
        endExpression();

        startExpression("compressionType");
        addLong(static_cast<std::int64_t>(columnMetaData.compressionType)); 
        endExpression();

        startExpression("pages");
        for (const auto &pageHeader : columnMetaData.pageHeaders) 
        {
            addPageHeader(pageHeader, columnMetaData.physicalType);
        }
        endExpression();
    }

    void addPageHeader(
        PageHeader const &pageHeader, 
        PhysicalType const &physicalType
    ) {
        startExpression("Page");

        // EXPRESSION_COUNT_PER_PAGE_HEADER
        startExpression("pageType");
        addLong(static_cast<std::int64_t>(pageHeader.pageType));
        endExpression();

        startExpression("numberOfValues");
        addLong(pageHeader.numberOfValues);
        endExpression();

        startExpression("firstRowIndex");
        addLong(pageHeader.firstRowIndex);
        endExpression();

        startExpression("uncompressedPageSize");
        addLong(pageHeader.uncompressedPageSize);
        endExpression();

        startExpression("compressedPageSize");
        addLong(pageHeader.compressedPageSize);
        endExpression();

        startExpression("nullCount");
        addLong(pageHeader.pageStatistics.nullCount);
        endExpression();

        startExpression("distinctCount");
        addLong(pageHeader.pageStatistics.distinctCount);
        endExpression();
        
        switch (physicalType) 
        {
            case PhysicalType::INT64:
                startExpression("minValue");
                addLong(pageHeader.pageStatistics.minInt.value());
                endExpression();
                startExpression("maxValue");
                addLong(pageHeader.pageStatistics.maxInt.value());
                endExpression();
                break;

            case PhysicalType::DOUBLE:
                startExpression("minValue");
                addDouble(pageHeader.pageStatistics.minDouble.value());
                endExpression();
                startExpression("maxValue");
                addDouble(pageHeader.pageStatistics.maxDouble.value());
                endExpression();
                break;

            case PhysicalType::BYTE_ARRAY:
                startExpression("minValue");
                addString(pageHeader.pageStatistics.minString.value());
                endExpression();
                startExpression("maxValue");
                addString(pageHeader.pageStatistics.maxString.value());
                endExpression();
                break;

            default:
                throw std::runtime_error("Unsupported physical type for minValue");
        }

        startExpression("isDictionaryPage");
        addSymbol(pageHeader.isDictionaryPage ? "True" : "False");
        endExpression();

        if (pageHeader.dictionaryPageSize) 
        {
            startExpression("dictionaryPageSize");
            addLong(static_cast<std::int64_t>(pageHeader.dictionaryPageSize.value()));
            endExpression();
        }

        startExpression("pageData"); 
        addByteArray(pageHeader.byteArray); 
        endExpression();

        endExpression(); 
    }

    void handleCsvColumnWithCompression(
        rapidcsv::Document const &doc,
        std::string const &columnName, 
        ColumnMetaData const &columnMetaData
    ) {
        startExpression(columnName); 
        addColumnMetaDataExpressions(columnMetaData); 
        endExpression(); 
    }
};

