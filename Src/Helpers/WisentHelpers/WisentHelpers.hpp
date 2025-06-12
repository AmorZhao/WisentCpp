#ifndef WISENTHELPERS_HPP
#define WISENTHELPERS_HPP

#include <cstring>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <iostream>

extern "C" {

// NOLINTBEGIN(hicpp-use-auto,cppcoreguidelines-pro-type-union-access)
// NOLINTBEGIN(cppcoreguidelines-pro-type-cstyle-cast)

//////////////////////////////// Helper Functions ///////////////////////////////

static uint64_t alignTo8Bytes(uint64_t bytes) { return (bytes + (uint64_t)7) & -(uint64_t)8; }

//////////////////////////////// Data Structures ///////////////////////////////

typedef size_t WisentString;
typedef size_t WisentExpressionIndex;

union WisentArgumentValue {
    bool asBool;
    int8_t asChar;
    int16_t asShort;
    int32_t asInt;
    int64_t asLong;
    float asFloat;
    double asDouble;
    WisentString asString;
    WisentExpressionIndex asExpression;
};

constexpr uint64_t PortableBossArgument_BOOL_SIZE = sizeof(bool);
constexpr uint64_t PortableBossArgument_CHAR_SIZE = sizeof(int8_t);
constexpr uint64_t PortableBossArgument_SHORT_SIZE = sizeof(int16_t);
constexpr uint64_t PortableBossArgument_INT_SIZE = sizeof(int32_t);
constexpr uint64_t PortableBossArgument_LONG_SIZE = sizeof(int64_t);
constexpr uint64_t PortableBossArgument_FLOAT_SIZE = sizeof(float);
constexpr uint64_t PortableBossArgument_DOUBLE_SIZE = sizeof(double);
constexpr uint64_t PortableBossArgument_STRING_SIZE = sizeof(WisentString);
constexpr uint64_t PortableBossArgument_EXPRESSION_SIZE = sizeof(WisentExpressionIndex);

enum WisentArgumentType : size_t {
    ARGUMENT_TYPE_BOOL,         // 0
    ARGUMENT_TYPE_CHAR,         // 1
    ARGUMENT_TYPE_SHORT,        // 2
    ARGUMENT_TYPE_INT,          // 3
    ARGUMENT_TYPE_LONG,         // 4
    ARGUMENT_TYPE_FLOAT,        // 5
    ARGUMENT_TYPE_DOUBLE,       // 6
    ARGUMENT_TYPE_STRING,       // 7
    ARGUMENT_TYPE_SYMBOL,       // 8
    ARGUMENT_TYPE_EXPRESSION,   // 9
    ARGUMENT_TYPE_BYTE_ARRAY    // 10
};

static size_t const WisentArgumentType_RLE_MINIMUM_SIZE =
    5; // assuming WisentArgumentType ideally stored in 1 byte only,
       // to store RLE-type, need 1 byte to declare the type and 4 bytes to
       // define the length

static size_t const WisentArgumentType_RLE_BIT =
    0x80; // first bit of WisentArgumentType to set RLE on/off

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

struct WisentExpression {
    /*
     * The index offset in the string buffer
     * (relative to the start of the string buffer)
     * where the expression head's name (string) is stored. 
    */
    uint64_t symbolNameOffset;

    // The index of the first child in the arguments buffer
    uint64_t firstChildOffset;

    // The index of the last child in the arguments buffer
    uint64_t lastChildOffset;
};

/**
 * A single-allocation representation of an expression, including its arguments
 * (i.e., a flattened array of all arguments, another flattened array of
 * argument types and an array of PortableExpressions to encode the structure)
 */
struct WisentRootExpression 
{
    uint64_t const argumentCount;
    uint64_t const expressionCount;
    void *const originalAddress;
    /**
     * The index of the last used byte in the arguments buffer 
     * (relative to the char* returned by getStringBuffer() )
     */
    size_t stringBufferBytesWritten;

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
struct PortableBossRootExpression 
{
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

/***************************************************************/
/*                                                             */
/*                  WisentRootExpression Flat Layout           */
/*                                                             */
/* ┌───────────────────────────────────────────────────────┐   */
/* │                 (header fields)                       │   */
/* ├───────────────────────────────────────────────────────┤   */
/* │   argumentCount (uint64_t)                            │   */
/* │   expressionCount (uint64_t)                          │   */
/* │   originalAddress (void*)                             │   */
/* │   stringBufferBytesWritten (size_t)                   │   */
/* │                                                       │   */
/* ├───────────────────────────────────────────────────────┤                                 */
/* │                    arguments[]                        │  Part Extraction functions:     */
/* │                                                       │   each gives the first          */
/* │ ┌─────────────────────────────────────────────────┐   │   char* in the buffer           */
/* │ │ Argument Values:                                │                                     */
/* │ │   [WisentArgumentValue x argumentCount]         │◄──── getArgumentsBuffer(root)       */
/* │ └─────────────────────────────────────────────────┘                                     */
/* │ ┌─────────────────────────────────────────────────┐                                     */
/* │ │ Argument Types:                                 │                                     */
/* │ │   [WisentArgumentType x argumentCount]          │◄───── getArgumentTypesBuffer(root)  */
/* │ └─────────────────────────────────────────────────┘                                     */
/* │ ┌─────────────────────────────────────────────────┐                                     */
/* │ │ Expressions (subtree structure):                │                                     */
/* │ │   [WisentExpression x expressionCount]          │◄───── getSubexpressionsBuffer(root) */
/* │ └─────────────────────────────────────────────────┘                                     */
/* │ ┌─────────────────────────────────────────────────┐                                     */
/* │ │ String Buffer:                                  │                                     */
/* │ │   [stringBufferBytesWritten]                    │◄───── getStringBuffer(root)         */
/* │ └─────────────────────────────────────────────────┘                                     */
/* └───────────────────────────────────────────────────────┘   */
/*                                                             */
/***************************************************************/

//////////////////////////////// Part Extraction ///////////////////////////////

WisentRootExpression* getDummySerializedExpression();

inline WisentArgumentValue* getArgumentsBuffer(WisentRootExpression* root) 
{
    return reinterpret_cast<WisentArgumentValue*>(root->arguments);
}

inline WisentArgumentType* getArgumentTypesBuffer(WisentRootExpression* root) 
{
    return reinterpret_cast<WisentArgumentType*>(&root->arguments[
        root->argumentCount * sizeof(WisentArgumentValue)]);
}

inline WisentExpression* getSubexpressionsBuffer(WisentRootExpression* root) 
{
    return reinterpret_cast<WisentExpression*>(&root->arguments[
        root->argumentCount * (sizeof(WisentArgumentValue) + sizeof(WisentArgumentType))]);
}

inline char* getStringBuffer(WisentRootExpression* root) 
{
    return reinterpret_cast<char*>(&root->arguments[
        root->argumentCount * (sizeof(WisentArgumentValue) + sizeof(WisentArgumentType)) 
            + root->expressionCount * sizeof(WisentExpression)]);
}

/***************************************************************/
/*                                                             */
/*            PortableBossRootExpression Flat Layout           */
/*                                                             */
/* ┌───────────────────────────────────────────────────────┐   */
/* │                 (header fields)                       │   */
/* ├───────────────────────────────────────────────────────┤   */
/* │   argumentCount (uint64_t)                            │   */
/* │   argumentBytesCount (uint64_t)                       │   */
/* │   expressionCount (uint64_t)                          │   */
/* │   argumentDictionaryBytesCount (uint64_t)             │   */
/* │   originalAddress (void*)                             │   */
/* │   stringBufferBytesWritten (size_t)                   │   */
/* ├───────────────────────────────────────────────────────┤   */
/* │                    arguments[]                        │   */
/* │                                                       │                                 */
/* │ ┌─────────────────────────────────────────────────┐   │  Part Extraction functions:     */
/* │ │ Argument Values:                                │   │                                 */
/* │ │   [WisentArgumentValue x argumentCount]         │◄──┼── getExpressionArguments(root)  */
/* │ └─────────────────────────────────────────────────┘   │                                 */
/* │ ┌─────────────────────────────────────────────────┐   │                                 */
/* │ │ Argument Types:                                 │   │                                 */
/* │ │   [WisentArgumentType x argumentCount]          │◄──┼── getArgumentTypes(root)        */
/* │ └─────────────────────────────────────────────────┘   │                                 */
/* │ ┌─────────────────────────────────────────────────┐   │                                 */
/* │ │ Expressions (subtree structure):                │   │                                 */
/* │ │   [PortableBossExpression x expressionCount]    │◄──┼── getExpressionSubexpressions(root) */
/* │ └─────────────────────────────────────────────────┘   │                                 */
/* │ ┌─────────────────────────────────────────────────┐   │                                 */
/* │ │ Span Dictionaries:                              │   │                                 */
/* │ │   [WisentArgumentValue x dictEntryCount]        │◄──┼── getSpanDictionaries(root)     */
/* │ └─────────────────────────────────────────────────┘   │                                 */
/* │ ┌─────────────────────────────────────────────────┐   │                                 */
/* │ │ String Buffer:                                  │   │                                 */
/* │ │   [stringBufferBytesWritten]                    │◄──┼── getStringBuffer(root)         */
/* │ └─────────────────────────────────────────────────┘   │                                 */
/* └───────────────────────────────────────────────────────┘   */
/*                                                             */
/***************************************************************/

static union WisentArgumentValue *getExpressionArguments(PortableBossRootExpression *root) 
{
    return reinterpret_cast<WisentArgumentValue*>(root->arguments);
}

static WisentArgumentType *getArgumentTypes(PortableBossRootExpression *root)
{
    return reinterpret_cast<WisentArgumentType *>(&root->arguments[
        alignTo8Bytes(root->argumentBytesCount)]);
        // root->argumentCount * sizeof(WisentArgumentValue)];
}

static PortableBossExpression *getExpressionSubexpressions(PortableBossRootExpression *root)
{
    return reinterpret_cast<PortableBossExpression*>(&root->arguments[
        alignTo8Bytes(root->argumentBytesCount) 
        + alignTo8Bytes(root->argumentCount * sizeof(WisentArgumentType))
    ]);
}

static union WisentArgumentValue *getSpanDictionaries(PortableBossRootExpression *root)
{
    return reinterpret_cast<WisentArgumentValue *>(&root->arguments[
        alignTo8Bytes(root->argumentBytesCount)
        + alignTo8Bytes(root->argumentCount * sizeof(WisentArgumentType))
        + root->expressionCount * (sizeof(PortableBossExpression))
    ]);
}

static char *getStringBuffer(PortableBossRootExpression *root)
{
    return reinterpret_cast<char *>(&root->arguments[
        alignTo8Bytes(root->argumentBytesCount)
        + alignTo8Bytes(root->argumentCount * sizeof(WisentArgumentType))
        + root->expressionCount * (sizeof(PortableBossExpression))
        + root->argumentDictionaryBytesCount
    ]);
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
    *const_cast<size_t*>(&root->stringBufferBytesWritten) = 0;
    *const_cast<void**>(&root->originalAddress) = root;
    return root;
}

inline void freeExpressionTree(
    WisentRootExpression* root, 
    void (*freeFunction)(void *)
) {
    freeFunction(root);
}

static PortableBossRootExpression *allocateExpressionTree(
    uint64_t argumentCount,
    uint64_t expressionCount,
    uint64_t stringBytesCount,
    void *(*allocateFunction)(size_t)
) {
    PortableBossRootExpression *root = reinterpret_cast<PortableBossRootExpression *>(allocateFunction( 
        sizeof(PortableBossRootExpression)
        + sizeof(WisentArgumentValue) * argumentCount
        + alignTo8Bytes(sizeof(WisentArgumentType) * argumentCount)
        + sizeof(PortableBossExpression) * expressionCount
        + stringBytesCount
    ));

    *const_cast<uint64_t*>(&root->argumentCount) = argumentCount;
    *const_cast<uint64_t*>(&root->argumentBytesCount) = argumentCount * sizeof(WisentArgumentValue);
    *const_cast<uint64_t*>(&root->expressionCount) = expressionCount;
    *const_cast<uint64_t*>(&root->argumentDictionaryBytesCount) = 0;
    *const_cast<uint64_t*>(&root->stringBufferBytesWritten) = 0;
    *const_cast<void **>(&root->originalAddress) = root;
    return root;
}

// includes argumentBytesCount
// (for compact Portable Boss Expressions with bit packing)
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
        + alignTo8Bytes(sizeof(WisentArgumentType) * argumentCount) 
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

// includes argumentBytesCount and argumentDictionaryBytesCount
// (for compact Portable Boss Expressions with bit packing)
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
        + alignTo8Bytes(sizeof(WisentArgumentType) * argumentCount) 
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

/*****************************************************************/
/*  make(TYPE)Argument()                                         */
/*                                                               */
/*  Parameters:                                                  */
/*      - WisentRootExpression *root                             */
/*      - uint64_t argumentOutputIndex                           */
/*               (index of the new argument）                    */
/*                                                               */
/*  Operations:                                                  */
/*      (Stores TYPE in the type buffer at the specified index)  */
/*      getArgumentTypesBuffer(root)[argumentOutputIndex]        */
/*                 = WisentArgumentType::TYPE;                   */
/*                                                               */
/*  Returns:                                                     */
/*      (a TYPE* pointer to the argument in the arguments buffer */
/*         , casted to the appropriate type)                     */
/*      return &getArgumentsBuffer(root)[argumentOutputIndex].asTYPE; */
/*                                                               */
/*****************************************************************/


inline int64_t *makeLongArgument(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex
) {
    getArgumentTypesBuffer(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_LONG;
    return &getArgumentsBuffer(root)[argumentOutputIndex].asLong;
};

inline size_t *makeSymbolArgument(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex
) {
    getArgumentTypesBuffer(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_SYMBOL;
    return &getArgumentsBuffer(root)[argumentOutputIndex].asString;
};

inline size_t *makeExpressionArgument(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex
) {
    getArgumentTypesBuffer(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_EXPRESSION;
    return &getArgumentsBuffer(root)[argumentOutputIndex].asString;
};

inline size_t *makeStringArgument(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex
) {
    getArgumentTypesBuffer(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_STRING;
    return &getArgumentsBuffer(root)[argumentOutputIndex].asString;
};

inline double *makeDoubleArgument(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex
) {
    getArgumentTypesBuffer(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_DOUBLE;
    return &getArgumentsBuffer(root)[argumentOutputIndex].asDouble;
};

inline size_t *makeByteArrayArgument(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex
) {
    getArgumentTypesBuffer(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_BYTE_ARRAY;
    return &getArgumentsBuffer(root)[argumentOutputIndex].asString;
};

/*****************************************************************/
/*  make(TYPE)Argument()                                         */
/*                                                               */
/*  make(TYPE)ArgumentTYPE()                                     */
/*                                                               */
/*****************************************************************/

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
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_BOOL;
    return &getExpressionArguments(root)[argumentOutputIndex].asBool;
};

static bool *makeBoolArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex,
    uint64_t typeOutputIndex
) {
    getArgumentTypes(root)[typeOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_BOOL;
    return &getExpressionArguments(root)[argumentOutputIndex].asBool;
};

static void makeBoolArgumentType(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_BOOL;
};

static int8_t *makeCharArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_CHAR;
    return &getExpressionArguments(root)[argumentOutputIndex].asChar;
};

static int8_t *makeCharArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex,
    uint64_t typeOutputIndex
) {
    getArgumentTypes(root)[typeOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_CHAR;
    return &getExpressionArguments(root)[argumentOutputIndex].asChar;
};

static void makeCharArgumentType(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_CHAR;
};

static int16_t *makeShortArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_SHORT;
    return &getExpressionArguments(root)[argumentOutputIndex].asShort;
};

static int16_t *makeShortArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex,
    uint64_t typeOutputIndex
) {
    getArgumentTypes(root)[typeOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_SHORT;
    return &getExpressionArguments(root)[argumentOutputIndex].asShort;
};

static void makeShortArgumentType(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_SHORT;
};

static int32_t *makeIntArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_INT;
    return &getExpressionArguments(root)[argumentOutputIndex].asInt;
};

static int32_t *makeIntArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex,
    uint64_t typeOutputIndex
) {
    getArgumentTypes(root)[typeOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_INT;
    return &getExpressionArguments(root)[argumentOutputIndex].asInt;
};

static void makeIntArgumentType(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_INT;
};

static int64_t *makeLongArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_LONG;
    return &getExpressionArguments(root)[argumentOutputIndex].asLong;
};

static int64_t *makeLongArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex,
    uint64_t typeOutputIndex
) {
    getArgumentTypes(root)[typeOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_LONG;
    return &getExpressionArguments(root)[argumentOutputIndex].asLong;
};

static void makeLongArgumentType(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_LONG;
};

static float *makeFloatArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_FLOAT;
    return &getExpressionArguments(root)[argumentOutputIndex].asFloat;
};

static float *makeFloatArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex,
    uint64_t typeOutputIndex
) {
    getArgumentTypes(root)[typeOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_FLOAT;
    return &getExpressionArguments(root)[argumentOutputIndex].asFloat;
};

static void makeFloatArgumentType(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_FLOAT;
};

static double *makeDoubleArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_DOUBLE;
    return &getExpressionArguments(root)[argumentOutputIndex].asDouble;
};

static double *makeDoubleArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex,
    uint64_t typeOutputIndex
) {
    getArgumentTypes(root)[typeOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_DOUBLE;
    return &getExpressionArguments(root)[argumentOutputIndex].asDouble;
};

static void makeDoubleArgumentType(
    PortableBossRootExpression *root,
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_DOUBLE;
};

static size_t *makeStringArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_STRING;
    return &getExpressionArguments(root)[argumentOutputIndex].asString;
};

static size_t *makeStringArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex,
    uint64_t typeOutputIndex
) {
    getArgumentTypes(root)[typeOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_STRING;
    return &getExpressionArguments(root)[argumentOutputIndex].asString;
};

static void makeStringArgumentType(
    PortableBossRootExpression *root,
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_STRING;
};

static size_t *makeSymbolArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_SYMBOL;
    return &getExpressionArguments(root)[argumentOutputIndex].asString;
};

static size_t *makeSymbolArgument(
    PortableBossRootExpression *root, 
    uint64_t argumentOutputIndex,
    uint64_t typeOutputIndex
) {
    getArgumentTypes(root)[typeOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_SYMBOL;
    return &getExpressionArguments(root)[argumentOutputIndex].asString;
};

static void makeSymbolArgumentType(
    PortableBossRootExpression *root,
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_SYMBOL;
};

static size_t *makeExpressionArgument(
    PortableBossRootExpression *root,
    uint64_t argumentOutputIndex
) {
    getArgumentTypes(root)[argumentOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_EXPRESSION;
    return &getExpressionArguments(root)[argumentOutputIndex].asExpression;
};

static size_t *makeExpressionArgument(
    PortableBossRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint64_t typeOutputIndex
) {
    getArgumentTypes(root)[typeOutputIndex] = WisentArgumentType::ARGUMENT_TYPE_EXPRESSION;
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

/////////////////////////////// Encoding Helpers ///////////////////////////////

/// TODO: revisit

static void setRLEArgumentFlagOrPropagateTypes(   
    WisentRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint64_t size
) {
    if (size < WisentArgumentType_RLE_MINIMUM_SIZE) 
    {
        WisentArgumentType const type = getArgumentTypesBuffer(root)[argumentOutputIndex];
        for (uint64_t i = argumentOutputIndex + 1; i < argumentOutputIndex + size; ++i) 
        {
            getArgumentTypesBuffer(root)[i] = type;
        }
        return;
    }

    WisentArgumentType *types = getArgumentTypesBuffer(root);
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
    uint64_t size
) {
    int64_t *value = makeLongArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

inline size_t *makeSymbolArgumentsRun(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint64_t size
) {
    size_t *value = makeSymbolArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

inline size_t *makeExpressionArgumentsRun(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex,
    uint64_t size
) {
    size_t *value = makeExpressionArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

inline size_t *makeStringArgumentsRun(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint64_t size
) {
    size_t *value = makeStringArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

inline double *makeDoubleArgumentsRun(
    WisentRootExpression *root,
    uint64_t argumentOutputIndex, 
    uint64_t size
) {
    double *value = makeDoubleArgument(root, argumentOutputIndex);
    setRLEArgumentFlagOrPropagateTypes(root, argumentOutputIndex, size);
    return value;
}

inline WisentExpression *makeExpression(
    WisentRootExpression *root, 
    uint64_t argumentOutputIndex
) {
    return &getSubexpressionsBuffer(root)[argumentOutputIndex];
}


static void setRLEArgumentFlagOrPropagateTypes(
    PortableBossRootExpression *root,
    uint64_t argumentOutputIndex,
    uint32_t size
) {
    if (size < PortableBossArgumentType_RLE_MINIMUM_SIZE) {
        // RLE is not supported, fallback to set the argument types
        enum WisentArgumentType const type = getArgumentTypes(root)[argumentOutputIndex];
        for (uint64_t i = argumentOutputIndex + 1; i < argumentOutputIndex + size; ++i) {
            getArgumentTypes(root)[i] = type;
        }
        return;
    }
    WisentArgumentType *argTypes = getArgumentTypes(root);
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
    WisentArgumentType *argTypes = getArgumentTypes(root);
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

/*******************************************************************************************/
/*                                                                                         */
/*  Before storeString():                                                                  */
/*                                                                                         */
/*  <-- pre allocated 3 buffers -->                                                        */
/*  +-----------+-------+---------+-------------------------+                              */
/*  | Arguments | Types | Subexpr |      String Buffer      |                              */
/*  +-----------+-------+---------+-------------------------+                              */
/*                                ^                         ^                              */
/*                                |                         |                              */
/*                        stringBufferStart    stringBufferBytesWritten                    */
/*                                                                                         */
/*  After reallocation:                                                                    */
/*                                                                                         */
/*  +-----------+-------+---------+-------------------------+-------------------+--+       */
/*  | Arguments | Types | Subexpr |      String Buffer      | inputStringLength |\0|       */
/*  +-----------+-------+---------+-------------------------+-------------------+--+       */
/*                                ^                         ^               (terminator)   */
/*                                |                         |                              */
/*                        stringBufferStart    stringBufferBytesWritten                    */
/*                                                                                         */
/*  After strncpy and stringBufferBytesWritten updating:                                   */
/*                                                                                         */
/*  +-----------+-------+---------+-------------------------+-------------------+--+       */
/*  | Arguments | Types | Subexpr |      String Buffer      |   inputString     |\0|       */
/*  +-----------+-------+---------+-------------------------+-------------------+--+       */
/*                                ^                         ^                      ^       */
/*                                |                         |                      |       */
/*                        stringBufferStart           destination             Updated      */
/*                                |                         |     stringBufferBytesWritten */
/*                                |                         |                              */
/*                                |                         |                              */
/*                                |                         |                              */
/*  Returned offset:              <--------- offset -------->                              */
/*         （this offset then gets stored in the arguments buffer)                         */
/*                                                                                         */
/*******************************************************************************************/

inline size_t storeString(
    WisentRootExpression **root,
    char const *inputString,
    void *(*reallocateFunction)(void *, size_t)
) {
    const size_t inputStringLength = strlen(inputString);
    char *stringBufferStart = getStringBuffer(*root); 

    *root = reinterpret_cast<WisentRootExpression*>(reallocateFunction(
        *root, 
        (stringBufferStart - reinterpret_cast<char*>(*root))   // everything before string buffer
            + (*root)->stringBufferBytesWritten                // current length of string buffer
            + inputStringLength + 1                            // new string length + 1 for terminator
    ));

    stringBufferStart = getStringBuffer(*root); // update start after reallocation
    
    char* destination = stringBufferStart + (*root)->stringBufferBytesWritten;
    strncpy(
        destination,
        inputString, 
        inputStringLength + 1
    );

    (*root)->stringBufferBytesWritten += inputStringLength + 1;
    return destination - stringBufferStart;  // offset 
}

// same as storeString(), but for byte arrays
inline size_t storeBytes(
    WisentRootExpression **root,
    const std::vector<uint8_t> &inputBytes,
    void *(*reallocateFunction)(void *, size_t)
) {
    const size_t inputBytesLength = inputBytes.size();
    char *stringBufferStart = getStringBuffer(*root); 

    *root = reinterpret_cast<WisentRootExpression*>(reallocateFunction(
        *root,
        (stringBufferStart - reinterpret_cast<char*>(*root))  // everything before string buffer
            + (*root)->stringBufferBytesWritten               // current length of string buffer
            + inputBytesLength + 1                            // new bytes length + terminator
    ));

    stringBufferStart = getStringBuffer(*root);     // update start after reallocation

    char *destination = stringBufferStart + (*root)->stringBufferBytesWritten;
    memcpy(
        destination, 
        inputBytes.data(), 
        inputBytesLength
    );
    destination[inputBytesLength] = '\0';  // manually add null terminator

    (*root)->stringBufferBytesWritten += inputBytesLength + 1;
    return destination - stringBufferStart;  // offset 
}

inline const char* viewString(
    WisentRootExpression *root,
    size_t inputStringOffset
) {
    return getStringBuffer(root) + inputStringOffset;
};

// Portable Boss version of storeString, storeBytes and viewString

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

static size_t storeString(
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


// NOLINTEND(hicpp-use-auto,cppcoreguidelines-pro-type-union-access)
// NOLINTEND(cppcoreguidelines-pro-type-cstyle-cast)

}

#endif /* WISENTHELPERS_HPP */

