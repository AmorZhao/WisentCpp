#ifndef WISENTHELPERS_H
#define WISENTHELPERS_H
#ifdef __cplusplus
#include <cstring>
extern "C" {
#else
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#endif
// NOLINTBEGIN(hicpp-use-auto,cppcoreguidelines-pro-type-union-access)
// NOLINTBEGIN(cppcoreguidelines-pro-type-cstyle-cast)

#include <stdlib.h>
#include <stdint.h>

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

//////////////////////////////   Memory Management /////////////////////////////

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

static void freeExpressionTree(   // ToDo: this is not being called? 
    struct WisentRootExpression *root,
    void (*freeFunction)(void *))
{
    freeFunction(root); 
}

static int64_t *makeLongArgument(
    struct WisentRootExpression *root,
    uint64_t argumentOutputIndex)
{
#ifdef __cplusplus
    auto ARGUMENT_TYPE_LONG = WisentArgumentType::ARGUMENT_TYPE_LONG;
#endif
    getArgumentTypes(root)[argumentOutputIndex] = ARGUMENT_TYPE_LONG;
    return &getExpressionArguments(root)[argumentOutputIndex].asLong;
};

static size_t *makeSymbolArgument(
    struct WisentRootExpression *root,
    uint64_t argumentOutputIndex)
{
#ifdef __cplusplus
    auto ARGUMENT_TYPE_SYMBOL = WisentArgumentType::ARGUMENT_TYPE_SYMBOL;
#endif
    getArgumentTypes(root)[argumentOutputIndex] = ARGUMENT_TYPE_SYMBOL;
    return &getExpressionArguments(root)[argumentOutputIndex].asString;
};

static size_t *makeExpressionArgument(
    struct WisentRootExpression *root,
    uint64_t argumentOutputIndex)
{
#ifdef __cplusplus
    auto ARGUMENT_TYPE_SYMBOL = WisentArgumentType::ARGUMENT_TYPE_EXPRESSION;
#endif
    getArgumentTypes(root)[argumentOutputIndex] = ARGUMENT_TYPE_EXPRESSION;
    return &getExpressionArguments(root)[argumentOutputIndex].asString;
};

static size_t *makeStringArgument(
    struct WisentRootExpression *root,
    uint64_t argumentOutputIndex)
{
#ifdef __cplusplus
    auto ARGUMENT_TYPE_STRING = WisentArgumentType::ARGUMENT_TYPE_STRING;
#endif
    getArgumentTypes(root)[argumentOutputIndex] = ARGUMENT_TYPE_STRING;
    return &getExpressionArguments(root)[argumentOutputIndex].asString;
};

static double *makeDoubleArgument(
    struct WisentRootExpression *root,
    uint64_t argumentOutputIndex)
{
#ifdef __cplusplus
    auto ARGUMENT_TYPE_DOUBLE = WisentArgumentType::ARGUMENT_TYPE_DOUBLE;
#endif
    getArgumentTypes(root)[argumentOutputIndex] = ARGUMENT_TYPE_DOUBLE;
    return &getExpressionArguments(root)[argumentOutputIndex].asDouble;
};

/////////////////////////////// Encoding Helpers ///////////////////////////////

static void deltaEncode(const int64_t *values, uint64_t count, int64_t *output) 
{
    if (count == 0) return;
    output[0] = values[0]; 
    for (uint64_t i = 1; i < count; ++i) 
    {
        output[i] = values[i] - values[i - 1]; 
    }
}

static void deltaDecode(const int64_t *encoded, uint64_t count, int64_t *output) 
{
    if (count == 0) return;
    output[0] = encoded[0]; 
    for (uint64_t i = 1; i < count; ++i) 
    {
        output[i] = output[i - 1] + encoded[i]; 
    }
}

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
    (*(size_t*)(&getArgumentTypes(root)[argumentOutputIndex])) |= WisentArgumentType_RLE_BIT;
    (*(size_t*)(&getArgumentTypes(root)[argumentOutputIndex + 1])) = (size_t)size;
}

static void setDeltaEncodedFlagOrPropagateTypes(
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
    (*(size_t*)(&getArgumentTypes(root)[argumentOutputIndex])) |= 
        WisentArgumentType_RLE_BIT | WisentArgumentType_DELTA_ENCODED_BIT;
    (*(size_t*)(&getArgumentTypes(root)[argumentOutputIndex + 1])) = (size_t)size;
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

static int64_t *makeDeltaEncodedLongArgumentsRun(
    struct WisentRootExpression *root,
    uint64_t argumentOutputIndex,
    uint32_t size)
{
    int64_t *value = makeLongArgument(root, argumentOutputIndex);
    int64_t *originalValues = &getExpressionArguments(root)[argumentOutputIndex].asLong;
    deltaEncode(originalValues, size, value);
    setDeltaEncodedFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

static struct WisentExpression *makeExpression(
    struct WisentRootExpression *root, 
    uint64_t expressionOutputI)
{
    return &getExpressionSubexpressions(root)[expressionOutputI];
}

static size_t storeString(
    struct WisentRootExpression **root,
    char const *inputString,
    void *(*reallocateFunction)(void *, size_t))
{
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

static char const *viewString(
    struct WisentRootExpression *root,
    size_t inputStringOffset)
{
    return getStringBuffer(root) + inputStringOffset;
};

#ifdef __cplusplus
}
#endif
// NOLINTEND(hicpp-use-auto,cppcoreguidelines-pro-type-union-access)
// NOLINTEND(cppcoreguidelines-pro-type-cstyle-cast)

#endif /* WISENTHELPERS_H */
