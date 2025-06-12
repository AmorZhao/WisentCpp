#include "WisentCompressor.hpp"
#include "../Helpers/WisentHelpers/JsonToWisent.hpp"
#include "../Helpers/CsvLoading.hpp"
#include "../Helpers/ISharedMemorySegment.hpp"
#include "CompressionPipeline.hpp"
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <fstream> 
#include <unordered_map>

void handleCsvColumnWithCompression(
    rapidcsv::Document const &doc,
    std::string const &columnName, 
    CompressionPipeline const &pipeline, 
    ColumnMetaData &metadata, 
    Result<WisentRootExpression*> result
) {
    std::optional<ColumnDataType> columnData = tryLoadColumn(doc, columnName); 
    std::vector<std::vector<uint8_t>> encodedData;

    std::visit([&](auto&& data) 
    {
        using T = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<T, std::vector<int64_t>>) 
        {
            encodedData = encodeIntColumn(
                data, 
                metadata
            );
        } 
        else if constexpr (std::is_same_v<T, std::vector<double>>) 
        {
            encodedData = encodeDoubleColumn(
                data,
                metadata
            );
        } 
        else if constexpr (std::is_same_v<T, std::vector<std::string>>) 
        {
            encodedData = encodeStringColumn(
                data, 
                metadata
            );
        } 
        else 
        {
            result.setError("Unhandled column data type in visitor.");
            return; 
        }
    }, *columnData);

    for (size_t i = 0; i < encodedData.size(); ++i)
    {
        Result<std::vector<uint8_t>> compressedData = pipeline.compress(
            encodedData[i]
        );
        if (!compressedData.success()) 
        {
            std::string errorMessage = "Failed to compress column '" + columnName + "': " + compressedData.getError();
            result.setError(errorMessage);
            return; 
        }
        std::vector<uint8_t> compressedDataValue = compressedData.getValue(); 
        metadata.pageHeaders[i].compressedPageSize = compressedDataValue.size();
        metadata.pageHeaders[i].byteArray = std::move(compressedDataValue);
        metadata.totalCompressedSize += compressedDataValue.size();
    }
    metadata.compressionTypes = pipeline.getPipeline();
}

Result<WisentRootExpression*> wisent::compressor::CompressAndLoadJson(
    std::string const& filepath, 
    std::string const& filename,
    std::string const& csvPrefix, 
    std::unordered_map<std::string, CompressionPipeline> &compressionPipelineMap,
    bool disableRLE,
    bool disableCsvHandling, 
    bool forceReload, 
    bool verbose
) {
    Result<WisentRootExpression*> result; 

    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(filename);
    if (!forceReload && sharedMemory->exists() && !sharedMemory->isLoaded()) 
    {
        sharedMemory->load();
    }
    if (sharedMemory->isLoaded()) 
    {
        if (!forceReload) 
        {
            WisentRootExpression *loadedValue = reinterpret_cast<WisentRootExpression *>(
                sharedMemory->getBaseAddress()
            );
            result.setValue(loadedValue);
            return result;
        }
        sharedMemory->erase();
        SharedMemorySegments::getSharedMemorySegments().erase(filename);
        sharedMemory = SharedMemorySegments::createOrGetMemorySegment(filename);
    }
    SharedMemorySegments::setCurrentSharedMemory(sharedMemory);

    std::ifstream ifs(filepath);
    if (!ifs.good()) 
    {
        std::string errorMessage = "failed to read: " + filepath;
        result.setError(errorMessage);
        return result;
    }

    // compress columns
    // count & calculate the total size needed
    uint64_t expressionCount = 0;
    std::vector<uint64_t> argumentCountPerLayer;
    argumentCountPerLayer.reserve(16);
    std::unordered_map<std::string, ColumnMetaData> processedColumns; 
    json _ = json::parse(
        ifs, 
        [                   // lambda captures
            &csvPrefix, 
            &disableCsvHandling, 
            &expressionCount,
            &argumentCountPerLayer, 
            &compressionPipelineMap,
            &processedColumns, 
            layerIndex = uint64_t{0},
            wasKeyValue = std::vector<bool>(16), 
            result,
            verbose
        ](                  // lambda params
            int depth, 
            json::parse_event_t event, 
            json &parsed
        ) mutable {
            if (wasKeyValue.size() <= depth) 
            {
                wasKeyValue.resize(wasKeyValue.size() * 2, false);
            }
            if (argumentCountPerLayer.size() <= layerIndex) 
            {
                argumentCountPerLayer.resize(layerIndex + 1, 0);
            }

            switch (event) {
                case json::parse_event_t::key: 
                    expressionCount++;
                    argumentCountPerLayer[layerIndex]++;
                    wasKeyValue[depth] = true;
                    layerIndex++;
                    return true;
            
                case json::parse_event_t::object_start:
                case json::parse_event_t::array_start:
                    expressionCount++;
                    argumentCountPerLayer[layerIndex]++;
                    layerIndex++;
                    return true;

                case json::parse_event_t::object_end:
                case json::parse_event_t::array_end:
                    layerIndex--;
                    if (wasKeyValue[depth]) 
                    {
                        wasKeyValue[depth] = false;
                        layerIndex--;
                    }
                    return true;

                case json::parse_event_t::value:
                    argumentCountPerLayer[layerIndex]++;
                    if (!disableCsvHandling && parsed.is_string()) 
                    {
                        std::string filename = parsed.get<std::string>();
                        size_t extPos = filename.find_last_of(".");
                        if (extPos != std::string::npos && filename.substr(extPos) == ".csv") 
                        {
                            if (verbose)
                            {
                                std::cout << "Handling csv file: " << filename << std::endl;
                            }
                            rapidcsv::Document doc = openCsvFile(csvPrefix + filename);
                            size_t rows = doc.GetRowCount();
                            size_t cols = doc.GetColumnCount();

                            expressionCount++; // Table expression
                            argumentCountPerLayer[layerIndex + 1] += cols; // Columns layer
                            expressionCount += cols;

                            const size_t NumTableLayers = 2; // ColumnName & Data
                            if (argumentCountPerLayer.size() <= layerIndex + NumTableLayers) 
                            {
                                argumentCountPerLayer.resize(layerIndex + NumTableLayers + 1, 0);
                            }

                            size_t matchedColumns = 0;
                            for (size_t col = 0; col < cols; ++col) 
                            {
                                std::string columnName = doc.GetColumnName(col);
                                if (compressionPipelineMap.find(columnName) != compressionPipelineMap.end()) 
                                {
                                    matchedColumns++;

                                    if (argumentCountPerLayer.size() <= layerIndex + 5) 
                                    {
                                        argumentCountPerLayer.resize(layerIndex + 5 + 1, 0);
                                    }

                                    CompressionPipeline pipeline = compressionPipelineMap[columnName]; 

                                    ColumnMetaData columnMetaData; 
                                    handleCsvColumnWithCompression(
                                        doc, 
                                        columnName, 
                                        pipeline, 
                                        columnMetaData, 
                                        result
                                    ); 
                                    if (verbose) 
                                    {
                                        std::cout << "Handling column: " << columnName << std::endl;
                                        std::cout << "using compression pipeline: "; 
                                        for (const CompressionType &step : pipeline.getPipeline()) 
                                        {
                                            std::cout << compressionTypeToString(step) << " ";
                                        }
                                        std::cout << std::endl;
                                    }

                                    size_t pageCount = columnMetaData.pageHeaders.size();

                                    // layer +2 arguments: each Metadata entry's key
                                    argumentCountPerLayer[layerIndex + 2] += KEY_VALUE_PAIR_PER_COLUMNMETADATA; 
                                    expressionCount += KEY_VALUE_PAIR_PER_COLUMNMETADATA;

                                    // layer +3 arguments: each Metadata entry's value 
                                    //                      + each CompressionType in pipeline
                                    //                      + each Page object in "pages" entry
                                    argumentCountPerLayer[layerIndex + 3] += KEY_VALUE_PAIR_PER_COLUMNMETADATA - 2 
                                        + columnMetaData.compressionTypes.size() + pageCount;
                                    expressionCount += pageCount;

                                    // layer +4 arguments: each Page object's PageHeader entry's key
                                    argumentCountPerLayer[layerIndex + 4] += pageCount * EXPRESSION_COUNT_PER_PAGE_HEADER; 
                                    expressionCount += pageCount * EXPRESSION_COUNT_PER_PAGE_HEADER;

                                    // layer +5 arguments: each Page object's PageHeader entry's value
                                    argumentCountPerLayer[layerIndex + 5] += pageCount * EXPRESSION_COUNT_PER_PAGE_HEADER; 

                                    processedColumns[columnName] = std::move(columnMetaData);
                                    continue; 
                                }

                                // else: no compression found for this column, simply add flat data
                                argumentCountPerLayer[layerIndex + 2] += rows; 
                            }
                        }
                    }
                    if (wasKeyValue[depth]) 
                    {
                        wasKeyValue[depth] = false;
                        layerIndex--;
                    }
                    return true;

                default:
                    return true;   // never reached
            }
        }
    );

    if (result.hasError()) 
    {
        ifs.close();
        return result; 
    }

    JsonToWisent jsonToWisent(
        expressionCount,
        std::move(argumentCountPerLayer),
        sharedMemory,
        csvPrefix,
        disableRLE,
        disableCsvHandling,
        processedColumns
    );

    // 2nd traversal: parse and populate 
    ifs.seekg(0);
    json::sax_parse(ifs, &jsonToWisent);
    ifs.close();

    result.setValue(jsonToWisent.getRoot());
    return result; 
}
