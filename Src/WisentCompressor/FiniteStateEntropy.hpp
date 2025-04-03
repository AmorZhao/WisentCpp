#ifndef COMPRESSION_ALGORITHMS_FSE_HPP
#define COMPRESSION_ALGORITHMS_FSE_HPP

#include <cstdint>
#include <vector>

namespace FiniteStateEntropy 
{
    std::vector<uint8_t> compress(const std::vector<uint8_t>& input, bool verbose = false);

    std::vector<uint8_t> decompress(const std::vector<uint8_t>& input, bool verbose = false);
} // FiniteStateEntropy

namespace FiniteStateEntropy_Simplified
{
    std::vector<uint8_t> compress(const std::vector<uint8_t>& input, bool verbose = false);

    std::vector<uint8_t> decompress(const std::vector<uint8_t>& input, bool verbose = false);
} // FiniteStateEntropy_Simplified

#endif // COMPRESSION_ALGORITHMS_FSE_HPP
