#ifndef COMPRESSION_ALGORITHMS_HUFFMAN_HPP
#define COMPRESSION_ALGORITHMS_HUFFMAN_HPP

#include <cstdint>
#include <vector>

namespace Huffman 
{
    struct HuffmanCoder {
        static std::vector<uint8_t> compress(const std::vector<uint8_t>& input);
        static std::vector<uint8_t> decompress(const std::vector<uint8_t>& input);
    }; 
} // Huffman

#endif // COMPRESSION_ALGORITHMS_HUFFMAN_HPP
