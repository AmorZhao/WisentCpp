#include "WisentCompressor.hpp"
#include "../Helpers/CsvLoading.hpp"
#include "../Helpers/ISharedMemorySegment.hpp"
#include "../WisentSerializer/WisentSerializer.hpp"
#include "CompressionPipeline.hpp"
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <fstream> 

Result<std::pair<WisentRootExpression*, size_t>> CompressAndLoadJson(
    const char* data,
    const size_t length,
    const std::unordered_map<std::string, rapidcsv::Document> &preloadedCsvData,
    const std::unordered_map<std::string, CompressionPipeline*> &CompressionPipelineMap,
    bool disableRLE = false,
    bool disableCsvHandling = false
) {
    Result<std::pair<WisentRootExpression*, size_t>> result; 
    return makeError<std::pair<WisentRootExpression*, size_t>>("not implemented"); 

    // // compress columns
    // // count & calculate the total size needed
    // uint64_t expressionCount = 0;
    // std::vector<uint64_t> argumentCountPerLayer;
    // argumentCountPerLayer.reserve(16);
    // auto _ = json::parse(
    //     data, 
    //     [                   // lambda captures
    //         &csvPrefix, 
    //         &disableCsvHandling, 
    //         &expressionCount,
    //         &argumentCountPerLayer, 
    //         layerIndex = uint64_t{0},
    //         wasKeyValue = std::vector<bool>(16)
    //     ](                  // lambda params
    //         int depth, 
    //         json::parse_event_t event, 
    //         json &parsed
    //     ) mutable {
    //         if (wasKeyValue.size() <= depth) 
    //         {
    //             wasKeyValue.resize(wasKeyValue.size() * 2, false);
    //         }
    //         if (argumentCountPerLayer.size() <= layerIndex) 
    //         {
    //             argumentCountPerLayer.resize(layerIndex + 1, 0);
    //         }
    //         if (event == json::parse_event_t::key) 
    //         {
    //             argumentCountPerLayer[layerIndex]++;
    //             expressionCount++;
    //             wasKeyValue[depth] = true;
    //             layerIndex++;
    //             return true;
    //         }
    //         if (event == json::parse_event_t::object_start ||
    //             event == json::parse_event_t::array_start) 
    //         {
    //             argumentCountPerLayer[layerIndex]++;
    //             expressionCount++;
    //             layerIndex++;
    //             return true;
    //         }
    //         if (event == json::parse_event_t::object_end ||
    //             event == json::parse_event_t::array_end) 
    //         {
    //             layerIndex--;
    //             if (wasKeyValue[depth]) {
    //                 wasKeyValue[depth] = false;
    //                 layerIndex--;
    //             }
    //             return true;
    //         }
    //         if (event == json::parse_event_t::value) 
    //         {
    //             argumentCountPerLayer[layerIndex]++;
    //             if (!disableCsvHandling && parsed.is_string()) 
    //             {
    //                 auto filename = parsed.get<std::string>();
    //                 auto extPos = filename.find_last_of(".");
    //                 if (extPos != std::string::npos &&
    //                     filename.substr(extPos) == ".csv") 
    //                 {
    //                     std::cout << "Handling csv file: " << filename << std::endl;
    //                     auto doc = openCsvFile(csvPrefix + filename);
    //                     auto rows = doc.GetRowCount();
    //                     auto cols = doc.GetColumnCount();
    //                     static const size_t numTableLayers = 2; // Column/Data
    //                     if (argumentCountPerLayer.size() <= layerIndex + numTableLayers) 
    //                     {
    //                         argumentCountPerLayer.resize(layerIndex + numTableLayers + 1, 0);
    //                     }
    //                     expressionCount++; // Table expression
    //                     argumentCountPerLayer[layerIndex + 1] += cols; // Column expressions
    //                     expressionCount += cols;
    //                     argumentCountPerLayer[layerIndex + 2] += cols * rows; // Column data
    //                 }
    //             }
    //             if (wasKeyValue[depth]) 
    //             {
    //                 wasKeyValue[depth] = false;
    //                 layerIndex--;
    //             }
    //             return true;
    //         }
    //         return true;   // never reached
    //     }
    // );

    // JsonToWisent jsonToWisent(
    //     expressionCount,
    //     std::move(argumentCountPerLayer),
    //     sharedMemory,
    //     csvPrefix,
    //     disableRLE,
    //     disableCsvHandling,
    //     false, 
    //     false
    // );

    // // 2nd traversal: parse and populate 
    // ifs.seekg(0);
    // json::sax_parse(ifs, &jsonToWisent);
    // ifs.close();

    // std::cout << "loaded: " << path << std::endl;
    // result.setValue(jsonToWisent.getRoot());
    // return result; 
}

Result<WisentRootExpression*> wisent::compressor::CompressAndLoadJson(
    std::string const& filepath, 
    std::string const& filename,
    std::string const& csvPrefix, 
    std::unordered_map<std::string, CompressionPipeline*> &CompressionPipelineMap,
    bool disableRLE,
    bool disableCsvHandling
) {
    Result<WisentRootExpression*> result; 

    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(filename);
    // if (!forceReload && sharedMemory->exists() && !sharedMemory->isLoaded()) 
    // {
    //     sharedMemory->load();
    // }
    if (sharedMemory->isLoaded()) 
    {
        // if (!forceReload) 
        // {
        //     WisentRootExpression *loadedValue = reinterpret_cast<WisentRootExpression *>(
        //         sharedMemory->getBaseAddress()
        //     );
        //     result.setValue(loadedValue);
        //     return result;
        // }
        sharedMemory->erase();
        SharedMemorySegments::getSharedMemorySegments().erase(filename);
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
    auto _ = json::parse(
        ifs, 
        [                   // lambda captures
            &csvPrefix, 
            &disableCsvHandling, 
            &expressionCount,
            &argumentCountPerLayer, 
            layerIndex = uint64_t{0},
            wasKeyValue = std::vector<bool>(16)
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
            if (event == json::parse_event_t::key) 
            {
                argumentCountPerLayer[layerIndex]++;
                expressionCount++;
                wasKeyValue[depth] = true;
                layerIndex++;
                return true;
            }
            if (event == json::parse_event_t::object_start ||
                event == json::parse_event_t::array_start) 
            {
                argumentCountPerLayer[layerIndex]++;
                expressionCount++;
                layerIndex++;
                return true;
            }
            if (event == json::parse_event_t::object_end ||
                event == json::parse_event_t::array_end) 
            {
                layerIndex--;
                if (wasKeyValue[depth]) {
                    wasKeyValue[depth] = false;
                    layerIndex--;
                }
                return true;
            }
            if (event == json::parse_event_t::value) 
            {
                argumentCountPerLayer[layerIndex]++;
                if (!disableCsvHandling && parsed.is_string()) 
                {
                    auto filename = parsed.get<std::string>();
                    auto extPos = filename.find_last_of(".");
                    if (extPos != std::string::npos &&
                        filename.substr(extPos) == ".csv") 
                    {
                        std::cout << "Handling csv file: " << filename << std::endl;
                        auto doc = openCsvFile(csvPrefix + filename);
                        auto rows = doc.GetRowCount();
                        auto cols = doc.GetColumnCount();
                        static const size_t numTableLayers = 2; // Column/Data
                        if (argumentCountPerLayer.size() <= layerIndex + numTableLayers) 
                        {
                            argumentCountPerLayer.resize(layerIndex + numTableLayers + 1, 0);
                        }
                        expressionCount++; // Table expression
                        argumentCountPerLayer[layerIndex + 1] += cols; // Column expressions
                        expressionCount += cols;
                        argumentCountPerLayer[layerIndex + 2] += cols * rows; // Column data
                    }
                }
                if (wasKeyValue[depth]) 
                {
                    wasKeyValue[depth] = false;
                    layerIndex--;
                }
                return true;
            }
            return true;   // never reached
        }
    );

    JsonToWisent jsonToWisent(
        expressionCount,
        std::move(argumentCountPerLayer),
        sharedMemory,
        csvPrefix,
        disableRLE,
        disableCsvHandling,
        CompressionPipelineMap
    );

    // 2nd traversal: parse and populate 
    ifs.seekg(0);
    json::sax_parse(ifs, &jsonToWisent);
    ifs.close();

    result.setValue(jsonToWisent.getRoot());
    return result; 
}

Result<WisentRootExpression*> wisent::compressor::CompressAndLoadBossExpression(
    const char* data,
    size_t length,
    std::string const& csvPrefix, 
    std::unordered_map<std::string, CompressionPipeline*> &CompressionPipelineMap,
    bool disableRLE,
    bool disableCsvHandling, 
    bool forceReload
){
    Result<WisentRootExpression*> result; 
    result.setError("Not implemented");
    return result; 
}

Result<std::string> wisent::compressor::decompress(  // Todo - change return type
    std::string const& sharedMemoryName
) {
    Result<std::string> result; 
    result.setError("Not implemented");
    return result; 
}