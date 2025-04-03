#include "SharedMemorySegment.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <unordered_map>

std::unordered_map<std::string, std::unique_ptr<ISharedMemory>> &sharedMemorySegments()
{
    static std::unordered_map<std::string, std::unique_ptr<ISharedMemory>> segments;
    return segments;
}

std::unique_ptr<ISharedMemory> &currentSharedMemory()
{
    static std::unique_ptr<ISharedMemory> currentSharedMemoryPtr = nullptr;
    return currentSharedMemoryPtr;
}

void setCurrentSharedMemory(std::unique_ptr<ISharedMemory> &sharedMemory)
{
    currentSharedMemory() = std::move(sharedMemory);
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

std::unique_ptr<ISharedMemory> &createOrGetMemorySegment(std::string const &name)
{
    return sharedMemorySegments()
        .emplace(name, std::make_unique<SharedMemorySegment>(name))
        .first->second;
}
