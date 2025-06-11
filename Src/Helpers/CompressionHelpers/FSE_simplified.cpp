#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include "FiniteStateEntropy.hpp"

namespace FiniteStateEntropy_Simplified
{
    const unsigned char Precision = 6; 
    const int MaxSymbolValue = 255; 

    static double getSourceEntropy(const std::vector<uint8_t>& data) 
    {
        if (data.empty()) return 0.0;

        std::vector<int> count(256, 0);
        for (uint8_t value : data) {
            ++count[value];
        }

        double entropy = 0.0;
        double dataSize = static_cast<double>(data.size());
        double log2 = std::log(2.0);

        for (int count : count) 
        {
            if (count > 0) 
            {
                double p = count / dataSize;
                entropy -= p * (std::log(p) / log2);
            }
        }

        return entropy;
    }

    static int getLengthInBit(unsigned D) 
    {
        int len = 0;
        while (D > 0) {
            D >>= 1;
            ++len;
        }
        return len;
    }

    void getOrder(const std::vector<int>& data, std::vector<int>& order) 
    {
        int min = *std::min_element(data.begin(), data.end());
        int max = *std::max_element(data.begin(), data.end());

        if (min == max) {
            std::iota(order.begin(), order.end(), 0);
            return;
        }
        ++max;

        std::vector<int> tmp(data);
        for (int k = 0; k < data.size(); ++k) {
            int pos = std::distance(tmp.begin(), std::min_element(tmp.begin(), tmp.end()));
            order[k] = pos;
            tmp[pos] = max;
        }
    }

    void getCount(
        const std::vector<uint8_t>& data, 
        std::vector<int>& count, 
        int probabilityPrecision
    ) {
        std::fill(count.begin(), count.end(), 0);
        for (int value : data) {
            ++count[value];
        }

        double coef = static_cast<double>(1 << probabilityPrecision) / data.size();
        for (int& f : count) {
            f = static_cast<int>(f * coef);
        }

        int total = std::accumulate(count.begin(), count.end(), 0);
        int diff = (1 << probabilityPrecision) - total;

        int maxPos = std::distance(count.begin(), std::max_element(count.begin(), count.end()));
        count[maxPos] += diff;

        for (int& f : count) {
            if (f == 0) {
                f = 1;
                --count[maxPos];
            }
        }
    }

    void writeSymbolStates(
        std::vector<int>& symbolStates, 
        const std::vector<int>& count, 
        std::vector<int>& sizes, 
        int probabilityPrecision, 
        int maxStateValue
    ) {
        std::fill(sizes.begin(), sizes.end(), 1); // this is because table starts from 1 not from 0

        std::vector<int> order(count.size());
        getOrder(count, order);

        int denominator = 1 << probabilityPrecision;
        std::fill(symbolStates.begin(), symbolStates.end(), denominator);

        for (int j = 1; j <= maxStateValue; ++j) 
        {
            for (int i = 0; i < count.size(); ++i) 
            {
                int state = (j << probabilityPrecision) / count[order[i]];
                if (state < 2) state = 2;
                if (state <= maxStateValue) 
                {
                    if (symbolStates[state] == denominator) 
                    {
                        symbolStates[state] = order[i];
                        ++sizes[order[i]];
                    } 
                    else 
                    {
                        while (++state <= maxStateValue) 
                        {
                            if (symbolStates[state] == denominator) 
                            {
                                symbolStates[state] = order[i];
                                ++sizes[order[i]];
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    void buildDescendTable(
        std::vector<std::vector<int>>& descend, 
        const std::vector<int>& symbolStates, 
        const std::vector<int>& sizes, 
        int probabilityPrecision, 
        int maxStateValue
    ) {
        int denominator = 1 << probabilityPrecision;
        std::vector<int> order(sizes.size(), 1);

        for (int j = 2; j <= maxStateValue; ++j) 
        {
            if (symbolStates[j] != denominator) 
            {
                descend[symbolStates[j]][order[symbolStates[j]]++] = j;
            }
        }
    }

    void buildAscendTable(
        const std::vector<std::vector<int>>& descend, 
        std::vector<int>& ascendSymbol, 
        std::vector<int>& ascendState, 
        const std::vector<int>& sizes
    ) {
        ascendSymbol[0] = 0;
        ascendState[0] = 0;
        for (int i = 0; i < sizes.size(); ++i) 
        {
            int counter = 1;
            for (int j = 1; j < sizes[i]; ++j) 
            {
                ascendSymbol[descend[i][j]] = i;
                ascendState[descend[i][j]] = counter++;
            }
        }
    }

    void EncodeData(
        const std::vector<uint8_t>& source, 
        std::vector<uint8_t>& compressed, 
        const std::vector<std::vector<int>>& descend,
        const std::vector<int>& sizes,
        int statePrecision,
        int maxStateValue
    ) {
        bool encodingIsCorrect = true;
        int state = maxStateValue;
        uint8_t byte = 0;
        uint8_t bit = 0;
        int control_MASK = 1 << (statePrecision - 1);
        for (int value : source) {
            while (state > sizes[value] - 1) {
                ++bit;
                byte |= state & 1;
                if (bit == 8) {
                    compressed.push_back(byte);
                    bit = 0;
                    byte = 0;
                } else {
                    byte <<= 1;
                }
                state >>= 1;
            }
            state = descend[value][state];
            if (state < control_MASK) {
                std::cerr << "Problem with data symbol " << value << " " << state << std::endl;
                encodingIsCorrect = false;
                break;
            }
        }

        int byte_offset;
        if (bit == 0) {
            byte_offset = 0;
        } else {
            byte_offset = 8 - bit;
            compressed.push_back(byte << (7 - bit));
        }

        compressed.push_back(static_cast<uint8_t>(byte_offset));
        compressed.push_back(0); // byte_size placeholder
        compressed.push_back(0);
        compressed.push_back(0);
        compressed.push_back(0);
        compressed.push_back(static_cast<uint8_t>(state & 0xFF));
        compressed.push_back(static_cast<uint8_t>((state >> 8) & 0xFF));
        compressed.push_back(static_cast<uint8_t>((state >> 16) & 0xFF));
        compressed.push_back(static_cast<uint8_t>((state >> 24) & 0xFF));
    }

    std::vector<uint8_t> compress(const std::vector<uint8_t>& input, bool verbose) 
    {

        std::vector<uint8_t> compressed;

        int nSymbols = MaxSymbolValue + 1;
        int probabilityPrecision = getLengthInBit(nSymbols) + Precision;
        int statePrecision = probabilityPrecision + 1;
        int maxStateValue = (1 << statePrecision) - 1;

        std::vector<int> count(nSymbols);
        getCount(input, count, probabilityPrecision);

        // Output normalized frequencies and sizes
        compressed.push_back(static_cast<uint8_t>(nSymbols & 0xFF));
        compressed.push_back(static_cast<uint8_t>((nSymbols >> 8) & 0xFF));

        for (int f : count) 
        {
            compressed.push_back(static_cast<uint8_t>(f & 0xFF));
            compressed.push_back(static_cast<uint8_t>((f >> 8) & 0xFF));
        }

        int inputSize = input.size();
        compressed.push_back(static_cast<uint8_t>(inputSize & 0xFF));
        compressed.push_back(static_cast<uint8_t>((inputSize >> 8) & 0xFF));
        compressed.push_back(static_cast<uint8_t>((inputSize >> 16) & 0xFF));
        compressed.push_back(static_cast<uint8_t>((inputSize >> 24) & 0xFF));

        std::vector<int> symbolStates(maxStateValue + 1);
        std::vector<int> sizes(nSymbols);
        writeSymbolStates(symbolStates, count, sizes, probabilityPrecision, maxStateValue);

        std::vector<std::vector<int>> descend(nSymbols);
        for (int i = 0; i < nSymbols; ++i) {
            descend[i].resize(sizes[i]);
        }
        buildDescendTable(descend, symbolStates, sizes, probabilityPrecision, maxStateValue);

        EncodeData(input, compressed, descend, sizes, statePrecision, maxStateValue);

        if (verbose) 
        {
            int data_size = input.size();
            std::cout << "Data size = " << data_size << std::endl;
            double entropy = getSourceEntropy(input);
            int theoreticalSize = static_cast<int>(std::ceil(static_cast<double>(data_size) * entropy) / 8.0);
            std::cout << "Entropy " << entropy << ", estimated compressed size " << theoreticalSize << std::endl;
            std::cout << "Actual size of compressed data " << compressed.size() << std::endl;
        }

        return compressed;
    }

    std::vector<uint8_t> decompress(const std::vector<uint8_t>& input, bool verbose) 
    {
        std::vector<uint8_t> decompressed;

		int offset = 0;
        int nSymbols = input[offset] | (input[offset + 1] << 8);
        offset += 2;

        std::vector<int> count(nSymbols, 0);
        for (int i = 0; i < nSymbols; ++i) {
            count[i] = input[offset] | (input[offset + 1] << 8);
            offset += 2;
        }

        int probabilityPrecision = getLengthInBit(nSymbols) + Precision;
        int statePrecision = probabilityPrecision + 1;
        int maxStateValue = (1 << statePrecision) - 1;

        int decoded_size = input[offset] | (input[offset + 1] << 8) | (input[offset + 2] << 16) | (input[offset + 3] << 24);
        offset += 4;

        std::vector<int> symbolStates(maxStateValue + 1);
        std::vector<int> sizes(nSymbols);
        writeSymbolStates(symbolStates, count, sizes, probabilityPrecision, maxStateValue);

        std::vector<std::vector<int>> descend(nSymbols);
        for (int i = 0; i < nSymbols; ++i) {
            descend[i].resize(sizes[i]);
        }
        buildDescendTable(descend, symbolStates, sizes, probabilityPrecision, maxStateValue);

        std::vector<int> ascendSymbol(maxStateValue + 1);
        std::vector<int> ascendState(maxStateValue + 1);
        buildAscendTable(descend, ascendSymbol, ascendState, sizes);

        int state = input[input.size() - 4] | (input[input.size() - 3] << 8) | (input[input.size() - 2] << 16) | (input[input.size() - 1] << 24);
        int byte_size = input[input.size() - 8] | (input[input.size() - 7] << 8) | (input[input.size() - 6] << 16) | (input[input.size() - 5] << 24);
        int byte_offset = input[input.size() - 9];

        int MASK = 1 << (statePrecision - 1);
        unsigned char shift = byte_offset;
        int counter = input.size() - 10;
        decompressed.resize(decoded_size);
        for (int i = decoded_size - 1; i >= 0; --i) 
        {
            decompressed[i] = ascendSymbol[state];
            state = ascendState[state];
            while (state < MASK) 
            {
                state = (state << 1) | ((input[counter] & (1 << shift)) >> shift);
                ++shift;
                if (shift == 8) {
                    shift = 0;
                    --counter;
                }
            }
        }

        return decompressed;
    }
} 