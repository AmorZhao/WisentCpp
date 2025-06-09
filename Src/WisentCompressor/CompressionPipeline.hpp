#pragma once
#include <iostream>
#include <vector>
#include <string>
#include "../Helpers/CompressionHelpers/Algorithms.hpp"
#include "../Helpers/Result.hpp"

using namespace wisent::algorithms; 

class CompressionPipeline 
{
private:
    std::vector<CompressionType> pipeline;

    CompressionPipeline(const std::vector<CompressionType>& steps) : pipeline(steps) {}

public:
    void log() const 
    {
        std::cout << "Logging compression pipeline:" << std::endl;
        for (CompressionType step : pipeline) 
        {
            std::cout << " - " << compressionTypeToString(step) << std::endl;
        }
    }

    std::vector<uint8_t> compress(
        const std::vector<uint8_t>& data, 
        Result<size_t>& result
    ) const {
        for (CompressionType type : pipeline) 
        {
            auto compressedData = performCompression(type, data);
        }
        return data;
    }

    class Builder 
    {
    private:
        std::vector<CompressionType> steps;

    public:
        Builder& addStep(CompressionType type) 
        {
            steps.push_back(type);
            return *this;
        }

        Builder& addStep(const std::string& typeString) 
        {
            steps.push_back(stringToCompressionType(typeString));
            return *this;
        }

        CompressionPipeline build() 
        {
            return CompressionPipeline(steps);
        }
    };
};
