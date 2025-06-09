#pragma once
#ifndef DELTA_HPP
#define DELTA_HPP

#include <cstddef>
#include "../../Helpers/Result.hpp"

namespace wisent::algorithms
{
    struct DELTA
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
}

#endif // DELTA_HPP