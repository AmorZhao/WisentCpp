#include "RLE.hpp"
#include "LZ77.hpp"
#include "Delta.hpp"
// #include "FSE.hpp"
// #include "Huffman.hpp"
#include "Algorithms.hpp"
#include <algorithm>
#include <stdexcept>

namespace wisent::algorithms
{
    std::vector<uint8_t> performCompression(
        CompressionType type,
        const std::vector<uint8_t>& buffer
    ) {
        switch (type) 
        {
            case CompressionType::DELTA:
                return compressWith<DELTA>(
                    reinterpret_cast<const char*>(buffer.data()),
                    buffer.size()
                );
            case CompressionType::RLE:
                return compressWith<RLE>(
                    reinterpret_cast<const char*>(buffer.data()),
                    buffer.size()
                );
            case CompressionType::LZ77:
                return compressWith<LZ77>(
                    reinterpret_cast<const char*>(buffer.data()),
                    buffer.size()
                );
            default:
                throw std::invalid_argument("Unsupported compression type");
        }
    }; 
}