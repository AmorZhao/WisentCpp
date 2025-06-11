#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include "../Helpers/CompressionHelpers/Algorithms.hpp"
#include "../Helpers/Result.hpp"

using namespace wisent::algorithms;

class CompressionPipeline 
{
  private:
    std::vector<CompressionType> pipeline;
    std::unordered_map<CompressionType, std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>> customFunctions;

  public:
    CompressionPipeline() = default;
    CompressionPipeline(
        const std::vector<CompressionType>& steps,
        const std::unordered_map<CompressionType, std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>>& customFunctions
    ) : pipeline(steps), customFunctions(customFunctions) {}

    void log() const 
    {
        std::cout << "Logging compression pipeline:\n";
        for (CompressionType step : pipeline) 
        {
            std::cout << " - " << compressionTypeToString(step) << std::endl;
        }
    }

    std::vector<uint8_t> compress(
        const std::vector<uint8_t>& data,
        Result<size_t>& result
    ) const {
        std::vector<uint8_t> current = data;

        for (CompressionType type : pipeline) 
        {
            std::vector<uint8_t> compressed;
            if (type == CompressionType::CUSTOM) 
            {
                auto it = customFunctions.find(type);
                if (it == customFunctions.end()) 
                {
                    throw std::runtime_error("Custom compression function not found.");
                }
                compressed = it->second(current);
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

        result.value = current.size();
        return current;
    }

    class Builder 
    {
      private:
        std::vector<CompressionType> steps;
        std::unordered_map<CompressionType, std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>> customFunctions;

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
            customFunctions[CompressionType::CUSTOM] = std::move(customFunction);
            return *this;
        }

        CompressionPipeline build() {
            return CompressionPipeline(steps, customFunctions);
        }
    };
};
