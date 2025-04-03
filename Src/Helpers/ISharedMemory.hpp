#pragma once
#include <cstddef>
#include <unordered_map>
#include <memory>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <string>

class ISharedMemory
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
    virtual ~ISharedMemory() = default;
};

std::unordered_map<std::string, std::unique_ptr<ISharedMemory>> &sharedMemorySegments();

std::unique_ptr<ISharedMemory> &currentSharedMemory();

void setCurrentSharedMemory(std::unique_ptr<ISharedMemory> &sharedMemory);

void *sharedMemoryMalloc(size_t size);

void *sharedMemoryRealloc(void *pointer, size_t size);

void sharedMemoryFree(void *pointer);

std::unique_ptr<ISharedMemory> &createOrGetMemorySegment(std::string const &name);
