#pragma once
#include "WisentHelpers.hpp"
#include "../Helpers/CsvLoading.hpp"
#include "../Helpers/ISharedMemorySegment.hpp"
#include <cstdint>
#include <string>
#include <cassert>
#include <vector>
#include <sys/resource.h>
#include "../../Include/json.h"
#include "../WisentCompressor/CompressionPipeline.hpp"

using json = nlohmann::json;

class JsonToWisent : public json::json_sax_t 
{
  private:
    WisentRootExpression *root;
    std::vector<uint64_t> cumulArgCountPerLayer;
    std::vector<bool> wasKeyValue;
    std::vector<uint64_t> argumentIteratorStack{0};
    std::vector<uint64_t> expressionIndexStack{0};
    uint64_t nextExpressionIndex{0};
    uint64_t layerIndex{0};
    ISharedMemorySegment *sharedMemory;
    std::string const &csvPrefix;
    bool disableRLE;
    bool disableCsvHandling;
    uint64_t repeatedArgumentTypeCount; 
    bool enableColumnCompression; 
    std::unordered_map<std::string, CompressionPipeline*> compressionPipelineMap; 

  public:
    // Constructor for serialisation
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
            cumulArgCountPerLayer.back(),
            expressionCount, 
            SharedMemorySegments::sharedMemoryMalloc
        );
        wasKeyValue.resize(cumulArgCountPerLayer.size(), false);
    }

    // Constructor for compression, pipeline map included
    JsonToWisent(
        uint64_t expressionCount,
        std::vector<uint64_t> &&argumentCountPerLayer,
        ISharedMemorySegment *sharedMemory,
        std::string const &csvPrefix, 
        bool disableRLE, 
        bool disableCsvHandling,
        std::unordered_map<std::string, CompressionPipeline*> &compressionPipelineMap
    ): 
        root(nullptr),
        cumulArgCountPerLayer(std::move(argumentCountPerLayer)),
        sharedMemory(sharedMemory), 
        csvPrefix(csvPrefix),
        disableRLE(disableRLE), 
        disableCsvHandling(disableCsvHandling),
        repeatedArgumentTypeCount(0), 
        enableColumnCompression(true),
        compressionPipelineMap(compressionPipelineMap)
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
            "parse_error(position=" + std::to_string(position) +
            ", last_token=" + last_token +
            ",\n            ex=" + std::string(ex.what()) + ")");
    }

  private:
    uint64_t getNextArgumentIndex()
    {
        return getExpressionSubexpressions(root)[expressionIndexStack.back()].startChildOffset + argumentIteratorStack.back()++;
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
        if (getArgumentTypes(root)[argIndex - 1] != getArgumentTypes(root)[argIndex]) 
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
                root, endIndex - repeatedArgumentTypeCount,
                repeatedArgumentTypeCount);
        }
        repeatedArgumentTypeCount = 0;
    }

    void addLong(std::int64_t input)
    {
        uint64_t argIndex = getNextArgumentIndex();
        *makeLongArgument(root, argIndex) = input;
        applyTypeRLE(argIndex);
    }

    void addDouble(double_t input)
    {
        uint64_t argIndex = getNextArgumentIndex();
        *makeDoubleArgument(root, argIndex) = input;
        applyTypeRLE(argIndex);
    }

    void addString(std::string const &input)
    {
        auto storedString = storeString(
            &root, 
            input.c_str(), 
            SharedMemorySegments::sharedMemoryRealloc
        );
        uint64_t argIndex = getNextArgumentIndex();
        *makeStringArgument(root, argIndex) = storedString;
        applyTypeRLE(argIndex);
    }

    void addSymbol(std::string const &symbol)
    {
        auto storedString = storeString(
            &root, 
            symbol.c_str(), 
            SharedMemorySegments::sharedMemoryRealloc
        );
        uint64_t argIndex = getNextArgumentIndex();
        *makeSymbolArgument(root, argIndex) = storedString;
        applyTypeRLE(argIndex);
    }

    void addExpression(size_t expressionIndex)
    {
        uint64_t argIndex = getNextArgumentIndex();
        *makeExpressionArgument(root, argIndex) = expressionIndex;
        resetTypeRLE(argIndex);
    }

    void startExpression(std::string const &head)
    {
        auto expressionIndex = nextExpressionIndex++;
        addExpression(expressionIndex);
        auto storedString = storeString(
            &root, 
            head.c_str(), 
            SharedMemorySegments::sharedMemoryRealloc
        );
        auto startChildOffset = cumulArgCountPerLayer[layerIndex++];
        *makeExpression(root, expressionIndex) = WisentExpression{
            storedString, startChildOffset,
            0 // not known yet; set during endExpression()
        };
        argumentIteratorStack.push_back(0);
        expressionIndexStack.push_back(expressionIndex);
    }

    void endExpression()
    {
        auto &expression =
            getExpressionSubexpressions(root)[expressionIndexStack.back()];
        expression.endChildOffset =
            expression.startChildOffset + argumentIteratorStack.back();
        resetTypeRLE(expression.endChildOffset);
        argumentIteratorStack.pop_back();
        expressionIndexStack.pop_back();
        cumulArgCountPerLayer[--layerIndex] = expression.endChildOffset;
    }

    void addCompressedValue(const std::vector<uint8_t>compressedData)  
    {
        auto storedString = storeBytes(
            &root, 
            compressedData,
            SharedMemorySegments::sharedMemoryRealloc
        );
        uint64_t argIndex = getNextArgumentIndex();
        *makeCompressedArgument(root, argIndex) = storedString;
        applyTypeRLE(argIndex);
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
                if (compressionPipelineMap.find(columnName) != compressionPipelineMap.end())
                {
                    handleCsvColumnWithCompression(
                        doc, 
                        columnName, 
                        compressionPipelineMap[columnName]
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

    void handleCsvColumnWithCompression(
        rapidcsv::Document const &doc,
        std::string const &columnName, 
        CompressionPipeline *pipeline
    ) {
        Result<size_t> result;

        std::optional<ColumnDataType> columnData = tryLoadColumn(doc, columnName); 

        ColumnMetaData columnMetaData; 
        std::vector<std::vector<uint8_t>> pages; 

        std::visit([&](auto&& data) 
        {
            using T = std::decay_t<decltype(data)>;

            if constexpr (std::is_same_v<T, std::vector<int64_t>>) 
            {
                pages = encodeIntColumn(
                    data, 
                    columnMetaData
                );
            } 
            else if constexpr (std::is_same_v<T, std::vector<double>>) 
            {
                pages = encodeDoubleColumn(
                    data,
                    columnMetaData
                );
            } 
            else if constexpr (std::is_same_v<T, std::vector<std::string>>) 
            {
                pages = encodeStringColumn(
                    data, 
                    columnMetaData
                );
            } 
            else 
            {
                result.setError("Unhandled column data type in visitor.");
                return; 
            }
        }, *columnData);

        startCompressedColumnExpression(columnName);
        for (size_t i = 0; i < pages.size(); ++i)
        {
            std::vector<uint8_t> compressedData = pipeline->compress(
                pages[i], 
                result
            );
            columnMetaData.pageHeaders[i].compressedPageSize = compressedData.size();
            writeCompressedPages(compressedData);
        }
        endCompressedColumnExpression();
    }
};