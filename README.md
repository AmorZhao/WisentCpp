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
└── Misc/
    ├── ... (ad-hoc tests)
    └── Tests/
        ├── Benchmark/
        └── UnitTests/
```

<br>
<br>
<br>



