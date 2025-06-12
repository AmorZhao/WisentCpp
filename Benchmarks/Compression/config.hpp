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
    // "data_0.05G",
    // "data_0.1G",
    // "data_0.2G",
    // "data_0.5G"
};

const std::unordered_map<std::string, std::vector<std::string>> CompressionSpecifier
{
    // nation
    {"N_NATIONKEY", {"DELTA"}},
    {"N_NAME", {"RLE"}},
    {"N_REGIONKEY", {"DELTA"}},
    {"N_COMMENT", {"LZ77"}},

    // region
    {"R_REGIONKEY", {"DELTA"}},
    {"R_NAME", {"RLE"}},
    {"R_COMMENT", {"LZ77"}},

    // part
    {"P_PARTKEY", {"RLE"}},
    {"P_NAME", {"LZ77"}},
    {"P_MFGR", {"RLE"}},
    {"P_BRAND", {"RLE"}},
    {"P_TYPE", {"RLE"}},
    {"P_SIZE", {"DELTA"}},
    {"P_CONTAINER", {"RLE"}},
    {"P_RETAILPRICE", {"DELTA"}},
    {"P_COMMENT", {"LZ77"}},

    // supplier
    {"S_SUPPKEY", {"DELTA"}},
    {"S_NAME", {"LZ77"}},
    {"S_ADDRESS", {"LZ77"}},
    {"S_NATIONKEY", {"DELTA"}},
    {"S_PHONE", {"RLE"}},
    {"S_ACCTBAL", {"DELTA"}},
    {"S_COMMENT", {"LZ77"}},

    // partsupp
    {"PS_PARTKEY", {"DELTA"}},
    {"PS_SUPPKEY", {"DELTA"}},
    {"PS_AVAILQTY", {"DELTA"}},
    {"PS_SUPPLYCOST", {"DELTA"}},
    {"PS_COMMENT", {"LZ77"}},

    // customer
    {"C_CUSTKEY", {"DELTA"}},
    {"C_NAME", {"LZ77"}},
    {"C_ADDRESS", {"LZ77"}},
    {"C_NATIONKEY", {"DELTA"}},
    {"C_PHONE", {"RLE"}},
    {"C_ACCTBAL", {"DELTA"}},
    {"C_MKTSEGMENT", {"RLE"}},
    {"C_COMMENT", {"LZ77"}},

    // orders
    {"O_ORDERKEY", {"DELTA"}},
    {"O_CUSTKEY", {"DELTA"}},
    {"O_ORDERSTATUS", {"RLE"}},
    {"O_TOTALPRICE", {"DELTA"}},
    {"O_ORDERDATE", {"DELTA"}},
    {"O_ORDERPRIORITY", {"RLE"}},
    {"O_CLERK", {"RLE"}},
    {"O_SHIPPRIORITY", {"DELTA"}},
    {"O_COMMENT", {"LZ77"}},

    // lineitem
    {"L_ORDERKEY", {"DELTA"}},
    {"L_PARTKEY", {"DELTA"}},
    {"L_SUPPKEY", {"DELTA"}},
    {"L_LINENUMBER", {"DELTA"}},
    {"L_QUANTITY", {"DELTA"}},
    {"L_EXTENDEDPRICE", {"DELTA"}},
    {"L_DISCOUNT", {"DELTA"}},
    {"L_TAX", {"DELTA"}},
    {"L_RETURNFLAG", {"RLE"}},
    {"L_LINESTATUS", {"RLE"}},
    {"L_SHIPDATE", {"DELTA"}},
    {"L_COMMITDATE", {"DELTA"}},
    {"L_RECEIPTDATE", {"DELTA"}},
    {"L_SHIPINSTRUCT", {"RLE"}},
    {"L_SHIPMODE", {"RLE"}},
    {"L_COMMENT", {"LZ77"}}
};

// const std::string DatasetPath = "/root/Documents/WisentCpp/Data/owid-deaths/";  
// const std::string DatasetName = "datapackage.json"; 

// const std::unordered_map<std::string, std::vector<std::string>> CompressionSpecifier
// {
//     {"Year", {"DELTA"}}
// }; 

const std::string SharedMemoryName = "benchmark_sharedMemorySegment";

