#include "Delta.hpp"

Result<size_t> wisent::algorithms::DELTA::compress(
    const std::byte* input,
    const size_t inputSize,
    std::byte* output
) {
    Result<size_t> result;
    if (inputSize == 0 || input == nullptr || output == nullptr) 
        return makeError<size_t>("Invalid input or output buffer");

    if (inputSize < sizeof(std::byte)) {
        result.addWarning("Input size is too small");
    }

    output[0] = input[0];
    size_t outputSize = 1;

    for (size_t i = 1; i < inputSize; i++) {
        output[outputSize++] = static_cast<std::byte>(
            static_cast<uint8_t>(input[i]) - static_cast<uint8_t>(input[i-1])
        );
    }
    return makeResult<size_t>(outputSize, &result);
}; 

Result<size_t> wisent::algorithms::DELTA::decompress(
    const std::byte* input,
    const size_t inputSize, 
    std::byte* output
) {
    if (inputSize == 0 || input == nullptr || output == nullptr) 
        return makeError<size_t>("Invalid input or output buffer");

    output[0] = input[0];
    size_t outputSize = 1;
    for (size_t i = 1; i < inputSize; i++) 
    {
        output[i] = static_cast<std::byte>(
            static_cast<uint8_t>(output[i-1]) + static_cast<uint8_t>(input[i])
        );
        outputSize++;
    }
    return makeResult<size_t>(outputSize);
}; 