#pragma once
#include <string>
#include <unordered_map>
#include <vector>

const int IterationTimes = 5; 

const bool DisableRLE = false;
const bool DisableCSV = false;
const bool ForceReload = true; 

const std::string DatasetPath = "/root/Documents/WisentCpp/Data/tpch/data/";  
const std::string DatasetName = "tpch_metadata.json"; 

const std::string CsvPath = DatasetPath; 
const std::vector<std::string> CsvSubDirs = {
    "data_0.005G",
    "data_0.01G", 
    "data_0.05G",
    "data_0.1G",
    "data_0.2G",
    "data_0.5G"
};

const std::vector<std::string> zstdCompressionSteps = {
    "LZ77", 
    "FSE", 
    "HUFFMAN"
};

const std::unordered_map<std::string, std::vector<std::string>> CompressionSpecifier
{
    // nation
    {"N_NATIONKEY", zstdCompressionSteps},
    {"N_NAME", zstdCompressionSteps},
    {"N_REGIONKEY", zstdCompressionSteps},
    {"N_COMMENT", zstdCompressionSteps},

    // region
    {"R_REGIONKEY", zstdCompressionSteps},
    {"R_NAME", zstdCompressionSteps},
    {"R_COMMENT", zstdCompressionSteps},

    // part
    {"P_PARTKEY", zstdCompressionSteps},
    {"P_NAME", zstdCompressionSteps},
    {"P_MFGR", zstdCompressionSteps},
    {"P_BRAND", zstdCompressionSteps},
    {"P_TYPE", zstdCompressionSteps},
    {"P_SIZE", zstdCompressionSteps},
    {"P_CONTAINER", zstdCompressionSteps},
    {"P_RETAILPRICE", zstdCompressionSteps},
    {"P_COMMENT", zstdCompressionSteps},

    // supplier
    {"S_SUPPKEY", zstdCompressionSteps},
    {"S_NAME", zstdCompressionSteps},
    {"S_ADDRESS", zstdCompressionSteps},
    {"S_NATIONKEY", zstdCompressionSteps},
    {"S_PHONE", zstdCompressionSteps},
    {"S_ACCTBAL", zstdCompressionSteps},
    {"S_COMMENT", zstdCompressionSteps},

    // partsupp
    {"PS_PARTKEY", zstdCompressionSteps},
    {"PS_SUPPKEY", zstdCompressionSteps},
    {"PS_AVAILQTY", zstdCompressionSteps},
    {"PS_SUPPLYCOST", zstdCompressionSteps},
    {"PS_COMMENT", zstdCompressionSteps},

    // customer
    {"C_CUSTKEY", zstdCompressionSteps},
    {"C_NAME", zstdCompressionSteps},
    {"C_ADDRESS", zstdCompressionSteps},
    {"C_NATIONKEY", zstdCompressionSteps},
    {"C_PHONE", zstdCompressionSteps},
    {"C_ACCTBAL", zstdCompressionSteps},
    {"C_MKTSEGMENT", zstdCompressionSteps},
    {"C_COMMENT", zstdCompressionSteps},

    // orders
    {"O_ORDERKEY", zstdCompressionSteps},
    {"O_CUSTKEY", zstdCompressionSteps},
    {"O_ORDERSTATUS", zstdCompressionSteps},
    {"O_TOTALPRICE", zstdCompressionSteps},
    {"O_ORDERDATE", zstdCompressionSteps},
    {"O_ORDERPRIORITY", zstdCompressionSteps},
    {"O_CLERK", zstdCompressionSteps},
    {"O_SHIPPRIORITY", zstdCompressionSteps},
    {"O_COMMENT", zstdCompressionSteps},

    // lineitem
    {"L_ORDERKEY", zstdCompressionSteps},
    {"L_PARTKEY", zstdCompressionSteps},
    {"L_SUPPKEY", zstdCompressionSteps},
    {"L_LINENUMBER", zstdCompressionSteps},
    {"L_QUANTITY", zstdCompressionSteps},
    {"L_EXTENDEDPRICE", zstdCompressionSteps},
    {"L_DISCOUNT", zstdCompressionSteps},
    {"L_TAX", zstdCompressionSteps},
    {"L_RETURNFLAG", zstdCompressionSteps},
    {"L_LINESTATUS", zstdCompressionSteps},
    {"L_SHIPDATE", zstdCompressionSteps},
    {"L_COMMITDATE", zstdCompressionSteps},
    {"L_RECEIPTDATE", zstdCompressionSteps},
    {"L_SHIPINSTRUCT", zstdCompressionSteps},
    {"L_SHIPMODE", zstdCompressionSteps},
    {"L_COMMENT", zstdCompressionSteps}
};


// const std::unordered_map<std::string, std::vector<std::string>> CompressionSpecifier
// {
//     // nation
//     {"N_NATIONKEY", {"FSE"}},
//     {"N_NAME", {"FSE"}},
//     {"N_REGIONKEY", {"FSE"}},
//     {"N_COMMENT", {"FSE"}},

//     // region
//     {"R_REGIONKEY", {"DELTA"}},
//     {"R_NAME", {"RLE"}},
//     {"R_COMMENT", {"LZ77"}},

//     // part
//     {"P_PARTKEY", {"RLE"}},
//     {"P_NAME", {"LZ77"}},
//     {"P_MFGR", {"RLE"}},
//     {"P_BRAND", {"RLE"}},
//     {"P_TYPE", {"RLE"}},
//     {"P_SIZE", {"DELTA"}},
//     {"P_CONTAINER", {"RLE"}},
//     {"P_RETAILPRICE", {"DELTA"}},
//     {"P_COMMENT", {"LZ77"}},

//     // supplier
//     {"S_SUPPKEY", {"DELTA"}},
//     {"S_NAME", {"LZ77"}},
//     {"S_ADDRESS", {"LZ77"}},
//     {"S_NATIONKEY", {"DELTA"}},
//     {"S_PHONE", {"RLE"}},
//     {"S_ACCTBAL", {"DELTA"}},
//     {"S_COMMENT", {"LZ77"}},

//     // partsupp
//     {"PS_PARTKEY", {"DELTA"}},
//     {"PS_SUPPKEY", {"DELTA"}},
//     {"PS_AVAILQTY", {"DELTA"}},
//     {"PS_SUPPLYCOST", {"DELTA"}},
//     {"PS_COMMENT", {"LZ77"}},

//     // customer
//     {"C_CUSTKEY", {"DELTA"}},
//     {"C_NAME", {"LZ77"}},
//     {"C_ADDRESS", {"LZ77"}},
//     {"C_NATIONKEY", {"DELTA"}},
//     {"C_PHONE", {"RLE"}},
//     {"C_ACCTBAL", {"DELTA"}},
//     {"C_MKTSEGMENT", {"RLE"}},
//     {"C_COMMENT", {"LZ77"}},

//     // orders
//     {"O_ORDERKEY", {"DELTA"}},
//     {"O_CUSTKEY", {"DELTA"}},
//     {"O_ORDERSTATUS", {"RLE"}},
//     {"O_TOTALPRICE", {"DELTA"}},
//     {"O_ORDERDATE", {"DELTA"}},
//     {"O_ORDERPRIORITY", {"RLE"}},
//     {"O_CLERK", {"RLE"}},
//     {"O_SHIPPRIORITY", {"DELTA"}},
//     {"O_COMMENT", {"LZ77"}},

//     // lineitem
//     {"L_ORDERKEY", {"DELTA"}},
//     {"L_PARTKEY", {"DELTA"}},
//     {"L_SUPPKEY", {"DELTA"}},
//     {"L_LINENUMBER", {"DELTA"}},
//     {"L_QUANTITY", {"DELTA"}},
//     {"L_EXTENDEDPRICE", {"DELTA"}},
//     {"L_DISCOUNT", {"DELTA"}},
//     {"L_TAX", {"DELTA"}},
//     {"L_RETURNFLAG", {"RLE"}},
//     {"L_LINESTATUS", {"RLE"}},
//     {"L_SHIPDATE", {"DELTA"}},
//     {"L_COMMITDATE", {"DELTA"}},
//     {"L_RECEIPTDATE", {"DELTA"}},
//     {"L_SHIPINSTRUCT", {"RLE"}},
//     {"L_SHIPMODE", {"RLE"}},
//     {"L_COMMENT", {"LZ77"}}
// };

// const std::string DatasetPath = "/root/Documents/WisentCpp/Data/owid-deaths/";  
// const std::string DatasetName = "datapackage.json"; 

// const std::unordered_map<std::string, std::vector<std::string>> CompressionSpecifier
// {
//     {"Year", {"DELTA"}}
// }; 

const std::string SharedMemoryName = "benchmark_sharedMemorySegment";

