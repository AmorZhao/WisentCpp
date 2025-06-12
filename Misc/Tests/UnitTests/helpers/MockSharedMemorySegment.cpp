#include "../../../../Src/Helpers/ISharedMemorySegment.hpp"
#include <memory>
#include <utility>
#include <vector>
#include <cassert>
#include <iostream>

/* Mock shared memory behaviour, without interacting with system-level memory */
class MockSharedMemorySegment : public ISharedMemorySegment
{
  private:
    std::vector<char> memory;
    std::string segmentName; 
    bool isLoadedFlag;

  public:
    MockSharedMemorySegment(std::string const &name) 
        : isLoadedFlag(false) 
        , segmentName(name)
    {}

    void *malloc(size_t size) override
    {
        assert(!isLoaded());
        memory.resize(size);
        load(); 
        return getBaseAddress();
    }

    void *realloc(void *pointer, size_t size) override
    {
        assert(isLoaded());
        assert(pointer == getBaseAddress());
        unload();
        memory.resize(size);
        load();
        return getBaseAddress();
    }

    void load() override 
    {
        isLoadedFlag = true;
    }

    void unload() override 
    {
        isLoadedFlag = false;
        memory.clear();
    }

    void erase() override {
        unload();
    }

    void free(void *pointer) override
    {
        assert(pointer == getBaseAddress());
        unload();
        erase();
    }

    bool exists() const override 
    { 
        return !memory.empty(); 
    }
    
    bool isLoaded() const override 
    { 
        return isLoadedFlag;
    }
    
    void *getBaseAddress() const override 
    {
        assert(isLoaded());
        return (void *)memory.data(); 
    }
    
    size_t getSize() const override 
    {
        assert(isLoaded());
        return memory.size(); 
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

    void setCurrentSharedMemory(ISharedMemorySegment *sharedMemory) 
    {
        currentSharedMemoryPtr = sharedMemory;
    }

    void *sharedMemoryMalloc(size_t size) 
    {
        return getCurrentSharedMemory()->malloc(size);
    }

    void *sharedMemoryRealloc(void *pointer, size_t size) 
    {
        return getCurrentSharedMemory()->realloc(pointer, size);
    }

    void sharedMemoryFree(void *pointer) 
    {
        if (getCurrentSharedMemory() == nullptr) 
        {
            std::cerr << "Cannot free memory as currentSharedMemory is nullptr" << std::endl;
        }
        getCurrentSharedMemory()->free(pointer);
    }

    ISharedMemorySegment *createOrGetMemorySegment(std::string const &name) 
    {
        auto it = getSharedMemorySegments().find(name);
        if (it != getSharedMemorySegments().end()) 
        {
            return it->second.get();
        }

        std::unique_ptr<MockSharedMemorySegment> newSegment = std::make_unique<MockSharedMemorySegment>(name);
        MockSharedMemorySegment *newSegmentPtr = newSegment.get();
        getSharedMemorySegments().insert(std::make_pair(name, std::move(newSegment)));
        return newSegmentPtr;
    }
}
