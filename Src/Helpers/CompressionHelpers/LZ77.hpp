#pragma once
#ifndef LZ77_HPP
#define LZ77_HPP

#include "../../Helpers/Result.hpp"

namespace wisent::algorithms 
{
    struct LZ77 {
        static Result<size_t> compress(
            const std::byte* input,
            size_t inputSize,
            std::byte* output,
            size_t windowSize = 4096,
            size_t lookaheadBufferSize = 18
        );

        Result<size_t> decompress(
            const std::byte* input,
            size_t inputSize,
            std::byte* output
        );
    }; 
} // LZ77

#endif // LZ77_HPP
