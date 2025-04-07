#include <gtest/gtest.h>
#include "../../Src/BsonSerializer/BsonSerializer.hpp"
#include "../../Src/Helpers/ISharedMemorySegment.hpp"
#include "helpers/unitTestHelpers.hpp"
#include <string>

class BsonSerializerTest : public ::testing::Test 
{
protected:
    const std::string MockSharedMemoryName = "MockSharedMemory";

    const std::string MockCsvPrefix = "";
    const std::string MockCsvFileName = "MockCsvFilename.csv";
    const std::string MockCsvFileContent = "Name,Age\nAlice,30\nBob,25";
    
    const std::string MockFileName = "MockFileName.json";
    const std::string MockFileContent = R"({
        "Name": "string", 
        "Age": "int",
        "data": "MockCsvFilename.csv"
    })";

    void SetUp() override 
    {
        createTempFile(MockCsvFileName, MockCsvFileContent);
        createTempFile(MockFileName, MockFileContent);
    }

    void TearDown() override 
    {
        std::remove(MockCsvFileName.c_str());
        std::remove(MockFileName.c_str());
    }
};

TEST_F(BsonSerializerTest, LoadAsBson) 
{
    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(MockSharedMemoryName);

    void* result = bson::serializer::loadAsBson(
        MockFileName, 
        MockSharedMemoryName, 
        MockCsvPrefix
    );
    ASSERT_NE(result, nullptr);

    bson::serializer::free(MockSharedMemoryName);
}

TEST_F(BsonSerializerTest, LoadAsJson) 
{
    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(MockSharedMemoryName);

    void* result = bson::serializer::loadAsJson(
        MockFileName, 
        MockSharedMemoryName, 
        MockCsvPrefix
    );
    ASSERT_NE(result, nullptr);

    bson::serializer::free(MockSharedMemoryName);
}

TEST_F(BsonSerializerTest, Unload) 
{   
    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(MockSharedMemoryName); 
    sharedMemory->load();
    ASSERT_TRUE(sharedMemory->isLoaded());
    
    bson::serializer::unload(MockSharedMemoryName);
    ASSERT_FALSE(sharedMemory->isLoaded());
}

TEST_F(BsonSerializerTest, Free) 
{
    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(MockSharedMemoryName); 
    sharedMemory->load();
    ASSERT_TRUE(sharedMemory->isLoaded());
    
    bson::serializer::free(MockSharedMemoryName);
    // ASSERT_EQ(SharedMemorySegments::getCurrentSharedMemory(), nullptr);
    ASSERT_EQ(SharedMemorySegments::getSharedMemorySegments().size(), 0);
}