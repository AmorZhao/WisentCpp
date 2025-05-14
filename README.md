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
│
├── Src/
│   ├── BossHelpers/
│   ├── CompressionHelpers/
│   ├── Helpers/ 
│   │   ├── ISharedMemory
│   │   ├── SharedMemorySegment
│   │   └── CsvLoading
│   │
│   ├── BsonSerializer/
│   │
│   ├── WisentSerializer/
│   │   ├── WisentHelpers
│   │   └── WisentSerializer
│   │
│   ├── WisentCompressor/
│   │   ├── CompressionPipeline
│   │   └── WisentCompressor
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



