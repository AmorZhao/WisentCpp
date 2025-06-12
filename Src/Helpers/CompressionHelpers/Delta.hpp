#pragma once

#include "../../Helpers/Result.hpp"

namespace wisent::algorithms
{
    struct DELTA
    {
        static Result<std::vector<uint8_t>> compress(
            const std::vector<uint8_t>& input
        ); 

        static Result<std::vector<uint8_t>> decompress(
           const std::vector<uint8_t>& input
        );
    }; 
}
