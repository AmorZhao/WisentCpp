#include "utilities.hpp"
#include "../../Src/WisentSerializer/WisentSerializer.hpp"
#include "../../Src/WisentCompressor/WisentCompressor.hpp"
#include "config.hpp"
#include <benchmark/benchmark.h>
#include <memory>
#include <string>

void benchmark::utilities::WisentSerialize(
    std::string csvPath
) {
    Result<WisentRootExpression*> result =  wisent::serializer::load(
        DatasetPath + DatasetName, 
        SharedMemoryName,
        csvPath,
        DisableRLE,
        DisableCSV,
        ForceReload
    ); 
}

std::unordered_map<std::string, CompressionPipeline> 
benchmark::utilities::ConstructCompressionPipelineMap()
{
    std::unordered_map<std::string, CompressionPipeline> compressionPipelineMap;
    for (const auto& [columnName, steps] : CompressionSpecifier) 
    {
        CompressionPipeline::Builder builder;
        for (const std::string& step : steps) 
        {
            builder.addStep(step);
        }
        compressionPipelineMap[columnName] = builder.build();
    }
    return compressionPipelineMap; 
}

void benchmark::utilities::WisentCompressWithPipeline(
    std::unordered_map<std::string, CompressionPipeline> &compressionPipelineMap, 
    std::string csvPath
) {
    std::cout << "Compression Pipeline Map constructed with " 
              << compressionPipelineMap.size() << " entries." << std::endl;

    Result<WisentRootExpression*> result =  wisent::compressor::CompressAndLoadJson(
        DatasetPath+DatasetName, 
        SharedMemoryName,
        csvPath,
        compressionPipelineMap,
        DisableRLE,
        DisableCSV,
        ForceReload
    ); 

    if (!result.success())
    {
        std::cerr << "Error during compression: " << result.getError() << std::endl; 
    }
    if (result.hasWarning())
    {
        auto warnings = result.getWarnings();
        for (const auto& warning : warnings)
        {
            std::cerr << "Warning: " << warning << std::endl;
        }
    }
    // else
    // {
    //     WisentRootExpression *root = result.getValue();
    //     assert(root != nullptr);
    //     // wisent::compressor::unload(SharedMemoryName);
    //     delete root;  // Assuming WisentRootExpression has a proper destructor
    // }
}
