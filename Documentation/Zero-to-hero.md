# WisentCpp docs

This is my casual note of the WisentCpp repo. 

## Requirements

To run this project: 

- cmake 3.16 or above
- C++ compiler that supports C++17 or above
    - Clang 9.0 or above
    - GCC 9.3 or above

If running benchmark: 

- Google benchmark
- TPC-H benchmark

## Run Wisent server locally

In repository root folder, run: 

```
./build_server.sh
```

Or if already built server: 

```
./build/WisentServer
```

This will build and start the Wisent server at http://localhost:8000/. 

The server provides GET or POST handlers to serialise, compress or load your leafy data. Check the [parseRequestParams](https://github.com/AmorZhao/WisentCpp/blob/06c9c7647b6e1a8bcb880aeb8f42a39ebd1b3a62/Src/ServerHelpers.cpp#L9C5-L9C25) function for params information. 

## Generate benchmarking data

[Data/tpch/scripts/](https://github.com/AmorZhao/WisentCpp/tree/master/Data/tpch/scripts) includes several scripts that generates testing data from a TPC-H benchmark database generator. 

1. Update the `DBGEN_PATH` in the script, this should be were the dbgen tool lives
2. `generate_plain_data.sh` generates data with a single approximate size, while `generate_plain_data_scaled.sh` generates a list of datasets with approximate size. 
3. The script converts all `.tbl` type data into csv format, (check there's no irrelevant `.tbl` files in `DBGEN_PATH`)

    ```
    ./generate_plain_data.sh
    ./generate_plain_data_scaled.sh
    ```

The following creates a nested data structure which adapt the csv files as it's children: 

```
python3 generate_nested_data.py
```


## Run benchmarks

[Benchmarks/](https://github.com/AmorZhao/WisentCpp/tree/master/Benchmarks) includes several benchmarks for testing & evaluation purposes. 

1. Change the `BENCHMARK_FOLDERS` list to specify the benchmarks to run
2. Run `./benchmark.sh`
3. (The `config.hpp` file in each benchmark specifies: 
    - `CsvSubDirs`: list of datasets to load
    - `CompressionSpecifier`: compression pipeline to use)

The folders include benchmarks for: 

#### [`WisentFileSize/`](https://github.com/AmorZhao/WisentCpp/tree/master/Benchmarks/WisentFileSize)

For "the first plot". ("Look how large Wisent is compared with other solutions")

#### [`ParquetFileSize/`](https://github.com/AmorZhao/WisentCpp/tree/master/Benchmarks/ParquetFileSize)

A python3 script that loads / compresses the dataset using apache pyarrow. Also for "the first plot". 

(Run: `python3 parquet.py`)

#### [`WisentCompressor`](https://github.com/AmorZhao/WisentCpp/tree/master/Benchmarks/WisentCompressor)

Simply calls the JSON / BSON / Wisent serialisers / Wisent compressor. Uses one dataset a time. For testing / debugging functionalities. 

#### [`Compression/`](https://github.com/AmorZhao/WisentCpp/tree/master/Benchmarks/Compression)

Serialises with / without compression to Wisent format. Uses a list of datasets. For evaluation. 

#### [`WisentProcessing/`](https://github.com/AmorZhao/WisentCpp/tree/master/Benchmarks/WisentProcessing)

Loads serialised (compressed / uncompressed) Wisent data, then performs queries. For evaluation. 
