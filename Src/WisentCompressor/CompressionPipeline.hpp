#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include "../Helpers/CompressionHelpers/Algorithms.hpp"
#include "../Helpers/Result.hpp"

using namespace wisent::algorithms;

class CompressionPipeline 
{
  private:
    std::vector<CompressionType> pipeline;
    std::vector<std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>> customFunctions;

  public:
    CompressionPipeline() = default;
    CompressionPipeline(
        const std::vector<CompressionType>& steps,
        const std::vector<std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>>& customFunctions
    ) 
    : pipeline(steps)
    , customFunctions(customFunctions)
    {}

    void log() const 
    {
        std::cout << "Logging compression pipeline:\n";
        for (CompressionType step : pipeline) 
        {
            std::cout << " - " << compressionTypeToString(step) << std::endl;
        }
    }

    std::vector<CompressionType> getPipeline() const 
    {
        return pipeline;
    }

    Result<std::vector<uint8_t>> compress(
        const std::vector<uint8_t>& data
    ) const {
        Result<std::vector<uint8_t>> result; 
        std::vector<uint8_t> current = data;

        for (CompressionType type : pipeline) 
        {
            std::vector<uint8_t> compressed;
            if (type == CompressionType::CUSTOM) 
            {
                static size_t customIndex = 0;
                if (customIndex >= customFunctions.size()) 
                {
                    result.setError("Custom compression function not found.");
                    return result; 
                }
                compressed = customFunctions[customIndex](current);
                ++customIndex;
            } 
            else 
            {
                compressed = performCompression(type, current);
            }
            if (compressed.size() >= current.size()*2) 
            {
                result.addWarning("Custom compression " + compressionTypeToString(type) + " did not reduce size, skipping.");
            }
            current = compressed;
        }

        result.setValue(current);
        return result;
    }

    class Builder 
    {
      private:
        std::vector<CompressionType> steps;
        std::vector<std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>> customFunctions;

      public:
        Builder& addStep(CompressionType type) {
            steps.push_back(type);
            return *this;
        }

        Builder& addStep(const std::string& typeString) {
            steps.push_back(stringToCompressionType(typeString));
            return *this;
        }

        Builder& addStep(std::function<std::vector<uint8_t>(
            const std::vector<uint8_t>&)> customFunction
        ) {
            steps.push_back(CompressionType::CUSTOM);
            customFunctions.push_back(std::move(customFunction));
            return *this;
        }

        CompressionPipeline build() {
            return CompressionPipeline(steps, customFunctions);
        }
    };
};


// same builder implementation but calls decompress functions
#pragma region decompression_pipeline

class DecompressionPipeline 
{
  private:
    std::vector<CompressionType> pipeline;
    std::vector<std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>> customFunctions;

  public:
    DecompressionPipeline() = default;
    DecompressionPipeline(
        const std::vector<CompressionType>& steps,
        const std::vector<std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>>& customFunctions
    ) 
    : pipeline(steps)
    , customFunctions(customFunctions)
    {}

    void log() const 
    {
        std::cout << "Logging compression pipeline:\n";
        for (CompressionType step : pipeline) 
        {
            std::cout << " - " << compressionTypeToString(step) << std::endl;
        }
    }

    std::vector<CompressionType> getPipeline() const 
    {
        return pipeline;
    }

    Result<std::vector<uint8_t>> decompress(
        const std::vector<uint8_t>& data
    ) const {
        Result<std::vector<uint8_t>> result; 
        std::vector<uint8_t> current = data;

        for (CompressionType type : pipeline) 
        {
            std::vector<uint8_t> compressed;
            if (type == CompressionType::CUSTOM) 
            {
                static size_t customIndex = 0;
                if (customIndex >= customFunctions.size()) 
                {
                    result.setError("Custom compression function not found.");
                    return result; 
                }
                compressed = customFunctions[customIndex](current);
                ++customIndex;
            } 
            else 
            {
                compressed = performDecompression(type, current);
            }
            if (compressed.size() >= current.size()*2) 
            {
                result.addWarning("Custom compression " + compressionTypeToString(type) + " did not reduce size, skipping.");
            }
            current = compressed;
        }

        result.setValue(current);
        return result;
    }

    class Builder 
    {
      private:
        std::vector<CompressionType> steps;
        std::vector<std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>> customFunctions;

      public:
        Builder& addStep(CompressionType type) {
            steps.push_back(type);
            return *this;
        }

        Builder& addStep(const std::string& typeString) {
            steps.push_back(stringToCompressionType(typeString));
            return *this;
        }

        Builder& addStep(std::function<std::vector<uint8_t>(
            const std::vector<uint8_t>&)> customFunction
        ) {
            steps.push_back(CompressionType::CUSTOM);
            customFunctions.push_back(std::move(customFunction));
            return *this;
        }

        CompressionPipeline build() {
            return CompressionPipeline(steps, customFunctions);
        }
    };
};

#pragma endregion decompression_pipeline