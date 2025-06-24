#include "utilities.hpp"
#include "config.hpp"
#include "ITTNotify.hpp"
#include "../../Src/Helpers/ISharedMemorySegment.hpp"
#include <benchmark/benchmark.h>
#include <unordered_map>
#include <string>

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

    std::unordered_map<std::string, CompressionPipeline> compressionPipelineMap = 
        benchmark::utilities::ConstructCompressionPipelineMap();

    for (auto _ : state) 
    {
        vtune_start_task("WisentCompressWithPipeline");
        benchmark::utilities::WisentCompressWithPipeline(compressionPipelineMap, path); 
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

static void BM_Process_JsonToWisent(benchmark::State& state) 
{
    SharedMemorySegments::sharedMemorySegmentsList.clear();

    // const int index = state.range(0); 
    // std::string path = benchmark::utilities::GetCsvPath(CsvSubDirs[index]);

    // benchmark::utilities::WisentSerialize(path);
    
    auto loaded = SharedMemorySegments::createOrGetMemorySegment(SharedMemoryName);
    if (!loaded->exists()) 
    {
        std::cerr << "Error: Shared memory segment does not exist." << std::endl;
        return;
    }
    if (loaded->isLoaded()) 
    {
        int length = loaded->getSize();
        std::cout<< " -> Compressed Wisent expression tree size: " << length << " bytes" << std::endl;
    }

    for (auto _ : state) 
    {
        vtune_start_task("ParsingWisent");
        benchmark::utilities::parserWisentForString(static_cast<WisentRootExpression*>(loaded->getBaseAddress()));
        vtune_end_task();
    }
}

static void BM_Process_JsonToWisent_compressed(benchmark::State& state) 
{    
    auto loaded = SharedMemorySegments::createOrGetMemorySegment(SharedMemoryName);
    if (!loaded->exists()) 
    {
        std::cerr << "Error: Shared memory segment does not exist." << std::endl;
        return;
    }
    if (loaded->isLoaded()) 
    {
        int length = loaded->getSize();
        std::cout << " -> Compressed Wisent expression tree size: " << length << " bytes" << std::endl;
    }

    for (auto _ : state) 
    {
        vtune_start_task("ParsingCompressedWisent");
        benchmark::utilities::parserWisentForString(static_cast<WisentRootExpression*>(loaded->getBaseAddress())); 
        vtune_end_task();
    }
}

static void BM_Decompress_JsonToWisent_compressed(benchmark::State& state) 
{    
    auto loaded = SharedMemorySegments::createOrGetMemorySegment(SharedMemoryName);
    if (!loaded->exists()) 
    {
        std::cerr << "Error: Shared memory segment does not exist." << std::endl;
        return;
    }
    if (loaded->isLoaded()) 
    {
        int length = loaded->getSize();
        std::cout << " -> Compressed Wisent expression tree size: " << length << " bytes" << std::endl;
    }

    for (auto _ : state) 
    {
        vtune_start_task("ParsingCompressedWisent");
        benchmark::utilities::deserialiseWisentForString(static_cast<WisentRootExpression*>(loaded->getBaseAddress())); 
        vtune_end_task();
    }
}

int main(int argc, char** argv) 
{
    benchmark::Initialize(&argc, argv);

    for (int i = 0; i < CsvSubDirs.size(); ++i) 
    {
        benchmark::RegisterBenchmark(
            ("BM_Load_JsonToWisent/" + CsvSubDirs[i]).c_str(),
            &BM_Load_JsonToWisent
        )->Arg(i)->Iterations(1);

        benchmark::RegisterBenchmark(
            ("BM_Process_JsonToWisent/" + CsvSubDirs[i]).c_str(),
            &BM_Process_JsonToWisent
        )->Iterations(IterationTimes);

        benchmark::RegisterBenchmark(
            ("BM_LoadAndCompress_JsonToWisent/" + CsvSubDirs[i]).c_str(),
            &BM_LoadAndCompress_JsonToWisent
        )->Arg(i)->Iterations(1);

        benchmark::RegisterBenchmark(
            ("BM_Process_JsonToWisent_compressed/" + CsvSubDirs[i]).c_str(),
            &BM_Process_JsonToWisent_compressed
        )->Iterations(IterationTimes);

        benchmark::RegisterBenchmark(
            ("BM_Decompress_JsonToWisent_compressed/" + CsvSubDirs[i]).c_str(),
            &BM_Decompress_JsonToWisent_compressed
        )->Iterations(IterationTimes);
    }

    benchmark::RunSpecifiedBenchmarks();
    return 0;
}
