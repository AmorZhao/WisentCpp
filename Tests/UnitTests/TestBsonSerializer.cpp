#include <gtest/gtest.h>
#include "../../Src/BsonSerializer/BsonSerializer.hpp"
#include "../../Src/Helpers/SharedMemorySegment.hpp"
#include "helpers/unitTestHelpers.hpp"
#include <string>

TEST(BsonSerializerTest, Unload) {
    std::string sharedMemoryName = "TestSharedMemory";
    
    auto &sharedMemory = createOrGetMemorySegment(sharedMemoryName);
    sharedMemory.load();
    
    ASSERT_TRUE(sharedMemory.loaded());
    
    bson::serializer::unload(sharedMemoryName);
    
    ASSERT_FALSE(sharedMemory.loaded());
}

TEST(BsonSerializerTest, Free) {
    std::string sharedMemoryName = "TestSharedMemory";
    
    auto &sharedMemory = createOrGetMemorySegment(sharedMemoryName);
    sharedMemory.load();
    
    ASSERT_TRUE(sharedMemory.loaded());
    
    bson::serializer::free(sharedMemoryName);
    
    ASSERT_FALSE(sharedMemory.loaded());
    ASSERT_EQ(sharedMemorySegments().count(sharedMemoryName), 0);
}

TEST(BsonSerializerTest, LoadAsBson) {
    std::string sharedMemoryName = "TestSharedMemory";
    auto &sharedMemory = createOrGetMemorySegment(sharedMemoryName);
    std::string csvPrefix = "test_prefix";
    std::string content = R"({"key": "value"})";
    std::string filename = createTempFile(content);

    void* result = bson::serializer::loadAsBson(
        filename, 
        sharedMemoryName, 
        csvPrefix
    );
    ASSERT_NE(result, nullptr);

    std::remove(filename.c_str());
    bson::serializer::free(sharedMemoryName);
}

TEST(BsonSerializerTest, LoadAsJson) {
    std::string sharedMemoryName = "TestSharedMemory";
    std::string csvPrefix = "test_prefix";
    std::string content = R"({"key": "value"})";
    std::string filename = createTempFile(content);

    void* result = bson::serializer::loadAsJson(
        filename, 
        sharedMemoryName, 
        csvPrefix
    );
    ASSERT_NE(result, nullptr);

    std::remove(filename.c_str());
    bson::serializer::free(sharedMemoryName);
}