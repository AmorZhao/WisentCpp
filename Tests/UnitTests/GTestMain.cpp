#include <gtest/gtest.h>

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::GTEST_FLAG(filter) = 
        ":MockSharedMemorySegmentsTest.*"
        ":CsvLoadingTest.*"
        ":TestCompression.*"
        ":BsonSerializerTest.*"
        ":WisentSerializerTest.*"
        ":WisentCompressorTest.*"; 
    return RUN_ALL_TESTS();
}