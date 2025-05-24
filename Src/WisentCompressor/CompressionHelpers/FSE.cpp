#include <iostream>
#include <cstring>
#include <vector>
#include <iomanip>
#include "FiniteStateEntropy.hpp"

const unsigned DefaultMaxSymbolValue = 255; 
const unsigned DefaultTableLog = 11;
const unsigned MinTablelog = 5; 
const unsigned MaxTableLog = 15;

// ========== Compression ==========

struct SymbolCompressionTransform {
    uint32_t stateBasedBitsOut;
    uint32_t maxBitsOut; 
    uint32_t minStatePlus;
    uint32_t nextStateOffset;
};

struct CTable {
    unsigned tableLog;
    unsigned maxSymbolValue;
    std::vector<uint16_t> stateTable;
    std::vector<SymbolCompressionTransform> symbolTransformTable;
};

std::vector<unsigned> simpleCount(
    unsigned& maxSymbolValue,
    const std::vector<uint8_t>& input
) {
    std::vector<unsigned> count(maxSymbolValue + 1, 0);

    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] > maxSymbolValue) {
            throw std::runtime_error("Symbol value exceeds maxSymbolValue");
        }
        count[input[i]]++;
    }

    while (!count[maxSymbolValue]) maxSymbolValue--;
    return count;
}

std::vector<unsigned> parallelCount(
    unsigned& maxSymbolValue,
    const std::vector<uint8_t>& input
) {
    std::cout << "Using parallel Count" << std::endl;
    unsigned max = 0;
    std::vector<unsigned> count(maxSymbolValue + 1, 0);
    
    const uint8_t* ip = input.data();
    const uint8_t* const iend = ip + input.size();

    // by stripes of 16 bytes 
    while (ip < iend - 15) 
    {
        count[ip[0]]++;
        count[ip[1]]++;
        count[ip[2]]++;
        count[ip[3]]++;
        count[ip[4]]++;
        count[ip[5]]++;
        count[ip[6]]++;
        count[ip[7]]++;
        count[ip[8]]++;
        count[ip[9]]++;
        count[ip[10]]++;
        count[ip[11]]++;
        count[ip[12]]++;
        count[ip[13]]++;
        count[ip[14]]++;
        count[ip[15]]++;
        ip += 16;
    }
    while (ip < iend) count[*ip++]++;

    while (!count[maxSymbolValue]) maxSymbolValue--;

    // does not check for symbol value exceeding maxSymbolValue
    // trust that the input is valid
    return count;
}

std::vector<unsigned> countSymbols(
    unsigned& maxSymbolValue,
    const std::vector<uint8_t>& input
) {
    if (input.size() < 1500) 
    {
        return simpleCount(maxSymbolValue, input);
    } 
    return parallelCount(maxSymbolValue, input);
}

// eg: 32'b10000 returns (highest bit position = 4)
static unsigned getHighestBitPosition(uint32_t val) {
    return 31 - __builtin_clz(val);
}

static unsigned getOptimalTableLog(
    unsigned maxTableLog, 
    size_t inputSize, 
    unsigned maxSymbolValue
) {
    unsigned maxBitsSrc = getHighestBitPosition(static_cast<uint32_t>(inputSize - 1));
    unsigned tableLog = maxTableLog;
    if (inputSize <= 1) {
        std::cerr << "Warning: Source size too small, RLE should be used instead" << std::endl;
    }
    if (tableLog == 0) tableLog = DefaultTableLog;
    if (maxBitsSrc < tableLog) tableLog = maxBitsSrc;   /* Accuracy can be reduced */
    if (tableLog < MinTablelog) tableLog = MinTablelog;
    if (tableLog > maxTableLog) tableLog = maxTableLog;
    return tableLog;
}

std::vector<short> normalizeCount(
    unsigned tableLog, 
    const std::vector<unsigned>& symbolCounts, 
    size_t totalSymbols, 
    unsigned& maxSymbolValue
) {
    static const uint32_t restToBeatTable[] = {0, 473195, 504333, 520860, 550000, 700000, 750000, 830000};

    std::vector<short> normalizedCounts(maxSymbolValue + 1, 0);
    uint64_t const scale = 62 - tableLog;
    uint64_t const step = ((uint64_t)1 << 62) / totalSymbols; // Only one division  
    uint64_t const vStep = 1ULL << (scale - 20);
    int remainingNormalizedSymbols = 1 << tableLog;
    unsigned symbolWithLargestProbability = 0;
    short largestProbability = 0;
    uint32_t lowThreshold = static_cast<uint32_t>(totalSymbols >> tableLog);

    for (unsigned symbol = 0; symbol <= maxSymbolValue; symbol++) 
    {
        if (symbolCounts[symbol] == totalSymbols) 
        {
            throw std::runtime_error("RLE special case: entire count is in one symbol");
        }
        if (symbolCounts[symbol] == 0) 
        {
            normalizedCounts[symbol] = 0;
            continue;
        }
        if (symbolCounts[symbol] <= lowThreshold) 
        {
            normalizedCounts[symbol] = -1;
            remainingNormalizedSymbols--;
        } 
        else 
        {
            short probability = static_cast<short>((symbolCounts[symbol] * step) >> scale);
            if (probability < 8) 
            {
                uint64_t restToBeat = vStep * restToBeatTable[probability];
                if ((symbolCounts[symbol] * step) - (static_cast<uint64_t>(probability) << scale) > restToBeat) {
                    probability++;
                }
            }
            if (probability > largestProbability) 
            {
                largestProbability = probability;
                symbolWithLargestProbability = symbol;
            }
            normalizedCounts[symbol] = probability;
            remainingNormalizedSymbols -= probability;
        }
    }

    if (-remainingNormalizedSymbols >= (normalizedCounts[symbolWithLargestProbability] >> 1)) 
    {
        throw std::runtime_error("Normalization failed");
    } 
    else 
    {
        normalizedCounts[symbolWithLargestProbability] += static_cast<short>(remainingNormalizedSymbols);
    }

    return normalizedCounts;
}

void writeNormalizedCounts(
    std::vector<uint8_t>& compressed,
    const std::vector<short>& normalizedCounts, 
    unsigned maxSymbolValue, 
    unsigned tableLog
) {
    compressed.push_back(static_cast<uint8_t>(tableLog - MinTablelog));
    compressed.push_back(static_cast<uint8_t>(maxSymbolValue));

    for (unsigned symbol = 0; symbol <= maxSymbolValue; ++symbol) 
    {
        if (normalizedCounts[symbol] == 0) continue;

        int count = normalizedCounts[symbol];
        compressed.push_back(static_cast<uint8_t>(symbol));
        if (tableLog > 8) 
        {
            compressed.push_back(static_cast<uint8_t>(count >> 8));
            compressed.push_back(static_cast<uint8_t>(count & 0xFF));
        }
        else compressed.push_back(static_cast<uint8_t>(count));
    }
}

void printNormalizedCounter(const std::vector<short>& normalizedCounts) 
{
    std::cout << "Normalized Counter (non zero):" << std::endl;
    for (unsigned i = 0; i < normalizedCounts.size(); ++i) {
        if (normalizedCounts[i] != 0) {
            std::cout << "Symbol " << i << " '" << static_cast<char>(i) << "': " << normalizedCounts[i] << std::endl;
        }
    }
    std::cout << std::endl;
}

CTable buildCTable(
    const std::vector<short>& normalizedCounts, 
    unsigned maxSymbolValue, 
    unsigned tableLog
) {
    CTable cTable; 

    const uint32_t tableSize = 1 << tableLog; 
    const uint32_t tableMask = tableSize - 1; 
    const uint32_t step = (tableSize >> 1) + (tableSize >> 3) + 3; 

    std::vector<uint32_t> cumulativeCounts(maxSymbolValue + 2, 0);  
    std::vector<uint32_t> symbolTable(tableSize, 0); 
        
    cTable.tableLog = tableLog;
    cTable.maxSymbolValue = maxSymbolValue;
    cTable.stateTable.resize(tableSize);
    cTable.symbolTransformTable.resize(maxSymbolValue + 1);

    // cumulativeCounts: gives symbol start positions
    uint32_t highThreshold = tableSize - 1; 
    for (unsigned symbol = 1; symbol <= maxSymbolValue + 1; ++symbol) 
    {
        if (normalizedCounts[symbol - 1] == -1) 
        {
            cumulativeCounts[symbol] = cumulativeCounts[symbol - 1] + 1;
            symbolTable[highThreshold--] = symbol - 1;
        } 
        else { 
            cumulativeCounts[symbol] = cumulativeCounts[symbol - 1] + normalizedCounts[symbol - 1];
        }
    }

    // symbolTable
    uint32_t position = 0;
    for (unsigned symbol = 0; symbol <= maxSymbolValue; ++symbol) 
    {
        int occurrences = normalizedCounts[symbol];
        for (int i = 0; i < occurrences; ++i) 
        {
            symbolTable[position] = symbol;
            position = (position + step) & tableMask;
            while (position > highThreshold) 
            {
                position = (position + step) & tableMask;
            }
        }
    }

    // stateTable: sorted by symbol order; gives next state value 
    for (unsigned i = 0; i < tableSize; ++i) 
    {
        uint32_t symbol = symbolTable[i];
        cTable.stateTable[cumulativeCounts[symbol]++] = tableSize + i;
    } 

    // symbolTransformTable
    unsigned total = 0;
    for (unsigned symbol = 0; symbol <= maxSymbolValue; ++symbol) 
    {
        switch (normalizedCounts[symbol]) 
        {
            case 0: 
                break;
            case -1:
            case 1:
                cTable.symbolTransformTable[symbol].stateBasedBitsOut = (tableLog << 16) - (1 << tableLog);  // 4?
                cTable.symbolTransformTable[symbol].maxBitsOut = tableLog;
                cTable.symbolTransformTable[symbol].minStatePlus = 1 << tableLog;
                cTable.symbolTransformTable[symbol].nextStateOffset = total - 1;
                total++;
                break;
            default:
                uint32_t maxBitsOut = tableLog - getHighestBitPosition(normalizedCounts[symbol] - 1);
                uint32_t minStatePlus = normalizedCounts[symbol] << maxBitsOut;
                cTable.symbolTransformTable[symbol].stateBasedBitsOut = (maxBitsOut << 16) - minStatePlus;
                cTable.symbolTransformTable[symbol].maxBitsOut = maxBitsOut;
                cTable.symbolTransformTable[symbol].minStatePlus = minStatePlus;
                cTable.symbolTransformTable[symbol].nextStateOffset = total - normalizedCounts[symbol];
                total += normalizedCounts[symbol];
                break;
        }
    }

    return cTable;
}

void printCTable(const CTable& cTable) 
{
    std::cout << "CTable:" << std::endl;
    std::cout << "Table Log: " << cTable.tableLog << std::endl;
    std::cout << "Max Symbol Value: " << cTable.maxSymbolValue << std::endl;
    std::cout << "State Table:" << std::endl;
    for (size_t i = 0; i < cTable.stateTable.size(); ++i) 
    {
        std::cout << cTable.stateTable[i] << " ";
    }
    std::cout << std::endl;
    std::cout << "Symbol Compression Transform Table (Non-zero Next State Offset):" << std::endl;
    std::cout << std::setw(6) << "Symbol" << " | " 
                << std::setw(10) << "maxBitsOut" << " | " 
                << std::setw(12) << "minStatePlus" << " | " 
                << std::setw(15) << "nextStateOffset" << std::endl;
    std::cout << "----------------------------------------------------" << std::endl;
    size_t totalSymbols = 0;
    for (size_t i = 0; i < cTable.symbolTransformTable.size(); ++i) 
    {
        if (cTable.symbolTransformTable[i].nextStateOffset != 0) 
        {
            std::cout << std::setw(6) << i << " | " 
                        << std::setw(10) << static_cast<int32_t>(cTable.symbolTransformTable[i].maxBitsOut) << " | "
                        << std::setw(12) << static_cast<int32_t>(cTable.symbolTransformTable[i].minStatePlus) << " | "
                        << std::setw(15) << static_cast<int32_t>(cTable.symbolTransformTable[i].nextStateOffset) << std::endl;
            totalSymbols++;
        }
    }
    std::cout << "Total number of symbols: " << totalSymbols << std::endl;
}

void compressDataUsingCTable(
    std::vector<uint8_t>& compressed,
    const std::vector<uint8_t>& input, 
    const CTable& cTable
) {        
    size_t inputSize = input.size();
    size_t encodingPosition = inputSize;
    
    uint64_t bitContainer = 0;
    int bitPos = 0;
    
    auto flushBits = [&]() {
        while (bitPos >= 8) {
            compressed.push_back(static_cast<uint8_t>(bitContainer));
            bitContainer >>= 8;
            bitPos -= 8;
        }
    };
    
    auto outputBits = [&](uint32_t cState, uint32_t numberOfBitsToOutput) {
        const uint32_t bitMask = (1U << numberOfBitsToOutput) - 1;
        bitContainer |= static_cast<uint64_t>(cState & bitMask) << bitPos;
        bitPos += numberOfBitsToOutput;
    };
    
    auto encodeSymbol = [&](uint32_t& cState, uint8_t nextSymbol) 
    {
        const SymbolCompressionTransform& nextSymbolTransformInfo = cTable.symbolTransformTable[nextSymbol];

        // output bits
        const uint32_t numberOfBitsToOutput = (cState + nextSymbolTransformInfo.stateBasedBitsOut) >> 16;
        outputBits(cState, numberOfBitsToOutput);
        
        // update state
        cState = cTable.stateTable[(cState >> numberOfBitsToOutput) + nextSymbolTransformInfo.nextStateOffset];
    };
    
    auto initCState = [&](uint32_t& cState, uint8_t nextSymbol) {
        const uint32_t numberOfBitsToOutput = (cTable.symbolTransformTable[nextSymbol].stateBasedBitsOut + (1<<15) ) >> 16;
        cState = (numberOfBitsToOutput << 16) - cTable.symbolTransformTable[nextSymbol].stateBasedBitsOut;
        cState = cTable.stateTable[(cState >> numberOfBitsToOutput) + cTable.symbolTransformTable[nextSymbol].nextStateOffset];
    };

    uint32_t CState1, CState2;
    
    // isOdd(inputSize) ? true : false    // check last bit
    if (inputSize & 1) {    
        initCState(CState1, input[--encodingPosition]); 
        initCState(CState2, input[--encodingPosition]);
        encodeSymbol(CState1, input[--encodingPosition]); 
        flushBits();
    } 
    else {
        initCState(CState2, input[--encodingPosition]);
        initCState(CState1, input[--encodingPosition]);
    }
    inputSize -= 2;  

    // ensure the rest number of symbols % 4 = 0   // check 2nd last bit 
    if ((sizeof(bitContainer) * 8 > cTable.tableLog * 4 + 7) && (inputSize & 2)) 
    {
        encodeSymbol(CState2, input[--encodingPosition]);
        encodeSymbol(CState1, input[--encodingPosition]);
        flushBits();
    }
    
    // encode 4 symbols each loop
    while (encodingPosition > 3) 
    {
        encodeSymbol(CState2, input[--encodingPosition]);

        if (sizeof(bitContainer) * 8 < cTable.tableLog * 2 + 7) { // if (tableLog > 29) 
            flushBits();
        }
        
        encodeSymbol(CState1, input[--encodingPosition]);

        if (sizeof(bitContainer) * 8 > cTable.tableLog * 4 + 7) { // if (tableLog < 15) 
            encodeSymbol(CState2, input[--encodingPosition]);
            encodeSymbol(CState1, input[--encodingPosition]);
        }
        flushBits();
    }

    outputBits(CState2, cTable.tableLog);
    outputBits(CState1, cTable.tableLog);

    outputBits(1, 1); // endmark
    flushBits();
    
    if (bitPos > 0) {
        compressed.push_back(static_cast<uint8_t>(bitContainer));
    }
}

Result<size_t> algorithms::FSE::compress(
    const std::byte* input,
    const size_t inputSize,
    const std::byte* output, 
    bool verbose
) { 
    std::vector<uint8_t> compressed;

    //  1. Count all symbols
    unsigned maxSymbolValue = DefaultMaxSymbolValue; 
    const std::vector<unsigned> count = countSymbols(
        maxSymbolValue, 
        input
    );

    //  2. Normalize symbol frequencies
    unsigned optimalTableLog = getOptimalTableLog(
        DefaultTableLog, 
        input.size(), 
        maxSymbolValue
    );

    const std::vector<short> normalizedCounts = normalizeCount(
        optimalTableLog, 
        count, 
        input.size(), 
        maxSymbolValue
    );

    if (verbose) printNormalizedCounter(normalizedCounts);

    writeNormalizedCounts(
        compressed,
        normalizedCounts, 
        maxSymbolValue, 
        optimalTableLog
    );

    //  3. Build FSE CTable
    CTable cTable = buildCTable(
        normalizedCounts, 
        maxSymbolValue, 
        optimalTableLog
    );

    if (verbose) printCTable(cTable);

    // 4. Compress 
    compressDataUsingCTable(
        compressed, 
        input, 
        cTable
    );

    return compressed;
}

// ========== Decompression ==========

struct SymbolDecompressionTransform {
    uint16_t newState;
    uint8_t symbol;
    uint8_t numberOfBitsToRead;
};

std::vector<int16_t> readNormalizedCount(
    unsigned& maxSymbolValue, 
    unsigned& tableLog, 
    const std::vector<uint8_t>& compressed, 
    int& normalizeCounterOffset
) {
    tableLog = compressed[normalizeCounterOffset++] + MinTablelog;
    maxSymbolValue = compressed[normalizeCounterOffset++];
    std::vector<int16_t> normalizedCounts(maxSymbolValue + 1, 0);

    const uint16_t maxCount = (tableLog <= 8) ? 0xFF : 0xFFFF;

    for (unsigned symbol = 0; symbol <= maxSymbolValue; ++symbol) 
    {
        symbol = compressed[normalizeCounterOffset++];
        if (tableLog > 8) {
            normalizedCounts[symbol] = static_cast<int16_t>((compressed[normalizeCounterOffset] << 8) + compressed[normalizeCounterOffset + 1]); 
            normalizeCounterOffset += 2;
        }
        else normalizedCounts[symbol] = static_cast<int16_t>(compressed[normalizeCounterOffset++]);
    }
    
    return normalizedCounts;
}

static void buildDTable(
    const std::vector<int16_t>& normalizedCounts, 
    unsigned maxSymbolValue, 
    unsigned tableLog, 
    std::vector<SymbolDecompressionTransform>& dTable
) { 
    const unsigned tableSize = 1U << tableLog;
    dTable.resize(tableSize); 

    uint32_t highThreshold = tableSize - 1;
    std::vector<uint16_t> symbolNext(maxSymbolValue + 1, 0);
    for (uint32_t symbol = 0; symbol <= maxSymbolValue; symbol++) 
    {
        if (normalizedCounts[symbol] == -1)   // handle low probability
        {
            dTable[highThreshold--].symbol = static_cast<uint8_t>(symbol);
            symbolNext[symbol] = 1;
        } 
        else 
        {
            symbolNext[symbol] = static_cast<uint16_t>(normalizedCounts[symbol]);
        }
    }

    // Spread symbol table (same as cTable)
    const uint32_t step = (tableSize >> 1) + (tableSize >> 3) + 3; 
    uint32_t position = 0;
    const uint32_t tableMask = tableSize - 1; 
    for (unsigned symbol = 0; symbol <= maxSymbolValue; ++symbol) 
    {
        int occurrences = normalizedCounts[symbol];
        for (int i = 0; i < occurrences; i++) 
        {
            dTable[position].symbol = static_cast<uint8_t>(symbol);
            position = (position + step) & tableMask;
            while (position > highThreshold) 
            {
                position = (position + step) & tableMask;
            }
        }
    }

    for (uint32_t index = 0; index < tableSize; index++) 
    {
        const uint8_t symbol = dTable[index].symbol;
        const uint16_t nextState = symbolNext[symbol]++;
        dTable[index].numberOfBitsToRead = static_cast<uint8_t>(tableLog - getHighestBitPosition(nextState));
        dTable[index].newState = static_cast<uint16_t>((nextState << dTable[index].numberOfBitsToRead) - tableSize);
    }
}

static void printDTable(const std::vector<SymbolDecompressionTransform>& dTable) 
{        
    std::cout << "Decode Table:" << std::endl;
    std::cout << "Index | New State | Symbol | Number of bits to read" << std::endl;
    std::cout << "----------------------------------------------" << std::endl;
    for (size_t i = 0; i < dTable.size(); ++i) {
        const SymbolDecompressionTransform& entry = dTable[i];
        std::cout << std::setw(5) << i << " | " 
                << std::setw(9) << entry.newState << " | " 
                << std::setw(6) << static_cast<int>(entry.symbol) << " | " 
                << std::setw(7) << static_cast<int>(entry.numberOfBitsToRead) << std::endl;
    }
}

std::vector<uint8_t> decompressDataUsingDTable(
    const std::vector<uint8_t>& compressed, 
    const std::vector<SymbolDecompressionTransform>& dTable, 
    unsigned tableLog, 
    unsigned maxSymbolValue, 
    int normalizeCounterOffset
) {
    std::vector<uint8_t> decompressed;

    uint64_t bitContainer = 0;
    int bitPos = 0;
    int decodingPosition = compressed.size() - 1;

    auto initBitContainer = [&]() {
        while (decodingPosition >= normalizeCounterOffset && bitPos < sizeof(bitContainer) * 8)
        {
            bitContainer <<= 8;
            bitContainer |= static_cast<uint64_t>(compressed[decodingPosition--]);
            bitPos += 8;
        }
        while((bitContainer >> bitPos) != 1) bitPos--; // skip endmark
    };

    auto reloadBitContainer = [&]() {
        while (decodingPosition >= normalizeCounterOffset && bitPos < sizeof(bitContainer)* 8 - 8) 
        {
            bitContainer <<= 8;
            bitContainer |= static_cast<uint64_t>(compressed[decodingPosition--]);
            bitPos += 8;
        }
    };

    auto readBits = [&](uint32_t numberOfBits) {
        size_t value = (bitContainer >> (bitPos - numberOfBits)) & ((1U << numberOfBits) - 1);
        bitPos -= numberOfBits;
        return value;
    };

    auto decodeSymbol = [&](size_t& dState) {
        uint8_t decodedSymbol = dTable[dState].symbol;
        size_t lowBits = readBits(dTable[dState].numberOfBitsToRead);
        dState = dTable[dState].newState + lowBits;
        return decodedSymbol;
    };

    initBitContainer(); 

    size_t state1 = readBits(tableLog);
    size_t state2 = readBits(tableLog);

    while (decodingPosition >= normalizeCounterOffset) 
    {
        decompressed.push_back(decodeSymbol(state1));
        decompressed.push_back(decodeSymbol(state2));
        reloadBitContainer();
    }

    while (bitPos > 0) 
    {
        decompressed.push_back(decodeSymbol(state1));
        if (bitPos == 0) 
        {
            decompressed.push_back(dTable[state2].symbol);
            decompressed.push_back(dTable[state1].symbol);
            break; 
        }
        decompressed.push_back(decodeSymbol(state2));
        if (bitPos == 0)
        {
            decompressed.push_back(dTable[state1].symbol);
            decompressed.push_back(dTable[state2].symbol);
        }
    }

    return decompressed;
}

Result<size_t> algorithms::FSE::decompress(
    const std::byte* input,
    const size_t inputSize, 
    const std::byte* output, 
    bool verbose
) {
    //  1. Read frequencies of symbols.
    unsigned maxSymbolValue = DefaultMaxSymbolValue;
    unsigned tableLog = 0;

    int normalizeCounterOffset = 0; 
    std::vector<int16_t> normalizedCounts = readNormalizedCount(
        maxSymbolValue, 
        tableLog, 
        input, 
        normalizeCounterOffset
    );

    if (verbose) 
    {
        std::cout << "Table Log: " << tableLog << std::endl;
        std::cout << "Max Symbol Value: " << maxSymbolValue << std::endl;
        printNormalizedCounter(normalizedCounts);
    }
    
    //  2. Build DTable from normalizedCounts
    std::vector<SymbolDecompressionTransform> dTable; 

    buildDTable(
        normalizedCounts, 
        maxSymbolValue,
        tableLog, 
        dTable
    ); 

    if (verbose) printDTable(dTable);
    
    //  3. Decompress the data
    std::vector<uint8_t> decompressed = decompressDataUsingDTable(
        input, 
        dTable,
        tableLog,
        maxSymbolValue, 
        normalizeCounterOffset
    );

    return decompressed;
}
