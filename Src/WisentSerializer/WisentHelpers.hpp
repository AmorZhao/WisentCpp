#ifndef WISENTHELPERS_HPP
#define WISENTHELPERS_HPP

#include <cstring>
#include <vector>
#include <cstdint>
#include <cstdlib>

extern "C" {

// NOLINTBEGIN(hicpp-use-auto,cppcoreguidelines-pro-type-union-access)
// NOLINTBEGIN(cppcoreguidelines-pro-type-cstyle-cast)

//////////////////////////////// Data Structures ///////////////////////////////

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
    ARGUMENT_TYPE_EXPRESSION, 
    ARGUMENT_TYPE_COMPRESSED
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

WisentRootExpression* getDummySerializedExpression();

inline WisentArgumentValue* getExpressionArguments(WisentRootExpression* root) 
{
    return reinterpret_cast<WisentArgumentValue*>(root->arguments);
}

inline WisentArgumentType* getArgumentTypes(WisentRootExpression* root) 
{
    return reinterpret_cast<WisentArgumentType*>(&root->arguments[
        root->argumentCount * sizeof(WisentArgumentValue)]);
}

inline WisentExpression* getExpressionSubexpressions(WisentRootExpression* root) 
{
    return reinterpret_cast<WisentExpression*>(&root->arguments[
        root->argumentCount * (sizeof(WisentArgumentValue) + sizeof(WisentArgumentType))]);
}

inline char* getStringBuffer(WisentRootExpression* root) 
{
    return (char *) &root->arguments[
        root->argumentCount * (sizeof(WisentArgumentValue) + sizeof(WisentArgumentType)) +
        root->expressionCount * sizeof(WisentExpression)];
}

////////////////////////////// Memory Management ////////////////////////////////

inline WisentRootExpression* allocateExpressionTree(
    uint64_t argumentCount,
    uint64_t expressionCount,
    void* (*allocateFunction)(size_t)   // sharedMemoryMalloc(size)
) {
    WisentRootExpression *root = reinterpret_cast<WisentRootExpression*>(allocateFunction(
        sizeof(WisentRootExpression) +
        sizeof(WisentArgumentValue) * argumentCount +
        sizeof(WisentArgumentType) * argumentCount +
        sizeof(WisentExpression) * expressionCount)
    );

    *const_cast<uint64_t*>(&root->argumentCount) = argumentCount;
    *const_cast<uint64_t*>(&root->expressionCount) = expressionCount;
    *const_cast<size_t*>(&root->stringArgumentsFillIndex) = 0;
    *const_cast<void**>(&root->originalAddress) = root;
    return root;
}

inline void freeExpressionTree(
    WisentRootExpression* root, 
    void (*freeFunction)(void *)
) {
    freeFunction(root);
}

//////////////////////////////   Memory Management /////////////////////////////

inline int64_t *makeLongArgument(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex)
{
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_LONG;
    return &getExpressionArguments(root)[argumentOutputIndex].asLong;
};

inline size_t *makeSymbolArgument(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex)
{
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_SYMBOL;
    return &getExpressionArguments(root)[argumentOutputIndex].asString;
};

inline size_t *makeExpressionArgument(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex)
{
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_EXPRESSION;
    return &getExpressionArguments(root)[argumentOutputIndex].asString;
};

inline size_t *makeStringArgument(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex)
{
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_STRING;
    return &getExpressionArguments(root)[argumentOutputIndex].asString;
};

inline double *makeDoubleArgument(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex)
{
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_DOUBLE;
    return &getExpressionArguments(root)[argumentOutputIndex].asDouble;
};

inline size_t *makeBinaryArgument(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex)
{
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_COMPRESSED;
    return &getExpressionArguments(root)[argumentOutputIndex].asString;
};

/////////////////////////////// Encoding Helpers ///////////////////////////////

static void setRLEArgumentFlagOrPropagateTypes(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint32_t size
) {
    if (size < WisentArgumentType_RLE_MINIMUM_SIZE) 
    {
        WisentArgumentType const type = getArgumentTypes(root)[argumentOutputIndex];
        for (uint64_t i = argumentOutputIndex + 1; i < argumentOutputIndex + size; ++i) 
        {
            getArgumentTypes(root)[i] = type;
        }
        return;
    }

    WisentArgumentType *types = getArgumentTypes(root);
    types[argumentOutputIndex] = static_cast<WisentArgumentType>(types[argumentOutputIndex] | WisentArgumentType_RLE_BIT);

    memmove(
        &types[argumentOutputIndex + 2],
        &types[argumentOutputIndex + 1],
        (root->argumentCount - argumentOutputIndex - 1) * sizeof(WisentArgumentType)
    );

    types[argumentOutputIndex + 1] = static_cast<WisentArgumentType>(size);

    *((uint64_t *)&root->argumentCount) -= (size - 2);
}

inline int64_t *makeLongArgumentsRun(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint32_t size)
{
    int64_t *value = makeLongArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

inline size_t *makeSymbolArgumentsRun(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint32_t size)
{
    size_t *value = makeSymbolArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

inline size_t *makeExpressionArgumentsRun(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex,
    uint64_t size)
{
    size_t *value = makeExpressionArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

inline size_t *makeStringArgumentsRun(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint64_t size)
{
    size_t *value = makeStringArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

inline double *makeDoubleArgumentsRun(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint64_t size)
{
    double *value = makeDoubleArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

inline WisentExpression *makeExpression(
    WisentRootExpression *root, 
    uint64_t expressionOutputI)
{
    return &getExpressionSubexpressions(root)[expressionOutputI];
}

inline size_t storeString(
    WisentRootExpression **root,
    char const *inputString,
    void *(*reallocateFunction)(void *, size_t)
) {
    size_t const inputStringLength = strlen(inputString);
    *root = reinterpret_cast<WisentRootExpression*>(reallocateFunction(
        *root, 
        ((char *)(getStringBuffer(*root)) - reinterpret_cast<char*>(*root)) 
                + (*root)->stringArgumentsFillIndex + inputStringLength + 1
    ));

    char* destination = getStringBuffer(*root) + (*root)->stringArgumentsFillIndex;
    char const *result = strncpy(
            destination,
            inputString, 
            inputStringLength + 1
        );

    (*root)->stringArgumentsFillIndex += inputStringLength + 1;
    return result - getStringBuffer(*root);
};

inline size_t storeBytes(
    WisentRootExpression **root,
    const std::vector<uint8_t> &inputBytes,
    void *(*reallocateFunction)(void *, size_t)
) {
    size_t inputBytesLength = inputBytes.size();
    *root = reinterpret_cast<WisentRootExpression*>(reallocateFunction(
        *root,
        ((getStringBuffer(*root)) - reinterpret_cast<char*>(*root)) 
            + (*root)->stringArgumentsFillIndex + inputBytesLength
    ));

    char *destination = getStringBuffer(*root) + (*root)->stringArgumentsFillIndex;
    if (inputBytesLength > 0) 
    {
        memcpy(destination, inputBytes.data(), inputBytesLength);
    }

    size_t offset = (*root)->stringArgumentsFillIndex;
    (*root)->stringArgumentsFillIndex += inputBytesLength;
    return offset;
}

inline const char* viewString(
    WisentRootExpression *root,
    size_t inputStringOffset)
{
    return getStringBuffer(root) + inputStringOffset;
};

// NOLINTEND(hicpp-use-auto,cppcoreguidelines-pro-type-union-access)
// NOLINTEND(cppcoreguidelines-pro-type-cstyle-cast)

}
#endif /* WISENTHELPERS_HPP */

