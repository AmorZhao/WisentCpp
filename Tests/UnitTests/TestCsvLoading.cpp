#include <gtest/gtest.h>
#include "../../Src/Helpers/CsvLoading.hpp"
#include "helpers/unitTestHelpers.hpp"

class CsvLoadingTest : public ::testing::Test 
{
protected:
    std::string MockCsvFileContent = "Name,Age,Height\nAlice,30,165.5\nBob,25,185.5";
    std::string MockCsvFilename = "mock_data.csv";

    void SetUp() override 
    {
        createTempFile(MockCsvFilename, MockCsvFileContent);
    }

    void TearDown() override 
    {
        std::remove(MockCsvFilename.c_str());
    }
};

TEST_F(CsvLoadingTest, OpenCsvFile) 
{
    auto doc = openCsvFile(MockCsvFilename);
    ASSERT_EQ(doc.GetRowCount(), 2);
    ASSERT_EQ(doc.GetColumnCount(), 3);
}

TEST_F(CsvLoadingTest, LoadCsvData) 
{
    auto doc = openCsvFile(MockCsvFilename);
    auto data = loadCsvData<int64_t>(doc, "Age");

    ASSERT_EQ(data.size(), 2);
    ASSERT_TRUE(data[0].has_value());
    ASSERT_EQ(data[0].value(), 30);
    ASSERT_TRUE(data[1].has_value());
    ASSERT_EQ(data[1].value(), 25);
}

TEST_F(CsvLoadingTest, LoadCsvDataToJson) 
{
    auto doc = openCsvFile(MockCsvFilename);
    
    json jsonData = loadCsvDataToJson<int64_t>(doc, "Age");
    ASSERT_EQ(jsonData.size(), 2);
    ASSERT_EQ(jsonData[0], 30);
    ASSERT_EQ(jsonData[1], 25);

    json jsonData2 = loadCsvDataToJson<int64_t>(doc, "Height");
    ASSERT_TRUE(jsonData2.is_null());
    jsonData2 = loadCsvDataToJson<double_t>(doc, "Height");
    ASSERT_EQ(jsonData2.size(), 2);
    ASSERT_EQ(jsonData2[0], 165.5);
    ASSERT_EQ(jsonData2[1], 185.5);

    json jsonData0 = loadCsvDataToJson<int64_t>(doc, "Name");
    ASSERT_TRUE(jsonData0.is_null());
    jsonData0 = loadCsvDataToJson<double_t>(doc, "Name");
    ASSERT_TRUE(jsonData0.is_null());
    jsonData0 = loadCsvDataToJson<std::string>(doc, "Name");
    ASSERT_EQ(jsonData0.size(), 2);
    ASSERT_EQ(jsonData0[0], "Alice");
    ASSERT_EQ(jsonData0[1], "Bob");
}
