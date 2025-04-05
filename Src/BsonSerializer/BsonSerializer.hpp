#include <string>
#include "../Helpers/ISharedMemorySegment.hpp"
namespace bson 
{
    namespace serializer 
    {
        void* loadAsBson(
            ISharedMemorySegments *sharedMemorySegments, 
            std::string const& path, 
            std::string const& sharedMemoryName,
            std::string const& csvPrefix, 
            bool disableCsvHandling = false,
            bool forceReload = false
        );

        void* loadAsJson(
            ISharedMemorySegments *sharedMemorySegments,
            std::string const& path, 
            std::string const& sharedMemoryName,
            std::string const& csvPrefix, 
            bool disableCsvHandling = false,
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