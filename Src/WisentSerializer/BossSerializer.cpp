#include "BossSerializer.hpp"
#include "../Helpers/ISharedMemorySegment.hpp"

template <
    void* (*Allocate)(size_t),
    void* (*Reallocate)(void*, size_t),
    void  (*Free)(void*)
>
Result<boss::serialization::SerializedBossExpression<Allocate, Reallocate, Free>*>
wisent::serializer::load(
    boss::Expression&& input,
    std::string const &sharedMemoryName,
    bool dictEncodeStrings,
    bool dictEncodeDoublesAndLongs, 
    bool forceReload
) {
    Result<boss::serialization::SerializedBossExpression<Allocate, Reallocate, Free>*> result;
    
    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(sharedMemoryName);
    if (!forceReload && sharedMemory->exists() && !sharedMemory->isLoaded()) 
    {
        sharedMemory->load();
    }
    if (sharedMemory->isLoaded()) 
    {
        if (!forceReload) 
        {
            WisentRootExpression *loadedValue = reinterpret_cast<WisentRootExpression *>(
                sharedMemory->getBaseAddress()
            );
            result.setValue(loadedValue);
            return result;
        }
        sharedMemory->erase();
        SharedMemorySegments::getSharedMemorySegments().erase(sharedMemoryName);
    }
    SharedMemorySegments::setCurrentSharedMemory(sharedMemory);

    using SerializedBossExpression = boss::serialization::SerializedBossExpression<
        SharedMemorySegments::sharedMemoryMalloc, 
        SharedMemorySegments::sharedMemoryRealloc, 
        SharedMemorySegments::sharedMemoryFree
    >;
    
    auto serializedBossExpression = new SerializedBossExpression(
        std::move(input), 
        dictEncodeStrings, 
        dictEncodeDoublesAndLongs
    );

    result.setValue(serializedBossExpression);
    return result;
}