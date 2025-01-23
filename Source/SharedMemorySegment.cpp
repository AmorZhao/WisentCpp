#include "SharedMemorySegment.hpp"
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

std::unordered_map<std::string, SharedMemorySegment> &sharedMemorySegments()
{
    static std::unordered_map<std::string, SharedMemorySegment> segments;
    return segments;
}

SharedMemorySegment *&currentSharedMemory()
{
    static SharedMemorySegment *currentSharedMemoryPtr = nullptr;
    return currentSharedMemoryPtr;
}

void setCurrentSharedMemory(SharedMemorySegment &sharedMemory)
{
    currentSharedMemory() = &sharedMemory;
}

void *sharedMemoryMalloc(size_t size)
{
    if (currentSharedMemory() == nullptr) 
    {
        std::cerr << "Cannot malloc memory as currentSharedMemory is nullptr" << std::endl;
    }
    return currentSharedMemory()->malloc(size);
}

void *sharedMemoryRealloc(void *pointer, size_t size)
{
    if (currentSharedMemory() == nullptr) 
    {
        std::cerr << "Cannot realloc memory as currentSharedMemory is nullptr" << std::endl;
    }
    return currentSharedMemory()->realloc(pointer, size);
}

void sharedMemoryFree(void *pointer)
{
    if (currentSharedMemory() == nullptr) 
    {
        std::cerr << "Cannot free memory as currentSharedMemory is nullptr" << std::endl;
    }
    currentSharedMemory()->free(pointer);
}

SharedMemorySegment &createOrGetMemorySegment(std::string const &name)
{
    return sharedMemorySegments().emplace(name, name).first->second;
}
