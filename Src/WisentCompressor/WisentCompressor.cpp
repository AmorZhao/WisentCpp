#include "WisentCompressor.hpp"
#include "../Helpers/ISharedMemorySegment.hpp"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

std::string wisent::compressor::compress(
    std::string const& sharedMemoryName)
{
    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(sharedMemoryName);
    if (!sharedMemory->isLoaded()) 
    {
        std::cerr << "Can't compress wisent file: Shared memory segment is not loaded." << std::endl;
        return "";
    }
    auto baseAddress = sharedMemory->getBaseAddress();
    auto size = sharedMemory->getSize();

    std::string buffer(static_cast<char*>(baseAddress), size);

    return "compressed";
}