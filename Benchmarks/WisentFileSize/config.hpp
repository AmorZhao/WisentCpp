#pragma once
#include <string>
#include <vector>

const int IterationTimes = 5; 

const bool DisableRLE = false;
const bool DisableCSV = false;
const bool ForceReload = true; 

const std::string DatasetPath = "/root/Documents/WisentCpp/Data/tpch/data/";  
const std::string DatasetName = "tpch_metadata.json"; 

const std::string SharedMemoryName = "benchmark_sharedMemorySegment";
const std::string CsvPath = DatasetPath; 

const std::vector<std::string> CsvSubDirs = {
    "data_0.005G",
    "data_0.01G",
    "data_0.05G",
    "data_0.1G",
    "data_0.2G",
    "data_0.5G"
};
