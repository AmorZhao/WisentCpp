#include "gtest/gtest.h"
#include <memory>
#include "../../Src/Helpers/ISharedMemory.hpp"

const std::string MockSharedMemoryName = "MockSharedMemoryName";
const size_t MockSharedMemorySize = 1024;

TEST(MockSharedMemorySegmentTest, CreateOrGetMemorySegment_NewSegmentGetsCreated) 
{
    std::unique_ptr<ISharedMemory> &mockSharedMemory = createOrGetMemorySegment(MockSharedMemoryName);
    ASSERT_NE(mockSharedMemory, nullptr);
    ASSERT_EQ(mockSharedMemory->isLoaded(), false);

    ASSERT_EQ(currentSharedMemory(), nullptr);
    setCurrentSharedMemory(mockSharedMemory);
    ASSERT_NE(currentSharedMemory(), nullptr);

    auto pointer = sharedMemoryMalloc(MockSharedMemorySize);
    ASSERT_NE(pointer, nullptr);
    ASSERT_EQ(sharedMemorySegments().size(), 1);

    sharedMemorySegments().clear();
    currentSharedMemory() = nullptr;
    pointer = nullptr;
}

TEST(MockSharedMemorySegmentTest, CreateOrGetMemorySegment_DuplicatedSegmentGetsReturned) 
{
    ASSERT_EQ(sharedMemorySegments().size(), 0);
    std::unique_ptr<ISharedMemory> &mockSharedMemory = createOrGetMemorySegment("MockSharedMemoryName");
    ASSERT_EQ(sharedMemorySegments().size(), 1);
    
    setCurrentSharedMemory(mockSharedMemory);
    sharedMemoryMalloc(MockSharedMemorySize);

    auto pointer = currentSharedMemory()->baseAddress(); 

    std::unique_ptr<ISharedMemory> &mockSharedMemoryDuplicated = createOrGetMemorySegment(MockSharedMemoryName);
    setCurrentSharedMemory(mockSharedMemoryDuplicated);
    ASSERT_EQ(sharedMemorySegments().size(), 1);
    // ASSERT_EQ(currentSharedMemory()->baseAddress(), pointer);

    std::unique_ptr<ISharedMemory> &mockSharedMemoryNotDuplicated = createOrGetMemorySegment("DifferentName");
    ASSERT_EQ(sharedMemorySegments().size(), 2);

    sharedMemorySegments().clear();
    currentSharedMemory() = nullptr;
    mockSharedMemory = nullptr; 
    mockSharedMemoryDuplicated = nullptr;   
}