#include <cstddef>
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include "Delta.hpp"
#include "RLE.hpp"
#include "LZ77.hpp"
// #include "FSE.hpp"
// #include "Huffman.hpp"


namespace wisent::algorithms
{
    const size_t BytesPerLong = 8;
    const bool usingBlockSize = false;
    const size_t BlockSize = 1024 * 1024; 

    enum class CompressionType {
        NONE,
        DELTA,
        RLE,
        LZ77,
        HUFFMAN,
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
        {"finitestateentropy", CompressionType::FSE},
        {"delta", CompressionType::DELTA},
        {"de", CompressionType::DELTA}
    };

    static const std::unordered_map<CompressionType, std::string> compressionTypeNames = 
    {
        {CompressionType::NONE, "none"},
        {CompressionType::RLE, "rle"},
        {CompressionType::HUFFMAN, "huffman"},
        {CompressionType::LZ77, "lz77"},
        {CompressionType::FSE, "fse"},
        {CompressionType::DELTA, "delta"}
    };

    static std::string compressionTypeToString(CompressionType type)
    {
        return compressionTypeNames.at(type); 
    }

    static CompressionType stringToCompressionType(std::string type); 

    template <typename Coder>
    std::vector<uint8_t> compressWith(
        const char* data, 
        const size_t size
    ) {
        const std::byte* input = reinterpret_cast<const std::byte*>(data);

        std::vector<std::byte> output(size * 2); 
        auto result = Coder::compress(input, size, output.data());
        if (!result.success()) {
            throw std::runtime_error("Compression failed");
        }

        output.resize(result.value.value());

        return std::vector<uint8_t>(
            reinterpret_cast<const uint8_t*>(output.data()),
            reinterpret_cast<const uint8_t*>(output.data() + output.size())
        );
    }

    std::vector<uint8_t> performCompression(
        CompressionType type,
        const std::vector<uint8_t>& buffer
    ); 


    template <typename T>
    std::vector<uint8_t> encode_column(
        const std::vector<T>& column,
        std::vector<std::string>& dictionary,
        std::vector<uint32_t>& indices)
    {
        std::vector<uint8_t> encoded;

        // Handle integer and float types
        if constexpr (std::is_integral<T>::value || std::is_floating_point<T>::value) {
            encoded.reserve(column.size() * sizeof(T));
            for (const auto& value : column) {
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
                encoded.insert(encoded.end(), bytes, bytes + sizeof(T));
            }
        }

        // Handle strings with dictionary encoding
        else if constexpr (std::is_same<T, std::string>::value) {
            std::unordered_map<std::string, uint32_t> dict_map;
            uint32_t dict_index = 0;

            for (const auto& str : column) {
                auto it = dict_map.find(str);
                if (it == dict_map.end()) {
                    dict_map[str] = dict_index++;
                    dictionary.push_back(str);
                    indices.push_back(dict_index - 1);
                } else {
                    indices.push_back(it->second);
                }
            }

            // Store indices as little-endian uint32_t values
            encoded.reserve(indices.size() * sizeof(uint32_t));
            for (uint32_t idx : indices) {
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&idx);
                encoded.insert(encoded.end(), bytes, bytes + sizeof(uint32_t));
            }
        }

        else {
            throw std::runtime_error("Unsupported data type for encoding.");
        }

        return encoded;
    }

}
