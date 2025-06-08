#pragma once
#ifndef RLE_HPP
#define RLE_HPP

#include "../../Helpers/Result.hpp"

namespace wisent::algorithms 
{
    struct RLE 
    {
        static Result<size_t> compress(
            const std::byte* input,
            const size_t inputSize,
            std::byte* output
        );

        static Result<size_t> decompress(
            const std::byte* input,
            const size_t inputSize, 
            std::byte* output
        );
    }; 
} // RLE

#endif // RLE_HPP