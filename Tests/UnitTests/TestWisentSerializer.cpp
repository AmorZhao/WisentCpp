#include <gtest/gtest.h>
#include "../../Src/WisentSerializer/WisentSerializer.hpp"
#include "../../Src/Helpers/ISharedMemorySegment.hpp"
#include "helpers/unitTestHelpers.hpp"
#include <string>

class WisentSerializerTest : public ::testing::Test 
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

TEST_F(WisentSerializerTest, WisentLoad) 
{
    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(MockSharedMemoryName);

    WisentRootExpression* result = wisent::serializer::load(
        MockFileName, 
        MockSharedMemoryName, 
        MockCsvPrefix
    );
    ASSERT_NE(result, nullptr);

    wisent::serializer::free(MockSharedMemoryName);
}

TEST_F(WisentSerializerTest, WisentUnload) {

    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(MockSharedMemoryName);

    WisentRootExpression* result = wisent::serializer::load(
        MockFileName, 
        MockSharedMemoryName, 
        MockCsvPrefix
    );

    wisent::serializer::unload(MockSharedMemoryName);
    ASSERT_FALSE(sharedMemory->isLoaded());
    ASSERT_EQ(SharedMemorySegments::getSharedMemorySegments().size(), 1);
}

TEST_F(WisentSerializerTest, WisentFree) {
    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(MockSharedMemoryName);

    WisentRootExpression* result = wisent::serializer::load(
        MockFileName, 
        MockSharedMemoryName, 
        MockCsvPrefix
    );

    wisent::serializer::free(MockSharedMemoryName);
    ASSERT_EQ(SharedMemorySegments::getSharedMemorySegments().size(), 0);
}
