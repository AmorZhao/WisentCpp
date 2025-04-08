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
│   │
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
│   ├── WisentParser/
│   │
│   ├── WisentCompressionHelpers/
│   │
│   └── WisentServer
│
├── Tests/
│   ├── Benchmark/
│   └── UnitTests/
│
├── client-app/ 
│
└── Misc/
```

## Requirements

```bash
GCC >= 5.0
Clang >= 3.4
cmake >= 3.10
```

QQ - [this stack overflow post](https://stackoverflow.com/questions/30714175/clang-3-4-c14-support)


## Run locally 

### Build project

```
./test_build.sh
./build/WisentServer
```

QQ - data preparation

<br>
<br>
<br>

---

This codebase was initialised using the [TheLartians/ModernCppStarter](https://github.com/TheLartians/ModernCppStarter) template. 

