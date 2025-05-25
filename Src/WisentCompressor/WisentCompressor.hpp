#pragma once
#include <string>
#include <unordered_map>
#include "CompressionPipeline.hpp"
#include "../WisentSerializer/WisentHelpers.h"
#include "../Helpers/Result.hpp"

namespace wisent 
{
    namespace compressor 
    {
        Result<std::pair<WisentRootExpression*, size_t>> CompressAndLoadJson(
            const char* data,
            const size_t length,
            const std::unordered_map<std::string, std::vector<char>> &preloadedCsvData,
            const std::unordered_map<std::string, CompressionPipeline*> &CompressionPipelineMap,
            bool disableRLE = false,
            bool disableCsvHandling = false
        ); 

        Result<WisentRootExpression*> CompressAndLoadJson(
            std::string const& filepath, 
            std::string const& filename,
            std::string const& csvPrefix, 
            std::unordered_map<std::string, CompressionPipeline*> &CompressionPipelineMap,
            bool disableRLE,
            bool disableCsvHandling
        ); 

        Result<WisentRootExpression*> CompressAndLoadBossExpression(
            const char* data,
            size_t length,
            std::string const& csvPrefix, 
            std::unordered_map<std::string, CompressionPipeline*> &CompressionPipelineMap,
            bool disableRLE = false,
            bool disableCsvHandling = false, 
            bool forceReload = false
        );

        Result<std::string> decompress(
            std::string const& sharedMemoryName
        );
    }
}
