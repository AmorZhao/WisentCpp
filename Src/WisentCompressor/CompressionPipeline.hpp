#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <stdexcept>

enum class CompressionType {
    NONE,
    RLE,
    HUFFMAN,
    LZ77,
    FSE, 
    DELTA
};

static const std::unordered_map<std::string, CompressionType> compressionAliases = 
{
    {"none", CompressionType::NONE},
    {"rle", CompressionType::RLE},
    {"runlengthencoding", CompressionType::RLE},
    {"huffman", CompressionType::HUFFMAN},
    {"lz77", CompressionType::LZ77},
    {"fse", CompressionType::FSE},
    {"finitestateentropy", CompressionType::FSE},
    {"delta", CompressionType::DELTA},
    {"de", CompressionType::DELTA}
};

static CompressionType stringToCompressionType(std::string type) 
{
    std::transform(type.begin(), type.end(), type.begin(), ::tolower);
    auto it = compressionAliases.find(type);
    if (it == compressionAliases.end()) 
    {
        throw std::invalid_argument("Unknown compression type: " + type);
    }
    return it->second;
}

class CompressionPipeline 
{
private:
    std::vector<CompressionType> pipeline;

    CompressionPipeline(const std::vector<CompressionType>& steps) : pipeline(steps) {}

    std::string compressionTypeToString(CompressionType type) const 
    {
        switch (type) 
        {
            case CompressionType::NONE: return "None";
            case CompressionType::RLE: return "RLE";
            case CompressionType::HUFFMAN: return "Huffman";
            case CompressionType::LZ77: return "LZ77";
            case CompressionType::FSE: return "FSE";
            case CompressionType::DELTA: return "Delta";
            default: return "Unknown";
        }
    }

public:
    void log() const 
    {
        std::cout << "Logging compression pipeline:" << std::endl;
        for (CompressionType step : pipeline) 
        {
            std::cout << " - " << compressionTypeToString(step) << std::endl;
        }
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

        Builder& addStep(const std::string& typeStr) 
        {
            steps.push_back(stringToCompressionType(typeStr));
            return *this;
        }

        CompressionPipeline build() 
        {
            return CompressionPipeline(steps);
        }
    };
};
