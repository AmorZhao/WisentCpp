#include <gtest/gtest.h>
#include <string>
#include "../../Src/CompressionHelpers/LZ77.hpp"
#include "../../Src/CompressionHelpers/Huffman.hpp"
#include "../../Src/CompressionHelpers/FiniteStateEntropy.hpp"

const int MockLZ77WindowSize = 64;
const int MockLZ77LookaheadBufferSize = 32;

const std::string MockInputString =  // short and easy
    "Nulla pulvinar lectus et felis sodales maximus. Nulla pulvinar lectus et felis sodales maximus."
    "Nulla pulvinar lectus et felis sodales maximus. Nulla pulvinar lectus et felis sodales maximus."
    "Nulla pulvinar lectus et felis sodales maximus. Nulla pulvinar lectus et felis sodales maximus."
    "Nulla pulvinar lectus et felis sodales maximus. Nulla pulvinar lectus et felis sodales maximus.";
    
const std::vector<uint8_t> MockInput(MockInputString.begin(), MockInputString.end());
const int MockInputSize = MockInput.size();

static std::vector<uint8_t> genRandomTextInput(int size) 
{
    std::vector<uint8_t> res;
    const std::string popularSymbols = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (int i = 0; i < size; i++) {
        res.push_back(popularSymbols[rand() % popularSymbols.size()]);
    }
    return res;
}

static std::vector<uint8_t> genRandomPopularSymbolsInput(int size) // using ascii symbols from 32 to 126
{ 
    std::vector<uint8_t> res;
    for (int i = 0; i < size; i++) {
        res.push_back(32 + (rand() % 95));
    }
    return res;
}

const int MockLargeInputSize = 10000;
const std::vector<uint8_t> MockLargeTextInput = genRandomTextInput(MockLargeInputSize);
const std::vector<uint8_t> MockLargeSymbolInput = genRandomPopularSymbolsInput(MockLargeInputSize);

TEST(TestCompression, LZ77_Compression_ReturnsSmallerSize)
{
    std::vector<uint8_t> compressed = LZ77::compress(
        MockInput, 
        MockLZ77WindowSize, 
        MockLZ77LookaheadBufferSize);
    int compressedSize = compressed.size();
    std::cout << "Compression ratio: " << (double)MockInputSize / compressedSize << std::endl;
    EXPECT_LT(compressedSize, MockInputSize);
}

TEST(TestCompression, LZ77_Decompression_ReturnsDecodedData)
{
    std::vector<uint8_t> compressed = LZ77::compress(
        MockInput, MockLZ77WindowSize, MockLZ77LookaheadBufferSize);

    std::vector<uint8_t> decompressed = LZ77::decompress(compressed);

    EXPECT_EQ(MockInputSize, decompressed.size());
    EXPECT_EQ(MockInput, decompressed);

    std::string decompressedString(decompressed.begin(), decompressed.end());
    EXPECT_EQ(MockInputString, decompressedString);
}

TEST(TestCompression, Huffman_Compression_ReturnsSmallerSize)
{
    std::vector<uint8_t> compressed = Huffman::compress(MockInput);
    int compressedSize = compressed.size();
    std::cout << "Compression ratio: " << (double)MockInputSize / compressedSize << std::endl;
    EXPECT_LT(compressedSize, MockInputSize);
}

TEST(TestCompression, Huffman_Decompression_ReturnsDecodedData)
{
    std::vector<uint8_t> compressed = Huffman::compress(MockInput);
    std::vector<uint8_t> decompressed = Huffman::decompress(compressed);
    EXPECT_EQ(MockInputSize, decompressed.size());
    EXPECT_EQ(MockInput, decompressed);

    std::string decompressedString(decompressed.begin(), decompressed.end());
    EXPECT_EQ(MockInputString, decompressedString);
}

TEST(TestCompression, FSE_Compression_ReturnsSmallerSize) 
{
    std::vector<uint8_t> compressed = FiniteStateEntropy::compress(MockInput);
    int compressedSize = compressed.size();
    std::cout << "Compression ratio: " << (double)MockInputSize / compressedSize << std::endl;
    EXPECT_LT(compressedSize, MockInputSize); 
}

TEST(TestCompression, FSE_Compression_ReturnsSmallerSize_RandomTextInput)
{
    std::vector<uint8_t> compressed = FiniteStateEntropy::compress(MockLargeTextInput);
    int compressedSize = compressed.size();
    std::cout << "Compression ratio: " << (double)MockLargeInputSize / compressedSize << std::endl;
    EXPECT_LT(compressedSize, MockLargeInputSize); 
}

TEST(TestCompression, FSE_Compression_ReturnsSmallerSize_RandomPopularSymbolsInput)
{
    std::vector<uint8_t> compressed = FiniteStateEntropy::compress(MockLargeSymbolInput);
    int compressedSize = compressed.size();
    std::cout << "Compression ratio: " << (double)MockLargeInputSize / compressedSize << std::endl;
    EXPECT_LT(compressedSize, MockLargeInputSize); 
}

TEST(TestCompression, FSE_Decompression_ReturnsDecodedData)
{
    std::vector<uint8_t> compressed = FiniteStateEntropy::compress(MockInput);
    std::vector<uint8_t> decompressed = FiniteStateEntropy::decompress(compressed);
    EXPECT_EQ(MockInputSize, decompressed.size());
    EXPECT_EQ(MockInput, decompressed);

    std::string decompressedString(decompressed.begin(), decompressed.end());
    EXPECT_EQ(MockInputString, decompressedString);
}

TEST(TestCompression, FSE_Decompression_ReturnsDecodedData_RandomPopularTextInput)
{
    std::vector<uint8_t> compressed = FiniteStateEntropy::compress(MockLargeTextInput);
    std::vector<uint8_t> decompressed = FiniteStateEntropy::decompress(compressed);
    EXPECT_EQ(MockLargeInputSize, decompressed.size());
    EXPECT_EQ(MockLargeTextInput, decompressed);
}

TEST(TestCompression, FSE_Decompression_ReturnsDecodedData_RandomPopularSymbolsInput)
{
    std::vector<uint8_t> compressed = FiniteStateEntropy::compress(MockLargeSymbolInput);
    std::vector<uint8_t> decompressed = FiniteStateEntropy::decompress(compressed);
    EXPECT_EQ(MockLargeInputSize, decompressed.size());
    EXPECT_EQ(MockLargeSymbolInput, decompressed);
}
