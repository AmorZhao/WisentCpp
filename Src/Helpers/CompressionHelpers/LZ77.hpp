#pragma once

#include <cstdint>
#include <vector>

#include "../../Helpers/Result.hpp"

namespace wisent::algorithms 
{
    struct LZ77 
    {
        static Result<std::vector<uint8_t>> compress(
            const std::vector<uint8_t>& input, 
            int64_t windowSize = 4096, 
            int64_t lookaheadBufferSize = 18
        );
        
        static Result<std::vector<uint8_t>> decompress(
            const std::vector<uint8_t>& input
        );
    }; 
}

