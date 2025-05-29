#pragma once
#ifndef FSE_HPP
#define FSE_HPP

#include "../Helpers/Result.hpp"

namespace wisent::algorithms 
{
    struct FSE 
    {
        static Result<size_t> compress(
            const std::byte* input,
            const size_t inputSize,
            const std::byte* output, 
            bool verbose = false
        );
        static Result<size_t> decompress(
            const std::byte* input,
            const size_t inputSize, 
            const std::byte* output, 
            bool verbose = false
        );
    };
} // FSE

#endif // FSE_HPP
