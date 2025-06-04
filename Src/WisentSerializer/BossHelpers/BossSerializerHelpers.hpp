/*
 *  The following code is adapted from the BOSS library: 
 *   https://github.com/symbol-store/BOSS/blob/temp_lazy_loading_compression/Source/PortableBOSSSerialization.h
 */

#ifndef PORTABLEBOSSSERIALIZATION_HPP
#define PORTABLEBOSSSERIALIZATION_HPP

#include <cstdint>
#include <cstring>
#include <stdlib.h>
#include <stdint.h>

extern "C" {

// NOLINTBEGIN(hicpp-use-auto,cppcoreguidelines-pro-type-union-access)
// NOLINTBEGIN(cppcoreguidelines-pro-type-cstyle-cast)

//////////////////////////////// Helper Functions ///////////////////////////////

static uint64_t alignTo8Bytes(uint64_t bytes) { return (bytes + (uint64_t)7) & -(uint64_t)8; }

//////////////////////////////// Data Structures ///////////////////////////////

typedef size_t PortableBossString;
typedef size_t PortableBossExpressionIndex;

union PortableBossArgumentValue {
    bool asBool;
    int8_t asChar;
    int16_t asShort;
    int32_t asInt;
    int64_t asLong;
    float asFloat;
    double asDouble;
    PortableBossString asString;
    PortableBossExpressionIndex asExpression;
};

constexpr uint64_t PortableBossArgument_BOOL_SIZE = sizeof(bool);
constexpr uint64_t PortableBossArgument_CHAR_SIZE = sizeof(int8_t);
constexpr uint64_t PortableBossArgument_SHORT_SIZE = sizeof(int16_t);
constexpr uint64_t PortableBossArgument_INT_SIZE = sizeof(int32_t);
constexpr uint64_t PortableBossArgument_LONG_SIZE = sizeof(int64_t);
constexpr uint64_t PortableBossArgument_FLOAT_SIZE = sizeof(float);
constexpr uint64_t PortableBossArgument_DOUBLE_SIZE = sizeof(double);
constexpr uint64_t PortableBossArgument_STRING_SIZE = sizeof(PortableBossString);
constexpr uint64_t PortableBossArgument_EXPRESSION_SIZE = sizeof(PortableBossExpressionIndex);

enum PortableBossArgumentType : uint8_t {
    ARGUMENT_TYPE_BOOL,
    ARGUMENT_TYPE_CHAR,
    ARGUMENT_TYPE_SHORT,
    ARGUMENT_TYPE_INT,
    ARGUMENT_TYPE_LONG,
    ARGUMENT_TYPE_FLOAT,
    ARGUMENT_TYPE_DOUBLE,
    ARGUMENT_TYPE_STRING,
    ARGUMENT_TYPE_SYMBOL,
    ARGUMENT_TYPE_EXPRESSION
};

static uint8_t const PortableBossArgumentType_RLE_MINIMUM_SIZE =
    13; // assuming PortableBossArgumentType ideally stored in 1 byte only,
        // to store RLE-type, need 1 byte to declare the type and 4 bytes to define the length

static uint8_t const PortableBossArgumentType_RLE_BIT =
    0x80; // first bit of PortableBossArgumentType to set RLE on/off

static uint8_t const PortableBossArgumentType_DICT_ENC_BIT =
    0x40; // second bit of PortableBossArgumentType to set Dictionary Encoding on/off

static uint8_t const PortableBossArgumentType_DICT_ENC_SIZE_BIT =
    0x20; // third bit of PortableBossArgumentType, only has semantics when the DICT_ENC_BIT is set
          // 0 indicates the CHAR type is used for the dictionary offset, 1 indicates the INT type
          // needs extension to two bits if finer granularity types are used

static uint8_t const PortableBossArgumentType_MASK =
    0x0F; // used to clear the top 4 bits of an argument type

struct PortableBossExpression {
    uint64_t symbolNameOffset;

    // argument buffer offsets
    uint64_t firstChildOffset; 
    uint64_t lastChildOffset;

    // type buffer offsets
    uint64_t firstChildTypeOffset;
    uint64_t lastChildTypeOffset; 
};

/**
 * A single-allocation representation of an expression, including its arguments (i.e., a flattened
 * array of all arguments, another flattened array of argument types and an array of
 * PortableExpressions to encode the structure)
 */
struct PortableBossRootExpression {
    uint64_t const argumentCount; // if used directly for type bytes, may need to be aligned to 8 bytes
    uint64_t const argumentBytesCount; // if used directly, may need to be aligned to 8 bytes
    uint64_t const expressionCount;
    uint64_t const argumentDictionaryBytesCount;
    void *const originalAddress;
    /**
     * The index of the last used byte in the arguments buffer relative to the pointer returned by
     * getStringBuffer()
     */
    size_t stringBufferBytesWritten;

    /**
     * This buffer holds all data associated with the expression in a single untyped array. As the
     * three kinds of data (ArgumentsValues, ArgumentTypes and Expressions) have different sizes,
     * holding them in an array of unions would waste a lot of memory. A union of variable-sized
     * arrays is not supported in ANSI C. So it is held in an untyped buffer which is essentially a
     * concatenation of the three types of buffers that are required. Utility functions exist to
     * extract the different sub-arrays.
     */
    char arguments[];
};

//////////////////////////////// Part Extraction ///////////////////////////////

// PortableBossRootExpression *getDummySerializedExpression();

static union PortableBossArgumentValue *getExpressionArguments(PortableBossRootExpression *root) 
{
    return reinterpret_cast<PortableBossArgumentValue*>(root->arguments);
}

static PortableBossArgumentType *getArgumentTypes(PortableBossRootExpression *root)
{
    return reinterpret_cast<PortableBossArgumentType *>(&root->arguments[
        alignTo8Bytes(root->argumentBytesCount)]);
        // root->argumentCount * sizeof(union PortableBossArgumentValue)];
}

static PortableBossExpression *getExpressionSubexpressions(PortableBossRootExpression *root)
{
    return reinterpret_cast<PortableBossExpression*>(&root->arguments[
        alignTo8Bytes(root->argumentBytesCount) 
        + alignTo8Bytes(root->argumentCount * sizeof(PortableBossArgumentType))
    ]);
}

static union PortableBossArgumentValue *getSpanDictionaries(PortableBossRootExpression *root)
{
    return reinterpret_cast<PortableBossArgumentValue *>(&root->arguments[
        alignTo8Bytes(root->argumentBytesCount)
        + alignTo8Bytes(root->argumentCount * sizeof(PortableBossArgumentType))
        + root->expressionCount * (sizeof(PortableBossExpression))
    ]);
}

static char *getStringBuffer(PortableBossRootExpression *root)
{
    return reinterpret_cast<char *>(&root->arguments[
        alignTo8Bytes(root->argumentBytesCount)
        + alignTo8Bytes(root->argumentCount * sizeof(PortableBossArgumentType))
        + root->expressionCount * (sizeof(PortableBossExpression))
        + root->argumentDictionaryBytesCount
    ]);
}

//////////////////////////////   Memory Management /////////////////////////////

static PortableBossRootExpression *allocateExpressionTree(
    uint64_t argumentCount,
    uint64_t expressionCount,
    uint64_t stringBytesCount,
    void *(*allocateFunction)(size_t)
) {
    PortableBossRootExpression *root = reinterpret_cast<PortableBossRootExpression *>(allocateFunction( 
        sizeof(PortableBossRootExpression)
        + sizeof(PortableBossArgumentValue) * argumentCount
        + alignTo8Bytes(sizeof(PortableBossArgumentType) * argumentCount)
        + sizeof(PortableBossExpression) * expressionCount
        + stringBytesCount
    ));

    *const_cast<uint64_t*>(&root->argumentCount) = argumentCount;
    *const_cast<uint64_t*>(&root->argumentBytesCount) = argumentCount * sizeof(PortableBossArgumentValue);
    *const_cast<uint64_t*>(&root->expressionCount) = expressionCount;
    *const_cast<uint64_t*>(&root->argumentDictionaryBytesCount) = 0;
    *const_cast<uint64_t*>(&root->stringBufferBytesWritten) = 0;
    *const_cast<void **>(&root->originalAddress) = root;
    return root;
}

static PortableBossRootExpression *allocateExpressionTree(
    uint64_t argumentCount,
    uint64_t argumentBytesCount,
    uint64_t expressionCount,
    uint64_t stringBytesCount,
    void *(*allocateFunction)(size_t)
) {
    PortableBossRootExpression *root = reinterpret_cast<PortableBossRootExpression *>(allocateFunction( 
        sizeof(PortableBossRootExpression) 
        + alignTo8Bytes(argumentBytesCount) 
        + alignTo8Bytes(sizeof(PortableBossArgumentType) * argumentCount) 
        + sizeof(PortableBossExpression) * expressionCount 
        + stringBytesCount
    ));

    *const_cast<uint64_t*>(&root->argumentCount) = argumentCount;
    *const_cast<uint64_t*>(&root->argumentBytesCount) = argumentBytesCount;
    *const_cast<uint64_t*>(&root->expressionCount) = expressionCount;
    *const_cast<uint64_t*>(&root->argumentDictionaryBytesCount) = 0;
    *const_cast<uint64_t*>(&root->stringBufferBytesWritten) = 0;
    *const_cast<void **>(&root->originalAddress) = root;
    return root;
}

static PortableBossRootExpression *allocateExpressionTree(
    uint64_t argumentCount, 
    uint64_t argumentBytesCount,
    uint64_t expressionCount, 
    uint64_t argumentDictionaryBytesCount,
    uint64_t stringBytesCount, 
    void *(*allocateFunction)(size_t)
) {
    PortableBossRootExpression *root = reinterpret_cast<PortableBossRootExpression *>(allocateFunction( 
        sizeof(PortableBossRootExpression) 
        + alignTo8Bytes(argumentBytesCount) 
        + alignTo8Bytes(sizeof(PortableBossArgumentType) * argumentCount) 
        + sizeof(PortableBossExpression) * expressionCount 
        + argumentDictionaryBytesCount
        + stringBytesCount
    ));

    *const_cast<uint64_t*>(&root->argumentCount) = argumentCount;
    *const_cast<uint64_t*>(&root->argumentBytesCount) = argumentBytesCount;
    *const_cast<uint64_t*>(&root->expressionCount) = expressionCount;
    *const_cast<uint64_t*>(&root->argumentDictionaryBytesCount) = argumentDictionaryBytesCount;
    *const_cast<uint64_t*>(&root->stringBufferBytesWritten) = 0;
    *const_cast<void **>(&root->originalAddress) = root;
    return root;
}

static void freeExpressionTree(
    PortableBossRootExpression *root,
    void (*freeFunction)(void *)
) {
    freeFunction(root);
}

/// //////////////////////////////// Argument Creation ///////////////////////////////

static uint64_t *makeArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    return (uint64_t *)&getExpressionArguments(root)[argumentOutputIndex];
};

static bool *makeBoolArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_BOOL;
    return &getExpressionArguments(root)[argumentOutputIndex].asBool;
};

static bool *makeBoolArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex,
    uint64_t typeOutputIndex
) {
    getArgumentTypes(root)[typeOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_BOOL;
    return &getExpressionArguments(root)[argumentOutputIndex].asBool;
};

static void makeBoolArgumentType(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_BOOL;
};

static int8_t *makeCharArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_CHAR;
    return &getExpressionArguments(root)[argumentOutputIndex].asChar;
};

static int8_t *makeCharArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex,
    uint64_t typeOutputIndex
) {
    getArgumentTypes(root)[typeOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_CHAR;
    return &getExpressionArguments(root)[argumentOutputIndex].asChar;
};

static void makeCharArgumentType(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_CHAR;
};

static int16_t *makeShortArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_SHORT;
    return &getExpressionArguments(root)[argumentOutputIndex].asShort;
};

static int16_t *makeShortArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex,
    uint64_t typeOutputIndex
) {
    getArgumentTypes(root)[typeOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_SHORT;
    return &getExpressionArguments(root)[argumentOutputIndex].asShort;
};

static void makeShortArgumentType(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_SHORT;
};

static int32_t *makeIntArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_INT;
    return &getExpressionArguments(root)[argumentOutputIndex].asInt;
};

static int32_t *makeIntArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex,
    uint64_t typeOutputIndex
) {
    getArgumentTypes(root)[typeOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_INT;
    return &getExpressionArguments(root)[argumentOutputIndex].asInt;
};

static void makeIntArgumentType(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_INT;
};

static int64_t *makeLongArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_LONG;
    return &getExpressionArguments(root)[argumentOutputIndex].asLong;
};

static int64_t *makeLongArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex,
    uint64_t typeOutputIndex
) {
    getArgumentTypes(root)[typeOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_LONG;
    return &getExpressionArguments(root)[argumentOutputIndex].asLong;
};

static void makeLongArgumentType(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_LONG;
};

static float *makeFloatArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_FLOAT;
    return &getExpressionArguments(root)[argumentOutputIndex].asFloat;
};

static float *makeFloatArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex,
    uint64_t typeOutputIndex
) {
    getArgumentTypes(root)[typeOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_FLOAT;
    return &getExpressionArguments(root)[argumentOutputIndex].asFloat;
};

static void makeFloatArgumentType(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_FLOAT;
};

static double *makeDoubleArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_DOUBLE;
    return &getExpressionArguments(root)[argumentOutputIndex].asDouble;
};

static double *makeDoubleArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex,
    uint64_t typeOutputIndex
) {
    getArgumentTypes(root)[typeOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_DOUBLE;
    return &getExpressionArguments(root)[argumentOutputIndex].asDouble;
};

static void makeDoubleArgumentType(
    PortableBossRootExpression *root,
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_DOUBLE;
};

static size_t *makeStringArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_STRING;
    return &getExpressionArguments(root)[argumentOutputIndex].asString;
};

static size_t *makeStringArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex,
    uint64_t typeOutputIndex
) {
    getArgumentTypes(root)[typeOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_STRING;
    return &getExpressionArguments(root)[argumentOutputIndex].asString;
};

static void makeStringArgumentType(
    PortableBossRootExpression *root,
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_STRING;
};

static size_t *makeSymbolArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_SYMBOL;
    return &getExpressionArguments(root)[argumentOutputIndex].asString;
};

static size_t *makeSymbolArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex,
    uint64_t typeOutputIndex
) {
    getArgumentTypes(root)[typeOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_SYMBOL;
    return &getExpressionArguments(root)[argumentOutputIndex].asString;
};

static void makeSymbolArgumentType(
    PortableBossRootExpression *root,
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_SYMBOL;
};

static size_t *makeExpressionArgument(
    PortableBossRootExpression *root,
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_EXPRESSION;
    return &getExpressionArguments(root)[argumentOutputIndex].asExpression;
};

static size_t *makeExpressionArgument(
    PortableBossRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint64_t typeOutputIndex
) {
    getArgumentTypes(root)[typeOutputIndex] = PortableBossArgumentType::ARGUMENT_TYPE_EXPRESSION;
    return &getExpressionArguments(root)[argumentOutputIndex].asExpression;
};

static int64_t *makeLongDictionaryEntry(
    PortableBossRootExpression *root,
    uint64_t dictionaryOutputIndex
) {
    return &getSpanDictionaries(root)[dictionaryOutputIndex].asLong;
};

static double *makeDoubleDictionaryEntry(
    PortableBossRootExpression *root,
    uint64_t dictionaryOutputIndex
) {
    return &getSpanDictionaries(root)[dictionaryOutputIndex].asDouble;
};

static size_t *makeStringDictionaryEntry(
    PortableBossRootExpression *root,
    uint64_t dictionaryOutputIndex
) {
    return &getSpanDictionaries(root)[dictionaryOutputIndex].asString;
};

static void setRLEArgumentFlagOrPropagateTypes(
    PortableBossRootExpression *root,
    uint64_t argumentOutputIndex,
    uint32_t size
) {
    if (size < PortableBossArgumentType_RLE_MINIMUM_SIZE) {
        // RLE is not supported, fallback to set the argument types
        enum PortableBossArgumentType const type = getArgumentTypes(root)[argumentOutputIndex];
        for (uint64_t i = argumentOutputIndex + 1; i < argumentOutputIndex + size; ++i) {
            getArgumentTypes(root)[i] = type;
        }
        return;
    }
    PortableBossArgumentType *argTypes = getArgumentTypes(root);
    *reinterpret_cast<uint8_t *>(&argTypes[argumentOutputIndex]) |= PortableBossArgumentType_RLE_BIT;
    *reinterpret_cast<uint8_t *>(&argTypes[argumentOutputIndex + 4]) = (size >> 24) & 0xFF;
    *reinterpret_cast<uint8_t *>(&argTypes[argumentOutputIndex + 3]) = (size >> 16) & 0xFF;
    *reinterpret_cast<uint8_t *>(&argTypes[argumentOutputIndex + 2]) = (size >> 8) & 0xFF;
    *reinterpret_cast<uint8_t *>(&argTypes[argumentOutputIndex + 1]) = size & 0xFF;
}

static void setDictionaryStartAndFlag(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex,
    uint64_t dictionaryOutputIndex, 
    size_t dictArgumentSize
) {
    PortableBossArgumentType *argTypes = getArgumentTypes(root);
    *reinterpret_cast<uint8_t *>(&argTypes[argumentOutputIndex]) |= PortableBossArgumentType_DICT_ENC_BIT;
    *reinterpret_cast<uint8_t *>(&argTypes[argumentOutputIndex]) |= PortableBossArgumentType_DICT_ENC_SIZE_BIT * (dictArgumentSize == PortableBossArgument_INT_SIZE);
    *reinterpret_cast<uint8_t *>(&argTypes[argumentOutputIndex + 12]) = (dictionaryOutputIndex >> 56) & 0xFF;
    *reinterpret_cast<uint8_t *>(&argTypes[argumentOutputIndex + 11]) = (dictionaryOutputIndex >> 48) & 0xFF;
    *reinterpret_cast<uint8_t *>(&argTypes[argumentOutputIndex + 10]) = (dictionaryOutputIndex >> 40) & 0xFF;
    *reinterpret_cast<uint8_t *>(&argTypes[argumentOutputIndex + 9]) = (dictionaryOutputIndex >> 32) & 0xFF;
    *reinterpret_cast<uint8_t *>(&argTypes[argumentOutputIndex + 8]) = (dictionaryOutputIndex >> 24) & 0xFF;
    *reinterpret_cast<uint8_t *>(&argTypes[argumentOutputIndex + 7]) = (dictionaryOutputIndex >> 16) & 0xFF;
    *reinterpret_cast<uint8_t *>(&argTypes[argumentOutputIndex + 6]) = (dictionaryOutputIndex >> 8) & 0xFF;
    *reinterpret_cast<uint8_t *>(&argTypes[argumentOutputIndex + 5]) = dictionaryOutputIndex & 0xFF;
}

static int8_t *makeCharArgumentsRun(
    PortableBossRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint32_t size
) {
    int8_t *value = makeCharArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

static int16_t *makeShortArgumentsRun(
    PortableBossRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint32_t size
) {
    int16_t *value = makeShortArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

static int32_t *makeIntArgumentsRun(
    PortableBossRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint32_t size
) {
    int32_t *value = makeIntArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

static int64_t *makeLongArgumentsRun(
    PortableBossRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint32_t size
) {
    int64_t *value = makeLongArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

static float *makeFloatArgumentsRun(
    PortableBossRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint32_t size
) {
    float *value = makeFloatArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

static double *makeDoubleArgumentsRun(
    PortableBossRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint32_t size
) {
    double *value = makeDoubleArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

static size_t *makeStringArgumentsRun(
    PortableBossRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint64_t size
) {
    size_t *value = makeStringArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

static size_t *makeSymbolArgumentsRun(
    PortableBossRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint32_t size
) {
    size_t *value = makeSymbolArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

static size_t *makeExpressionArgumentsRun(
    PortableBossRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint64_t size
) {
    size_t *value = makeExpressionArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

static PortableBossExpression *makeExpression(
    PortableBossRootExpression *root,
    uint64_t expressionOutputIndex
) {
    return &getExpressionSubexpressions(root)[expressionOutputIndex];
}

static size_t storeString(
    PortableBossRootExpression **root, 
    char const *inputString
) {
    size_t const inputStringLength = strlen(inputString);
    char* destination = getStringBuffer(*root) + (*root)->stringBufferBytesWritten;
    strncpy(
        destination,
        inputString, 
        inputStringLength + 1
    );
    (*root)->stringBufferBytesWritten += inputStringLength + 1;
    return destination - getStringBuffer(*root);
};

static size_t storeStringReallocation(
    PortableBossRootExpression **root,
    char const *inputString,
    void *(*reallocateFunction)(void *, size_t)
) {
    size_t const inputStringLength = strlen(inputString);
    char *stringBufferStart = getStringBuffer(*root); 
    
    *root = reinterpret_cast<PortableBossRootExpression *>(reallocateFunction(
        *root, 
        (stringBufferStart - reinterpret_cast<char*>(*root))
            + (*root)->stringBufferBytesWritten 
            + inputStringLength + 1
    ));

    char* destination = stringBufferStart + (*root)->stringBufferBytesWritten;
    strncpy(
        destination,
        inputString, 
        inputStringLength + 1
    );
    (*root)->stringBufferBytesWritten += inputStringLength + 1;
    return destination - stringBufferStart;
};

static char const *viewString(
    PortableBossRootExpression *root, 
    size_t inputStringOffset
) {
    return getStringBuffer(root) + inputStringOffset;
};

PortableBossRootExpression *serializeBOSSExpression(struct BOSSExpression *expression);
struct BOSSExpression *deserializeBOSSExpression(PortableBossRootExpression *root);
struct BOSSExpression *parseURL(char const *url);
}

// NOLINTEND(hicpp-use-auto,cppcoreguidelines-pro-type-union-access)
// NOLINTBEGIN(cppcoreguidelines-pro-type-cstyle-cast)

#endif /* PORTABLEBOSSSERIALIZATION_HPP */