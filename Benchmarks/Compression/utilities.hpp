#include "../../Src/WisentCompressor/CompressionPipeline.hpp"
#include <benchmark/benchmark.h>
#include <boost/dynamic_bitset.hpp>
#include "config.hpp"

namespace benchmark
{
    namespace utilities 
    {
        void WisentSerialize(
            std::string csvPath
        );

        void ConstructCompressionPipelineMap(
            std::unordered_map<std::string, CompressionPipeline> &CompressionPipelineMap); 

        void WisentCompressWithPipeline(
            // std::unordered_map<std::string, CompressionPipeline> &compressionPipelineMap, 
            std::string csvPath
        ); 

        inline std::string GetCsvPath(const std::string& subDir) {
            return CsvPath + subDir + "/";
        }
    }
}
