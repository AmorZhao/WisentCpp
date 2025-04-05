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
        return baseAddress();
    }

    void *realloc(void *pointer, size_t size) override
    {
        assert(isLoaded());
        assert(pointer == baseAddress());
        unload();
        object.truncate(size);
        load();
        return baseAddress();
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
        assert(pointer == baseAddress());
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

    void *baseAddress() const override
    {
        assert(isLoaded());
        return region->get_address();
    }

    size_t size() const override
    {
        assert(isLoaded());
        return region->get_size();
    }
};

class SharedMemorySegments : public ISharedMemorySegments
{
private:
    std::unordered_map<std::string, std::unique_ptr<ISharedMemorySegment>> sharedMemorySegments;
    ISharedMemorySegment *currentSharedMemory;

public: 
    SharedMemorySegments() 
        : currentSharedMemory(nullptr)
        , sharedMemorySegments()
    {}

    ~SharedMemorySegments() override = default;

    std::unordered_map<std::string, std::unique_ptr<ISharedMemorySegment>> &getSharedMemorySegments() override
    {
        return sharedMemorySegments;
    }

    ISharedMemorySegment *getCurrentSharedMemory() override
    {
        return currentSharedMemory;
    }

    void setCurrentSharedMemory(ISharedMemorySegment* sharedMemory) override
    {
        currentSharedMemory = sharedMemory;
    }

    void *sharedMemoryMalloc(size_t size) override
    {
        if (currentSharedMemory == nullptr) 
        {
            std::cerr << "Cannot malloc memory as currentSharedMemory is nullptr" << std::endl;
        }
        return currentSharedMemory->malloc(size);
    }

    void *sharedMemoryRealloc(void *pointer, size_t size) override
    {
        if (currentSharedMemory == nullptr) 
        {
            std::cerr << "Cannot realloc memory as currentSharedMemory is nullptr" << std::endl;
        }
        return currentSharedMemory->realloc(pointer, size);
    }

    void sharedMemoryFree(void *pointer) override
    {
        if (currentSharedMemory == nullptr) 
        {
            std::cerr << "Cannot free memory as currentSharedMemory is nullptr" << std::endl;
        }
        currentSharedMemory->free(pointer);
    }

    ISharedMemorySegment *createOrGetMemorySegment(std::string const &name) override
    {
        auto it = getSharedMemorySegments().find(name);
        if (it != getSharedMemorySegments().end()) 
        {
            return it->second.get();
        }

        getSharedMemorySegments().insert(std::make_pair(name, std::make_unique<SharedMemorySegment>(name)));
        auto it2 = getSharedMemorySegments().find(name);
        return it2->second.get(); 
    }
}; 