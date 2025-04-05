#ifndef WISENTSERIALIZER_H
#define WISENTSERIALIZER_H

#include "../Helpers/ISharedMemorySegment.hpp" 

char* wisentLoad(
    ISharedMemorySegments *sharedMemorySegments,
    char const* path, 
    char const* sharedMemoryName, 
    char const* csvPrefix, 
    bool disableRLE, 
    bool disableCsvHandling,
    bool enableDeltaEncoding, 
    bool enableHuffmanEncoding
);

void wisentUnload(
    ISharedMemorySegments *sharedMemorySegments,
    char const* sharedMemoryName
);

void wisentFree(
    ISharedMemorySegments *sharedMemorySegments,
    char const* sharedMemoryName
);

#endif /* WISENTSERIALIZER_H */
