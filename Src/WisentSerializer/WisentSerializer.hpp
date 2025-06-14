#pragma once
#include "../Helpers/WisentHelpers/WisentHelpers.hpp"
#include "../Helpers/Result.hpp"
#include <string>
#include <cassert>
#include <sys/resource.h>

namespace wisent 
{
    namespace serializer 
    {
        Result<WisentRootExpression*> load(
            std::string const& path, 
            std::string const& sharedMemoryName,
            std::string const& csvPrefix, 
            bool disableRLE = false,
            bool disableCsvHandling = false, 
            bool forceReload = false
        );

        void unload(
            std::string const& sharedMemoryName
        );

        void free(
            std::string const& sharedMemoryName
        );
    }
}
