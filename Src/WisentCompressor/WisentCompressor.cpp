#include "WisentCompressor.hpp"
#include "../Helpers/ISharedMemorySegment.hpp"
#include "../CompressionHelpers/Huffman.hpp"
#include "../CompressionHelpers/FiniteStateEntropy.hpp"
#include "../CompressionHelpers/LZ77.hpp"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

const size_t BytesPerLong = 8;

template <typename Compressor>
std::vector<uint8_t> compressWith(const std::vector<uint8_t>& buffer) {
    return Compressor::compress(buffer);
}

struct HuffmanCompressor {
    static std::vector<uint8_t> compress(const std::vector<uint8_t>& buffer) {
        return Huffman::compress(buffer);
    }
};

struct LZ77Compressor {
    static std::vector<uint8_t> compress(const std::vector<uint8_t>& buffer) {
        return LZ77::compress(buffer);
    }
};

struct FSECompressor {
    static std::vector<uint8_t> compress(const std::vector<uint8_t>& buffer) {
        return FSE::compress(buffer);
    }
};


std::vector<uint8_t> dispatchCompression(
    wisent::compressor::CompressionType type,
    const std::vector<uint8_t>& buffer
) {
    switch (type) {
        case wisent::compressor::CompressionType::HUFFMAN:
            return compressWith<HuffmanCompressor>(buffer);
        case wisent::compressor::CompressionType::LZ77:
            return compressWith<LZ77Compressor>(buffer);
        case wisent::compressor::CompressionType::FSE:
            return compressWith<FSECompressor>(buffer);
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
        std::cout << "Compression type is None. No compression applied." << std::endl;
        return "";
    }

    auto baseAddress = sharedMemory->getBaseAddress();
    auto size = sharedMemory->getSize();
    std::string buffer(static_cast<char*>(baseAddress), size);

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

    std::vector<uint8_t> compressedData = dispatchCompression(
        compressionType,
        std::vector<uint8_t>(buffer.begin(), buffer.end())
    );
    
    return "compressed";
}