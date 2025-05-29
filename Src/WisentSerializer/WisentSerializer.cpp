#include "WisentSerializer.hpp"
#include <cstdint>
#include <string>
#include <cassert>
#include <vector>
#include "JsonToWisent.hpp"

Result<WisentRootExpression*> wisent::serializer::load(
    std::string const &filepath,
    std::string const &sharedMemoryName,
    std::string const &csvPrefix, 
    bool disableRLE,
    bool disableCsvHandling, 
    bool forceReload
) {
    Result<WisentRootExpression*> result; 

    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(sharedMemoryName);
    // if (!forceReload && sharedMemory->exists() && !sharedMemory->isLoaded()) 
    // {
    //     sharedMemory->load();
    // }
    if (sharedMemory->isLoaded()) 
    {
        // if (!forceReload) 
        // {
        //     return reinterpret_cast<WisentRootExpression *>(
        //         sharedMemory->getBaseAddress()
        //     );
        // }
        free(sharedMemoryName);
    }
    SharedMemorySegments::setCurrentSharedMemory(sharedMemory);

    std::ifstream ifs(filepath);
    if (!ifs.good()) 
    {
        std::string errorMessage = "failed to read: " + filepath;
        result.setError(errorMessage);
        return result;
    }

    // 1st traversal: count & calculate the total size needed
    uint64_t expressionCount = 0;
    std::vector<uint64_t> argumentCountPerLayer;
    argumentCountPerLayer.reserve(16);
    auto _ = json::parse(
        ifs, 
        [                   // lambda captures
            &csvPrefix, 
            &disableCsvHandling, 
            &expressionCount,
            &argumentCountPerLayer, 
            layerIndex = uint64_t{0},
            wasKeyValue = std::vector<bool>(16)
        ](                  // lambda params
            int depth, 
            json::parse_event_t event, 
            json &parsed
        ) mutable {
            if (wasKeyValue.size() <= depth) 
            {
                wasKeyValue.resize(wasKeyValue.size() * 2, false);
            }
            if (argumentCountPerLayer.size() <= layerIndex) 
            {
                argumentCountPerLayer.resize(layerIndex + 1, 0);
            }
            if (event == json::parse_event_t::key) 
            {
                argumentCountPerLayer[layerIndex]++;
                expressionCount++;
                wasKeyValue[depth] = true;
                layerIndex++;
                return true;
            }
            if (event == json::parse_event_t::object_start ||
                event == json::parse_event_t::array_start) 
            {
                argumentCountPerLayer[layerIndex]++;
                expressionCount++;
                layerIndex++;
                return true;
            }
            if (event == json::parse_event_t::object_end ||
                event == json::parse_event_t::array_end) 
            {
                layerIndex--;
                if (wasKeyValue[depth]) {
                    wasKeyValue[depth] = false;
                    layerIndex--;
                }
                return true;
            }
            if (event == json::parse_event_t::value) 
            {
                argumentCountPerLayer[layerIndex]++;
                if (!disableCsvHandling && parsed.is_string()) 
                {
                    auto filename = parsed.get<std::string>();
                    auto extPos = filename.find_last_of(".");
                    if (extPos != std::string::npos &&
                        filename.substr(extPos) == ".csv") 
                    {
                        std::cout << "Handling csv file: " << filename << std::endl;
                        auto doc = openCsvFile(csvPrefix + filename);
                        auto rows = doc.GetRowCount();
                        auto cols = doc.GetColumnCount();
                        static const size_t numTableLayers = 2; // Column/Data
                        if (argumentCountPerLayer.size() <= layerIndex + numTableLayers) 
                        {
                            argumentCountPerLayer.resize(layerIndex + numTableLayers + 1, 0);
                        }
                        expressionCount++; // Table expression
                        argumentCountPerLayer[layerIndex + 1] += cols; // Column expressions
                        expressionCount += cols;
                        argumentCountPerLayer[layerIndex + 2] += cols * rows; // Column data
                    }
                }
                if (wasKeyValue[depth]) 
                {
                    wasKeyValue[depth] = false;
                    layerIndex--;
                }
                return true;
            }
            return true;   // never reached
        }
    );

    std::unordered_map<std::string, CompressionPipeline*> dummyMap; 

    // initialise Wisent expression tree
    JsonToWisent jsonToWisent(
        expressionCount,
        std::move(argumentCountPerLayer),
        sharedMemory,
        csvPrefix,
        disableRLE,
        disableCsvHandling
    );

    // 2nd traversal: parse and populate 
    ifs.seekg(0);
    json::sax_parse(ifs, &jsonToWisent);
    ifs.close();

    std::cout << "loaded: " << filepath << std::endl;
    result.setValue(jsonToWisent.getRoot());
    return result; 
}

void wisent::serializer::unload (std::string const &sharedMemoryName)
{
    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(sharedMemoryName);
    if (!sharedMemory->isLoaded()) 
    {
        std::cerr << "Error: Shared memory segment is not loaded." << std::endl;
        return;
    }
    sharedMemory->unload();
    std::cout << "Shared memory segment unloaded successfully." << std::endl;
}

void wisent::serializer::free(std::string const &sharedMemoryName)
{
    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(sharedMemoryName);
    sharedMemory->erase();
    SharedMemorySegments::getSharedMemorySegments().erase(sharedMemoryName);
    std::cout << "Shared memory segment erased from list." << std::endl;
}
