#ifndef COMPRESSION_ALGORITHMS_FSE_HPP
#define COMPRESSION_ALGORITHMS_FSE_HPP

#include <cstdint>
#include <vector>

namespace FSE 
{
    struct FSECoder {
        static std::vector<uint8_t> compress(const std::vector<uint8_t>& input, bool verbose = false);
        
        static std::vector<uint8_t> decompress(const std::vector<uint8_t>& input, bool verbose = false);
    };
} // FSE

#endif // COMPRESSION_ALGORITHMS_FSE_HPP
