#include "utilities.hpp"
#include "config.hpp"
#include "ITTNotify.hpp"
#include "../../Src/Helpers/ISharedMemorySegment.hpp"
#include <benchmark/benchmark.h>
#include <unordered_map>
#include <string>
#include <iostream>

static void BM_Load_Bson(benchmark::State& state)
{
    const int index = state.range(0); 
    std::string path = benchmark::utilities::GetCsvPath(CsvSubDirs[index]);

    for (auto _ : state) 
    {
        SharedMemorySegments::sharedMemorySegmentsList.clear();

        vtune_start_task("BsonSerialize");
        benchmark::utilities::BsonSerialize(path);
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
                  << " -> Bson size: " << length << " bytes" << std::endl;
    }
}

static void BM_Load_Json(benchmark::State& state)
{
    const int index = state.range(0); 
    std::string path = benchmark::utilities::GetCsvPath(CsvSubDirs[index]);

    for (auto _ : state) 
    {
        SharedMemorySegments::sharedMemorySegmentsList.clear();

        vtune_start_task("JsonSerialize");
        benchmark::utilities::JsonSerialize(path);
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
                  << " -> Json size: " << length << " bytes" << std::endl;
    }
}


static void BM_Load_JsonToWisent(benchmark::State& state)
{
    const int index = state.range(0); 
    std::string path = benchmark::utilities::GetCsvPath(CsvSubDirs[index]);

    for (auto _ : state) 
    {
        SharedMemorySegments::sharedMemorySegmentsList.clear();

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

int main(int argc, char** argv) 
{
    benchmark::Initialize(&argc, argv);

    for (int i = 0; i < CsvSubDirs.size(); ++i) 
    {
        benchmark::RegisterBenchmark(
            ("BM_Load_Bson/" + CsvSubDirs[i]).c_str(),
            &BM_Load_Bson
        )->Arg(i)->Iterations(IterationTimes);

        benchmark::RegisterBenchmark(
            ("BM_Load_Json/" + CsvSubDirs[i]).c_str(),
            &BM_Load_Json
        )->Arg(i)->Iterations(IterationTimes);

        benchmark::RegisterBenchmark(
            ("BM_Load_JsonToWisent/" + CsvSubDirs[i]).c_str(),
            &BM_Load_JsonToWisent
        )->Arg(i)->Iterations(IterationTimes);
    }

    benchmark::RunSpecifiedBenchmarks();
    return 0;
}
