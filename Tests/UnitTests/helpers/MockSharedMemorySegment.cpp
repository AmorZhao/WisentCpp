#include "../../../Src/Helpers/ISharedMemory.hpp"
#include <utility>
#include <vector>
#include <cassert>
#include <iostream>

/* Mock shared memory behaviour, without interacting with system-level memory */
class MockSharedMemorySegment : public ISharedMemory
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
        return baseAddress();
    }

    void *realloc(void *pointer, size_t size) override
    {
        assert(isLoaded());
        assert(pointer == baseAddress());
        // unload();
        memory.resize(size);
        load();
        return baseAddress();
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
        sharedMemorySegments().erase(segmentName);
    }

    void free(void *pointer) override
    {
        // assert(pointer == baseAddress());
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
    
    void *baseAddress() const override 
    {
        // assert(isLoaded());
        return (void *)memory.data(); 
    }
    
    size_t size() const override 
    {
        // assert(isLoaded());
        return memory.size(); 
    }
};

static std::unordered_map<std::string, std::unique_ptr<ISharedMemory>> segments;

std::unordered_map<std::string, std::unique_ptr<ISharedMemory>> &sharedMemorySegments()
{
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
    return currentSharedMemory()->malloc(size);
}

void *sharedMemoryRealloc(void *pointer, size_t size)
{
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
    // Check if the segment already exists
    auto it = segments.find(name);
    if (it != segments.end()) {
        std::cout << "Returning existing shared memory segment: " << name << std::endl;
        return it->second;
    }

    // If not found, create and insert a new one
    std::cout << "Creating new shared memory segment: " << name << std::endl;
    segments.insert(std::make_pair(name, std::make_unique<MockSharedMemorySegment>(name)));

    auto it2 = segments.find(name);
    return it2->second; 
}

