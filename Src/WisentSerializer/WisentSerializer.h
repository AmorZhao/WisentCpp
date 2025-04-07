#ifndef WISENTSERIALIZER_H
#define WISENTSERIALIZER_H

#include "../Helpers/ISharedMemorySegment.hpp" 

char* wisentLoad(
    char const* path, 
    char const* sharedMemoryName, 
    char const* csvPrefix, 
    bool disableRLE, 
    bool disableCsvHandling,
    bool enableDeltaEncoding, 
    bool enableHuffmanEncoding
);

void wisentUnload(
    char const* sharedMemoryName
);

void wisentFree(
    char const* sharedMemoryName
);

#endif /* WISENTSERIALIZER_H */
