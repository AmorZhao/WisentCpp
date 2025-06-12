#include "utilities.hpp"
#include "config.hpp"
#include "ITTNotify.hpp"
#include "../../Src/Helpers/ISharedMemorySegment.hpp"
#include <benchmark/benchmark.h>
#include <unordered_map>
#include <string>

#include "../../Include/json.h"

// void parseCompressionPipeline(
//     std::unordered_map<std::string, CompressionPipeline> &result
// ) {
//     using nlohmann::json; 

//     const std::string body = R"({
//         "GR_temperature": [
//             "LZ77",
//             "Huffman", 
//             "FSE"
//         ],
//         "BG_temperature": [
//             "LZ77",
//             "Huffman", 
//             "FSE"
//         ]
//     })";

//     json pipelineSpecification; 
//     try {
//         pipelineSpecification = json::parse(body);
//     } 
//     catch (const std::exception &e) {
//         std::string errorMessage = "Error parsing request body: " + std::string(e.what());
//         // result.setError(errorMessage);
//     }

//     std::unordered_map<std::string, CompressionPipeline> CompressionPipelineMap;
//     for (const auto& [columnName, steps] : pipelineSpecification.items()) 
//     {
//         CompressionPipeline::Builder builder;
//         for (const std::string& step : steps) 
//         {
//             builder.addStep(step);
//         }
//         CompressionPipelineMap[columnName] = builder.build();
//     }
//     // result.setValue(CompressionPipelineMap);
// }

static void BM_Load_JsonToWisent(benchmark::State& state)
{
    const int index = state.range(0); 
    std::string path = benchmark::utilities::GetCsvPath(CsvSubDirs[index]);

    SharedMemorySegments::sharedMemorySegmentsList.clear();

    for (auto _ : state) 
    {
        vtune_start_task("WisentSerialize");
        benchmark::utilities::WisentSerialize(path);
        vtune_end_task();
    }

    auto loaded = SharedMemorySegments::createOrGetMemorySegment(SharedMemoryName);
    if (!loaded->exists()) 
    {
        std::cerr << "Error: Shared memory segment does not exist." << std::endl;
        return;
    }
    if (loaded->isLoaded()) 
    {
        int length = loaded->getSize();
        std::cout << "CSV size: " << CsvSubDirs[index]
                  << " -> Wisent expression tree size: " << length << " bytes" << std::endl;
    }
}

static void BM_LoadAndCompress_JsonToWisent(benchmark::State& state) 
{
    SharedMemorySegments::sharedMemorySegmentsList.clear();

    const int index = state.range(0); 
    std::string path = benchmark::utilities::GetCsvPath(CsvSubDirs[index]);

    for (auto _ : state) 
    {
        // std::unordered_map<std::string, CompressionPipeline> compressionPipelineMap; 
        // parseCompressionPipeline(compressionPipelineMap);

        vtune_start_task("WisentCompressWithPipeline");
        benchmark::utilities::WisentCompressWithPipeline(path); 
        vtune_end_task();
    }

    auto loaded = SharedMemorySegments::createOrGetMemorySegment(SharedMemoryName);
    if (!loaded->exists()) 
    {
        std::cerr << "Error: Shared memory segment does not exist." << std::endl;
        return;
    }
    if (loaded->isLoaded()) 
    {
        int length = loaded->getSize();
        std::cout << "CSV size: " << CsvSubDirs[index]
                  << " -> Compressed Wisent expression tree size: " << length << " bytes" << std::endl;
    }
}

int main(int argc, char** argv) 
{
    benchmark::Initialize(&argc, argv);

    for (int i = 0; i < CsvSubDirs.size(); ++i) 
    {
        // benchmark::RegisterBenchmark(
        //     ("BM_Load_JsonToWisent/" + CsvSubDirs[i]).c_str(),
        //     &BM_Load_JsonToWisent
        // )->Arg(i)->Iterations(1);

        benchmark::RegisterBenchmark(
            ("BM_LoadAndCompress_JsonToWisent/" + CsvSubDirs[i]).c_str(),
            &BM_LoadAndCompress_JsonToWisent
        )->Arg(i)->Iterations(1);
    }

    benchmark::RunSpecifiedBenchmarks();
    return 0;
}
