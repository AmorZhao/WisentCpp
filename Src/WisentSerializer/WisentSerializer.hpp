#include "WisentHelpers.h"
#include "../Helpers/ISharedMemorySegment.hpp"
#include <string>
namespace wisent 
{
    namespace serializer 
    {
        WisentRootExpression* load(
            ISharedMemorySegments *sharedMemorySegments,
            std::string const& path, 
            std::string const& sharedMemoryName,
            std::string const& csvPrefix, 
            bool disableRLE = false,
            bool disableCsvHandling = false, 
            bool enableDeltaEncoding = false,
            bool enableHuffmanEncoding = false, 
            bool forceReload = false
        );

        void unload(
            ISharedMemorySegments *sharedMemorySegments,
            std::string const& sharedMemoryName
        );

        void free(
            ISharedMemorySegments *sharedMemorySegments,
            std::string const& sharedMemoryName
        );
    }
}
