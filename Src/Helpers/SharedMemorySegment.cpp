#include "ISharedMemorySegment.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <unordered_map>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>

using namespace boost::interprocess;

/* Not a general implementation: assuming always a single allocation! */
class SharedMemorySegment : public ISharedMemorySegment
{
  private:
    shared_memory_object object;
    std::unique_ptr<mapped_region> region;

  public:
    SharedMemorySegment(std::string const &name)
        : object(open_or_create, name.c_str(), read_write), region(nullptr)
    {}
    SharedMemorySegment(SharedMemorySegment &&other) = default;
    ~SharedMemorySegment() = default;

    SharedMemorySegment(SharedMemorySegment const &other) = delete;
    SharedMemorySegment &operator=(SharedMemorySegment const &other) = delete;
    SharedMemorySegment &operator=(SharedMemorySegment &&other) = delete;

    void *malloc(size_t size) override
    {
        assert(!isLoaded());
        object.truncate(size);
        load();
        return getBaseAddress();
    }

    void *realloc(void *pointer, size_t size) override
    {
        assert(isLoaded());
        assert(pointer == getBaseAddress());
        unload();
        object.truncate(size);
        load();
        return getBaseAddress();
    }

    void load() override
    { 
        region = std::make_unique<mapped_region>(object, read_write); 
    }

    void unload() override
    { 
        region.reset(); 
    }

    void erase() override
    {
        unload();
        shared_memory_object::remove(object.get_name());
    }

    void free(void *pointer) override
    {
        assert(pointer == getBaseAddress());
        unload();
        erase();
    }

    bool exists() const override
    {
        offset_t size;
        return object.get_size(size) && size > 0;
    }

    bool isLoaded() const override
    { 
        return exists() && region.get() != nullptr; 
    }

    void *getBaseAddress() const override
    {
        assert(isLoaded());
        return region->get_address();
    }

    size_t getSize() const override
    {
        assert(isLoaded());
        return region->get_size();
    }
};

namespace SharedMemorySegments
{
    std::unordered_map<std::string, std::unique_ptr<ISharedMemorySegment>> &getSharedMemorySegments() 
    {
        return sharedMemorySegmentsList;
    }

    ISharedMemorySegment *getCurrentSharedMemory() 
    {
        return currentSharedMemoryPtr;
    }

    void setCurrentSharedMemory(ISharedMemorySegment* sharedMemory) 
    {
        currentSharedMemoryPtr = sharedMemory;
    }

    void *sharedMemoryMalloc(size_t size) 
    {
        if (currentSharedMemoryPtr == nullptr) 
        {
            std::cerr << "Cannot malloc memory as currentSharedMemory is nullptr" << std::endl;
        }
        return currentSharedMemoryPtr->malloc(size);
    }

    void *sharedMemoryRealloc(void *pointer, size_t size) 
    {
        if (currentSharedMemoryPtr == nullptr) 
        {
            std::cerr << "Cannot realloc memory as currentSharedMemory is nullptr" << std::endl;
        }
        return currentSharedMemoryPtr->realloc(pointer, size);
    }

    void sharedMemoryFree(void *pointer) 
    {
        if (currentSharedMemoryPtr == nullptr) 
        {
            std::cerr << "Cannot free memory as currentSharedMemory is nullptr" << std::endl;
        }
        currentSharedMemoryPtr->free(pointer);
        if (currentSharedMemoryPtr != nullptr) 
        {
            std::cerr << "Free failed" << std::endl;
        }
    }

    ISharedMemorySegment *createOrGetMemorySegment(std::string const &name) 
    {
        auto it = sharedMemorySegmentsList.find(name);
        if (it != sharedMemorySegmentsList.end()) 
        {
            return it->second.get();
        }

        sharedMemorySegmentsList.insert(std::make_pair(name, std::make_unique<SharedMemorySegment>(name)));
        auto it2 = sharedMemorySegmentsList.find(name);
        return it2->second.get(); 
    }
}