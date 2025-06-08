#pragma once
#ifndef BITPACKING_HPP
#define BITPACKING_HPP

#include <cstdint>
#include <vector>
#include <cmath>

namespace wisent::algorithms
{
    struct BitPacking
    {
        static uint8_t requiredBits(int64_t maxValue);
        
        static std::vector<uint8_t> pack(
            const std::vector<int64_t>& values, 
            uint8_t bitsPerValue
        );

        static std::vector<int64_t> unpack(
            const std::vector<uint8_t>& buffer, 
            size_t valueCount, 
            uint8_t bitsPerValue
        );
    }; 
}

#endif // BITPACKING_HPP
