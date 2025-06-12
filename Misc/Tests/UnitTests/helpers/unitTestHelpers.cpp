#include "unitTestHelpers.hpp"
#include <fstream>

void createTempFile(const std::string& filename, const std::string& content) 
{
    std::ofstream file(filename);
    file << content;
    file.close();
}