#include "WisentCompressor.hpp"
#include "../Helpers/ISharedMemorySegment.hpp"
#include "../CompressionHelpers/Huffman.hpp"
#include "../CompressionHelpers/FiniteStateEntropy.hpp"
#include "../CompressionHelpers/LZ77.hpp"
#include "CompressionPipeline.hpp"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <fcntl.h>
#include <string>
#include <unistd.h>

const size_t BytesPerLong = 8;
const bool usingBlockSize = false;
const size_t BlockSize = 1024 * 1024; 

using namespace wisent::compressor;

template <typename Coder>
auto compressWith(const std::vector<uint8_t>& buffer) 
{
    if (!usingBlockSize)
        return Coder::compress(buffer);

    std::vector<uint8_t> compressedData;
    size_t totalSize = buffer.size();
    size_t offset = 0;
    while (offset < totalSize) 
    {
        size_t currentChunkSize = std::min(BlockSize, totalSize - offset);
        std::vector<uint8_t> chunk(buffer.begin() + offset, buffer.begin() + offset + currentChunkSize);
        auto compressedChunk = Coder::compress(chunk);
        compressedData.insert(compressedData.end(), compressedChunk.begin(), compressedChunk.end());
        offset += currentChunkSize;
    }
    return compressedData;
}

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

std::string wisent::compressor::compress(  // compress string buffer only
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
    for (const auto& compressionType : compressionPipeline) 
    {
        std::cout << "Compression type: " << static_cast<int>(compressionType) << std::endl;
        compressedSize = compressSingleBuffer(
            buffer,
            static_cast<uint8_t*>(bufferBaseAddress) + sizeof(size_t),
            compressionType
        );
    }
    std::memcpy(  // new buffer size
        bufferBaseAddress, 
        &compressedSize, 
        sizeof(size_t)
    );
    return compressedSize + sizeof(size_t);
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

    std::vector<CompressionType> argumentVectorChain = pipeline->getArgumentVectorChain();
    std::vector<CompressionType> typeBytefieldChain = pipeline->getTypeBytefieldChain();
    std::vector<CompressionType> structureVectorChain = pipeline->getStructureVectorChain();
    std::vector<CompressionType> stringBufferChain = pipeline->getStringBufferChain();

    size_t totalSize = argumentVectorSize + typeBytefieldSize + structureVectorSize + stringBufferSize;

    size_t saved = 0;
    std::cout << "compressing argumentVector" << std::endl;
    size_t newArgumentVectorSize = compressBufferWithPipeline(
        argumentVector,
        static_cast<uint8_t*>(baseAddress) + sizeof(size_t),
        argumentVectorChain,
        argumentVectorSize
    );
    saved += argumentVectorSize - newArgumentVectorSize;
    
    std::cout << "compressing typeBytefield" << std::endl;
    size_t newTypeBytefieldSize = compressBufferWithPipeline(
        typeBytefield,
        static_cast<uint8_t*>(baseAddress) + sizeof(size_t) + argumentVectorSize - saved,
        typeBytefieldChain,
        typeBytefieldSize
    );
    saved += typeBytefieldSize - newTypeBytefieldSize;

    std::cout << "compressing structureVector" << std::endl;
    size_t newStructureVectorSize= compressBufferWithPipeline(
        structureVector,
        static_cast<uint8_t*>(baseAddress) + sizeof(size_t) + argumentVectorSize + typeBytefieldSize - saved,
        structureVectorChain,
        structureVectorSize
    );
    saved += structureVectorSize - newStructureVectorSize;

    std::cout << "compressing stringBuffer" << std::endl;
    size_t newStringBufferSize = compressBufferWithPipeline(
        stringBuffer,
        static_cast<uint8_t*>(baseAddress) + sizeof(size_t) + argumentVectorSize + typeBytefieldSize + structureVectorSize - saved,
        stringBufferChain, 
        stringBufferSize
    );
    saved += stringBufferSize - newStringBufferSize;
    if (saved < 0) 
    {
        std::string errorMessage = "Error: Compression resulted in even bigger size.";
        std::cerr << errorMessage << std::endl;
        return errorMessage;
    }
    // Save compressed size
    size_t dataSize = totalSize - saved;
    std::memcpy(
        static_cast<uint8_t*>(baseAddress), 
        &dataSize, 
        sizeof(size_t)
    );

    std::vector<uint8_t> compressionInfo; 
    compressionInfo.push_back(argumentVectorChain.size());
    compressionInfo.push_back(typeBytefieldChain.size());
    compressionInfo.push_back(structureVectorChain.size());
    compressionInfo.push_back(stringBufferChain.size());

    for (const auto& type : argumentVectorChain) 
        compressionInfo.push_back(static_cast<uint8_t>(type));
    for (const auto& type : typeBytefieldChain)
        compressionInfo.push_back(static_cast<uint8_t>(type));
    for (const auto& type : structureVectorChain)
        compressionInfo.push_back(static_cast<uint8_t>(type));
    for (const auto& type : stringBufferChain)  
        compressionInfo.push_back(static_cast<uint8_t>(type));

    std::memcpy(
        static_cast<uint8_t*>(baseAddress) + sizeof(size_t) + dataSize,
        compressionInfo.data(),
        compressionInfo.size()
    );

    SharedMemorySegments::sharedMemoryRealloc(
        baseAddress, 
        sizeof(size_t) + dataSize + compressionInfo.size()
    );

    size_t newSize = SharedMemorySegments::getCurrentSharedMemory()->getSize(); 

    std::cout << "Initial total size: " << initialSize << std::endl;
    std::cout << "Compressed total size: " << newSize << std::endl;
    std::cout << "Added information size: " << compressionInfo.size() << std::endl;
    std::cout << "Overall compression ratio: " << (static_cast<double>(initialSize) / newSize) << std::endl;
    
    return "Compression ratio: " + std::to_string((static_cast<double>(initialSize) / newSize));
}

std::string wisent::compressor::decompress(
    std::string const& sharedMemoryName
) {
    return "Not implemented"; 
}
