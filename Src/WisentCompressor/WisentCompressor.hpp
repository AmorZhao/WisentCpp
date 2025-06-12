#pragma once
#include <string>
#include <unordered_map>
#include "../Helpers/Result.hpp"
#include "../Helpers/WisentHelpers/WisentHelpers.hpp"
#include "CompressionPipeline.hpp"

namespace wisent 
{
    namespace compressor 
    {
        Result<WisentRootExpression*> CompressAndLoadJson(
            std::string const &filepath, 
            std::string const &filename,
            std::string const &csvPrefix, 
            std::unordered_map<std::string, CompressionPipeline> &compressionPipelineMap,
            bool disableRLE = false,
            bool disableCsvHandling = false, 
            bool forceReload = false, 
            bool verbose = false
        ); 
    }
}
