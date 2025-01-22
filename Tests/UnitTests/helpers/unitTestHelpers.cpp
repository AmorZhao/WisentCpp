#include "unitTestHelpers.hpp"
#include <fstream>

std::string createTempFile(const std::string& content) 
{
    std::string filename = "temp_test_file.txt";
    std::ofstream file(filename);
    file << content;
    file.close();
    return filename;
}


std::string createTempCsvFile(const std::string& content) {
    std::string filename = "temp_test.csv";
    std::ofstream file(filename);
    file << content;
    file.close();
    return filename;
}