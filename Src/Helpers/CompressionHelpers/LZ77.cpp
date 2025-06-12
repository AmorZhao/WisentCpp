#include "LZ77.hpp"
#include <cstdint>
#include <vector>
#include <algorithm>

#include <unordered_map>
#include <deque>

Result<std::vector<uint8_t>> wisent::algorithms::LZ77::compress(
  const std::vector<uint8_t>& input,
  int64_t windowSize,
  int64_t lookaheadBufferSize
) {
    const int MIN_MATCH_LENGTH = 3;
    const int MAX_MATCH_CANDIDATES = 8;

    std::vector<uint8_t> compressed;
    int inputSize = input.size();
    int codingPosition = 0;

    std::unordered_map<uint32_t, std::deque<int>> hashTable;

    auto hash3 = [](const uint8_t* data) -> uint32_t {
        return (data[0] << 16) | (data[1] << 8) | data[2];
    };

    while (codingPosition < inputSize) 
    {
        int matchedLength = 0;
        int bestOffset = 0;

        if (codingPosition + MIN_MATCH_LENGTH <= inputSize) 
        {
            uint32_t hash = hash3(&input[codingPosition]);

            auto& positions = hashTable[hash];
            for (int i = positions.size() - 1; 
                i >= 0 && matchedLength < lookaheadBufferSize 
                && positions.size() - i <= MAX_MATCH_CANDIDATES; --i) 
            {
                int matchPos = positions[i];

                // matches outside the window
                if (codingPosition - matchPos > windowSize) continue;

                int length = 0;
                while (length < lookaheadBufferSize &&
                       codingPosition + length < inputSize &&
                       input[matchPos + length] == input[codingPosition + length]) {
                    ++length;
                }

                if (length > matchedLength) {
                    matchedLength = length;
                    bestOffset = codingPosition - matchPos;
                }
            }

            positions.push_back(codingPosition);
            if (positions.size() > MAX_MATCH_CANDIDATES) 
            {
                positions.pop_front();  
            }
        }

        if (matchedLength >= MIN_MATCH_LENGTH) 
        {
            compressed.push_back(0);
            compressed.push_back((bestOffset >> 8) & 0xFF);
            compressed.push_back(bestOffset & 0xFF);
            compressed.push_back(matchedLength & 0xFF);
            codingPosition += matchedLength;
        } 
        else 
        {
            compressed.push_back(1);
            compressed.push_back(input[codingPosition]);
            codingPosition++;
        }
    }

    return makeResult<std::vector<uint8_t>>(compressed);
}

// Result<std::vector<uint8_t>> wisent::algorithms::LZ77::compress(
//   const std::vector<uint8_t> &input, 
//   int64_t windowSize, 
//   int64_t lookaheadBufferSize
// ) {
//     std::vector<uint8_t> compressed;
//     int inputSize = input.size();
//     int codingPosition = 0;

//     while (codingPosition < inputSize) {
//         int matchedLength = 0;
//         int offset = 0;

//         for (int i = std::max(static_cast<int64_t>(0), codingPosition - windowSize); 
//           i < codingPosition; ++i
//         ) {
//             int length = 0;
//             while (length < lookaheadBufferSize && codingPosition + length < inputSize &&
//                    input[i + length] == input[codingPosition + length]) {
//                 ++length;
//             }
//             if (length > matchedLength) {
//                 matchedLength = length;
//                 offset = codingPosition - i;
//             }
//         }

//         if (matchedLength > 2) {
//             compressed.push_back(0);
//             compressed.push_back((offset >> 8) & 0xFF);
//             compressed.push_back(offset & 0xFF);
//             compressed.push_back(matchedLength & 0xFF);
//             codingPosition += matchedLength;
//         }
//         else {
//             compressed.push_back(1);
//             compressed.push_back(input[codingPosition]);
//             ++codingPosition;
//         }
//     }

//     Result<std::vector<uint8_t>> result = makeResult<std::vector<uint8_t>>(compressed);
//     return result;
// }

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