#include "gtest/gtest.h"
#include "../../Src/Helpers/ISharedMemorySegment.hpp"
#include "helpers/MockSharedMemorySegment.cpp"

class MockSharedMemorySegmentTest : public ::testing::Test 
{
protected:
    const std::string MockSharedMemoryName = "MockSharedMemoryName";
    const std::string MockDifferentName = "MockDifferentName";
    const size_t MockSharedMemorySize = 1024;
    
    ISharedMemorySegments *mockSharedMemorySegments;
    void SetUp() override 
    {
        mockSharedMemorySegments = new MockSharedMemorySegments();
    }

    void TearDown() override 
    {
    }
};

TEST_F(MockSharedMemorySegmentTest, CreateOrGetMemorySegment_NewSegmentGetsCreated) 
{
    ISharedMemorySegment *mockSharedMemory = mockSharedMemorySegments->createOrGetMemorySegment(MockSharedMemoryName);
    ASSERT_NE(mockSharedMemory, nullptr);
    ASSERT_EQ(mockSharedMemory->isLoaded(), false);

    ASSERT_EQ(mockSharedMemorySegments->getCurrentSharedMemory(), nullptr);
    mockSharedMemorySegments->setCurrentSharedMemory(mockSharedMemory);
    ASSERT_NE(mockSharedMemorySegments->getCurrentSharedMemory(), nullptr);

    auto pointer = mockSharedMemorySegments->sharedMemoryMalloc(MockSharedMemorySize);
    ASSERT_NE(pointer, nullptr);
    ASSERT_EQ(mockSharedMemorySegments->getSharedMemorySegments().size(), 1);

    mockSharedMemorySegments->getSharedMemorySegments().clear();
    mockSharedMemorySegments->setCurrentSharedMemory(nullptr);
    pointer = nullptr;
}

TEST_F(MockSharedMemorySegmentTest, CreateOrGetMemorySegment_DuplicatedSegmentGetsReturned) 
{
    ASSERT_EQ(mockSharedMemorySegments->getSharedMemorySegments().size(), 0);
    ISharedMemorySegment *mockSharedMemory = mockSharedMemorySegments->createOrGetMemorySegment(MockSharedMemoryName);
    ASSERT_EQ(mockSharedMemorySegments->getSharedMemorySegments().size(), 1);
    
    mockSharedMemorySegments->setCurrentSharedMemory(mockSharedMemory);
    mockSharedMemorySegments->sharedMemoryMalloc(MockSharedMemorySize);

    auto pointer = mockSharedMemorySegments->getCurrentSharedMemory()->baseAddress(); 

    ISharedMemorySegment *mockSharedMemoryDuplicated = mockSharedMemorySegments->createOrGetMemorySegment(MockSharedMemoryName);
    mockSharedMemorySegments->setCurrentSharedMemory(mockSharedMemoryDuplicated);
    ASSERT_EQ(mockSharedMemorySegments->getSharedMemorySegments().size(), 1);
    ASSERT_EQ(mockSharedMemorySegments->getCurrentSharedMemory()->baseAddress(), pointer);

    ISharedMemorySegment *mockSharedMemoryNotDuplicated = mockSharedMemorySegments->createOrGetMemorySegment(MockDifferentName);
    ASSERT_EQ(mockSharedMemorySegments->getSharedMemorySegments().size(), 2);

    mockSharedMemorySegments->getSharedMemorySegments().clear();
    mockSharedMemorySegments->setCurrentSharedMemory(nullptr);
    mockSharedMemory = nullptr; 
    mockSharedMemoryDuplicated = nullptr;   
}