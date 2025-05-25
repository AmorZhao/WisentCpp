#include "BitPacking.hpp"
#include <stdexcept>

uint8_t wisent::algorithms::BitPacking::requiredBits(int64_t maxValue) 
{
    if (maxValue < 0) 
        throw std::invalid_argument("BitPacking only supports non-negative values.");

    return (maxValue == 0) 
            ? 1 
            : static_cast<uint8_t>(std::ceil(std::log2(maxValue + 1)));
}

std::vector<uint8_t> wisent::algorithms::BitPacking::pack(
    const std::vector<int64_t>& values, 
    uint8_t bitsPerValue
) {
    if (bitsPerValue > 64 || bitsPerValue == 0)
        throw std::invalid_argument("bitsPerValue must be between 1 and 64");

    std::vector<uint8_t> output;
    uint64_t bitBuffer = 0;
    uint8_t bitCount = 0;

    for (int64_t value : values) 
    {
        if (value < 0 || (static_cast<uint64_t>(value) >> bitsPerValue))
            throw std::invalid_argument("Value too large for specified bit width");

        bitBuffer = (bitBuffer << bitsPerValue) | static_cast<uint64_t>(value);
        bitCount += bitsPerValue;

        while (bitCount >= 8) 
        {
            bitCount -= 8;
            output.push_back(static_cast<uint8_t>((bitBuffer >> bitCount) & 0xFF));
        }
    }

    if (bitCount > 0) 
    {
        output.push_back(static_cast<uint8_t>((bitBuffer << (8 - bitCount)) & 0xFF));
    }

    return output;
}

std::vector<int64_t> wisent::algorithms::BitPacking::unpack(
    const std::vector<uint8_t>& buffer, 
    size_t valueCount, 
    uint8_t bitsPerValue
) {
    if (bitsPerValue > 64 || bitsPerValue == 0)
        throw std::invalid_argument("bitsPerValue must be between 1 and 64");

    std::vector<int64_t> output;
    uint64_t bitBuffer = 0;
    uint8_t bitCount = 0;
    size_t byteIndex = 0;

    for (size_t i = 0; i < valueCount; ++i) 
    {
        while (bitCount < bitsPerValue && byteIndex < buffer.size()) 
        {
            bitBuffer = (bitBuffer << 8) | buffer[byteIndex++];
            bitCount += 8;
        }

        if (bitCount < bitsPerValue)
            throw std::runtime_error("Not enough bits to unpack");

        bitCount -= bitsPerValue;
        int64_t value = static_cast<int64_t>((bitBuffer >> bitCount) & ((1ULL << bitsPerValue) - 1));
        output.push_back(value);
    }

    return output;
}

