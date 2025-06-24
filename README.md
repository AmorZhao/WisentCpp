# Wisent++: A C++ Library for Composability-Enabled Data File Formats 

Wisent++ is a C++ library designed to enable efficient, composable, and compressible data file formats for both nested and flat data. It handles the leafy data structure flexibly by building upon a width-first serialisation format, while also supporting a range of lossless compression algorithms and user-defined schemes, by introducing a modular compression pipeline and a columnar metadata handling system. 

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


## Run locally 

To build Wisent server, run: 

```
./build_server
```

For more info, see [here](https://github.com/AmorZhao/WisentCpp/tree/master/Documentation/Zero-to-hero.md).

<br>
<br>
<br>



