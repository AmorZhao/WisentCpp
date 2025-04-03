#ifndef COMPRESSION_ALGORITHMS_LZ77_HPP
#define COMPRESSION_ALGORITHMS_LZ77_HPP

#include <cstdint>
#include <vector>

namespace LZ77 
{
    std::vector<uint8_t> compress(
        const std::vector<uint8_t>& input, 
        int64_t windowSize = 4096, 
        int64_t lookaheadBufferSize = 18
    );
    std::vector<uint8_t> compressFastHash(
        const std::vector<uint8_t>& input, 
        int64_t windowSize = 4096, 
        int64_t lookaheadBufferSize = 18
    );
    std::vector<uint8_t> compressFastSuffixArray(
        const std::vector<uint8_t>& input, 
        int64_t windowSize = 4096, 
        int64_t lookaheadBufferSize = 18
    );
    std::vector<uint8_t> compressFastSuffixTree(
        const std::vector<uint8_t>& input, 
        int64_t windowSize = 4096, 
        int64_t lookaheadBufferSize = 18
    );

    std::vector<uint8_t> decompress(const std::vector<uint8_t>& input);
} // LZ77

#endif // COMPRESSION_ALGORITHMS_LZ77_HPP
