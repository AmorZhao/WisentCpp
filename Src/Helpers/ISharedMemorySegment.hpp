#pragma once
#include <cstddef>
#include <unordered_map>
#include <memory>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <string>

class ISharedMemorySegment
{
public:
    virtual void *malloc(size_t size) = 0;
    virtual void *realloc(void *pointer, size_t size) = 0;
    virtual void load() = 0;
    virtual void unload() = 0;
    virtual void erase() = 0;
    virtual void free(void *pointer) = 0;
    virtual bool exists() const = 0;
    virtual bool isLoaded() const = 0;
    virtual void *getBaseAddress() const = 0;
    virtual size_t getSize() const = 0;
    virtual ~ISharedMemorySegment() = default;
};

namespace SharedMemorySegments
{
    static std::unordered_map<std::string, std::unique_ptr<ISharedMemorySegment>> sharedMemorySegmentsList;
    static ISharedMemorySegment *currentSharedMemoryPtr;

    ISharedMemorySegment *createOrGetMemorySegment(std::string const &name);
    std::unordered_map<std::string, std::unique_ptr<ISharedMemorySegment>> &getSharedMemorySegments();
    ISharedMemorySegment *getCurrentSharedMemory();
    void setCurrentSharedMemory(ISharedMemorySegment* sharedMemory);
    void *sharedMemoryMalloc(size_t size);
    void *sharedMemoryRealloc(void *pointer, size_t size);
    void sharedMemoryFree(void *pointer);
}
