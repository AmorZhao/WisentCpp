#include "ServerHelpers.hpp"
#include "BsonSerializer/BsonSerializer.hpp"
#include "Helpers/CsvLoading.hpp"
#include "WisentCompressor/CompressionPipeline.hpp"
#include <fstream>
#include <string>
#include <filesystem>

void parseRequestParams(
    const httplib::Params &params, 
    std::string &filename, 
    std::string &filepath, 
    std::string &csvPrefix,
    bool &disableRLE, 
    bool &disableCsvHandling
) {
    filename = params.find("name") != params.end() ? params.find("name")->second : "";
    filepath = params.find("path") != params.end() ? params.find("path")->second : "";
    csvPrefix = filepath.substr(0, filepath.find_last_of("/\\") + 1);

    if (params.find("disableRLE") != params.end()) 
    {
        auto const &str = params.find("disableRLE")->second;
        disableRLE = (str.empty() || str == "True" || str == "true" || atoi(str.c_str()) > 0);
    }

    if (params.find("disableCsvHandling") != params.end()) 
    {
        auto const &str = params.find("disableCsvHandling")->second;
        disableCsvHandling = (str.empty() || str == "True" || str == "true" || atoi(str.c_str()) > 0);
    }
}

void parseCompressionPipeline(
    const std::string &body, 
    Result<std::unordered_map<std::string, CompressionPipeline>> &result
) {
    json pipelineSpecification; 
    try {
        pipelineSpecification = json::parse(body);
    } 
    catch (const std::exception &e) {
        std::string errorMessage = "Error parsing request body: " + std::string(e.what());
        result.setError(errorMessage);
    }

    std::unordered_map<std::string, CompressionPipeline> CompressionPipelineMap;
    for (const auto& [columnName, steps] : pipelineSpecification.items()) 
    {
        CompressionPipeline::Builder builder;
        for (const std::string& step : steps) 
        {
            builder.addStep(step);
        }
        CompressionPipelineMap[columnName] = builder.build();
    }
    result.setValue(CompressionPipelineMap);
}

bool WriteBufferToFile(
    const char* folderPath, 
    const char* fileName, 
    const char* data, 
    size_t dataSize
) {
    std::filesystem::create_directories(folderPath);

    std::string fullPath = std::string(folderPath) + "/" + fileName;
    std::ofstream outFile(fullPath, std::ios::binary);
    if (!outFile) 
    {
        return false;
    }
    
    outFile.write(data, dataSize);
    return outFile.good();
}
