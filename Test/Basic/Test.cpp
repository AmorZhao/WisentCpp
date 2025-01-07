#include "../../Include/httplib.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <cassert>

long long measureRequestTime(const std::string& url) {
    httplib::Client cli("http://localhost:3000");
    auto start = std::chrono::high_resolution_clock::now();
    
    auto res = cli.Get(url.c_str());
    
    auto end = std::chrono::high_resolution_clock::now();
    auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    if (res && res->status == 200) 
    {
        return timeDiff;  
    } 
    else 
    {
        std::cerr << "Request failed: " << url << std::endl;
        return -1;
    }
}

void runTests() {
    std::vector<std::string> filepaths = {"/root/Wisent++/Data/owid-deaths/datapackage.json"};
    
    for (const auto& filepath : filepaths) {
        std::string baseUrl = "/load?name=" + filepath.substr(filepath.find_last_of("/\\") + 1);
        baseUrl += "&path=" + filepath;

        std::string urlJson = baseUrl + "&toJson=true";
        std::cout << "Testing loading JSON file: " << filepath << std::endl;
        long long loadTimeJson = measureRequestTime(urlJson);
        if (loadTimeJson != -1) {
            std::cout << "Time taken to load as JSON: " << loadTimeJson << " ms" << std::endl;
        }

        std::string urlBson = baseUrl + "&toBson=true";
        std::cout << "Testing loading BSON file: " << filepath << std::endl;
        long long loadTimeBson = measureRequestTime(urlBson);
        if (loadTimeBson != -1) {
            std::cout << "Time taken to load as BSON: " << loadTimeBson << " ms" << std::endl;
        }

        std::string urlCsv = baseUrl + "&loadCSV=true";
        std::cout << "Testing loading CSV file: " << filepath << std::endl;
        long long loadTimeCsv = measureRequestTime(urlCsv);
        if (loadTimeCsv != -1) {
            std::cout << "Time taken to load CSV file: " << loadTimeCsv << " ms" << std::endl;
        }

        std::cout << "Testing loading file default: " << filepath << std::endl;
        long long loadTimeWisent = measureRequestTime(baseUrl);
        if (loadTimeWisent != -1) {
            std::cout << "Time taken to load Wisent file: " << loadTimeWisent << " ms" << std::endl;
        }


        std::string urlUnload = "/unload?name=" + filepath.substr(filepath.find_last_of("/\\") + 1);
        std::cout << "Unloading dataset: " << filepath << std::endl;
        long long unloadTime = measureRequestTime(urlUnload);
        if (unloadTime != -1) {
            std::cout << "Time taken to unload: " << unloadTime << " ms" << std::endl;
        }

        std::string urlErase = "/erase?name=" + filepath.substr(filepath.find_last_of("/\\") + 1);
        std::cout << "Erasing dataset: " << filepath << std::endl;
        long long eraseTime = measureRequestTime(urlErase);
        if (eraseTime != -1) {
            std::cout << "Time taken to erase: " << eraseTime << " ms" << std::endl;
        }
    }
}

int main() 
{
    runTests();
    return 0;
}
