#include "RLE.hpp"

Result<size_t> wisent::algorithms::RLE::compress(
    const std::byte* input,
    const size_t inputSize,
    std::byte* output
) {
    if (input == nullptr || output == nullptr)  return makeError<size_t>("Invalid input or output buffer");

    size_t inPos = 0;
    size_t outPos = 0;

    while (inPos < inputSize) {
        std::byte current = input[inPos];
        size_t runLength = 1;

        while (inPos + runLength < inputSize && 
            input[inPos + runLength] == current && 
            runLength < 255) {
            runLength++;
        }

        output[outPos++] = static_cast<std::byte>(runLength);
        output[outPos++] = current;

        inPos += runLength;
    }

    return makeResult<size_t>(outPos);
}

Result<size_t> wisent::algorithms::RLE::decompress(
    const std::byte* input,
    const size_t inputSize, 
    std::byte* output
) {
    if (input == nullptr || output == nullptr)  return makeError<size_t>("Invalid input or output buffer");

    size_t inPos = 0;
    size_t outPos = 0;

    while (inPos < inputSize) 
    {
        std::byte count = input[inPos++];
        std::byte value = input[inPos++];

        size_t runLength = static_cast<unsigned char>(count);
        for (size_t i = 0; i < runLength; ++i) {
            output[outPos++] = value;
        }
    }
    return makeResult<size_t>(outPos);
}