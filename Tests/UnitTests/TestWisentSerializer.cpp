#include <gtest/gtest.h>
#include "../../Source/WisentSerializer/WisentSerializer.hpp"
#include "../../Source/Helpers/CsvLoading.hpp"
#include "../../Source/Helpers/SharedMemorySegment.hpp"
#include "helpers/unitTestHelpers.hpp"
#include <string>
#include <fstream>

TEST(WisentSerializerTest, WisentLoad) {
    const char* sharedMemoryName = "TestSharedMemory";
    const char* csvPrefix = "test_prefix";
    const char* content = R"({"key": "value"})";
    std::string filename = createTempFile(content);

    WisentRootExpression* result = wisent::serializer::load(filename.c_str(), sharedMemoryName, csvPrefix);
    ASSERT_NE(result, nullptr);

    std::remove(filename.c_str());
    wisent::serializer::free(sharedMemoryName);
}

TEST(WisentSerializerTest, WisentUnload) {
    const char* sharedMemoryName = "TestSharedMemory";
    const char* csvPrefix = "test_prefix";
    const char* content = R"({"key": "value"})";
    std::string filename = createTempFile(content);

    WisentRootExpression* result = wisent::serializer::load(filename.c_str(), sharedMemoryName, csvPrefix);
    ASSERT_NE(result, nullptr);

    wisent::serializer::unload(sharedMemoryName);

    auto& sharedMemory = createOrGetMemorySegment(sharedMemoryName);
    ASSERT_FALSE(sharedMemory.loaded());

    std::remove(filename.c_str());
    wisent::serializer::free(sharedMemoryName);
}

TEST(WisentSerializerTest, WisentFree) {
    const char* sharedMemoryName = "TestSharedMemory";
    const char* csvPrefix = "test_prefix";
    const char* content = R"({"key": "value"})";
    std::string filename = createTempFile(content);

    WisentRootExpression* result = wisent::serializer::load(filename.c_str(), sharedMemoryName, csvPrefix);
    ASSERT_NE(result, nullptr);

    wisent::serializer::free(sharedMemoryName);

    auto& sharedMemory = createOrGetMemorySegment(sharedMemoryName);
    ASSERT_FALSE(sharedMemory.loaded());
    ASSERT_EQ(sharedMemorySegments().count(sharedMemoryName), 0);

    std::remove(filename.c_str());
}
