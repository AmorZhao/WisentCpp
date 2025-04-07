#include "WisentCompressor.hpp"
#include "../Helpers/ISharedMemorySegment.hpp"
#include "../CompressionHelpers/Huffman.hpp"
#include "../CompressionHelpers/FiniteStateEntropy.hpp"
#include "../CompressionHelpers/LZ77.hpp"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <fcntl.h>
#include <string>
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
    std::string buffer
) {
    std::vector<uint8_t> vector = std::vector<uint8_t>(buffer.begin(), buffer.end());
    switch (type) 
    {
        case CompressionType::HUFFMAN:
            return compressWith<Huffman::HuffmanCoder>(vector);
        case CompressionType::LZ77:
            return compressWith<LZ77::LZ77Coder>(vector);
        case CompressionType::FSE:
            return compressWith<FSE::FSECoder>(vector);
        default:
            throw std::invalid_argument("Unsupported compression type");
    }
}

std::tuple<std::string, std::string, std::string, std::string> extractBuffer(
    const std::string& buffer, 
    size_t argumentCount, 
    size_t exprCount
) {
    size_t offset = 32; 
    size_t argumentVectorSize = argumentCount * BytesPerLong; 
    std::string argumentVector(buffer.data() + offset, argumentVectorSize);
    // std::cout << "argumentVectorSize: " << argumentVectorSize << std::endl;
    
    offset += argumentVectorSize;
    size_t typeBytefieldSize = argumentCount * BytesPerLong;
    std::string typeBytefield(buffer.data() + offset, typeBytefieldSize);
    // std::cout << "typeBytefieldSize: " << typeBytefieldSize << std::endl;
    
    offset += typeBytefieldSize;
    size_t structureVectorSize = exprCount * BytesPerLong * 3;
    std::string structureVector(buffer.data() + offset, structureVectorSize);
    // std::cout << "structureVectorSize: " << structureVectorSize << std::endl;

    offset += exprCount * 24;
    std::string stringBuffer(buffer.data() + offset, buffer.size() - offset);
    // std::cout << "stringBufferSize: " << initialSize - offset << std::endl;

    return {argumentVector, typeBytefield, structureVector, stringBuffer};
}

size_t compressSingleBuffer(
    const std::string& buffer, 
    void* baseAddress, 
    CompressionType compressionType
) {
    std::vector<uint8_t> compressedData = performCompression(
        compressionType,
        buffer
    );

    std::memcpy(
        static_cast<uint8_t*>(baseAddress), 
        compressedData.data(), 
        compressedData.size()
    );
    size_t compressedSize = compressedData.size();

    std::cout << "Initial buffer size: " << buffer.size() << std::endl;
    std::cout << "Compressed buffer size: " << compressedSize << std::endl;
    std::cout << "Compression ratio: " << (static_cast<double>(buffer.size()) / compressedSize) << std::endl;
    return compressedSize;
}

std::string wisent::compressor::compress(
    std::string const& sharedMemoryName, 
    CompressionType compressionType
) {
    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(sharedMemoryName);
    if (!sharedMemory->isLoaded()) 
    {
        std::string errorMessage = "Can't compress wisent file: Shared memory segment is not loaded.";
        std::cerr << errorMessage << std::endl;
        return errorMessage;
    }

    if (compressionType == CompressionType::NONE) 
    {
        std::string message = "No compression applied.";
        std::cout << message << std::endl;
        return message;
    }

    void *baseAddress = sharedMemory->getBaseAddress();
    size_t initialSize = sharedMemory->getSize();
    std::string buffer(static_cast<char*>(baseAddress), initialSize);

    size_t argumentCount, exprCount;
    std::memcpy(&argumentCount, buffer.data(), sizeof(size_t));
    std::memcpy(&exprCount, buffer.data() + sizeof(size_t), sizeof(size_t));

    auto extractedBuffer = extractBuffer(buffer, argumentCount, exprCount);
    std::string argumentVector = std::get<0>(extractedBuffer);
    std::string typeBytefield = std::get<1>(extractedBuffer);
    std::string structureVector = std::get<2>(extractedBuffer);
    std::string stringBuffer = std::get<3>(extractedBuffer);

    size_t argumentVectorSize = argumentVector.size();
    size_t typeBytefieldSize = typeBytefield.size();
    size_t structureVectorSize = structureVector.size();
    size_t stringBufferSize = stringBuffer.size();

    size_t compressedSize = compressSingleBuffer(
        stringBuffer, 
        static_cast<uint8_t*>(baseAddress) + argumentVectorSize + typeBytefieldSize + structureVectorSize, 
        compressionType
    );
    SharedMemorySegments::sharedMemoryRealloc(
        baseAddress, 
        argumentVectorSize + typeBytefieldSize + structureVectorSize + compressedSize
    );

    std::cout << "Initial total size: " << initialSize << std::endl;
    std::cout << "Compressed total size: " << argumentVectorSize + typeBytefieldSize + structureVectorSize + compressedSize << std::endl;
    std::cout << "Overall compression ratio: " << (static_cast<double>(initialSize) / SharedMemorySegments::getCurrentSharedMemory()->getSize()) << std::endl;
    
    return "Compression ratio: " + std::to_string((static_cast<double>(stringBuffer.size()) / compressedSize));
}

size_t compressBufferWithPipeline(
    const std::string& buffer,
    void* bufferBaseAddress,
    const std::vector<CompressionType>& compressionPipeline,
    size_t initialBufferSize
) {
    size_t compressedSize = initialBufferSize;
    for (const auto& compressionType : compressionPipeline) {
        compressedSize = compressSingleBuffer(
            buffer,
            bufferBaseAddress,
            compressionType
        );
    }
    return initialBufferSize - compressedSize;
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

    auto extractedBuffer = extractBuffer(buffer, argumentCount, exprCount);
    std::string argumentVector = std::get<0>(extractedBuffer);
    std::string typeBytefield = std::get<1>(extractedBuffer);
    std::string structureVector = std::get<2>(extractedBuffer);
    std::string stringBuffer = std::get<3>(extractedBuffer);

    size_t argumentVectorSize = argumentVector.size();
    size_t typeBytefieldSize = typeBytefield.size();
    size_t structureVectorSize = structureVector.size();
    size_t stringBufferSize = stringBuffer.size();

    size_t totalSize = argumentVectorSize + typeBytefieldSize + structureVectorSize + stringBufferSize;

    int64_t saved = compressBufferWithPipeline(
        argumentVector,
        static_cast<uint8_t*>(baseAddress),
        pipeline->getStringBufferChain(),
        argumentVectorSize
    );
    
    saved += compressBufferWithPipeline(
        typeBytefield,
        static_cast<uint8_t*>(baseAddress) + argumentVectorSize - saved,
        pipeline->getTypeBytefieldChain(),
        typeBytefieldSize
    );

    saved += compressBufferWithPipeline(
        structureVector,
        static_cast<uint8_t*>(baseAddress) + argumentVectorSize + typeBytefieldSize - saved,
        pipeline->getStructureVectorChain(),
        structureVectorSize
    );

    saved += compressBufferWithPipeline(
        stringBuffer,
        static_cast<uint8_t*>(baseAddress) + argumentVectorSize + typeBytefieldSize + structureVectorSize - saved,
        pipeline->getStringBufferChain(),
        stringBufferSize
    );
    SharedMemorySegments::sharedMemoryRealloc(
        baseAddress, 
        totalSize - saved
    );

    size_t newSize = SharedMemorySegments::getCurrentSharedMemory()->getSize(); 

    std::cout << "Initial total size: " << initialSize << std::endl;
    std::cout << "Compressed total size: " << newSize << std::endl;
    std::cout << "Overall compression ratio: " << (static_cast<double>(initialSize) / newSize) << std::endl;
    
    return "Compression ratio: " + std::to_string((static_cast<double>(initialSize) / newSize));
}

std::string wisent::compressor::decompress(
    std::string const& sharedMemoryName
) {
    return "Not implemented"; 
}
