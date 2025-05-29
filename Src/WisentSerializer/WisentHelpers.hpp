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
    ARGUMENT_TYPE_BYTE_ARRAY
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

/******************************************************************************/
/*                                                                            */
/*                  WisentRootExpression Flat Layout                          */
/*                                                                            */
/* ┌──────────────────────────────────────────────────────────────────────┐   */
/* │                         (header fields)                              │   */
/* ├──────────────────────────────────────────────────────────────────────┤   */
/* │   argumentCount (uint64_t)                                           │   */
/* │   expressionCount (uint64_t)                                         │   */
/* │   originalAddress (void*)                                            │   */
/* │   stringBufferBytesWritten (size_t) ← Indicates how much of the      │   */
/* │                                         string buffer is used        │   */
/* ├──────────────────────────────────────────────────────────────────────┤                                 */
/* │                            arguments[]                               │  Part Extraction functions:     */
/* │                                                                      │   each gives the first          */
/* │ ┌────────────────────────────────────────────────────────────────┐   │   char* in the buffer           */
/* │ │ Argument Values:                                               │                                     */
/* │ │   [WisentArgumentValue x argumentCount]                        │◄──── getArgumentsBuffer(root)       */
/* │ └────────────────────────────────────────────────────────────────┘                                     */
/* │ ┌────────────────────────────────────────────────────────────────┐                                     */
/* │ │ Argument Types:                                                │                                     */
/* │ │   [WisentArgumentType x argumentCount]                         │◄───── getArgumentTypesBuffer(root)  */
/* │ └────────────────────────────────────────────────────────────────┘                                     */
/* │ ┌────────────────────────────────────────────────────────────────┐                                     */
/* │ │ Expressions (subtree structure):                               │                                     */
/* │ │   [WisentExpression x expressionCount]                         │◄───── getSubexpressionsBuffer(root) */
/* │ └────────────────────────────────────────────────────────────────┘                                     */
/* │ ┌────────────────────────────────────────────────────────────────┐                                     */
/* │ │ String Buffer:                                                 │                                     */
/* │ │   [stringBufferBytesWritten]                                   │◄───── getStringBuffer(root)         */
/* │ └────────────────────────────────────────────────────────────────┘                                     */
/* └──────────────────────────────────────────────────────────────────────┘   */
/*                                                                            */
/******************************************************************************/

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

// NOLINTEND(hicpp-use-auto,cppcoreguidelines-pro-type-union-access)
// NOLINTEND(cppcoreguidelines-pro-type-cstyle-cast)

}

#endif /* WISENTHELPERS_HPP */

