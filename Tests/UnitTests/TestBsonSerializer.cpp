#include <gtest/gtest.h>
#include "../../Src/BsonSerializer/BsonSerializer.hpp"
#include "../../Src/Helpers/ISharedMemory.hpp"
#include "helpers/unitTestHelpers.hpp"
#include <string>

const std::string MockSharedMemoryName = "MockSharedMemory";
const std::string MockCsvPrefix = "MockCsvPrefix";
const std::string MockFileContent = R"({
    "key": "value",
    "nested": {
        "innerKey": "innerValue"
    },
    "array": [1, 2, 3]
})";
const std::istringstream MockFileStream(MockFileContent);
const std::string MockFileName = createTempFile(MockFileContent);

TEST(BsonSerializerTest, LoadAsBson) 
{
    auto &sharedMemory = createOrGetMemorySegment(MockSharedMemoryName);

    void* result = bson::serializer::loadAsBson(
        MockFileName, 
        MockSharedMemoryName, 
        MockCsvPrefix
    );
    ASSERT_NE(result, nullptr);

    std::remove(MockFileName.c_str());
    bson::serializer::free(MockSharedMemoryName);
}

TEST(BsonSerializerTest, LoadAsJson) 
{
    void* result = bson::serializer::loadAsJson(
        MockFileName, 
        MockSharedMemoryName, 
        MockCsvPrefix
    );
    ASSERT_NE(result, nullptr);

    std::remove(MockFileName.c_str());
    bson::serializer::free(MockSharedMemoryName);
}

TEST(BsonSerializerTest, Unload) 
{   
    auto &sharedMemory = createOrGetMemorySegment(MockSharedMemoryName);
    sharedMemory->load();
    ASSERT_TRUE(sharedMemory->isLoaded());
    
    bson::serializer::unload(MockSharedMemoryName);
    ASSERT_FALSE(sharedMemory->isLoaded());
}

TEST(BsonSerializerTest, Free) 
{
    auto &sharedMemory = createOrGetMemorySegment(MockSharedMemoryName);
    sharedMemory->load();
    ASSERT_TRUE(sharedMemory->isLoaded());
    
    bson::serializer::free(MockSharedMemoryName);
    ASSERT_FALSE(sharedMemory->isLoaded());
    ASSERT_EQ(sharedMemorySegments().count(MockSharedMemoryName), 0);
}