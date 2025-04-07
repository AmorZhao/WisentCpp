#pragma once
#include <string>
#include <iostream>
#include "../../Include/json.h"

using nlohmann::json; 

enum class CompressionType {
    NONE,
    RLE,
    HUFFMAN,
    LZ77,
    FSE
};

static const std::unordered_map<std::string, CompressionType> compressionAliases = 
{
    {"none", CompressionType::NONE},
    {"rle", CompressionType::RLE},
    {"runlengthencoding", CompressionType::RLE},
    {"huffman", CompressionType::HUFFMAN},
    {"lz77", CompressionType::LZ77},
    {"fse", CompressionType::FSE},
    {"finitestateentropy", CompressionType::FSE}
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
        std::vector<CompressionType> argumentVectorChain;
        std::vector<CompressionType> typeBytefieldChain;
        std::vector<CompressionType> structureVectorChain;
        std::vector<CompressionType> stringBufferChain;
        bool isValidPipeline = true; 

    public:
    CompressionPipeline(const json& pipelineSpec) 
    {
        const std::unordered_map<std::string, std::vector<CompressionType>*> chainMap = {
            {"argumentVector", &argumentVectorChain},
            {"typeBytefield", &typeBytefieldChain},
            {"structureVector", &structureVectorChain},
            {"stringBuffer", &stringBufferChain}
        };

        for (const auto& pair : chainMap) 
        {
            const auto& key = pair.first;
            auto& chain = pair.second;
            if (!pipelineSpec.contains(key)) 
            {
                std::cerr << "Missing compression spec for '" << key << "'" << std::endl;
                isValidPipeline = false;
                return;
            }

            for (const auto& item : pipelineSpec[key]) 
            {
                try {
                    chain->push_back(stringToCompressionType(item));
                } 
                catch (const std::exception& e) {
                    std::cerr << "Invalid compression type in '" << key << "': " << item << std::endl;
                    isValidPipeline = false;
                    return;
                }
            }
        }
    }

    std::vector<CompressionType> getArgumentVector() const 
    {   return argumentVectorChain; }

    std::vector<CompressionType> getTypeBytefield() const 
    {   return typeBytefieldChain; }

    std::vector<CompressionType> getStructureVector() const 
    {   return structureVectorChain; }

    std::vector<CompressionType> getStringBuffer() const 
    {   return stringBufferChain; } 

    bool isValid() const { return isValidPipeline; }
};
