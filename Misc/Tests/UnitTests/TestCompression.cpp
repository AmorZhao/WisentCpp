#include <gtest/gtest.h>
#include <string>
#include "../../../Src/Helpers/CompressionHelpers/LZ77.hpp"
#include "../../../Src/Helpers/CompressionHelpers/Huffman.hpp"
#include "../../../Src/Helpers/CompressionHelpers/FSE.hpp"
#include "../../../Src/Helpers/Result.hpp"

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
    Result<std::vector<uint8_t>> compressed = wisent::algorithms::LZ77::compress(
        MockInput, 
        MockLZ77WindowSize, 
        MockLZ77LookaheadBufferSize);
    EXPECT_TRUE(compressed.success()); 
    int compressedSize = compressed.getValue().size();
    std::cout << "Compression ratio: " << (double)MockInputSize / compressedSize << std::endl;
    EXPECT_LT(compressedSize, MockInputSize);
}

TEST(TestCompression, LZ77_Decompression_ReturnsDecodedData)
{
    Result<std::vector<uint8_t>> compressed = wisent::algorithms::LZ77::compress(
        MockInput, MockLZ77WindowSize, MockLZ77LookaheadBufferSize);

    EXPECT_TRUE(compressed.success());

    Result<std::vector<uint8_t>> decompressed = wisent::algorithms::LZ77::decompress(compressed.getValue());

    EXPECT_TRUE(decompressed.success());

    EXPECT_EQ(MockInputSize, decompressed.getValue().size());
    EXPECT_EQ(MockInput, decompressed.getValue());

    // std::string decompressedString(decompressed.getValue().begin(), decompressed.getValue().end());
    // EXPECT_EQ(MockInputString, decompressedString);
}

TEST(TestCompression, Huffman_Compression_ReturnsSmallerSize)
{
    Result<std::vector<uint8_t>> compressed = wisent::algorithms::Huffman::compress(MockInput);

    EXPECT_TRUE(compressed.success());
    int compressedSize = compressed.getValue().size();
    std::cout << "Compression ratio: " << (double)MockInputSize / compressedSize << std::endl;
    EXPECT_LT(compressedSize, MockInputSize);
}

TEST(TestCompression, Huffman_Decompression_ReturnsDecodedData)
{
    Result<std::vector<uint8_t>> compressed = wisent::algorithms::Huffman::compress(MockInput);
    EXPECT_TRUE(compressed.success());
    Result<std::vector<uint8_t>> decompressed = wisent::algorithms::Huffman::decompress(compressed.getValue());
    EXPECT_EQ(MockInputSize, decompressed.getValue().size());
    EXPECT_EQ(MockInput, decompressed.getValue());

    // std::string decompressedString(decompressed.getValue().begin(), decompressed.getValue().end());
    // EXPECT_EQ(MockInputString, decompressedString);
}

TEST(TestCompression, FSE_Compression_ReturnsSmallerSize) 
{
    Result<std::vector<uint8_t>> compressed = wisent::algorithms::FSE::compress(MockInput);
    int compressedSize = compressed.getValue().size();
    std::cout << "Compression ratio: " << (double)MockInputSize / compressedSize << std::endl;
    EXPECT_LT(compressedSize, MockInputSize); 
}

TEST(TestCompression, FSE_Compression_ReturnsSmallerSize_RandomTextInput)
{
    Result<std::vector<uint8_t>> compressed = wisent::algorithms::FSE::compress(MockLargeTextInput);
    int compressedSize = compressed.getValue().size();
    std::cout << "Compression ratio: " << (double)MockLargeInputSize / compressedSize << std::endl;
    EXPECT_LT(compressedSize, MockLargeInputSize); 
}

TEST(TestCompression, FSE_Compression_ReturnsSmallerSize_RandomPopularSymbolsInput)
{
    Result<std::vector<uint8_t>> compressed = wisent::algorithms::FSE::compress(MockLargeSymbolInput);
    int compressedSize = compressed.getValue().size();
    std::cout << "Compression ratio: " << (double)MockLargeInputSize / compressedSize << std::endl;
    EXPECT_LT(compressedSize, MockLargeInputSize); 
}

TEST(TestCompression, FSE_Decompression_ReturnsDecodedData)
{
    Result<std::vector<uint8_t>> compressed = wisent::algorithms::FSE::compress(MockInput);
    Result<std::vector<uint8_t>> decompressed = wisent::algorithms::FSE::decompress(compressed.getValue());
    EXPECT_EQ(MockInputSize, decompressed.getValue().size());
    EXPECT_EQ(MockInput, decompressed.getValue());

    // std::string decompressedString(decompressed.getValue().begin(), decompressed.getValue().end());
    // EXPECT_EQ(MockInputString, decompressedString);
}

TEST(TestCompression, FSE_Decompression_ReturnsDecodedData_RandomPopularTextInput)
{
    Result<std::vector<uint8_t>> compressed = wisent::algorithms::FSE::compress(MockLargeTextInput);
    Result<std::vector<uint8_t>> decompressed = wisent::algorithms::FSE::decompress(compressed.getValue());
    EXPECT_EQ(MockLargeInputSize, decompressed.getValue().size());
    EXPECT_EQ(MockLargeTextInput, decompressed.getValue());
}

TEST(TestCompression, FSE_Decompression_ReturnsDecodedData_RandomPopularSymbolsInput)
{
    Result<std::vector<uint8_t>> compressed = wisent::algorithms::FSE::compress(MockLargeSymbolInput);
    Result<std::vector<uint8_t>> decompressed = wisent::algorithms::FSE::decompress(compressed.getValue());
    EXPECT_EQ(MockLargeInputSize, decompressed.getValue().size());
    EXPECT_EQ(MockLargeSymbolInput, decompressed.getValue());
}
