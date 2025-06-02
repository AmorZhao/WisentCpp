# Wisent++: A C++ Library for Composability-Enabled Data File Formats 

## Project structure 

```
WisentCpp/
│
├── Data/ 
│
├── Documentation/
│   ├── Report/
│   └── WisentExample/
│
├── Include/
│   ├── httplib.h
│   ├── nlohmann/json.h
│   └── rapidcsv.h
│
├── Src/
│   ├── Helpers/ 
│   │   ├── ISharedMemory
│   │   ├── SharedMemorySegment
│   │   └── CsvLoading
│   │
│   ├── BsonSerializer/
│   │
│   ├── WisentSerializer/
│   │   ├── WisentHelpers.hpp
│   │   ├── JsonToWisent.hpp
│   │   ├── WisentSerializer (serializer functions)
│   │   │   
│   │   └── BossHelpers/
│   │       ├── BossExpression.hpp
│   │       ├── BossSerializerHelpers.hpp (BOSS equivalent to WisentHelpers.hpp)
│   │       └── BossSerializedExpression.hpp (BOSS equivalent to JsonToWisent.hpp)
│   │
│   ├── WisentCompressor/
│   │   ├── CompressionHelpers/
│   │   │   ├── Algorithms (overall wrapper)
│   │   │   └── ...(other compression algorithm implementations)
│   │   │
│   │   ├── CompressionPipeline.hpp 
│   │   ├── WisentCompressor (compressor functions)
│   │   │
│   │   └── BossHelpers/
│   │       └── BossCompressedExpression.hpp (BOSS equivalent to JsonToWisent.hpp)
│   │
│   ├── WisentParser/
│   │
│   └── WisentServer
│
├── Tests/
│   ├── Benchmark/
│   └── UnitTests/
│
└── Misc/
    ├── client-app/ 
    └── Wisent-standalone/
```

## Requirements

```bash
GCC >= 5.0
Clang >= 3.4
cmake >= 3.10
```


## Run locally 

### Build project

```
./build_server.sh
./build/WisentServer
```

<br>
<br>
<br>



