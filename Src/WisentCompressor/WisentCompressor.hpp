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
        Result<WisentRootExpression*> CompressAndLoadJson(
            const char* data,
            size_t length,
            std::unordered_map<std::string, CompressionPipeline*> & CompressionPipelineMap,
            bool disableRLE = false,
            bool disableCsvHandling = false, 
            bool forceReload = false
        ); 

        Result<WisentRootExpression*> CompressAndLoadJson(
            std::string const& path, 
            std::string const& sharedMemoryName,
            std::string const& csvPrefix, 
            std::unordered_map<std::string, CompressionPipeline*> & CompressionPipelineMap,
            bool disableRLE = false,
            bool disableCsvHandling = false, 
            bool forceReload = false
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
