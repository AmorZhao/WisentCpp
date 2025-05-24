#pragma once
#ifndef HUFFMAN_HPP
#define HUFFMAN_HPP

#include "../Helpers/Result.hpp"

namespace wisent::algorithms 
{
    struct Huffman 
    {
        static Result<size_t> compress(
            const std::byte* input,
            const size_t inputSize,
            const std::byte* output
        );
        static Result<size_t> decompress(
            const std::byte* input,
            const size_t inputSize, 
            const std::byte* output
        );
    }; 
} // Huffman

#endif // HUFFMAN_HPP
