#include "BsonSerializer.hpp"
#include "../Helpers/CsvLoading.hpp"
#include "../Helpers/ISharedMemorySegment.hpp"
#include <cassert>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <vector>
#include "../../Include/json.h"

using json = nlohmann::json;

template <class T> struct SharedMemoryAllocator 
{
    typedef T value_type;

    SharedMemoryAllocator() 
        : pointer(nullptr), size(0) 
    {}

    template <class U>
    SharedMemoryAllocator(const SharedMemoryAllocator<U> &other)
        : pointer(other.pointer)
        , size(other.size)
    {}

    template <class U>
    SharedMemoryAllocator(SharedMemoryAllocator<U> &&other)
        : pointer(std::move(other.pointer))
        , size(std::move(other.size))
    {}

    template <typename U> bool operator==(SharedMemoryAllocator<U> const &other)
    {
        return true;
    }

    template <typename U> bool operator!=(SharedMemoryAllocator<U> const &other)
    {
        return false;
    }


    T *allocate(std::size_t n)
    {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) 
        {
            throw std::bad_array_new_length();
        }

        size_t newSize = n * sizeof(T);
        if (pointer != nullptr) 
        {
            if (newSize < size) 
            {
                return static_cast<T *>(pointer);
            }
            pointer = SharedMemorySegments::sharedMemoryRealloc(pointer, newSize);
        }
        else 
        {
            pointer = SharedMemorySegments::sharedMemoryMalloc(newSize);
        }

        if (pointer == nullptr) 
        {
            throw std::bad_alloc();
        }
        size = newSize;
        return static_cast<T *>(pointer);
    }

    void deallocate(T * /*p*/, std::size_t /*n*/) noexcept
    {
        // this data persists with the shared memory
    }

private:
    void *pointer;
    size_t size;
};

json load(
    std::string const &path, 
    std::string const &csvPrefix,
    bool disableCsvHandling)
{
    std::ifstream ifs(path);
    if (!ifs.good()) 
    {
        throw std::runtime_error("failed to read: " + path);
    }

    json j = json::parse(
        ifs, 
        [&csvPrefix, &disableCsvHandling](
            int depth, 
            json::parse_event_t event,
            json &parsed) 
        {
            if (event == json::parse_event_t::value) 
            {
                if (!disableCsvHandling && parsed.is_string()) 
                {
                    std::string filename = parsed.get<std::string>();
                    size_t extPos = filename.find_last_of(".");

                    if (extPos != std::string::npos 
                        && filename.substr(extPos) == ".csv") 
                    {
                        rapidcsv::Document doc = openCsvFile(csvPrefix + filename);
                        json columns(json::value_t::object);

                        for (std::string const &columnName : doc.GetColumnNames()) 
                        {
                            json column = loadCsvDataToJson<int64_t>(doc, columnName);
                            if (column.is_null()) 
                            {
                                column = loadCsvDataToJson<double_t>(doc, columnName);
                                if (column.is_null()) 
                                {
                                    column = loadCsvDataToJson<std::string>(doc, columnName);
                                }
                            }
                            columns[columnName] = std::move(column);
                        }
                        parsed = {{"Table", std::move(columns)}};
                    }
                }
            }
            return true;
        }
    );
    ifs.close();
    return j;
}

void *bson::serializer::loadAsBson(
    std::string const &path,
    std::string const &sharedMemoryName,
    std::string const &csvPrefix,
    bool disableCsvHandling, 
    bool forceReload)
{
    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(sharedMemoryName);

    if (!forceReload && sharedMemory->exists() && !sharedMemory->isLoaded()) 
    {
        sharedMemory->load();
    }
    if (sharedMemory->isLoaded()) 
    {
        if (!forceReload) 
        {
            return sharedMemory->getBaseAddress();
        }
        free(sharedMemoryName);
        sharedMemory = SharedMemorySegments::createOrGetMemorySegment(sharedMemoryName);
    }
    SharedMemorySegments::setCurrentSharedMemory(sharedMemory);

    json j = load(
        path, 
        csvPrefix, 
        disableCsvHandling
    );
    std::vector<std::uint8_t> v = json::to_bson(j);

    void *base = SharedMemorySegments::sharedMemoryMalloc(v.size() + 1);
    memcpy(base, v.data(), v.size());
    reinterpret_cast<char *>(base)[v.size()] = '\0';
    return base;

    // static std::vector<std::uint8_t, SharedMemoryAllocator<std::uint8_t>> sharedV(
    //     v.begin(), 
    //     v.end()
    // );
    // return sharedV.data();
}

void *bson::serializer::loadAsJson(
    std::string const &path,
    std::string const &sharedMemoryName,
    std::string const &csvPrefix,
    bool disableCsvHandling, 
    bool forceReload)
{
    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(sharedMemoryName);

    if (!forceReload && sharedMemory->exists() && !sharedMemory->isLoaded()) 
    {
        sharedMemory->load();
    }
    if (sharedMemory->isLoaded()) 
    {
        if (!forceReload) 
        {
            return sharedMemory->getBaseAddress();
        }
        free(sharedMemoryName);
        sharedMemory = SharedMemorySegments::createOrGetMemorySegment(sharedMemoryName);
    }
    SharedMemorySegments::setCurrentSharedMemory(sharedMemory);

    json j = load(
        path, 
        csvPrefix, 
        disableCsvHandling
    );
    std::ostringstream ostream;
    ostream << j;

    std::string str = ostream.str();
    void *base = SharedMemorySegments::sharedMemoryMalloc(str.size() + 1);
    memcpy(base, str.data(), str.size());
    reinterpret_cast<char *>(base)[str.size()] = '\0';
    return base;

    // using SharedString = std::basic_string<char, std::char_traits<char>, SharedMemoryAllocator<char>>;
    // SharedString *str = new SharedString(ostream.str().begin(), ostream.str().end());
    // return const_cast<void *>(static_cast<const void *>(str->data())); 
}

void bson::serializer::unload(std::string const &sharedMemoryName)
{
    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(sharedMemoryName);
    assert(sharedMemory->isLoaded());
    sharedMemory->unload();
}

void bson::serializer::free(std::string const &sharedMemoryName)
{
    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(sharedMemoryName);
    sharedMemory->erase();
    SharedMemorySegments::getSharedMemorySegments().erase(sharedMemoryName);
}
