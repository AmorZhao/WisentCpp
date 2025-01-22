#include <gtest/gtest.h>
#include "../../Source/CsvLoading.hpp"
#include "helpers/unitTestHelpers.hpp"
#include <fstream>

TEST(CsvLoadingTest, OpenCsvFile) {
    std::string csvContent = "Name,Age\nAlice,30\nBob,25";
    std::string filename = createTempCsvFile(csvContent);

    auto doc = openCsvFile(filename);
    ASSERT_EQ(doc.GetRowCount(), 2);
    ASSERT_EQ(doc.GetColumnCount(), 2);

    std::remove(filename.c_str());
}

TEST(CsvLoadingTest, LoadCsvData) {
    std::string csvContent = "Name,Age\nAlice,30\nBob,25";
    std::string filename = createTempCsvFile(csvContent);

    auto doc = openCsvFile(filename);
    auto data = loadCsvData<int64_t>(doc, "Age");

    ASSERT_EQ(data.size(), 2);
    ASSERT_TRUE(data[0].has_value());
    ASSERT_EQ(data[0].value(), 30);
    ASSERT_TRUE(data[1].has_value());
    ASSERT_EQ(data[1].value(), 25);

    std::remove(filename.c_str());
}

TEST(CsvLoadingTest, LoadCsvDataToJson) {
    std::string csvContent = "Name,Age\nAlice,30\nBob,25";
    std::string filename = createTempCsvFile(csvContent);

    auto doc = openCsvFile(filename);
    auto jsonData = loadCsvDataToJson<int64_t>(doc, "Age");

    ASSERT_EQ(jsonData.size(), 2);
    ASSERT_EQ(jsonData[0], 30);
    ASSERT_EQ(jsonData[1], 25);

    std::remove(filename.c_str());
}
