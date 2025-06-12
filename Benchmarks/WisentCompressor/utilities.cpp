#include "utilities.hpp"
#include "../../Src/BsonSerializer/BsonSerializer.hpp"
#include "../../Src/WisentSerializer/WisentSerializer.hpp"
#include "../../Src/WisentCompressor/WisentCompressor.hpp"
#include "config.hpp"
#include <benchmark/benchmark.h>

void benchmark::utilities::BsonSerialize()
{
    auto result = bson::serializer::loadAsBson(
        DatasetPath + DatasetName, 
        SharedMemoryName,
        CsvPath,
        DisableCSV,
        ForceReload
    );
}

void benchmark::utilities::JsonSerialize()
{
    auto result = bson::serializer::loadAsJson(
        DatasetPath + DatasetName, 
        SharedMemoryName,
        CsvPath,
        DisableCSV,
        ForceReload
    );
}

void benchmark::utilities::WisentSerialize() 
{
    Result<WisentRootExpression*> result =  wisent::serializer::load(
        DatasetPath + DatasetName, 
        SharedMemoryName,
        CsvPath,
        DisableRLE,
        DisableCSV,
        ForceReload
    ); 
}

std::unordered_map<std::string, CompressionPipeline> 
benchmark::utilities::ConstructCompressionPipelineMap()
{
    std::unordered_map<std::string, CompressionPipeline> compressionPipelineMap;

    for (const auto& [column, steps] : CompressionSpecifier)
    {
        CompressionPipeline::Builder builder;
        for (const auto& step : steps)
        {
            builder.addStep(step);
        }
        compressionPipelineMap.emplace(column, builder.build());
    }
    return compressionPipelineMap;
}

void benchmark::utilities::WisentCompressWithPipeline(
    std::unordered_map<std::string, CompressionPipeline> &compressionPipelineMap
) {
    Result<WisentRootExpression*> result =  wisent::compressor::CompressAndLoadJson(
        DatasetPath+DatasetName, 
        SharedMemoryName,
        CsvPath,
        compressionPipelineMap,
        DisableRLE,
        DisableCSV,
        ForceReload, 
        CompressVerbose
    ); 

    if (!result.success())
    {
        std::cerr << "Error during compression: " << result.getError() << std::endl; 
    }
    else if (result.hasWarning())
    {
        for (const auto& warning : result.getWarnings())
        {
            std::cout << "Warning: " << warning << std::endl;
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
