#include <string>
#include "CompressionPipeline.hpp"

namespace wisent 
{
    namespace compressor 
    {
        std::string compress(
            std::string const& sharedMemoryName, 
            CompressionType compressionType = CompressionType::NONE
        );

        std::string compress(
            std::string const& sharedMemoryName, 
            CompressionPipeline *pipeline
        ); 

        std::string decompress(
            std::string const& sharedMemoryName
        );
    }
}
