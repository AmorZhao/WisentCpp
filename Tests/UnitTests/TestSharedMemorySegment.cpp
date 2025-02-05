#include <gtest/gtest.h>
#include "../../Source/Helpers/SharedMemorySegment.hpp"

class SharedMemorySegmentTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(SharedMemorySegmentTest, ConstructorTest) {
    SharedMemorySegment segment("TestSegment");
}

TEST_F(SharedMemorySegmentTest, MallocTest) {
    SharedMemorySegment segment("TestSegment");
    void* ptr = segment.malloc(1024);
    ASSERT_NE(ptr, nullptr); 
}

TEST_F(SharedMemorySegmentTest, ReallocTest) {
    SharedMemorySegment segment("TestSegment");
    void* ptr = segment.malloc(1024);
    ASSERT_NE(ptr, nullptr); 

    void* newPtr = segment.realloc(ptr, 2048);
    ASSERT_NE(newPtr, nullptr); 
    ASSERT_EQ(ptr, newPtr); 
}

TEST_F(SharedMemorySegmentTest, FreeTest) {
    SharedMemorySegment segment("TestSegment");
    void* ptr = segment.malloc(1024);
    ASSERT_NE(ptr, nullptr); 

    segment.free(ptr);
}

TEST_F(SharedMemorySegmentTest, EraseTest) {
    SharedMemorySegment segment("TestSegment");
    segment.erase();
}
