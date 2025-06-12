#pragma once

#include <cstdint>
#include <vector>
#include "../Result.hpp"

namespace wisent::algorithms
{
    struct Huffman
    {
        static Result<std::vector<uint8_t>> compress(const std::vector<uint8_t>& input);
        
        static Result<std::vector<uint8_t>> decompress(const std::vector<uint8_t>& input);
    }; 
}

