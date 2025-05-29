// // Exposes wisent functionalities 

#include "Wisent.hpp"
// #include <emscripten.h>

extern "C" {
    // EMSCRIPTEN_KEEPALIVE
    int simple_add(int a, int b) {
        return a + b;
    }

    // EMSCRIPTEN_KEEPALIVE
    void* loadWisent(
        const uint8_t* inputData,
        size_t inputSize,
        const uint8_t* csvPrefixData,
        size_t csvPrefixSize,
        bool disableRLE,
        bool disableCsvHandling
    ) {
        auto result = wisent::serializer::load(
            inputData,
            inputSize,
            csvPrefixData,
            csvPrefixSize,
            disableRLE,
            disableCsvHandling
        );
        if (result.success()) {
            return (void*)result.value.value_or(nullptr);
        }
        printf("Error loading Wisent: %s\n", result.error.value().c_str());
        return nullptr; 
    }

    // EMSCRIPTEN_KEEPALIVE
    void* compressWisent(
        const uint8_t* inputData,
        size_t inputSize,
        const uint8_t* csvPrefixData,
        size_t csvPrefixSize,
        bool disableRLE,
        bool disableCsvHandling
    ) {
        auto result = wisent::compressor::compress(
            inputData,
            inputSize,
            csvPrefixData,
            csvPrefixSize,
            disableRLE,
            disableCsvHandling
        );
        if (result.success()) {
            return (void*)result.value.value_or(nullptr);
        }
        printf("Error loading Wisent: %s\n", result.error.value().c_str());
        return nullptr; 
    }
}

