#include "../../Src/WisentCompressor/CompressionPipeline.hpp"
#include <benchmark/benchmark.h>
#include <boost/dynamic_bitset.hpp>

namespace benchmark
{
    namespace utilities 
    {
        void BsonSerialize(); 

        void JsonSerialize(); 

        void WisentSerialize();

        std::unordered_map<std::string, CompressionPipeline> ConstructCompressionPipelineMap(); 

        void WisentCompressWithPipeline(
            std::unordered_map<std::string, CompressionPipeline> &compressionPipelineMap
        ); 
    }
}
