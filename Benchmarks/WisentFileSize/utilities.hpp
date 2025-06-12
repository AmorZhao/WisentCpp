#include <benchmark/benchmark.h>
#include <boost/dynamic_bitset.hpp>
#include "config.hpp"

namespace benchmark
{
    namespace utilities 
    {
        void BsonSerialize(
            std::string csvPath
        ); 

        void JsonSerialize(
            std::string csvPath
        ); 

        void WisentSerialize(
            std::string csvPath
        );

        inline std::string GetCsvPath(const std::string& subDir) {
            return CsvPath + subDir + "/";
        }
    }
}
