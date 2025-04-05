#include <string>
#include "../Helpers/ISharedMemorySegment.hpp"

namespace wisent 
{
    namespace parser 
    {
        std::string query(std::string const& query);

        std::string parse(
            ISharedMemorySegments *sharedMemorySegments,
            std::string const& sharedMemoryName
        );
    }
}