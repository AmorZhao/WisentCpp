#include "BossSerializer.hpp"

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
) {
    Result<boss::serialization::SerializedExpression<Allocate, Reallocate, Free>*> result;

    using SerializedExpression = boss::serialization::SerializedExpression<Allocate, Reallocate, Free>;
    auto serializedExpression = new SerializedExpression(
        std::move(input), 
        dictEncodeStrings, 
        dictEncodeDoublesAndLongs
    );

    result.setValue(serializedExpression);
    return result;
}