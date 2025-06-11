#include "LZ77.hpp"
#include <cstdint>
#include <vector>
#include <algorithm>

Result<std::vector<uint8_t>> wisent::algorithms::LZ77::compress(
  const std::vector<uint8_t> &input, 
  int64_t windowSize, 
  int64_t lookaheadBufferSize
) {
    std::vector<uint8_t> compressed;
    int inputSize = input.size();
    int codingPosition = 0;

    while (codingPosition < inputSize) {
        int matchedLength = 0;
        int offset = 0;

        for (int i = std::max(static_cast<int64_t>(0), codingPosition - windowSize); 
          i < codingPosition; ++i
        ) {
            int length = 0;
            while (length < lookaheadBufferSize && codingPosition + length < inputSize &&
                   input[i + length] == input[codingPosition + length]) {
                ++length;
            }
            if (length > matchedLength) {
                matchedLength = length;
                offset = codingPosition - i;
            }
        }

        if (matchedLength > 2) {
            compressed.push_back(0);
            compressed.push_back((offset >> 8) & 0xFF);
            compressed.push_back(offset & 0xFF);
            compressed.push_back(matchedLength & 0xFF);
            codingPosition += matchedLength;
        }
        else {
            compressed.push_back(1);
            compressed.push_back(input[codingPosition]);
            ++codingPosition;
        }
    }

    Result<std::vector<uint8_t>> result = makeResult<std::vector<uint8_t>>(compressed);
    return result;
}

Result<std::vector<uint8_t>> wisent::algorithms::LZ77::decompress(
    const std::vector<uint8_t> &input
) {
    std::vector<uint8_t> decompressed;
    int codingPosition = 0;
    int inputSize = input.size();

    while (codingPosition < inputSize) {
        if (input[codingPosition] == 0) {
            int offset = (input[codingPosition + 1] << 8) | input[codingPosition + 2];
            int matchedLength = input[codingPosition + 3];
            int start = decompressed.size() - offset;

            for (int i = 0; i < matchedLength; ++i) {
                decompressed.push_back(decompressed[start + i]);
            }
            codingPosition += 4;
        }
        else {
            decompressed.push_back(input[codingPosition + 1]);
            codingPosition += 2;
        }
    }


    Result<std::vector<uint8_t>> result = makeResult<std::vector<uint8_t>>(decompressed);
    return result;
}