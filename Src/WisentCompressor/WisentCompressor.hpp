#include <string>

namespace wisent 
{
    namespace compressor 
    {
        enum class CompressionType {
            NONE,
            RLE,
            HUFFMAN,
            LZ77,
            FSE
        };

        std::string compress(
            std::string const& sharedMemoryName, 
            CompressionType compressionType = CompressionType::NONE
        );
    }
}