#include "LZ77.hpp"
#include <cstdint>

Result<size_t> wisent::algorithms::LZ77::compress(
    const std::byte* input,
    size_t inputSize,
    std::byte* output,
    size_t windowSize,
    size_t lookaheadBufferSize
) {
    size_t codingPosition = 0;
    size_t outputPosition = 0;

    while (codingPosition < inputSize) 
    {
        size_t bestLength = 0;
        size_t bestOffset = 0;
        uint8_t byteMask = 0xFF; 

        size_t start = (codingPosition >= windowSize) ? (codingPosition - windowSize) : 0;

        for (size_t i = start; i < codingPosition; ++i) 
        {
            size_t length = 0;
            while (
                length < lookaheadBufferSize &&
                codingPosition + length < inputSize &&
                input[i + length] == input[codingPosition + length]
            ) {
                ++length;
            }

            if (length > bestLength) 
            {
                bestLength = length;
                bestOffset = codingPosition - i;
            }
        }

        if (bestLength > 2) 
        {
            output[outputPosition++] = std::byte{0};
            output[outputPosition++] = std::byte{static_cast<uint8_t>((bestOffset >> 8) & byteMask)};
            output[outputPosition++] = std::byte{static_cast<uint8_t>(bestOffset & byteMask)};
            output[outputPosition++] = std::byte{static_cast<uint8_t>(bestLength & byteMask)};
            codingPosition += bestLength;
        } 
        else 
        {
            output[outputPosition++] = std::byte{1};
            output[outputPosition++] = input[codingPosition];
            ++codingPosition;
        }
    }
    Result<size_t> result;
    result.setValue(outputPosition);
    return result;
}; 

Result<size_t> wisent::algorithms::LZ77::decompress(
    const std::byte* input,
    size_t inputSize,
    std::byte* output
) {
    Result<size_t> result;
    size_t decodingPosition = 0;
    size_t outputPosition = 0;

    while (decodingPosition < inputSize) 
    {
        if (input[decodingPosition] == std::byte{0}) 
        {
            if (decodingPosition + 3 >= inputSize)  return makeError<size_t>("Invalid compressed data");

            size_t offset = (static_cast<uint8_t>(input[decodingPosition + 1]) << 8) |
                            static_cast<uint8_t>(input[decodingPosition + 2]);
            size_t length = static_cast<uint8_t>(input[decodingPosition + 3]);

            if (offset > outputPosition) return makeError<size_t>("Invalid offset");

            size_t sourcePosition = outputPosition - offset;
            for (size_t i = 0; i < length; ++i) 
            {
                output[outputPosition++] = output[sourcePosition + i];
            }

            decodingPosition += 4;
        } 
        else 
        {
            if (decodingPosition + 1 >= inputSize) return makeError<size_t>("Invalid compressed data");

            output[outputPosition++] = input[decodingPosition + 1];
            decodingPosition += 2;
        }
    }
    result.setValue(outputPosition);
    return result;
}; 