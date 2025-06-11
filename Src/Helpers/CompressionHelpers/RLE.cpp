#include <vector>
#include <cstddef>
#include <cstdint>
#include "../Result.hpp"
#include "RLE.hpp"

Result<std::vector<uint8_t>> wisent::algorithms::RLE::compress(const std::vector<uint8_t>& input) 
{
    Result<std::vector<uint8_t>> result;

    if (input.empty()) {
        return makeError<std::vector<uint8_t>>("Input vector is empty");
    }

    std::vector<uint8_t> output;
    output.reserve(input.size() * 2);  // Worst case: no repeats

    size_t inPos = 0;
    while (inPos < input.size()) {
        uint8_t current = input[inPos];
        size_t runLength = 1;

        while (inPos + runLength < input.size() &&
                input[inPos + runLength] == current &&
                runLength < 255) {
            runLength++;
        }

        output.push_back(static_cast<uint8_t>(runLength));
        output.push_back(current);

        inPos += runLength;
    }

    return makeResult<std::vector<uint8_t>>(output, &result);
}

Result<std::vector<uint8_t>> wisent::algorithms::RLE::decompress(const std::vector<uint8_t>& input) 
{
    Result<std::vector<uint8_t>> result;

    if (input.empty() || input.size() % 2 != 0) {
        return makeError<std::vector<uint8_t>>("Invalid or corrupted RLE input data");
    }

    std::vector<uint8_t> output;

    for (size_t i = 0; i < input.size(); i += 2) {
        uint8_t runLength = input[i];
        uint8_t value = input[i + 1];

        output.insert(output.end(), runLength, value);
    }

    return makeResult<std::vector<uint8_t>>(output, &result);
}
