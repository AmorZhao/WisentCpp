#pragma once
#include "../Include/httplib.h"
#include "WisentCompressor/CompressionPipeline.hpp"

void parseRequestParams(
    const httplib::Params &params, 
    std::string &filename, 
    std::string &filepath, 
    std::string &csvPrefix,
    bool &disableRLE, 
    bool &disableCsvHandling
); 

void parseCompressionPipeline(
    const std::string &body, 
    Result<std::unordered_map<std::string, CompressionPipeline>> &result
); 

bool WriteBufferToFile(
    const char* folderPath, 
    const char* fileName, 
    const char* data, 
    size_t dataSize
); 

template<typename T>
void handleResponse(
    httplib::Response &res,
    Result<T>& result, 
    const std::chrono::high_resolution_clock::time_point &start,
    const std::chrono::high_resolution_clock::time_point &end
) {
    if (!result.success()) 
    {
        std::string errorMessage = "Error: " + result.error.value();
        std::cerr << errorMessage << std::endl;
        res.status = httplib::BadRequest_400; 
        res.set_content(errorMessage, "text/plain");
    }

    auto timeDiff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    
    std::string successMessage = "Success in " + std::to_string(timeDiff * 0.000000001) + " s.";

    if (!result.warnings.empty()) 
    {
        std::string warningsStr = " Warnings: ";
        for (const auto& warning : result.warnings) 
        {
            warningsStr += warning + "; ";
        }
        successMessage += warningsStr;
    }
    if (result.value.has_value()) 
    {
        if constexpr (std::is_convertible_v<T, std::string> || std::is_same_v<T, std::string>) 
        {
            successMessage += " Result: " + result.value.value();
        } 
    }
    
    std::cout << successMessage << std::endl;
    res.set_content(successMessage, "text/plain");
}; 