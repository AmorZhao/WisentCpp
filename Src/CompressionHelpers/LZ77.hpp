#ifndef COMPRESSION_ALGORITHMS_LZ77_HPP
#define COMPRESSION_ALGORITHMS_LZ77_HPP

#include <cstdint>
#include <vector>

namespace LZ77 
{
    struct LZ77Coder {
        static std::vector<uint8_t> compress(
            const std::vector<uint8_t>& input, 
            int windowSize = 4096, 
            int lookaheadBufferSize = 18
        );

        static std::vector<uint8_t> decompress(const std::vector<uint8_t>& input);
    }; 
} // LZ77

#endif // COMPRESSION_ALGORITHMS_LZ77_HPP
