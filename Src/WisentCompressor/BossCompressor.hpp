#pragma once
#include <string>
#include <unordered_map>
#include "../Helpers/Result.hpp"
#include "../Helpers/BossHelpers/BossExpression.hpp"
#include "../Helpers/WisentHelpers/BossToPortableBoss.hpp"
#include "CompressionPipeline.hpp"

namespace wisent
{
    namespace compressor
    {
        template <
            void* (*Allocate)(size_t) = std::malloc,
            void* (*Reallocate)(void*, size_t) = std::realloc,
            void  (*Free)(void*) = std::free
        >
        Result<boss::serialization::SerializedBossExpression<Allocate, Reallocate, Free>*>
        CompressAndLoadBossExpression(
            boss::Expression &&input, 
            std::unordered_map<std::string, CompressionPipeline*> &compressionPipelineMap, 
            std::string const &sharedMemoryName, 
            bool dictEncodeStrings = true,
            bool dictEncodeDoublesAndLongs = false, 
            bool forceReload = false
        );
    }
}