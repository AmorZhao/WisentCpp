#pragma once
#include <string>
#include <unordered_map>
#include "../Helpers/Result.hpp"
#include "CompressionPipeline.hpp"
#include "../WisentSerializer/WisentHelpers.hpp"
#include "../WisentSerializer/JsonToWisent.hpp"
#include "../WisentSerializer/BossHelpers/BossExpression.hpp"

namespace wisent 
{
    namespace compressor 
    {
        Result<WisentRootExpression*> CompressAndLoadJson(
            std::string const &filepath, 
            std::string const &filename,
            std::string const &csvPrefix, 
            std::unordered_map<std::string, CompressionPipeline*> &compressionPipelineMap,
            bool disableRLE,
            bool disableCsvHandling
        ); 
        
        Result<WisentRootExpression*> CompressAndLoadJson(
            json const &preloadedJsonFile,
            std::unordered_map<std::string, rapidcsv::Document> const &preloadedCsvData,
            std::unordered_map<std::string, CompressionPipeline*> const &compressionPipelineMap,
            bool disableRLE = false,
            bool disableCsvHandling = false
        ); 

        Result<WisentRootExpression*> CompressAndLoadBossExpression(
            boss::Expression &&input, 
            std::unordered_map<std::string, CompressionPipeline*> &compressionPipelineMap, 
            bool dictEncodeStrings = true,
            bool dictEncodeDoublesAndLongs = false
        );
    }
}
