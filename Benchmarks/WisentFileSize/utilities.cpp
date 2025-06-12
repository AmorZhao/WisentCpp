#include "utilities.hpp"
#include "../../Src/BsonSerializer/BsonSerializer.hpp"
#include "../../Src/WisentSerializer/WisentSerializer.hpp"
#include "config.hpp"
#include <benchmark/benchmark.h>

void benchmark::utilities::BsonSerialize(
    std::string csvPath
) {
    auto result = bson::serializer::loadAsBson(
        DatasetPath + DatasetName, 
        SharedMemoryName,
        csvPath,
        DisableCSV,
        ForceReload
    );
}

void benchmark::utilities::JsonSerialize(
    std::string csvPath
) {
    auto result = bson::serializer::loadAsJson(
        DatasetPath + DatasetName, 
        SharedMemoryName,
        csvPath,
        DisableCSV,
        ForceReload
    );
}

void benchmark::utilities::WisentSerialize(
    std::string csvPath
) {
    Result<WisentRootExpression*> result =  wisent::serializer::load(
        DatasetPath + DatasetName, 
        SharedMemoryName,
        csvPath,
        DisableRLE,
        DisableCSV,
        ForceReload
    ); 
}
