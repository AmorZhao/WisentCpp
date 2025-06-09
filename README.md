# Wisent++: A C++ Library for Composability-Enabled Data File Formats 

## Project structure 

```
WisentCpp/
│
├── Data/ 
│
├── Documentation/
│   ├── Reports/
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
│   │   ├── CsvLoading
│   │   ├── BossHelpers/
│   │   │   ├── BossExpression.hpp                  # defines BOSS expressions
│   │   │   └── BossEngine                          # constructs or evaluates BOSS Expressions
│   │   │
│   │   ├── WisentHelpers/
│   │   │   ├── WisentHelpers.hpp                   # for Wisent & PortableBoss
│   │   │   ├── JsonToWisent.hpp                    # for Wisent serializer & compressor
│   │   │   └── BossToPortableBoss.hpp              # for BOSS serializer & compressor
│   │   │
│   │   └── CompressionHelpers/
│   │       ├── Algorithms                          # engine for all algorithms
│   │       └── ... (other compression algorithm implementations)
│   │
│   ├── BsonSerializer/
│   │
│   ├── WisentSerializer/
│   │   ├── WisentSerializer
│   │   └── BossSerializer
│   │
│   ├── WisentCompressor/
│   │   ├── CompressionPipeline.hpp                 # builder for compression algorithms
│   │   ├── WisentCompressor
│   │   └── BossCompressor
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



