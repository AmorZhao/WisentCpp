#include "utilities.hpp"
#include "config.hpp"
#include "ITTNotify.hpp"
#include "../../Src/Helpers/ISharedMemorySegment.hpp"
#include <benchmark/benchmark.h>
#include <unordered_map>
#include <string>

static void BM_Load_Bson(benchmark::State& state)
{
    SharedMemorySegments::sharedMemorySegmentsList.clear();

    for (auto _ : state) 
    {    
        vtune_start_task("BsonSerialize");
        benchmark::utilities::BsonSerialize(); 

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
        std::cout << "Bson loaded successfully, size: " << length << " bytes." << std::endl;
    }
}

static void BM_Load_Json(benchmark::State& state)
{
    SharedMemorySegments::sharedMemorySegmentsList.clear();

    for (auto _ : state) 
    {    
        vtune_start_task("JsonSerialize");
        benchmark::utilities::JsonSerialize(); 
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
        std::cout << "Json loaded successfully, size: " << length << " bytes." << std::endl;
    }
}

static void BM_Load_JsonToWisent(benchmark::State& state) 
{
    SharedMemorySegments::sharedMemorySegmentsList.clear();

    for (auto _ : state) 
    {    
        vtune_start_task("WisentSerialize");
        benchmark::utilities::WisentSerialize();
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
        std::cout << "Wisent expression tree loaded successfully, size: " << length << " bytes." << std::endl;
    }
}

static void BM_LoadAndCompress_JsonToWisent(benchmark::State& state) 
{
    SharedMemorySegments::sharedMemorySegmentsList.clear();

    std::unordered_map<std::string, CompressionPipeline> compressionPipelineMap =   
        benchmark::utilities::ConstructCompressionPipelineMap();

    for (auto _ : state) 
    {
        vtune_start_task("WisentCompressWithPipeline");
        benchmark::utilities::WisentCompressWithPipeline(compressionPipelineMap); 
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
        std::cout << "Wisent expression tree compressed successfully, size: " << length << " bytes." << std::endl;
    }
}

BENCHMARK(BM_Load_Bson)->Iterations(IterationTimes);
BENCHMARK(BM_Load_Json)->Iterations(IterationTimes);
BENCHMARK(BM_Load_JsonToWisent)->Iterations(IterationTimes);
BENCHMARK(BM_LoadAndCompress_JsonToWisent)->Iterations(IterationTimes);

BENCHMARK_MAIN();