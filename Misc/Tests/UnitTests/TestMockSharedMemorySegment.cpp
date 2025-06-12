#include "gtest/gtest.h"
#include "../../../Src/Helpers/ISharedMemorySegment.hpp"

const std::string MockSharedMemoryName = "MockSharedMemoryName";
const std::string MockDifferentName = "MockDifferentName";
const size_t MockSharedMemorySize = 1024;

using namespace SharedMemorySegments;

TEST(MockSharedMemorySegmentsTest, CreateOrGetMemorySegment_UniqueName_NewSegmentGetsCreated) 
{
    ASSERT_EQ(getSharedMemorySegments().size(), 0);
    ISharedMemorySegment *mockSharedMemory = createOrGetMemorySegment(MockSharedMemoryName);
    ASSERT_EQ(getSharedMemorySegments().size(), 1);

    ASSERT_NE(mockSharedMemory, nullptr);
    ASSERT_EQ(mockSharedMemory->isLoaded(), false);

    getSharedMemorySegments().clear();
    setCurrentSharedMemory(nullptr);
}

TEST(MockSharedMemorySegmentsTest, CreateOrGetMemorySegment_DuplicatedName_SegmentGetsReturned) 
{
    ISharedMemorySegment *mockSharedMemory = createOrGetMemorySegment(MockSharedMemoryName);

    setCurrentSharedMemory(mockSharedMemory);
    sharedMemoryMalloc(MockSharedMemorySize);
    auto initialSegmentPointer = getCurrentSharedMemory()->getBaseAddress(); 

    ISharedMemorySegment *mockSharedMemoryDuplicated = createOrGetMemorySegment(MockSharedMemoryName);
    setCurrentSharedMemory(mockSharedMemoryDuplicated);
    ASSERT_EQ(getSharedMemorySegments().size(), 1);
    ASSERT_EQ(getCurrentSharedMemory()->getBaseAddress(), initialSegmentPointer);

    getSharedMemorySegments().clear();
    setCurrentSharedMemory(nullptr);
}

TEST(MockSharedMemorySegmentsTest, CreateOrGetMemorySegment_DifferentName_NewSegmentGetsCreated) 
{
    ISharedMemorySegment *mockSharedMemory = createOrGetMemorySegment(MockSharedMemoryName);
    
    setCurrentSharedMemory(mockSharedMemory);
    sharedMemoryMalloc(MockSharedMemorySize);
    auto initialSegmentPointer = getCurrentSharedMemory()->getBaseAddress(); 

    ISharedMemorySegment *mockSharedMemoryNotDuplicated = createOrGetMemorySegment(MockDifferentName);
    setCurrentSharedMemory(mockSharedMemoryNotDuplicated);
    sharedMemoryMalloc(MockSharedMemorySize);
    ASSERT_EQ(getSharedMemorySegments().size(), 2);
    ASSERT_NE(getCurrentSharedMemory()->getBaseAddress(), initialSegmentPointer);

    getSharedMemorySegments().clear();
    setCurrentSharedMemory(nullptr);
}

TEST(MockSharedMemorySegmentsTest, SetCurrentSharedMemory_SetsSegmentPointer) 
{
    ISharedMemorySegment *mockSharedMemory = createOrGetMemorySegment(MockSharedMemoryName);
    ASSERT_EQ(getCurrentSharedMemory(), nullptr);
    setCurrentSharedMemory(mockSharedMemory);
    ASSERT_NE(getCurrentSharedMemory(), nullptr);

    getSharedMemorySegments().clear();
    setCurrentSharedMemory(nullptr);
}

TEST(MockSharedMemorySegmentsTest, SharedMemoryMalloc_AllocatesMemorySize) 
{
    ISharedMemorySegment *mockSharedMemory = createOrGetMemorySegment(MockSharedMemoryName);
    setCurrentSharedMemory(mockSharedMemory);

    void *sharedMemoryPtr = sharedMemoryMalloc(MockSharedMemorySize);
    ASSERT_NE(sharedMemoryPtr, nullptr);
    ASSERT_EQ(getCurrentSharedMemory()->getSize(), MockSharedMemorySize);
    ASSERT_EQ(getCurrentSharedMemory()->getBaseAddress(), sharedMemoryPtr);

    getSharedMemorySegments().clear();
    setCurrentSharedMemory(nullptr);
}

TEST(MockSharedMemorySegmentsTest, SharedMemoryRealloc_ReturnsNewSize) 
{
    ISharedMemorySegment *mockSharedMemory = createOrGetMemorySegment(MockSharedMemoryName);
    setCurrentSharedMemory(mockSharedMemory);

    void *sharedMemoryPtr = sharedMemoryMalloc(MockSharedMemorySize);
    sharedMemoryRealloc(getCurrentSharedMemory()->getBaseAddress(), MockSharedMemorySize*2);
    ASSERT_EQ(getCurrentSharedMemory()->getSize(), MockSharedMemorySize*2);

    getSharedMemorySegments().clear();
    setCurrentSharedMemory(nullptr);
}
