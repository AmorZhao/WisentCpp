#ifndef WISENT_HPP
#define WISENT_HPP

#include <cstdio>
#include <string>
#include <cstring>
#include <optional>
#include <vector>
#include <cstddef>
#include <unordered_map>
#include <queue>
#include <bitset>
#include "../../Include/json.h"
#include "../../Include/rapidcsv.h"
#include <zlib.h>

namespace wisent {

///////////////////////////////// Result handling ////////////////////////////////

template<typename T>
struct Result 
{
    std::optional<T> value;
    std::optional<std::string> error; 
    std::vector<std::string> warnings; 

    bool success() const { 
        return value.has_value() && !error.has_value(); 
    }

    bool hasWarning() const { 
        return !warnings.empty(); 
    }

    void setValue(const T& val) { 
        value = val; 
    }

    void setError(const std::string& errorMessage) { 
        error = errorMessage; 
    }

    void addWarning(const std::string& warningMessage) { 
        warnings.push_back(warningMessage); 
    }
}; 

template<typename T>
static Result<T> makeError(const std::string& errorMessage) 
{
    Result<T> result;
    result.setError(errorMessage);
    return result;
}; 

template<typename T>
static Result<T> makeResult(const T& value, Result<T>* result = nullptr) 
{
    if (result != nullptr) {
        result->setValue(value);
        return *result;
    }
    Result<T> newResult;
    newResult.setValue(value);
    return newResult;
};
/////////////////////////////////// CSV Handling //////////////////////////////////

using json = nlohmann::json;

static rapidcsv::Document openCsvFile(std::string const &filepath)
{
    try {
        printf("openCsvFile: %s\n", filepath.c_str());
        rapidcsv::Document doc(
            filepath,
            rapidcsv::LabelParams(),
            rapidcsv::SeparatorParams(),
            rapidcsv::ConverterParams(),
            rapidcsv::LineReaderParams()
        );
        return doc;
    } 
    catch (const std::exception &e) {
        std::cerr << "Error opening CSV file: " << e.what() << std::endl;
        throw;
    }
}

template <typename T>
static std::vector<std::optional<T>> loadCsvData(
    rapidcsv::Document const &doc,
    std::string const &columnName
) {
    std::vector<std::optional<T>> column;
    try {
        auto numRows = doc.GetRowCount();
        column.reserve(numRows);
        auto columnIndex = doc.GetColumnIdx(columnName);

        for (auto rowIndex = 0L; rowIndex < numRows; ++rowIndex)
        {
            column.emplace_back(doc.GetCell<std::optional<T>>(
                columnIndex,
                rowIndex,
                [](std::string const &str, std::optional<T> &val)
                {
                    if (!std::is_same<T, std::string>::value)
                    {
                        if (str.empty())
                        {
                            val = {}; 
                            return;
                        }
                    }
                    if (std::is_same<T, int64_t>::value)
                    {
                        size_t pos;
                        val = json(std::stol(str, &pos));
                        if (pos != str.length())
                        {
                            throw std::invalid_argument("failed to convert " + str + " into int");
                        }
                    }
                    else if (std::is_same<T, double_t>::value)
                    {
                        size_t pos;
                        val = json(std::stod(str, &pos));
                        if (pos != str.length())
                        {
                            throw std::invalid_argument("failed to convert " + str + " into double_t");
                        }
                    }
                    else {
                        val = json(str);
                    }
                }
            ));
        }
    }
    catch (std::invalid_argument const &e)
    {
        // no need to return error here, as `handleCsvColumn` will be called again 
        // to try loading data with different type
        // std::cerr << "Error opening CSV file: " << e.what() << std::endl;
        return {}; 
    }
    return std::move(column);
}

template <typename T>
static json loadCsvDataToJson(
    rapidcsv::Document const &doc,
    std::string const &columnName
) {
    json column(json::value_t::array);
    try {
        auto numRows = doc.GetRowCount();
        auto columnIndex = doc.GetColumnIdx(columnName);

        for (auto rowIndex = 0L; rowIndex < numRows; ++rowIndex)
        {
            column.push_back(doc.GetCell<json>(
                columnIndex,
                rowIndex,
                [](std::string const &str, json &val)
                {
                    if (!std::is_same<T, std::string>::value)
                    {
                        if (str.empty())
                        {
                            val = json{};
                            return;
                        }
                    }
                    if (std::is_same<T, double_t>::value)
                    {
                        size_t pos;
                        val = std::stod(str, &pos);
                        if (pos != str.length())
                        {
                            throw std::invalid_argument("failed to convert " + str + " into double");
                        }
                    }
                    else if (std::is_same<T, int64_t>::value)
                    {
                        size_t pos;
                        val = std::stol(str, &pos);
                        if (pos != str.length())
                        {
                            throw std::invalid_argument("failed to convert " + str + " into int");
                        }
                    }
                    else
                    {
                        val = str; 
                    }
                }
            ));
        }
    }
    catch (std::invalid_argument const &e)
    {
        // load function will try again with different type
        // std::cerr << "Error opening CSV file: " << e.what() << std::endl;
        return json{}; 
    }
    return std::move(column);
}

//////////////////////////////// Wisent Data Structures ///////////////////////////////
typedef size_t WisentString;
typedef size_t WisentExpressionIndex;

union WisentArgumentValue {
    bool asBool;
    int64_t asLong;
    double asDouble;
    WisentString asString;
    WisentExpressionIndex asExpression;
};

enum WisentArgumentType : size_t {
    ARGUMENT_TYPE_BOOL,
    ARGUMENT_TYPE_LONG,
    ARGUMENT_TYPE_DOUBLE,
    ARGUMENT_TYPE_STRING,
    ARGUMENT_TYPE_SYMBOL,
    ARGUMENT_TYPE_EXPRESSION
};

static size_t const WisentArgumentType_RLE_MINIMUM_SIZE =
    5; // assuming WisentArgumentType ideally stored in 1 byte only,
       // to store RLE-type, need 1 byte to declare the type and 4 bytes to
       // define the length

static size_t const WisentArgumentType_RLE_BIT =
    0x80; // first bit of WisentArgumentType to set RLE on/off

static size_t const WisentArgumentType_DELTA_ENCODED_BIT = 
    0x40; // Second-highest bit for delta encoding

struct WisentExpression {
    uint64_t symbolNameOffset;
    uint64_t startChildOffset;
    uint64_t endChildOffset;
};

/**
 * A single-allocation representation of an expression, including its arguments
 * (i.e., a flattened array of all arguments, another flattened array of
 * argument types and an array of PortableExpressions to encode the structure)
 */
struct WisentRootExpression {
    uint64_t const argumentCount;
    uint64_t const expressionCount;
    void *const originalAddress;
    /**
     * The index of the last used byte in the arguments buffer relative to the
     * pointer returned by getStringBuffer()
     */
    size_t stringArgumentsFillIndex;

    /**
     * This buffer holds all data associated with the expression in a single
     * untyped array. As the three kinds of data (ArgumentsValues, ArgumentTypes
     * and Expressions) have different sizes, holding them in an array of unions
     * would waste a lot of memory. A union of variable-sized arrays is not
     * supported in ANSI C. So it is held in an untyped buffer which is
     * essentially a concatenation of the three types of buffers that are
     * required. Utility functions exist to extract the different sub-arrays.
     */
    char arguments[];
};

//////////////////////////////// Part Extraction ///////////////////////////////

struct WisentRootExpression *getDummySerializedExpression();

static union WisentArgumentValue *getExpressionArguments(struct WisentRootExpression *root)
{
    return (union WisentArgumentValue*) root->arguments;
}

static enum WisentArgumentType *getArgumentTypes(struct WisentRootExpression *root)
{
    return (enum WisentArgumentType*) &root->arguments[
        root->argumentCount *sizeof(union WisentArgumentValue)];
}

static struct WisentExpression *getExpressionSubexpressions(struct WisentRootExpression *root)
{
    return (struct WisentExpression*) &root->arguments[
        root->argumentCount * (sizeof(union WisentArgumentValue) + sizeof(enum WisentArgumentType))];
}

static char *getStringBuffer(struct WisentRootExpression *root)
{
    return (char *) &root->arguments[
        root->argumentCount * (sizeof(union WisentArgumentValue) + sizeof(enum WisentArgumentType)) 
        + root->expressionCount * (sizeof(struct WisentExpression))];
}

////////////////////////////// Memory Management /////////////////////////////

// Not used 
static struct WisentRootExpression *allocateExpressionTree(
    uint64_t argumentCount, 
    uint64_t expressionCount,
    void *(*allocateFunction)(size_t))  // sharedMemoryMalloc(size)
{
    struct WisentRootExpression *root =
        (struct WisentRootExpression*) allocateFunction( 
            sizeof(struct WisentRootExpression) +
            sizeof(union WisentArgumentValue) * argumentCount +
            sizeof(enum WisentArgumentType) * argumentCount +
            sizeof(struct WisentExpression) * expressionCount
        );

    *((uint64_t *)&root->argumentCount) = argumentCount;
    *((uint64_t *)&root->expressionCount) = expressionCount;
    *((uint64_t *)&root->stringArgumentsFillIndex) = 0;
    *((void **)&root->originalAddress) = root;
    return root;
}

static struct WisentRootExpression* allocateExpressionTree(
    uint64_t argumentCount, 
    uint64_t expressionCount
) {
    size_t totalSize = sizeof(WisentRootExpression)
        + sizeof(WisentArgumentValue) * argumentCount
        + sizeof(WisentArgumentType) * argumentCount
        + sizeof(WisentExpression) * expressionCount;

    void* buffer = malloc(totalSize);
    if (!buffer) throw std::bad_alloc();
    WisentRootExpression* root = static_cast<WisentRootExpression*>(buffer);

    *((uint64_t *)&root->argumentCount) = argumentCount;
    *((uint64_t *)&root->expressionCount) = expressionCount;
    *((uint64_t *)&root->stringArgumentsFillIndex) = 0;
    *((void **)&root->originalAddress) = root;

    return root;
}


static int64_t *makeLongArgument(
    struct WisentRootExpression *root,
    uint64_t argumentOutputIndex)
{
    auto ARGUMENT_TYPE_LONG = WisentArgumentType::ARGUMENT_TYPE_LONG;
    getArgumentTypes(root)[argumentOutputIndex] = ARGUMENT_TYPE_LONG;
    return &getExpressionArguments(root)[argumentOutputIndex].asLong;
};

static size_t *makeSymbolArgument(
    struct WisentRootExpression *root,
    uint64_t argumentOutputIndex)
{
    auto ARGUMENT_TYPE_SYMBOL = WisentArgumentType::ARGUMENT_TYPE_SYMBOL;
    getArgumentTypes(root)[argumentOutputIndex] = ARGUMENT_TYPE_SYMBOL;
    return &getExpressionArguments(root)[argumentOutputIndex].asString;
};

static size_t *makeExpressionArgument(
    struct WisentRootExpression *root,
    uint64_t argumentOutputIndex)
{
    auto ARGUMENT_TYPE_SYMBOL = WisentArgumentType::ARGUMENT_TYPE_EXPRESSION;
    getArgumentTypes(root)[argumentOutputIndex] = ARGUMENT_TYPE_EXPRESSION;
    return &getExpressionArguments(root)[argumentOutputIndex].asString;
};

static size_t *makeStringArgument(
    struct WisentRootExpression *root,
    uint64_t argumentOutputIndex)
{
    auto ARGUMENT_TYPE_STRING = WisentArgumentType::ARGUMENT_TYPE_STRING;
    getArgumentTypes(root)[argumentOutputIndex] = ARGUMENT_TYPE_STRING;
    return &getExpressionArguments(root)[argumentOutputIndex].asString;
};

static double *makeDoubleArgument(
    struct WisentRootExpression *root,
    uint64_t argumentOutputIndex)
{
    auto ARGUMENT_TYPE_DOUBLE = WisentArgumentType::ARGUMENT_TYPE_DOUBLE;
    getArgumentTypes(root)[argumentOutputIndex] = ARGUMENT_TYPE_DOUBLE;
    return &getExpressionArguments(root)[argumentOutputIndex].asDouble;
};

/////////////////////////////// RLE Helpers ///////////////////////////////

static void setRLEArgumentFlagOrPropagateTypes(
    struct WisentRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint32_t size)
{
    if (size < WisentArgumentType_RLE_MINIMUM_SIZE) 
    {
        enum WisentArgumentType const type = getArgumentTypes(root)[argumentOutputIndex];
        for (uint64_t i = argumentOutputIndex + 1; i < argumentOutputIndex + size; ++i) 
        {
            getArgumentTypes(root)[i] = type;
        }
        return;
    }

    enum WisentArgumentType *types = getArgumentTypes(root);
    types[argumentOutputIndex] = (enum WisentArgumentType)(types[argumentOutputIndex] | WisentArgumentType_RLE_BIT);

    memmove(
        &types[argumentOutputIndex + 2],
        &types[argumentOutputIndex + 1],
        (root->argumentCount - argumentOutputIndex - 1) * sizeof(enum WisentArgumentType)
    );

    types[argumentOutputIndex + 1] = (enum WisentArgumentType)size;

    *((uint64_t *)&root->argumentCount) -= (size - 2);
}

static int64_t *makeLongArgumentsRun(
    struct WisentRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint32_t size)
{
    int64_t *value = makeLongArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

static size_t *makeSymbolArgumentsRun(
    struct WisentRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint32_t size)
{
    size_t *value = makeSymbolArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

static size_t *makeExpressionArgumentsRun(
    struct WisentRootExpression *root,
    uint64_t argumentOutputIndex,
    uint64_t size)
{
    size_t *value = makeExpressionArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

static size_t *makeStringArgumentsRun(
    struct WisentRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint64_t size)
{
    size_t *value = makeStringArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

static double *makeDoubleArgumentsRun(
    struct WisentRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint64_t size)
{
    double *value = makeDoubleArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

static struct WisentExpression *makeExpression(
    struct WisentRootExpression *root, 
    uint64_t expressionOutputI)
{
    return &getExpressionSubexpressions(root)[expressionOutputI];
}

// Not used
static size_t storeStringAndRealloc(  
    struct WisentRootExpression **root,
    char const *inputString,
    void *(*reallocateFunction)(void *, size_t)  // sharedMemoryRealloc
) {
    size_t const inputStringLength = strlen(inputString);
    *root = (struct WisentRootExpression*) 
        reallocateFunction(
            *root, 
            ((char *)(getStringBuffer(*root)) - ((char *)*root)) + 
            (*root)->stringArgumentsFillIndex + inputStringLength + 1
        );

    char const *result = strncpy(
            getStringBuffer(*root) + (*root)->stringArgumentsFillIndex,
            inputString, 
            inputStringLength + 1
        );

    (*root)->stringArgumentsFillIndex += inputStringLength + 1;
    return result - getStringBuffer(*root);
};

static size_t storeStringAndRealloc(
    WisentRootExpression** root,
    const char* inputString
) {
    size_t inputStringLength = strlen(inputString);
    size_t totalStringSize = inputStringLength + 1; 

    char* baseAddress = reinterpret_cast<char*>(*root);
    char* stringBuffer = reinterpret_cast<char*>(getStringBuffer(*root));
    size_t stringBufferOffset = static_cast<size_t>(stringBuffer - baseAddress);
    size_t requiredSize = stringBufferOffset + (*root)->stringArgumentsFillIndex + totalStringSize;

    void* buffer = realloc(*root, requiredSize);
    if (!buffer) throw std::bad_alloc();

    *root = reinterpret_cast<WisentRootExpression*>(buffer);
    stringBuffer = reinterpret_cast<char*>(getStringBuffer(*root));
    char* destination = stringBuffer + (*root)->stringArgumentsFillIndex;

    std::strncpy(destination, inputString, totalStringSize);

    size_t offset = (*root)->stringArgumentsFillIndex;
    (*root)->stringArgumentsFillIndex += totalStringSize;

    return offset;
}


static char const *viewString(
    struct WisentRootExpression *root,
    size_t inputStringOffset)
{
    return getStringBuffer(root) + inputStringOffset;
};


/////////////////////////////////// Serialisation //////////////////////////////////
namespace serializer
{       
    class JsonToWisent : public json::json_sax_t 
    {
    private:
        WisentRootExpression *root;
        std::vector<uint64_t> cumulArgCountPerLayer;
        std::vector<bool> wasKeyValue;
        std::vector<uint64_t> argumentIteratorStack{0};
        std::vector<uint64_t> expressionIndexStack{0};
        uint64_t nextExpressionIndex{0};
        uint64_t layerIndex{0};
        std::string const &csvPrefix;
        bool disableRLE;
        bool disableCsvHandling;
        uint64_t repeatedArgumentTypeCount; 
        const uint8_t* inputBuffer;
        size_t inputSize;

    public:    
        JsonToWisent(
            uint64_t expressionCount,
            std::vector<uint64_t> &&argumentCountPerLayer,
            const uint8_t* inputBuffer, 
            size_t inputSize, 
            std::string const &csvPrefix, 
            bool disableRLE, 
            bool disableCsvHandling
        ): 
            root(nullptr),
            cumulArgCountPerLayer(std::move(argumentCountPerLayer)),
            inputBuffer(inputBuffer), 
            inputSize(inputSize), 
            csvPrefix(csvPrefix),
            disableRLE(disableRLE), 
            disableCsvHandling(disableCsvHandling),
            repeatedArgumentTypeCount(0)
        {
            std::partial_sum(
                cumulArgCountPerLayer.begin(),
                cumulArgCountPerLayer.end(),
                cumulArgCountPerLayer.begin()
            );
            root = allocateExpressionTree(
                cumulArgCountPerLayer.back(),
                expressionCount
            );
            wasKeyValue.resize(cumulArgCountPerLayer.size(), false);
        }

        WisentRootExpression *getRoot() { return root; }

        bool null() override
        {
            addSymbol("Null");
            handleKeyValueEnd();
            return true;
        }

        bool boolean(bool val) override
        {
            addSymbol(val ? "True" : "False");
            handleKeyValueEnd();
            return true;
        }

        bool number_integer(number_integer_t val) override
        {
            addLong(val);
            handleKeyValueEnd();
            return true;
        }

        bool number_unsigned(number_unsigned_t val) override
        {
            addLong(val);
            handleKeyValueEnd();
            return true;
        }

        bool number_float(number_float_t val, const string_t & /*s*/) override
        {
            addDouble(val);
            handleKeyValueEnd();
            return true;
        }

        bool string(string_t &val) override
        {
            if (!handleCsvFile(val)) {
                addString(val);
            }
            handleKeyValueEnd();
            return true;
        }

        bool start_object(std::size_t /*elements*/) override
        {
            startExpression("Object");
            return true;
        }

        bool end_object() override
        {
            endExpression();
            handleKeyValueEnd();
            return true;
        }

        bool start_array(std::size_t /*elements*/) override
        {
            startExpression("List");
            return true;
        }

        bool end_array() override
        {
            endExpression();
            handleKeyValueEnd();
            return true;
        }

        bool key(string_t &val) override
        {
            startExpression(val);
            wasKeyValue[layerIndex] = true;
            return true;
        }

        void handleKeyValueEnd()
        {
            if (wasKeyValue[layerIndex]) {
                wasKeyValue[layerIndex] = false;
                endExpression();
            }
        }

        bool binary(json::binary_t &val) override
        {
            throw std::runtime_error("binary value not implemented");
        }

        bool parse_error(
            std::size_t position, 
            const std::string &last_token,
            const json::exception &ex) override
        {
            throw std::runtime_error(
                "parse_error(position=" + std::to_string(position) +
                ", last_token=" + last_token +
                ",\n            ex=" + std::string(ex.what()) + ")");
        }

    private:
        uint64_t getNextArgumentIndex()
        {
            return getExpressionSubexpressions(root)[expressionIndexStack.back()].startChildOffset + argumentIteratorStack.back()++;
        }

        void applyTypeRLE(std::uint64_t argIndex)
        {
            if (disableRLE) {
                return;
            }
            if (repeatedArgumentTypeCount == 0) {
                repeatedArgumentTypeCount = 1;
                return;
            }
            if (getArgumentTypes(root)[argIndex - 1] != getArgumentTypes(root)[argIndex]) 
            {
                resetTypeRLE(argIndex);
                repeatedArgumentTypeCount = 1;
                return;
            }
            ++repeatedArgumentTypeCount;
        }

        void resetTypeRLE(std::uint64_t endIndex)
        {
            if (repeatedArgumentTypeCount >= WisentArgumentType_RLE_MINIMUM_SIZE) 
            {
                setRLEArgumentFlagOrPropagateTypes(
                    root, endIndex - repeatedArgumentTypeCount,
                    repeatedArgumentTypeCount);
            }
            repeatedArgumentTypeCount = 0;
        }

        void addLong(std::int64_t input)
        {
            uint64_t argIndex = getNextArgumentIndex();
            *makeLongArgument(root, argIndex) = input;
            applyTypeRLE(argIndex);
        }

        void addDouble(double_t input)
        {
            uint64_t argIndex = getNextArgumentIndex();
            *makeDoubleArgument(root, argIndex) = input;
            applyTypeRLE(argIndex);
        }

        void addString(std::string const &input)
        {
            auto storedString = storeStringAndRealloc(
                &root, 
                input.c_str()
            );
            uint64_t argIndex = getNextArgumentIndex();
            *makeStringArgument(root, argIndex) = storedString;
            applyTypeRLE(argIndex);
        }

        void addSymbol(std::string const &symbol)
        {
            auto storedString = storeStringAndRealloc(
                &root, 
                symbol.c_str()
            );
            uint64_t argIndex = getNextArgumentIndex();
            *makeSymbolArgument(root, argIndex) = storedString;
            applyTypeRLE(argIndex);
        }

        void addExpression(size_t expressionIndex)
        {
            uint64_t argIndex = getNextArgumentIndex();
            *makeExpressionArgument(root, argIndex) = expressionIndex;
            resetTypeRLE(argIndex);
        }

        void startExpression(std::string const &head)
        {
            auto expressionIndex = nextExpressionIndex++;
            addExpression(expressionIndex);
            auto storedString = storeStringAndRealloc(
                &root, 
                head.c_str()
            );
            auto startChildOffset = cumulArgCountPerLayer[layerIndex++];
            *makeExpression(root, expressionIndex) = WisentExpression{
                storedString, startChildOffset,
                0 // not known yet; set during endExpression()
            };
            argumentIteratorStack.push_back(0);
            expressionIndexStack.push_back(expressionIndex);
        }

        void endExpression()
        {
            auto &expression =
                getExpressionSubexpressions(root)[expressionIndexStack.back()];
            expression.endChildOffset =
                expression.startChildOffset + argumentIteratorStack.back();
            resetTypeRLE(expression.endChildOffset);
            argumentIteratorStack.pop_back();
            expressionIndexStack.pop_back();
            cumulArgCountPerLayer[--layerIndex] = expression.endChildOffset;
        }

        bool handleCsvFile(std::string const &filename)
        {
            if (disableCsvHandling) 
            {
                return false;
            }
            auto extPos = filename.find_last_of(".");
            if (extPos == std::string::npos || filename.substr(extPos) != ".csv") 
            {
                return false;
            }
            startExpression("Table");
            auto doc = openCsvFile(csvPrefix + filename);
            for (auto const &columnName : doc.GetColumnNames()) 
            {
                if (!handleCsvColumn<int64_t>(doc, columnName, [this](auto val) 
                        { addLong(val); })) 
                {
                    if (!handleCsvColumn<double_t>(doc, columnName, [this](auto val) 
                            { addDouble(val); })) 
                    {
                        if (!handleCsvColumn<std::string>(doc, columnName, [this](auto const &val) 
                                { addString(val); })) 
                        {
                            throw std::runtime_error(
                                "failed to handle csv column: '" + columnName + "'"
                            );
                        }
                    }
                }
            }
            endExpression();
            return true;
        }

        template <typename T, typename Func>
        bool handleCsvColumn(
            rapidcsv::Document const &doc,
            std::string const &columnName, 
            Func &&addValueFunc)
        {
            auto column = loadCsvData<T>(doc, columnName);
            if (column.empty()) {
                return false;
            }
            startExpression(columnName);
            for (auto const &val : column) {
                val ? addValueFunc(*val) : addSymbol("Missing");
            }
            endExpression();
            return true;
        }
    };

    static Result<WisentRootExpression*> load(
        const uint8_t* inputData,
        size_t inputSize,
        const uint8_t* csvPrefixData,
        size_t csvPrefixSize,
        bool disableRLE,
        bool disableCsvHandling
    ) {
        char csvPrefixStr[csvPrefixSize + 1];
        strncpy(csvPrefixStr, reinterpret_cast<const char*>(csvPrefixData), csvPrefixSize);
        csvPrefixStr[csvPrefixSize] = '\0';
        std::string csvPrefix(csvPrefixStr);
        // printf("csvPrefixStr = %s\n", csvPrefix.c_str());

        // count & calculate total size
        uint64_t expressionCount = 0;
        std::vector<uint64_t> argumentCountPerLayer;
        argumentCountPerLayer.reserve(16);
        auto _ = json::parse(
            inputData, 
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

        // populate Wisent tree
        JsonToWisent jsonToWisent(
            expressionCount,
            std::move(argumentCountPerLayer),
            inputData,
            inputSize,
            csvPrefix,
            disableRLE,
            disableCsvHandling
        );

        json::sax_parse(inputData, &jsonToWisent);

        return Result<WisentRootExpression*>{jsonToWisent.getRoot(), std::nullopt, {}};
    }

} // namespace wisent::serializer

namespace compressor
{

    /////////////////////////////// Included Compression Algorithms ////////////////////////////

    namespace algorithms
    {
        struct RLE 
        {
            static Result<size_t> compress(
                const std::byte* input,
                const size_t inputSize,
                std::byte* output
            ) {
                if (input == nullptr || output == nullptr)  return makeError<size_t>("Invalid input or output buffer");

                size_t inPos = 0;
                size_t outPos = 0;

                while (inPos < inputSize) {
                    std::byte current = input[inPos];
                    size_t runLength = 1;

                    while (inPos + runLength < inputSize && 
                        input[inPos + runLength] == current && 
                        runLength < 255) {
                        runLength++;
                    }

                    output[outPos++] = static_cast<std::byte>(runLength);
                    output[outPos++] = current;

                    inPos += runLength;
                }

                return makeResult<size_t>(outPos);
            }

            static Result<size_t> decompress(
                const std::byte* input,
                const size_t inputSize, 
                std::byte* output
            ) {
                if (input == nullptr || output == nullptr)  return makeError<size_t>("Invalid input or output buffer");

                size_t inPos = 0;
                size_t outPos = 0;

                while (inPos < inputSize) 
                {
                    std::byte count = input[inPos++];
                    std::byte value = input[inPos++];

                    size_t runLength = static_cast<unsigned char>(count);
                    for (size_t i = 0; i < runLength; ++i) {
                        output[outPos++] = value;
                    }
                }
                return makeResult<size_t>(outPos);
            }
        }; 

        struct LZ77
        {
            static Result<size_t> compress(
                const std::byte* input,
                size_t inputSize,
                std::byte* output,
                size_t windowSize = 4096,
                size_t lookaheadBufferSize = 18
            ) {
                size_t codingPosition = 0;
                size_t outputPosition = 0;

                while (codingPosition < inputSize) 
                {
                    size_t bestLength = 0;
                    size_t bestOffset = 0;
                    uint8_t byteMask = 0xFF; 

                    size_t start = (codingPosition >= windowSize) ? (codingPosition - windowSize) : 0;

                    for (size_t i = start; i < codingPosition; ++i) 
                    {
                        size_t length = 0;
                        while (
                            length < lookaheadBufferSize &&
                            codingPosition + length < inputSize &&
                            input[i + length] == input[codingPosition + length]
                        ) {
                            ++length;
                        }

                        if (length > bestLength) 
                        {
                            bestLength = length;
                            bestOffset = codingPosition - i;
                        }
                    }

                    if (bestLength > 2) 
                    {
                        output[outputPosition++] = std::byte{0};
                        output[outputPosition++] = std::byte{static_cast<uint8_t>((bestOffset >> 8) & byteMask)};
                        output[outputPosition++] = std::byte{static_cast<uint8_t>(bestOffset & byteMask)};
                        output[outputPosition++] = std::byte{static_cast<uint8_t>(bestLength & byteMask)};
                        codingPosition += bestLength;
                    } 
                    else 
                    {
                        output[outputPosition++] = std::byte{1};
                        output[outputPosition++] = input[codingPosition];
                        ++codingPosition;
                    }
                }
                Result<size_t> result;
                result.setValue(outputPosition);
                return result;
            }; 

            static Result<size_t> decompress(
                const std::byte* input,
                size_t inputSize,
                std::byte* output
            ) {
                Result<size_t> result;
                size_t decodingPosition = 0;
                size_t outputPosition = 0;

                while (decodingPosition < inputSize) 
                {
                    if (input[decodingPosition] == std::byte{0}) 
                    {
                        if (decodingPosition + 3 >= inputSize)  return makeError<size_t>("Invalid compressed data");

                        size_t offset = (static_cast<uint8_t>(input[decodingPosition + 1]) << 8) |
                                        static_cast<uint8_t>(input[decodingPosition + 2]);
                        size_t length = static_cast<uint8_t>(input[decodingPosition + 3]);

                        if (offset > outputPosition) return makeError<size_t>("Invalid offset");

                        size_t sourcePosition = outputPosition - offset;
                        for (size_t i = 0; i < length; ++i) 
                        {
                            output[outputPosition++] = output[sourcePosition + i];
                        }

                        decodingPosition += 4;
                    } 
                    else 
                    {
                        if (decodingPosition + 1 >= inputSize) return makeError<size_t>("Invalid compressed data");

                        output[outputPosition++] = input[decodingPosition + 1];
                        decodingPosition += 2;
                    }
                }
                result.setValue(outputPosition);
                return result;
            }; 
        }; 

        struct Huffman {
            static Result<size_t> compress(
                const std::byte* input,
                const size_t inputSize,
                const std::byte* output
            );
            static Result<size_t> decompress(
                const std::byte* input,
                const size_t inputSize, 
                const std::byte* output
            );

            
            struct HuffmanNode 
            {
                char symbol;
                int64_t frequency;
                HuffmanNode* left;
                HuffmanNode* right;
                HuffmanNode(char s, int64_t f) : symbol(s), frequency(f), left(nullptr), right(nullptr) {}
            };

            struct Compare 
            {
                bool operator()(HuffmanNode* l, HuffmanNode* r) 
                {
                    return l->frequency > r->frequency;
                }
            };

            class HuffmanTree {
            private:
                HuffmanNode* root = nullptr;
                std::unordered_map<char, std::string> encodingTable;

                void buildEncodingTable(HuffmanNode* node, const std::string& str) {
                    if (!node) return;
                    if (!node->left && !node->right) {
                        encodingTable[node->symbol] = str;
                    }
                    buildEncodingTable(node->left, str + "0");
                    buildEncodingTable(node->right, str + "1");
                }

            public:
                void buildTreeWithInput(const std::byte* input, size_t inputSize) {
                    std::unordered_map<char, int64_t> frequencies;
                    for (size_t i = 0; i < inputSize; ++i) {
                        frequencies[static_cast<char>(input[i])]++;
                    }
                    frequencies['\0'] = 1; // EOF

                    std::priority_queue<HuffmanNode*, std::vector<HuffmanNode*>, Compare> tree;
                    for (auto& [ch, freq] : frequencies) {
                        tree.push(new HuffmanNode(ch, freq));
                    }

                    while (tree.size() > 1) {
                        HuffmanNode* left = tree.top(); tree.pop();
                        HuffmanNode* right = tree.top(); tree.pop();

                        HuffmanNode* parent = new HuffmanNode('*', left->frequency + right->frequency);
                        parent->left = left;
                        parent->right = right;
                        tree.push(parent);
                    }

                    root = tree.top();
                    buildEncodingTable(root, "");
                }

                std::vector<uint8_t> encode(const std::byte* input, size_t inputSize) {
                    std::vector<uint8_t> encodedBytes;

                    std::string eofCode = encodingTable['\0'];
                    encodedBytes.push_back(static_cast<uint8_t>(eofCode.size()));
                    encodeStringToBytes(eofCode, encodedBytes);

                    for (const auto& [symbol, code] : encodingTable) {
                        if (symbol == '\0') continue;
                        encodedBytes.push_back(static_cast<uint8_t>(symbol));
                        encodedBytes.push_back(static_cast<uint8_t>(code.size()));
                        encodeStringToBytes(code, encodedBytes);
                        encodedBytes.push_back(0); // Delimiter
                    }
                    encodedBytes.push_back(0); // Final delimiter

                    // Encode actual data
                    std::vector<bool> bits;
                    for (size_t i = 0; i < inputSize; ++i) {
                        for (char bit : encodingTable[static_cast<char>(input[i])]) {
                            bits.push_back(bit == '1');
                        }
                    }
                    for (char bit : encodingTable['\0']) {
                        bits.push_back(bit == '1');
                    }

                    uint8_t byte = 0, count = 0;
                    for (bool bit : bits) {
                        byte = (byte << 1) | bit;
                        ++count;
                        if (count == 8) {
                            encodedBytes.push_back(byte);
                            byte = 0;
                            count = 0;
                        }
                    }
                    if (count > 0) {
                        byte <<= (8 - count);
                        encodedBytes.push_back(byte);
                    }

                    return encodedBytes;
                }

                void encodeStringToBytes(const std::string& str, std::vector<uint8_t>& out) {
                    uint8_t buffer = 0;
                    int count = 0;
                    for (char bit : str) {
                        buffer = (buffer << 1) | (bit == '1' ? 1 : 0);
                        ++count;
                        if (count == 8) {
                            out.push_back(buffer);
                            buffer = 0;
                            count = 0;
                        }
                    }
                    if (count > 0) {
                        buffer <<= (8 - count);
                        out.push_back(buffer);
                    }
                }
            };

            static Result<size_t> compress(
                const std::byte* input,
                const size_t inputSize,
                const std::byte* output
            ) {
                if (!input || !output || inputSize == 0) return makeError<size_t>("Invalid input or output buffer");

                HuffmanTree tree;
                tree.buildTreeWithInput(input, inputSize);
                std::vector<uint8_t> encoded = tree.encode(input, inputSize);

                std::memcpy(output, encoded.data(), encoded.size());
                return makeResult<size_t>(encoded.size());
            }; 


            static std::string byteToBinaryString(uint8_t byte, uint8_t stringLength) 
            {
                std::bitset<8> eofBits(byte);
                std::string binaryString = eofBits.to_string();
                return (stringLength >= 8) ? binaryString : binaryString.substr(0, stringLength);
            }; 

            static std::vector<uint8_t> decompress(
                const std::vector<uint8_t>& input
            ) {
                HuffmanTree huffmanTree;
                std::unordered_map<char, std::string> encodingTable;

                uint8_t offset = 0; 

                // Read EOF symbol
                uint8_t eofCodeLength = input[offset++];
                int maxCodeLengthInByte = (eofCodeLength + 7 ) / 8;

                std::string eofBitString;
                for (int i = 1; i <= maxCodeLengthInByte; i++) 
                {
                    eofBitString += byteToBinaryString(input[offset++], 8);
                }
                eofBitString = eofBitString.substr(0, eofCodeLength);        
                encodingTable['\0'] = eofBitString;

                // Read encoding table
                while (offset < input.size()) 
                {
                    uint8_t symbol = input[offset++];
                    if (symbol == 0) break; // End of encoding table

                    uint8_t codeLength = input[offset++]; 
                    int codeLengthInByte = (codeLength + 7) / 8;
                    
                    std::string bitString;
                    uint8_t byteCount = 0; 
                    while (offset < input.size() && byteCount < codeLengthInByte) 
                    {
                        bitString += byteToBinaryString(input[offset++], 8);
                        byteCount++;
                    }
                    bitString = bitString.substr(0, codeLength);  
                    encodingTable[symbol] = bitString;
                    offset ++; // Skip delimiter
                }
                huffmanTree.buildTreeWithEncodingTable(encodingTable);

                std::vector<uint8_t> data(input.begin() + offset, input.end());

                std::vector<uint8_t> decoded = huffmanTree.decode(data);
            
                return decoded; 
            }; 
        }; 

        struct FSE {
            static Result<size_t> compress(
                const std::byte* input,
                const size_t inputSize,
                const std::byte* output, 
                bool verbose = false
            );
            static Result<size_t> decompress(
                const std::byte* input,
                const size_t inputSize, 
                const std::byte* output, 
                bool verbose = false
            );
        };

        struct Delta {
            static Result<size_t> compress(
                const std::byte* input,
                const size_t inputSize,
                std::byte* output
            ) {
                Result<size_t> result;
                if (inputSize == 0 || input == nullptr || output == nullptr) 
                    return makeError<size_t>("Invalid input or output buffer");

                if (inputSize < sizeof(std::byte)) {
                    result.addWarning("Input size is too small");
                }

                output[0] = input[0];
                size_t outputSize = 1;

                for (size_t i = 1; i < inputSize; i++) {
                    output[outputSize++] = static_cast<std::byte>(
                        static_cast<uint8_t>(input[i]) - static_cast<uint8_t>(input[i-1])
                    );
                }
                return makeResult<size_t>(outputSize, &result);
            }; 

            static Result<size_t> decompress(
                const std::byte* input,
                const size_t inputSize, 
                std::byte* output
            ) {
                if (inputSize == 0 || input == nullptr || output == nullptr) 
                    return makeError<size_t>("Invalid input or output buffer");

                output[0] = input[0];
                size_t outputSize = 1;
                for (size_t i = 1; i < inputSize; i++) 
                {
                    output[i] = static_cast<std::byte>(
                        static_cast<uint8_t>(output[i-1]) + static_cast<uint8_t>(input[i])
                    );
                    outputSize++;
                }
                return makeResult<size_t>(outputSize);
            }; 
        };

        struct Deflate {
            static Result<size_t> compress(
                const std::byte* input,
                const size_t inputSize,
                std::byte* output
            ) {
                if (inputSize == 0 || input == nullptr || output == nullptr) 
                    return makeError<size_t>("Invalid input or output buffer");

                uLongf compressedSize = compressBound(inputSize);
                int result = ::compress(
                    reinterpret_cast<Bytef*>(output), 
                    &compressedSize,
                    reinterpret_cast<const Bytef*>(input), 
                    inputSize
                );

                if (result != Z_OK) {
                    switch (result) {
                        case Z_MEM_ERROR:
                            return makeError<size_t>("Not enough memory for compression");
                        case Z_BUF_ERROR:
                            return makeError<size_t>("Output buffer too small");
                        default:
                            return makeError<size_t>("Compression failed with error code: " + std::to_string(result));
                    }
                }

                return makeResult<size_t>(compressedSize);
            }

            static Result<size_t> decompress(
                const std::byte* input,
                const size_t inputSize, 
                std::byte* output,
                const size_t outputSize
            ) {
                if (inputSize == 0 || input == nullptr || output == nullptr) 
                    return makeError<size_t>("Invalid input or output buffer");

                uLongf decompressedSize = outputSize;
                
                int result = ::uncompress(
                    reinterpret_cast<Bytef*>(output), 
                    &decompressedSize,
                    reinterpret_cast<const Bytef*>(input), 
                    inputSize
                );

                if (result != Z_OK) 
                {
                    switch (result) 
                    {
                        case Z_MEM_ERROR:
                            return makeError<size_t>("Not enough memory for decompression");
                        case Z_BUF_ERROR:
                            return makeError<size_t>("Output buffer too small");
                        case Z_DATA_ERROR:
                            return makeError<size_t>("Corrupted or invalid compressed data");
                        default:
                            return makeError<size_t>("Decompression failed with error code: " + std::to_string(result));
                    }
                }

                return makeResult<size_t>(decompressedSize);
            }; 
        };
    } // namespace wisent::algorithms

    //////////////////////////////// Wisent Compressor //////////////////////////////

    const size_t BytesPerLong = 8;
    const bool usingBlockSize = false;
    const size_t BlockSize = 1024 * 1024; 

    enum class CompressionType {
        NONE,
        RLE,
        HUFFMAN,
        LZ77,
        FSE, 
        DELTA
    };

    static const std::unordered_map<std::string, CompressionType> compressionAliases = 
    {
        {"none", CompressionType::NONE},
        {"rle", CompressionType::RLE},
        {"runlengthencoding", CompressionType::RLE},
        {"huffman", CompressionType::HUFFMAN},
        {"lz77", CompressionType::LZ77},
        {"fse", CompressionType::FSE},
        {"finitestateentropy", CompressionType::FSE},
        {"delta", CompressionType::DELTA},
        {"de", CompressionType::DELTA}
    };

    template <typename Coder>
    auto compressWith(const std::vector<uint8_t>& buffer) 
    {
        if (!usingBlockSize)
            return Coder::compress(buffer);

        std::vector<uint8_t> compressedData;
        size_t totalSize = buffer.size();
        size_t offset = 0;
        while (offset < totalSize) 
        {
            size_t currentChunkSize = std::min(BlockSize, totalSize - offset);
            std::vector<uint8_t> chunk(buffer.begin() + offset, buffer.begin() + offset + currentChunkSize);
            auto compressedChunk = Coder::compress(chunk);
            compressedData.insert(compressedData.end(), compressedChunk.begin(), compressedChunk.end());
            offset += currentChunkSize;
        }
        return compressedData;
    } 

    std::vector<uint8_t> performCompression(
        CompressionType type,
        std::string buffer
    ) {
        std::vector<uint8_t> vector = std::vector<uint8_t>(buffer.begin(), buffer.end());
        switch (type) 
        {
            case CompressionType::NONE:
                return vector;
            case CompressionType::RLE:
                return compressWith<algorithms::RLE>(vector);
            case CompressionType::LZ77:
                return compressWith<algorithms::LZ77>(vector);
            case CompressionType::HUFFMAN:
                return compressWith<algorithms::Huffman>(vector);
            case CompressionType::FSE:
                return compressWith<algorithms::FSE>(vector);
            case CompressionType::DELTA:
                return compressWith<algorithms::Delta>(vector);
            default:
                throw std::invalid_argument("Unsupported compression type");
        }
    }

    ///////////////////////// Compression Pipeline //////////////////////////

    static CompressionType stringToCompressionType(std::string type) 
    {
        std::transform(type.begin(), type.end(), type.begin(), ::tolower);
        auto it = compressionAliases.find(type);
        if (it == compressionAliases.end()) 
        {
            throw std::invalid_argument("Unknown compression type: " + type);
        }
        return it->second;
    }

    class CompressionPipeline 
    {
    private:
        std::vector<CompressionType> pipeline;

        CompressionPipeline(const std::vector<CompressionType>& steps) : pipeline(steps) {}

        std::string compressionTypeToString(CompressionType type) const 
        {
            switch (type) 
            {
                case CompressionType::NONE: return "None";
                case CompressionType::RLE: return "RLE";
                case CompressionType::HUFFMAN: return "Huffman";
                case CompressionType::LZ77: return "LZ77";
                case CompressionType::FSE: return "FSE";
                case CompressionType::DELTA: return "Delta";
                default: return "Unknown";
            }
        }

    public:
        void log() const 
        {
            std::cout << "Logging compression pipeline:" << std::endl;
            for (CompressionType step : pipeline) 
            {
                std::cout << " - " << compressionTypeToString(step) << std::endl;
            }
        }

        std::vector<uint8_t> compress(const std::string& input) const 
        {
            std::vector<uint8_t> data(input.begin(), input.end());
            for (CompressionType type : pipeline) 
            {
                data = algorithms::compressWithType(type, data);
            }
            return data;
        }

        class Builder 
        {
        private:
            std::vector<CompressionType> steps;

        public:
            Builder& addStep(CompressionType type) 
            {
                steps.push_back(type);
                return *this;
            }

            Builder& addStep(const std::string& typeStr) 
            {
                steps.push_back(stringToCompressionType(typeStr));
                return *this;
            }

            // support custom function input

            CompressionPipeline build() 
            {
                return CompressionPipeline(steps);
            }
        };
    };

    static Result<WisentRootExpression*> compress(
        const uint8_t* inputData,
        size_t inputSize,
        const uint8_t* csvPrefixData,
        size_t csvPrefixSize,
        std::vector<std::pair<std::string, CompressionPipeline>> compressionSpecifiers,
        bool disableRLE,
        bool disableCsvHandling
    ) {
        char csvPrefixStr[csvPrefixSize + 1];
        strncpy(csvPrefixStr, reinterpret_cast<const char*>(csvPrefixData), csvPrefixSize);
        csvPrefixStr[csvPrefixSize] = '\0';
        std::string csvPrefix(csvPrefixStr);
        // printf("csvPrefixStr = %s\n", csvPrefix.c_str());

        // count & calculate total size
        uint64_t expressionCount = 0;
        std::vector<uint64_t> argumentCountPerLayer;
        argumentCountPerLayer.reserve(16);
        auto _ = json::parse(
            inputData, 
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

        // populate Wisent tree
        wisent::serializer::JsonToWisent jsonToWisent(
            expressionCount,
            std::move(argumentCountPerLayer),
            inputData,
            inputSize,
            csvPrefix,
            disableRLE,
            disableCsvHandling
        );
        json::sax_parse(inputData, &jsonToWisent);

        return Result<WisentRootExpression*>{jsonToWisent.getRoot(), std::nullopt, {}};
    }

    // //////////////////////////// Wisent Decompressor ////////////////////////
    // template <typename Coder>
    // auto decompressWith(const std::vector<uint8_t>& buffer) 
    // {   return Coder::decompress(buffer); }

} // namespace wisent::compressor

} // namespace wisent


#endif // WISENT_HPP