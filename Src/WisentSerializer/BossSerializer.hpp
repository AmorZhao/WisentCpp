#pragma once
#include "../Helpers/WisentHelpers/BossToPortableBoss.hpp"
#include "../Helpers/Result.hpp"
#include <cassert>
#include <sys/resource.h>

namespace wisent 
{
    namespace serializer 
    {
        template <
            void* (*Allocate)(size_t) = std::malloc,
            void* (*Reallocate)(void*, size_t) = std::realloc,
            void  (*Free)(void*) = std::free
        >
        Result<boss::serialization::SerializedExpression<Allocate, Reallocate, Free>*>
        load(
            boss::Expression&& input,
            bool dictEncodeStrings = true,
            bool dictEncodeDoublesAndLongs = false
        );
    }
}