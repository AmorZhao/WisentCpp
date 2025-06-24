#include "utilities.hpp"
#include "../../Src/WisentSerializer/WisentSerializer.hpp"
#include "../../Src/WisentCompressor/WisentCompressor.hpp"
#include "config.hpp"
#include <benchmark/benchmark.h>
#include <string>

inline void printWisentRootExpressionBufferSizes(WisentRootExpression* root) 
{
    uint64_t argumentCount = root->argumentCount;
    uint64_t expressionCount = root->expressionCount;
    size_t stringBufferBytesWritten = root->stringBufferBytesWritten;

    size_t argumentsBufferSize = sizeof(WisentArgumentValue) * argumentCount;
    size_t argumentTypesBufferSize = sizeof(WisentArgumentType) * argumentCount;
    size_t expressionsBufferSize = sizeof(WisentExpression) * expressionCount;
    size_t stringBufferSize = stringBufferBytesWritten;

    std::cout << "WisentRootExpression buffer sizes:" << std::endl;
    std::cout << "  Arguments buffer:      " << argumentsBufferSize << " bytes (" << argumentCount << " elements)" << std::endl;
    std::cout << "  Argument types buffer: " << argumentTypesBufferSize << " bytes (" << argumentCount << " elements)" << std::endl;
    std::cout << "  Expressions buffer:    " << expressionsBufferSize << " bytes (" << expressionCount << " elements)" << std::endl;
    std::cout << "  String buffer:         " << stringBufferSize << " bytes (used)" << std::endl;
}

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
    WisentRootExpression* root = result.getValue();
    if (root != nullptr) 
    {
        printWisentRootExpressionBufferSizes(root);
    } 
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
    WisentRootExpression* root = result.getValue();
    if (root != nullptr) 
    {
        printWisentRootExpressionBufferSizes(root);
    } 
}

void benchmark::utilities::parserWisentForString(WisentRootExpression* root)
{
    if (root == nullptr) return;

    uint64_t argumentCount = root->argumentCount;
    uint64_t expressionCount = root->expressionCount;
    size_t stringBufferBytesWritten = root->stringBufferBytesWritten;

    size_t argumentsBufferSize = sizeof(WisentArgumentValue) * argumentCount;
    size_t argumentTypesBufferSize = sizeof(WisentArgumentType) * argumentCount;
    size_t expressionsBufferSize = sizeof(WisentExpression) * expressionCount;
    size_t stringBufferSize = stringBufferBytesWritten;

    for (uint64_t i = 0; i < argumentCount; ++i) 
    {
        WisentArgumentType type = getArgumentTypesBuffer(root)[i];
        WisentArgumentValue value = getArgumentsBuffer(root)[i];

        if (type == WisentArgumentType::ARGUMENT_TYPE_STRING) 
        {
            const char *stringBuffer = viewString(root, static_cast<size_t>(value.asString));

            if (ColumnNameToLoad == std::string(stringBuffer)) 
            {
                std::cout << "Found string argument: " << stringBuffer << std::endl;
                return;
            }
        } 
        else if (type == WisentArgumentType::ARGUMENT_TYPE_EXPRESSION) 
        {
            WisentExpression expr = getSubexpressionsBuffer(root)[static_cast<size_t>(value.asString)];

            const char *stringBuffer = viewString(root, static_cast<size_t>(expr.symbolNameOffset));

            if (ColumnNameToLoad == std::string(stringBuffer))
            {
                std::cout << "Found expression with head string: " << stringBuffer << std::endl;
                return;
            }
        }
        // skip other types
    }
    std::cerr << "Did not find string " << ColumnNameToLoad << std::endl;
}

void benchmark::utilities::deserialiseWisentForString(WisentRootExpression* root)
{
    if (root == nullptr) return;

    uint64_t argumentCount = root->argumentCount;
    uint64_t expressionCount = root->expressionCount;
    size_t stringBufferBytesWritten = root->stringBufferBytesWritten;

    size_t argumentsBufferSize = sizeof(WisentArgumentValue) * argumentCount;
    size_t argumentTypesBufferSize = sizeof(WisentArgumentType) * argumentCount;
    size_t expressionsBufferSize = sizeof(WisentExpression) * expressionCount;
    size_t stringBufferSize = stringBufferBytesWritten;

    for (uint64_t i = 0; i < argumentCount; ++i) 
    {
        WisentArgumentType type = getArgumentTypesBuffer(root)[i];
        WisentArgumentValue value = getArgumentsBuffer(root)[i];

        if (type == WisentArgumentType::ARGUMENT_TYPE_STRING) 
        {
            const char *stringBuffer = viewString(root, static_cast<size_t>(value.asString));

            if (ColumnNameToLoad == std::string(stringBuffer)) 
            {
                std::cout << "Found string argument: " << stringBuffer << std::endl;
                return;
            }
        } 
        else if (type == WisentArgumentType::ARGUMENT_TYPE_EXPRESSION) 
        {
            WisentExpression expr = getSubexpressionsBuffer(root)[static_cast<size_t>(value.asString)];

            const char *stringBuffer = viewString(root, static_cast<size_t>(expr.symbolNameOffset));

            if (ColumnNameToLoad == std::string(stringBuffer))
            {
                std::cout << "Found expression with head string: " << stringBuffer << std::endl;
                return;
            }
        }
        // skip other types
    }
    std::cerr << "Did not find string " << ColumnNameToLoad << std::endl;
}