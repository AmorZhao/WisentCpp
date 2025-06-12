#pragma once
#include "../Result.hpp"
#include <cstdint>
#include <vector>
#include <stdexcept>

namespace wisent::algorithms 
{
    struct FSE 
    {
        static Result<std::vector<uint8_t>> compress(
            const std::vector<uint8_t>& input, 
            bool verbose = false
        );
        static Result<std::vector<uint8_t>> decompress(
            const std::vector<uint8_t>& input, bool verbose = false
        );
    };
} // FSE


