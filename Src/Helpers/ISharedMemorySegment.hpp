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
    virtual void *baseAddress() const = 0;
    virtual size_t size() const = 0;
    virtual ~ISharedMemorySegment() = default;
};

class ISharedMemorySegments
{
  public:
    virtual ISharedMemorySegment *createOrGetMemorySegment(std::string const &name) = 0;
    virtual std::unordered_map<std::string, std::unique_ptr<ISharedMemorySegment>> &getSharedMemorySegments() = 0;
    virtual ISharedMemorySegment *getCurrentSharedMemory() = 0;
    virtual void setCurrentSharedMemory(ISharedMemorySegment* sharedMemory) = 0;
    virtual void *sharedMemoryMalloc(size_t size) = 0;
    virtual void *sharedMemoryRealloc(void *pointer, size_t size) = 0;
    virtual void sharedMemoryFree(void *pointer) = 0;
    virtual ~ISharedMemorySegments() = default;
}; 
