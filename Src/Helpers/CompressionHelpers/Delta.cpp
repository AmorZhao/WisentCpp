#include <vector>
#include <cstddef>
#include <cstdint>
#include "../Result.hpp"
#include "Delta.hpp"

Result<std::vector<uint8_t>> wisent::algorithms::DELTA::compress(const std::vector<uint8_t>& input) 
{
    Result<std::vector<uint8_t>> result;

    if (input.empty()) {
        return makeError<std::vector<uint8_t>>("Input vector is empty");
    }

    std::vector<uint8_t> output;
    output.reserve(input.size());
    output.push_back(input[0]);

    for (size_t i = 1; i < input.size(); ++i) {
        output.push_back(static_cast<uint8_t>(input[i] - input[i - 1]));
    }

    return makeResult<std::vector<uint8_t>>(output, &result);
}

Result<std::vector<uint8_t>> wisent::algorithms::DELTA::decompress(const std::vector<uint8_t>& input) 
{
    Result<std::vector<uint8_t>> result;

    if (input.empty()) {
        return makeError<std::vector<uint8_t>>("Input vector is empty");
    }

    std::vector<uint8_t> output;
    output.reserve(input.size());
    output.push_back(input[0]);

    for (size_t i = 1; i < input.size(); ++i) {
        output.push_back(static_cast<uint8_t>(output[i - 1] + input[i]));
    }

    return makeResult<std::vector<uint8_t>>(output, &result);
}
