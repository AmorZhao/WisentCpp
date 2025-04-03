#pragma once
#include "ISharedMemory.hpp"
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <memory>
#include <string>

using namespace boost::interprocess;

/* Not a general implementation: assuming always a single allocation! */
class SharedMemorySegment : public ISharedMemory
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