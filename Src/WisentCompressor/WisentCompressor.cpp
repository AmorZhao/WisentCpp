#include "WisentCompressor.hpp"
#include "../Helpers/ISharedMemorySegment.hpp"
#include "../CompressionHelpers/Huffman.hpp"
#include "../CompressionHelpers/FiniteStateEntropy.hpp"
#include "../CompressionHelpers/LZ77.hpp"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

const size_t BytesPerLong = 8;

using namespace wisent::compressor;

template <typename Coder>
auto compressWith(const std::vector<uint8_t>& buffer) 
{   return Coder::compress(buffer); }

template <typename Coder>
auto decompressWith(const std::vector<uint8_t>& buffer) 
{   return Coder::decompress(buffer); }

std::vector<uint8_t> performCompression(
    CompressionType type,
    const std::vector<uint8_t>& buffer
) {
    switch (type) 
    {
        case CompressionType::HUFFMAN:
            return compressWith<Huffman::HuffmanCoder>(buffer);
        case CompressionType::LZ77:
            return compressWith<LZ77::LZ77Coder>(buffer);
        case CompressionType::FSE:
            return compressWith<FSE::FSECoder>(buffer);
        default:
            throw std::invalid_argument("Unsupported compression type");
    }
}

std::string wisent::compressor::compress(
    std::string const& sharedMemoryName, 
    CompressionType compressionType
) {
    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(sharedMemoryName);
    if (!sharedMemory->isLoaded()) 
    {
        std::cerr << "Can't compress wisent file: Shared memory segment is not loaded." << std::endl;
        return "";
    }

    if (compressionType == CompressionType::NONE) 
    {
        std::cout << "No compression applied." << std::endl;
        return "";
    }

    void *baseAddress = sharedMemory->getBaseAddress();
    size_t initialSize = sharedMemory->getSize();
    std::string buffer(static_cast<char*>(baseAddress), initialSize);

    size_t argumentCount, exprCount;
    std::memcpy(&argumentCount, buffer.data(), sizeof(size_t));
    std::memcpy(&exprCount, buffer.data() + sizeof(size_t), sizeof(size_t));

    size_t offset = 32; 
    size_t argumentVectorSize = argumentCount * BytesPerLong; 
    std::string argumentVector(buffer.data() + offset, argumentVectorSize);
    std::cout << "argumentCount: " << argumentCount << std::endl;
    
    offset += argumentVectorSize;
    size_t typeBytefieldSize = argumentCount * BytesPerLong;
    std::string typeBytefield(buffer.data() + offset, typeBytefieldSize);
    std::cout << "typeBytefieldSize: " << typeBytefieldSize << std::endl;
    
    offset += typeBytefieldSize;
    size_t structureVectorSize = exprCount * BytesPerLong * 3;
    std::string structureVector(buffer.data() + offset, exprCount * 24);
    std::cout << "structureVectorSize: " << structureVectorSize << std::endl;

    offset += exprCount * 24;
    std::string stringBuffer(buffer.data() + offset, buffer.size() - offset);
    std::cout << "stringBufferSize: " << stringBuffer.size() << std::endl;

    std::vector<uint8_t> compressedData = performCompression(
        compressionType,
        std::vector<uint8_t>(buffer.begin(), buffer.end())
    );
    
    return "compressed";
}

std::string wisent::compressor::compress(
    std::string const& sharedMemoryName, 
    CompressionPipeline *pipeline
) {
    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(sharedMemoryName);
    if (!sharedMemory->isLoaded()) 
    {
        std::cerr << "Can't compress wisent file: Shared memory segment is not loaded." << std::endl;
        return "";
    }

    void *baseAddress = sharedMemory->getBaseAddress();
    size_t initialSize = sharedMemory->getSize();
    std::string buffer(static_cast<char*>(baseAddress), initialSize);

    size_t argumentCount, exprCount;
    std::memcpy(&argumentCount, buffer.data(), sizeof(size_t));
    std::memcpy(&exprCount, buffer.data() + sizeof(size_t), sizeof(size_t));

    size_t offset = 32; 
    size_t argumentVectorSize = argumentCount * BytesPerLong; 
    std::string argumentVector(buffer.data() + offset, argumentVectorSize);
    std::cout << "argumentCount: " << argumentCount << std::endl;
    
    offset += argumentVectorSize;
    size_t typeBytefieldSize = argumentCount * BytesPerLong;
    std::string typeBytefield(buffer.data() + offset, typeBytefieldSize);
    std::cout << "typeBytefieldSize: " << typeBytefieldSize << std::endl;
    
    offset += typeBytefieldSize;
    size_t structureVectorSize = exprCount * BytesPerLong * 3;
    std::string structureVector(buffer.data() + offset, exprCount * 24);
    std::cout << "structureVectorSize: " << structureVectorSize << std::endl;

    offset += exprCount * 24;
    std::string stringBuffer(buffer.data() + offset, buffer.size() - offset);
    std::cout << "stringBufferSize: " << stringBuffer.size() << std::endl;
    
    return "compressed";
}

std::string wisent::compressor::decompress(
    std::string const& sharedMemoryName
) {
    return "Not implemented"; 
}
