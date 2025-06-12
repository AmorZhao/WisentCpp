/*
 *  The SerializedBossExpression structure is used for both serialization 
 *  and compression of Boss expressions. (Both give a Portable Boss Expression structure)
 *
 *  Part 1 is used for simple serialization of Boss expressions: 
 *
 *  Mainly adapted from: 
 *   https://github.com/symbol-store/BOSS/blob/temp_lazy_loading_compression/Source/Serialization.hpp
 *  
 *  1.  Count (preprocessing) functions
 *       - countUniqueArguments
 *       - countArgumentBytes
 *       - countArgumentBytesDict
 *       - countArguments
 *       - countExpressions
 *       - countStringBytes
 *  
 *  2.  Flatten (serialization) functions
 *       - checkMapAndStoreString
 *       - countArgumentTypes
 *       - countArgumentsPacked
 *       - flattenArgumentsInTuple
 *       - flattenArguments
 *  
 *  3.  Serialization surface (constructor)
 *  
 *  4.  Output stream writer
 *  
 *  5.  Deserialization surface
 *       - deserializer
 *       - lazy deserializer
 *  --------------------------------------------------------------
 *
 *  Part 2 adds helper functions for the Boss compressor: 
 *   (under region "Compress_Boss_Expression")
 *   (the constructor is overloaded to accept a compression pipeline map)
 *  
 *  Some were adapted / inspired from: 
 *   https://github.com/symbol-store/BOSSKernelBenchmarks/blob/gen_compressed_wisent/Benchmarks/tpch.cpp 
 *      - convertSpansToSingleSpan  (preprocess table expression)
 *      - dictionary encoding       (not used directly, adapted as compression helper algorithms)
 *      - run length encoding         (... same as above)
 *      - frame of reference encoding (... same as above)
 *  
 *  pre-processing functions
 *      - countArguments
 *      - handleColumnExpression     (convertSpansToSingleSpan + rewrite dynamic arguments)
 */

#include "../BossHelpers/BossExpression.hpp"
#include "WisentHelpers.hpp"
#include "../../WisentCompressor/CompressionPipeline.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <inttypes.h>
#include <iostream>
#include <iterator>
#include <string.h>
#include <sys/types.h>
#include <type_traits>
#include <typeinfo>
#include <unordered_set>
#include <utility>
#include <variant>

#ifndef _MSC_VER
#include <cxxabi.h>
#include <memory>
#endif

template <class T, class U> inline constexpr bool is_same_v = std::is_same<T, U>::value;

template <typename T> void print_type_name()
{
    const char *typeName = typeid(T).name();
  #ifndef _MSC_VER
    // Demangle the type name on GCC/Clang
    int status = -1;
    std::unique_ptr<char, void (*)(void *)> res{abi::__cxa_demangle(typeName, nullptr, nullptr, &status), std::free};
    std::cout << (status == 0 ? res.get() : typeName) << std::endl;
  #else
    // On MSVC, typeid().name() returns a human-readable name.
    std::cout << typeName << std::endl;
  #endif
}

namespace boss::serialization 
{
// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)

static_assert(std::is_same_v<std::variant_alternative_t<ARGUMENT_TYPE_BOOL, boss::Expression>, bool>, "type ids wrong");
static_assert(std::is_same_v<std::variant_alternative_t<ARGUMENT_TYPE_CHAR, boss::Expression>, std::int8_t>, "type ids wrong");
static_assert(std::is_same_v<std::variant_alternative_t<ARGUMENT_TYPE_SHORT, boss::Expression>, std::int16_t>, "type ids wrong");
static_assert(std::is_same_v<std::variant_alternative_t<ARGUMENT_TYPE_INT, boss::Expression>, std::int32_t>, "type ids wrong");
static_assert(std::is_same_v<std::variant_alternative_t<ARGUMENT_TYPE_LONG, boss::Expression>, std::int64_t>, "type ids wrong");
static_assert(std::is_same_v<std::variant_alternative_t<ARGUMENT_TYPE_FLOAT, boss::Expression>, std::float_t>, "type ids wrong");
static_assert(std::is_same_v<std::variant_alternative_t<ARGUMENT_TYPE_DOUBLE, boss::Expression>, std::double_t>, "type ids wrong");
static_assert(std::is_same_v<std::variant_alternative_t<ARGUMENT_TYPE_STRING, boss::Expression>, std::string>, "type ids wrong");
static_assert(std::is_same_v<std::variant_alternative_t<ARGUMENT_TYPE_SYMBOL, boss::Expression>, boss::Symbol>, "type ids wrong");

using std::literals::string_literals::operator""s; // NOLINT(misc-unused-using-decls) clang-tidy bug
using boss::utilities::operator""_;                // NOLINT(misc-unused-using-decls) clang-tidy bug

using Argument = WisentArgumentValue;
using ArgumentType = WisentArgumentType; 
using Expression = PortableBossExpression;
using RootExpression = PortableBossRootExpression;

static const uint8_t &ArgumentType_RLE_MINIMUM_SIZE = PortableBossArgumentType_RLE_MINIMUM_SIZE;
static const uint8_t &ArgumentType_RLE_BIT = PortableBossArgumentType_RLE_BIT;
static const uint8_t &ArgumentType_DICT_ENC_BIT = PortableBossArgumentType_DICT_ENC_BIT;
static const uint8_t &ArgumentType_DICT_ENC_SIZE_BIT = PortableBossArgumentType_DICT_ENC_SIZE_BIT;
static const uint8_t &ArgumentType_MASK = PortableBossArgumentType_MASK;

constexpr uint64_t Argument_BOOL_SIZE = PortableBossArgument_BOOL_SIZE;
constexpr uint64_t Argument_CHAR_SIZE = PortableBossArgument_CHAR_SIZE;
constexpr uint64_t Argument_SHORT_SIZE = PortableBossArgument_SHORT_SIZE;
constexpr uint64_t Argument_INT_SIZE = PortableBossArgument_INT_SIZE;
constexpr uint64_t Argument_LONG_SIZE = PortableBossArgument_LONG_SIZE;
constexpr uint64_t Argument_FLOAT_SIZE = PortableBossArgument_FLOAT_SIZE;
constexpr uint64_t Argument_DOUBLE_SIZE = PortableBossArgument_DOUBLE_SIZE;
constexpr uint64_t Argument_STRING_SIZE = PortableBossArgument_STRING_SIZE;
constexpr uint64_t Argument_EXPRESSION_SIZE = PortableBossArgument_EXPRESSION_SIZE;

/**
 * Implements serialization/deserialization of a (complex) expression to/from a c-allocated buffer.
 * The buffer contains no pointers so it can be safely written to disk or passed to a different
 * processing using shared memory
 */
template <void *(*allocateFunction)(size_t) = std::malloc, 
          void *(*reallocateFunction)(void *, size_t) = std::realloc,
          void (*freeFunction)(void *) = std::free>
struct SerializedBossExpression 
{
    using BOSSArgumentPair =
        std::pair<boss::expressions::ExpressionArguments, 
                  boss::expressions::ExpressionSpanArguments>;

    // using DictKey = std::variant<bool, int8_t, int32_t, int64_t, float_t, double_t>;
    using DictKey = std::variant<int64_t, double_t, std::string>;
    struct VariantHash {
        template <typename T> std::size_t operator()(const T &value) const
        {
            if constexpr (std::is_same_v<T, bool>) {
                return std::hash<int>()(value);
            }
            else {
                return std::hash<T>()(value);
            }
        }

        std::size_t operator()(const DictKey &v) const
        {
            return std::visit([](auto &&arg) { return VariantHash{}(arg); }, v);
        }
    };
    struct VariantEqual {
        bool operator()(const DictKey &a, const DictKey &b) const { return a == b; }
    };

    using ExpressionDictionary = std::unordered_map<DictKey, int32_t, VariantHash, VariantEqual>;
    using SpanDictionary = std::unordered_map<size_t, ExpressionDictionary>;

    RootExpression *root = nullptr;
    uint64_t argumentCount() const { return root->argumentCount; };
    uint64_t argumentBytesCount() const { return root->argumentBytesCount; };
    uint64_t expressionCount() const { return root->expressionCount; };

    Argument *flattenedArguments() const { return getExpressionArguments(root); }
    ArgumentType *flattenedArgumentTypes() const { return getArgumentTypes(root); }
    Expression *expressionsBuffer() const { return getExpressionSubexpressions(root); }
    Argument *spanDictionariesBuffer() const { return getSpanDictionaries(root); }

    //////////////////////////////// Count Unique Arguments /////////////////////////////

    // construct dictionary
    #pragma region count_unique_arguments

    static size_t getArgumentSizeFromDictSize(ExpressionDictionary &dict)
    {
        if (dict.size() < ((2 << (Argument_CHAR_SIZE - 1)) - 1)) {
            return Argument_CHAR_SIZE;
        }
        else if (dict.size() < ((2 << (Argument_INT_SIZE - 1)) - 1)) {
            return Argument_INT_SIZE;
        }
        else {
            return Argument_LONG_SIZE;
        }
    }

    static uint64_t calculateDictionaryBytes(SpanDictionary &spanDict)
    {
        uint64_t sum = 0;
        for (const auto &entry : spanDict) {
            const auto &innerDict = entry.second;
            sum += innerDict.size() * sizeof(Argument);
        }
        return sum;
    }

    static void checkMapAndIncrement(DictKey &&input, ExpressionDictionary &dict)
    {
        auto it = dict.find(input);
        if (it == dict.end()) {
            dict.emplace(std::move(input), -1);
        }
    }

    template <typename TupleLike, uint64_t... Is>
    static void countUniqueArgumentsInTuple(
        SpanDictionary &dict, 
        size_t &spanI, 
        TupleLike const &tuple,
        std::index_sequence<Is...> /*unused*/
    ) {
        (countUniqueArgumentsStaticsAndSpans(std::get<Is>(tuple), dict, spanI), ...);
    };

    static SpanDictionary countUniqueArguments(boss::Expression const &input)
    {
        SpanDictionary res;
        size_t spanI = 0;
        int64_t level = 1;
        while (countUniqueArgumentsAtLevel(input, res, spanI, level)) {
            level++;
        }
        return std::move(res);
    };

    static bool countUniqueArgumentsAtLevel(
        boss::Expression const &input, 
        SpanDictionary &dict, 
        size_t &spanI,
        int64_t level
    ) {
        if (level == 1) {
            countUniqueArgumentsStaticsAndSpans(input, dict, spanI);
            return true;
        }
        bool recurse = false;
        std::visit(
            [&dict, &spanI, &level, &recurse](auto &input) {
                if constexpr (std::is_same_v<std::decay_t<decltype(input)>, boss::ComplexExpression>) {
                    std::for_each(
                        input.getDynamicArguments().begin(), 
                        input.getDynamicArguments().end(),
                        [&dict, &spanI, &level, &recurse](auto const &argument) {
                            recurse |= countUniqueArgumentsAtLevel(argument, dict, spanI, level - 1);
                        });
                }
            },
            input);
        return recurse;
    };

    static void countUniqueArgumentsStaticsAndSpans(
        boss::Expression const &input, 
        SpanDictionary &dict, 
        size_t &spanI
    ) {
        std::visit(
            [&dict, &spanI](auto &input) {
                if constexpr (std::is_same_v<std::decay_t<decltype(input)>, boss::ComplexExpression>) {
                    countUniqueArgumentsInTuple(
                        dict, spanI, input.getStaticArguments(),
                        std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(input.getStaticArguments())>>>());
                    std::for_each(
                        input.getSpanArguments().begin(), 
                        input.getSpanArguments().end(),
                        [&dict, &spanI](auto const &argument) {
                            std::visit(
                                [&](auto const &spanArgument) {
                                    ExpressionDictionary spanDict;
                                    auto spanSize = spanArgument.size();
                                    auto const &arg0 = spanArgument[0];
                                    if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, int64_t> ||
                                                  std::is_same_v<std::decay_t<decltype(arg0)>, double_t> ||
                                                  std::is_same_v<std::decay_t<decltype(arg0)>, std::string>) {
                                        std::for_each(
                                            spanArgument.begin(), spanArgument.end(),
                                            [&](auto arg) { checkMapAndIncrement(DictKey(arg), spanDict); });
                                        if (spanDict.size() < (spanSize / 2) &&
                                            spanDict.size() < ((2 << (Argument_LONG_SIZE - 1)) - 1)) {
                                            dict[spanI] = std::move(spanDict);
                                        }
                                    }
                                    spanI++;
                                },
                                std::forward<decltype(argument)>(argument));
                        });
                }
            },
            input);
    }

    #pragma endregion count_unique_arguments

    //////////////////////////////// Count Argument Bytes ///////////////////////////////

    #pragma region count_argument_bytes

    // Current assumes that only values within spans can be packed into a single 8 byte arg value
    // All else is treated as an 8 bytes arg value
    // Note: To read values at a specific index in a packed span, the span size must be known
    template <typename TupleLike, uint64_t... Is>
    static uint64_t countArgumentBytesInTuple(TupleLike const &tuple, std::index_sequence<Is...> /*unused*/)
    {
        return (countArgumentBytes(std::get<Is>(tuple)) + ... + static_cast<uint64_t>(0));
    };

    static uint64_t countArgumentBytes(boss::Expression const &input)
    {
        return std::visit(
            [](auto &input) -> uint64_t {
                if constexpr (std::is_same_v<std::decay_t<decltype(input)>, boss::ComplexExpression>) {
                    return static_cast<uint64_t>(sizeof(Argument)) 
                        + countArgumentBytesInTuple(
                               input.getStaticArguments(),
                               std::make_index_sequence<
                                   std::tuple_size_v<std::decay_t<decltype(input.getStaticArguments())>>>()) 
                        + std::accumulate(
                            input.getDynamicArguments().begin(), 
                            input.getDynamicArguments().end(), uint64_t(0),
                            [](uint64_t runningSum, auto const &argument) -> uint64_t {
                                return runningSum + countArgumentBytes(argument);
                            }) 
                        + std::accumulate(
                            input.getSpanArguments().begin(), 
                            input.getSpanArguments().end(), uint64_t(0),
                            [](auto runningSum, auto const &argument) -> uint64_t 
                            {
                                return runningSum + std::visit(
                                        [&](auto const &spanArgument) -> uint64_t {
                                            uint64_t spanBytes = 0;
                                            uint64_t spanSize = spanArgument.size();
                                            auto const &arg0 = spanArgument[0];
                                            if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, bool> ||
                                                        std::is_same_v<std::decay_t<decltype(arg0)>, std::_Bit_reference>) 
                                            {
                                                spanBytes = spanSize * static_cast<uint64_t>(sizeof(bool));
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, int8_t>) 
                                            {
                                                spanBytes = spanSize * static_cast<uint64_t>(sizeof(int8_t));
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, int16_t>) 
                                            {
                                                spanBytes = spanSize * static_cast<uint64_t>(sizeof(int16_t));
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, int32_t>) 
                                            {
                                                spanBytes = spanSize * static_cast<uint64_t>(sizeof(int32_t));
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, int64_t>) 
                                            {
                                                spanBytes = spanSize * static_cast<uint64_t>(sizeof(int64_t));
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, float_t>) 
                                            {
                                                spanBytes = spanSize * static_cast<uint64_t>(sizeof(float_t));
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, double_t>) 
                                            {
                                                spanBytes = spanSize * static_cast<uint64_t>(sizeof(double_t));
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, std::string>) 
                                            {
                                                spanBytes = spanSize * static_cast<uint64_t>(sizeof(Argument));
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, boss::Symbol>) 
                                            {
                                                spanBytes = spanSize * static_cast<uint64_t>(sizeof(Argument));
                                            }
                                            else {
                                                print_type_name<std::decay_t<decltype(arg0)>>();
                                                throw std::runtime_error("unknown type in span");
                                            }
                                            // std::cout << "SPAN BYTES: " << spanBytes <<
                                            // std::endl; std::cout << "ROUNDED SPAN BYTES: "
                                            // << ((spanBytes + sizeof(Argument) - 1) &
                                            // -sizeof(Argument)) << std::endl;
                                            return (spanBytes + static_cast<uint64_t>(sizeof(Argument)) - 1) &
                                                    -(static_cast<uint64_t>(sizeof(Argument)));
                                        },
                                        std::forward<decltype(argument)>(argument));
                            });
                }
                return static_cast<uint64_t>(sizeof(Argument));
            },
            input);
    }

    #pragma endregion count_argument_bytes

    //////////////////////////////// Count Argument Bytes with Dictionary ///////////////

    #pragma region count_argument_bytes_with_dictionary

    // Current assumes that only values within spans can be packed into a single 8 byte arg value
    // All else is treated as an 8 bytes arg value
    // Note: To read values at a specific index in a packed span, the span size must be known
    template <typename TupleLike, uint64_t... Is>
    static uint64_t countArgumentBytesInTupleDict(SpanDictionary &dict, size_t &spanI, TupleLike const &tuple,
                                                  std::index_sequence<Is...> /*unused*/)
    {
        return (countArgumentBytesDictStaticsAndSpans(std::get<Is>(tuple), dict, spanI) + ... + 0);
    };

    static uint64_t countArgumentBytesDict(boss::Expression const &input, SpanDictionary &dict)
    {
        size_t spanI = 0;
        uint64_t count = 0;
        int64_t level = 1;
        while (countArgumentBytesDictAtLevel(input, count, dict, spanI, level)) {
            level++;
        }
        return count;
    };

    static bool countArgumentBytesDictAtLevel(
        boss::Expression const &input, 
        uint64_t &count, 
        SpanDictionary &dict,
        size_t &spanI, 
        int64_t level
    ) {
        if (level == 1) {
            count += countArgumentBytesDictStaticsAndSpans(input, dict, spanI);
            return true;
        }
        bool recurse = false;
        std::visit(
            [&count, &dict, &spanI, &level, &recurse](auto &input) {
                if constexpr (std::is_same_v<std::decay_t<decltype(input)>, boss::ComplexExpression>) {
                    std::for_each(
                        input.getDynamicArguments().begin(), 
                        input.getDynamicArguments().end(),
                        [&count, &dict, &spanI, &level, &recurse](auto const &argument) {
                            recurse |= countArgumentBytesDictAtLevel(argument, count, dict, spanI, level - 1);
                        });
                }
            },
            input);
        return recurse;
    };

    static uint64_t countArgumentBytesDictStaticsAndSpans(
        boss::Expression const &input, 
        SpanDictionary &dict,
        size_t &spanI
    ) {
        return std::visit(
            [&dict, &spanI](auto &input) -> size_t {
                if constexpr (std::is_same_v<std::decay_t<decltype(input)>, boss::ComplexExpression>) {
                    return Argument_EXPRESSION_SIZE
                        + countArgumentBytesInTupleDict(
                            dict, spanI, input.getStaticArguments(),
                            std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(input.getStaticArguments())>>>())
                        + std::accumulate(
                            input.getSpanArguments().begin(), 
                            input.getSpanArguments().end(), 0,
                            [&dict, &spanI](auto runningSum, auto const &argument) {
                                return runningSum + std::visit(
                                        [&](auto const &spanArgument) {
                                            auto spanBytes = 0;
                                            auto spanSize = spanArgument.size();
                                            auto const &arg0 = spanArgument[0];
                                            if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, bool> ||
                                                        std::is_same_v<std::decay_t<decltype(arg0)>, std::_Bit_reference>) 
                                            {
                                                spanBytes = spanSize * Argument_BOOL_SIZE;
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, int8_t>) 
                                            {
                                                spanBytes = spanSize * Argument_CHAR_SIZE;
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, int16_t>) 
                                            {
                                                spanBytes = spanSize * Argument_SHORT_SIZE;
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, int32_t>) 
                                            {
                                                spanBytes = spanSize * Argument_INT_SIZE;
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, int64_t>) 
                                            {
                                                if (dict.find(spanI) == dict.end()) {
                                                    spanBytes = spanSize * Argument_LONG_SIZE;
                                                }
                                                else {
                                                    auto &spanDict = dict[spanI];
                                                    spanBytes = spanSize * getArgumentSizeFromDictSize(spanDict);
                                                }
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, float_t>) 
                                            {
                                                spanBytes = spanSize * Argument_FLOAT_SIZE;
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, double_t>) 
                                            {
                                                if (dict.find(spanI) == dict.end()) {
                                                    spanBytes = spanSize * Argument_DOUBLE_SIZE;
                                                }
                                                else {
                                                    auto &spanDict = dict[spanI];
                                                    spanBytes = spanSize * getArgumentSizeFromDictSize(spanDict);
                                                }
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, std::string>) 
                                            {
                                                if (dict.find(spanI) == dict.end()) {
                                                    spanBytes = spanSize * Argument_STRING_SIZE;
                                                }
                                                else {
                                                    auto &spanDict = dict[spanI];
                                                    spanBytes = spanSize * getArgumentSizeFromDictSize(spanDict);
                                                }
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, boss::Symbol>) 
                                            {
                                                spanBytes = spanSize * Argument_STRING_SIZE;
                                            }
                                            else {
                                                print_type_name<std::decay_t<decltype(arg0)>>();
                                                throw std::runtime_error("unknown type in span");
                                            }
                                            // std::cout << "SPAN BYTES: " << spanBytes <<
                                            // std::endl; std::cout << "ROUNDED SPAN BYTES: "
                                            // << ((spanBytes + sizeof(Argument) - 1) &
                                            // -sizeof(Argument)) << std::endl;
                                            spanI++;
                                            return (spanBytes + sizeof(Argument) - 1) & -sizeof(Argument);
                                        },
                                        std::forward<decltype(argument)>(argument));
                            });
                }
                return sizeof(Argument);
            },
            input);
    };

    #pragma endregion count_argument_bytes_with_dictionary

    //////////////////////////////// Count Arguments ///////////////////////////////////

    #pragma region count_arguments
    template <typename TupleLike, uint64_t... Is>
    static uint64_t countArgumentsInTuple(TupleLike const &tuple, std::index_sequence<Is...> /*unused*/)
    {
        return (countArguments(std::get<Is>(tuple)) + ... + 0);
    };

    static uint64_t countArguments(boss::Expression const &input)
    {
        return std::visit(
            [](auto &input) -> uint64_t {
                if constexpr (std::is_same_v<std::decay_t<decltype(input)>, boss::ComplexExpression>) {
                    return 1 +
                        countArgumentsInTuple(
                            input.getStaticArguments(),
                            std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(input.getStaticArguments())>>>() )
                        + std::accumulate(
                            input.getDynamicArguments().begin(), 
                            input.getDynamicArguments().end(), uint64_t(0),
                            [](uint64_t runningSum, auto const &argument) -> uint64_t {
                                return runningSum + countArguments(argument);
                            })
                        + std::accumulate(
                            input.getSpanArguments().begin(), 
                            input.getSpanArguments().end(), uint64_t(0),
                            [](uint64_t runningSum, auto const &argument) -> uint64_t {
                                return runningSum +
                                        std::visit([&](auto const &argument) -> uint64_t { return argument.size(); },
                                                    std::forward<decltype(argument)>(argument));
                            });
                }
                return 1;
            },
            input);
    }
    #pragma endregion count_arguments

    //////////////////////////////// Count Expressions /////////////////////////////////

    #pragma region count_expressions
    template <typename TupleLike, uint64_t... Is>
    static uint64_t countExpressionsInTuple(TupleLike const &tuple, std::index_sequence<Is...> /*unused*/)
    {
        return (countExpressions(std::get<Is>(tuple)) + ... + 0);
    };

    template <typename T> static uint64_t countExpressions(T const & /*unused*/) { return 0; }

    static uint64_t countExpressions(boss::Expression const &input)
    {
        return std::visit(
            utilities::overload(
                [](boss::ComplexExpression const &input) -> uint64_t {
                    return 1 +
                        countExpressionsInTuple(
                            input.getStaticArguments(),
                            std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(input.getStaticArguments())>>>()) 
                        + std::accumulate(
                            input.getDynamicArguments().begin(), 
                            input.getDynamicArguments().end(), uint64_t(0), 
                            [](uint64_t runningSum, auto const &argument) -> uint64_t {
                                return runningSum + countExpressions(argument);
                            });
                },
                [](auto const &) -> uint64_t { return 0; }),
            input);
    }
    #pragma endregion count_expressions

    //////////////////////////////// Count String Bytes ////////////////////////////////

    #pragma region count_string_bytes
    
    template <typename TupleLike, uint64_t... Is>
    static uint64_t countStringBytesInTuple(
        std::unordered_set<std::string> &stringSet, 
        bool dictEncodeStrings,
        TupleLike const &tuple, 
        std::index_sequence<Is...> /*unused*/
    ) {
        return (countStringBytes(std::get<Is>(tuple), stringSet, dictEncodeStrings) + ... + 0);
    };

    static uint64_t countStringBytes(
        boss::Expression const &input, 
        bool dictEncodeStrings = true
    ) {
        std::unordered_set<std::string> stringSet;
        return 1 + countStringBytes(input, stringSet, dictEncodeStrings);
    }

    static uint64_t countStringBytes(
        boss::Expression const &input, 
        std::unordered_set<std::string> &stringSet,
        bool dictEncodeStrings
    ) {
        return std::visit(
            [&](auto &input) -> uint64_t {
                if constexpr (std::is_same_v<std::decay_t<decltype(input)>, boss::ComplexExpression>) {
                    uint64_t headBytes = !dictEncodeStrings * (strlen(input.getHead().getName().c_str()) + 1);
                    if (dictEncodeStrings && stringSet.find(input.getHead().getName()) == stringSet.end()) 
                    {
                        stringSet.insert(input.getHead().getName());
                        headBytes = strlen(input.getHead().getName().c_str()) + 1;
                    }
                    uint64_t staticArgsBytes = countStringBytesInTuple(
                        stringSet, dictEncodeStrings, input.getStaticArguments(),
                        std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(input.getStaticArguments())>>>());

                    uint64_t dynamicArgsBytes = std::accumulate(
                        input.getDynamicArguments().begin(), 
                        input.getDynamicArguments().end(), uint64_t(0),
                        [&](uint64_t runningSum, auto const &argument) -> uint64_t {
                            return runningSum + countStringBytes(argument, stringSet, dictEncodeStrings);
                        });

                    uint64_t spanArgsBytes = std::accumulate(
                        input.getSpanArguments().begin(), 
                        input.getSpanArguments().end(), uint64_t(0),
                        [&](size_t runningSum, auto const &argument) -> uint64_t {
                            return runningSum + std::visit(
                                [&](auto const &argument) -> uint64_t 
                                {
                                    if constexpr (std::is_same_v<std::decay_t<decltype(argument)>, boss::Span<std::string>>) 
                                    {
                                        return std::accumulate(
                                            argument.begin(), argument.end(), uint64_t(0),
                                            [&](uint64_t innerRunningSum, auto const &stringArgument) -> uint64_t {
                                                uint64_t resRunningSum =
                                                    innerRunningSum +
                                                    (!dictEncodeStrings * (strlen(stringArgument.c_str()) + 1)); 
                                                if (dictEncodeStrings && stringSet.find(stringArgument) == stringSet.end()) 
                                                {
                                                    stringSet.insert(stringArgument);
                                                    resRunningSum += strlen(stringArgument.c_str()) + 1;
                                                }
                                                return resRunningSum;
                                            });
                                    }
                                    else if constexpr (std::is_same_v<std::decay_t<decltype(argument)>, boss::Span<boss::Symbol>>) 
                                    {
                                        return std::accumulate(
                                            argument.begin(), argument.end(), uint64_t(0),
                                            [&](uint64_t innerRunningSum, auto const &stringArgument) -> uint64_t {
                                                uint64_t resRunningSum = 
                                                    innerRunningSum + 
                                                    (!dictEncodeStrings * (strlen(stringArgument.getName().c_str()) + 1));
                                                if (dictEncodeStrings && stringSet.find(stringArgument.getName()) == stringSet.end()) 
                                                {
                                                    stringSet.insert(stringArgument.getName());
                                                    resRunningSum += strlen(stringArgument.getName().c_str()) + 1;
                                                }
                                                return resRunningSum;
                                            });
                                    }
                                    return 0;
                                },
                                std::forward<decltype(argument)>(argument));
                        });

                    return headBytes + staticArgsBytes + dynamicArgsBytes + spanArgsBytes;
                }
                else if constexpr (std::is_same_v<std::decay_t<decltype(input)>, boss::Symbol>) {
                    size_t res = !dictEncodeStrings * (strlen(input.getName().c_str()) + 1);
                    if (dictEncodeStrings && stringSet.find(input.getName()) == stringSet.end()) {
                        stringSet.insert(input.getName());
                        res = strlen(input.getName().c_str()) + 1;
                    }
                    return res;
                }
                else if constexpr (std::is_same_v<std::decay_t<decltype(input)>, std::string>) {
                    size_t res = !dictEncodeStrings * (strlen(input.c_str()) + 1);
                    if (dictEncodeStrings && stringSet.find(input) == stringSet.end()) {
                        stringSet.insert(input);
                        res = strlen(input.c_str()) + 1;
                    }
                    return res;
                }
                return 0;
            },
            input);
    }
    #pragma endregion count_string_bytes

    //////////////////////////////// Flatten Arguments /////////////////////////////////

    #pragma region flatten_arguments

    size_t checkMapAndStoreString(
        const std::string &key, 
        std::unordered_map<std::string, size_t> &stringMap,
        bool dictEncodeStrings
    ) {
        size_t storedString = 0;
        if (dictEncodeStrings) {
            auto it = stringMap.find(key);
            if (it == stringMap.end()) {
                storedString = storeString(&root, key.c_str());
                stringMap.emplace(key, storedString);
            }
            else {
                storedString = it->second;
            }
        }
        else {
            storedString = storeString(&root, key.c_str());
        }
        return storedString;
    }

    uint64_t countArgumentTypes(boss::ComplexExpression const &expression)
    {
        return std::tuple_size_v<std::decay_t<decltype(expression.getStaticArguments())>> 
                + expression.getDynamicArguments().size() 
                + std::accumulate(
                    expression.getSpanArguments().begin(), 
                    expression.getSpanArguments().end(), uint64_t(0),
                    [](uint64_t runningSum, auto const &spanArg) -> uint64_t {
                        return runningSum +
                                std::visit([&](auto const &spanArg) -> uint64_t { return spanArg.size(); },
                                            std::forward<decltype(spanArg)>(spanArg));
                    });
    }

    uint64_t countArgumentsPacked(boss::ComplexExpression const &expression, SpanDictionary &spanDict)
    {
        size_t spanI = 0;
        return countArgumentsPacked(expression, spanDict, spanI);
    }

    uint64_t countArgumentsPacked(
        boss::ComplexExpression const &expression, 
        SpanDictionary &spanDict, 
        size_t spanIInput
    ) {
        size_t spanI = spanIInput;
        uint64_t staticsCount = std::tuple_size_v<std::decay_t<decltype(expression.getStaticArguments())>>;
        uint64_t dynamicsCount = expression.getDynamicArguments().size();

        uint64_t spansCount = std::accumulate(
            expression.getSpanArguments().begin(), 
            expression.getSpanArguments().end(), 
            uint64_t(0),
            [&spanDict, &spanI](uint64_t runningSum, auto const &spanArg) -> uint64_t 
            {
                return runningSum + std::visit(
                        [&](auto const &spanArgument) -> uint64_t 
                        {
                            uint64_t spanSize = spanArgument.size();
                            auto const &arg0 = spanArgument[0];
                            uint64_t valsPerArg = 
                                static_cast<uint64_t>(
                                    sizeof(arg0) > sizeof(Argument) 
                                    ? 1 
                                    : sizeof(Argument) / sizeof(arg0)
                                );
                            if (spanDict.find(spanI) != spanDict.end()) 
                            {
                                auto &dict = spanDict[spanI];
                                valsPerArg = sizeof(Argument) / getArgumentSizeFromDictSize(dict);
                            }
                            spanI++;
                            return (spanSize + valsPerArg - 1) / valsPerArg;
                        },
                        std::forward<decltype(spanArg)>(spanArg));
            });
        return staticsCount + dynamicsCount + spansCount;
    }

    template <typename TupleLike, uint64_t... Is>
    void flattenArgumentsInTuple(
        TupleLike &&tuple, 
        std::index_sequence<Is...> /*unused*/, 
        uint64_t &argumentOutputI,
        uint64_t &typeOutputI, 
        uint64_t &expressionOutputI,
        uint64_t &dictOutputI, 
        SpanDictionary &spanDict, 
        size_t &spanI,
        std::unordered_map<std::string, size_t> &stringMap, 
        bool dictEncodeStrings
    ) {
        (flattenArguments(
            argumentOutputI, 
            typeOutputI, 
            std::get<Is>(tuple), 
            expressionOutputI,
            dictOutputI, 
            spanDict, 
            spanI, 
            stringMap,
            dictEncodeStrings),
         ...);
    };

    // assuming RLE encode for now
    uint64_t flattenArguments(
        uint64_t argumentOutputI, 
        uint64_t typeOutputI,
        std::vector<boss::ComplexExpression> &&inputs, 
        uint64_t &expressionOutputI,
        uint64_t dictOutputI, 
        SpanDictionary &spanDict, 
        bool dictEncodeStrings = true
    ) {
        std::unordered_map<std::string, size_t> stringMap;
        size_t spanI = 0;
        return flattenArguments(
            argumentOutputI, 
            typeOutputI, 
            std::move(inputs), 
            expressionOutputI, 
            dictOutputI,
            spanDict, 
            spanI, 
            stringMap, 
            dictEncodeStrings
        );
    }

    uint64_t flattenArguments(
        uint64_t argumentOutputI, 
        uint64_t typeOutputI,
        std::vector<boss::ComplexExpression> &&inputs, 
        uint64_t &expressionOutputI,
        uint64_t dictOutputI, 
        SpanDictionary &spanDict, 
        size_t &spanI,
        std::unordered_map<std::string, size_t> &stringMap, 
        bool dictEncodeStrings
    ) {
        auto const nextLayerTypeOffset = 
            typeOutputI 
            + std::accumulate(
                inputs.begin(), inputs.end(), 0, 
                [this](auto count, auto const &expression) {
                    return count + countArgumentTypes(expression);
                });
        auto const nextLayerOffset = 
            argumentOutputI 
            + std::accumulate(
                inputs.begin(), inputs.end(), 0,
                [this, &spanDict, spanI](auto count, auto const &expression) {
                    return count + countArgumentsPacked(expression, spanDict, spanI);
                });
        auto children = std::vector<boss::ComplexExpression>();
        auto childrenCountRunningSum = 0UL;
        auto childrenTypeCountRunningSum = 0UL;

        std::for_each(
            std::move_iterator(inputs.begin()), 
            std::move_iterator(inputs.end()),
            [this, &argumentOutputI, &typeOutputI, &children, &expressionOutputI, nextLayerTypeOffset, nextLayerOffset,
             &childrenCountRunningSum, &childrenTypeCountRunningSum, &dictOutputI, &spanDict, &spanI, &stringMap,
             &dictEncodeStrings](boss::ComplexExpression &&input) 
            {
                auto [head, statics, dynamics, spans] = std::move(input).decompose();

                // flatten statics
                flattenArgumentsInTuple(
                    statics, 
                    std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(statics)>>>(),
                    argumentOutputI, typeOutputI, dictOutputI, expressionOutputI,
                    spanDict, spanI, stringMap, dictEncodeStrings
                );

                // flatten dynamics
                std::for_each(
                    std::make_move_iterator(dynamics.begin()), 
                    std::make_move_iterator(dynamics.end()),
                    [this, &argumentOutputI, &typeOutputI, &children, &expressionOutputI, nextLayerTypeOffset,
                     nextLayerOffset, &childrenCountRunningSum, &childrenTypeCountRunningSum, &stringMap,
                     &dictEncodeStrings, &spanDict, &spanI](auto &&argument) 
                    {
                        std::visit(
                            [this, &children, &argumentOutputI, &typeOutputI, &expressionOutputI, nextLayerTypeOffset,
                             nextLayerOffset, &childrenCountRunningSum, &childrenTypeCountRunningSum, &stringMap,
                             &dictEncodeStrings, &spanDict, &spanI](auto &&argument) 
                            {
                                if constexpr (boss::expressions::generic::isComplexExpression<decltype(argument)>) 
                                {
                                    auto const childrenCount = countArgumentsPacked(argument, spanDict, spanI);
                                    auto const childrenTypeCount = countArgumentTypes(argument);
                                    auto const startChildArgOffset = nextLayerOffset + childrenCountRunningSum;
                                    auto const endChildArgOffset = nextLayerOffset + childrenCountRunningSum + childrenCount;
                                    auto const startChildTypeOffset = nextLayerTypeOffset + childrenTypeCountRunningSum;
                                    auto const endChildTypeOffset = nextLayerTypeOffset + childrenTypeCountRunningSum + childrenTypeCount;

                                    auto storedString = checkMapAndStoreString(
                                        argument.getHead().getName(), 
                                        stringMap,
                                        dictEncodeStrings
                                    );
                                    *makeExpression(root, expressionOutputI) =
                                        PortableBossExpression{storedString, startChildArgOffset, endChildArgOffset,
                                                               startChildTypeOffset, endChildTypeOffset};
                                    *makeExpressionArgument(root, argumentOutputI++, typeOutputI++) =
                                        expressionOutputI++;
                                    auto head = viewString(root, storedString);
                                    childrenCountRunningSum += childrenCount;
                                    childrenTypeCountRunningSum += childrenTypeCount;
                                    children.push_back(std::forward<decltype(argument)>(argument));
                                }
                                else if constexpr (std::is_same_v<std::decay_t<decltype(argument)>, bool>) {
                                    *makeBoolArgument(root, argumentOutputI++, typeOutputI++) = argument;
                                }
                                else if constexpr (std::is_same_v<std::decay_t<decltype(argument)>, int8_t>) {
                                    *makeCharArgument(root, argumentOutputI++, typeOutputI++) = argument;
                                }
                                else if constexpr (std::is_same_v<std::decay_t<decltype(argument)>, int16_t>) {
                                    *makeShortArgument(root, argumentOutputI++, typeOutputI++) = argument;
                                }
                                else if constexpr (std::is_same_v<std::decay_t<decltype(argument)>, int32_t>) {
                                    *makeIntArgument(root, argumentOutputI++, typeOutputI++) = argument;
                                }
                                else if constexpr (std::is_same_v<std::decay_t<decltype(argument)>, int64_t>) {
                                    *makeLongArgument(root, argumentOutputI++, typeOutputI++) = argument;
                                }
                                else if constexpr (std::is_same_v<std::decay_t<decltype(argument)>, float_t>) {
                                    *makeFloatArgument(root, argumentOutputI++, typeOutputI++) = argument;
                                }
                                else if constexpr (std::is_same_v<std::decay_t<decltype(argument)>, double_t>) {
                                    *makeDoubleArgument(root, argumentOutputI++, typeOutputI++) = argument;
                                }
                                else if constexpr (std::is_same_v<std::decay_t<decltype(argument)>, std::string>) {
                                    auto storedString = checkMapAndStoreString(argument, stringMap, dictEncodeStrings);
                                    *makeStringArgument(root, argumentOutputI++, typeOutputI++) = storedString;
                                }
                                else if constexpr (std::is_same_v<std::decay_t<decltype(argument)>, boss::Symbol>) {
                                    auto storedString =
                                        checkMapAndStoreString(argument.getName(), stringMap, dictEncodeStrings);
                                    *makeSymbolArgument(root, argumentOutputI++, typeOutputI++) = storedString;
                                }
                                else {
                                    print_type_name<std::decay_t<decltype(argument)>>();
                                    throw std::runtime_error("unknown type");
                                }
                            },
                            std::forward<decltype(argument)>(argument));
                    });

                // flatten spans
                std::for_each(
                    std::make_move_iterator(spans.begin()), 
                    std::make_move_iterator(spans.end()),
                    [this, &argumentOutputI, &typeOutputI, &dictOutputI, &spanDict, &spanI, &stringMap, &dictEncodeStrings](auto &&argument) 
                    {
                        std::visit(
                            [&](auto &&spanArgument) {
                                auto spanSize = spanArgument.size();
                                auto const &arg0 = spanArgument[0];
                                if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, bool> ||
                                              std::is_same_v<std::decay_t<decltype(arg0)>, std::_Bit_reference>) 
                                {
                                    size_t valsPerArg = sizeof(Argument) / Argument_BOOL_SIZE;
                                    for (size_t i = 0; i < spanSize; i += valsPerArg) 
                                    {
                                        uint64_t tmp = 0;
                                        for (size_t j = 0; j < valsPerArg && i + j < spanSize; j++) 
                                        {
                                            makeBoolArgumentType(root, typeOutputI++);
                                            tmp |= static_cast<uint64_t>(spanArgument[i + j])
                                                    << (Argument_BOOL_SIZE * sizeof(Argument) * (valsPerArg - 1 - j));
                                        }
                                        *makeArgument(root, argumentOutputI++) = static_cast<int64_t>(tmp);
                                    }
                                }
                                else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, int8_t>) 
                                {
                                    size_t valsPerArg = sizeof(Argument) / Argument_CHAR_SIZE;
                                    for (size_t i = 0; i < spanSize; i += valsPerArg) 
                                    {
                                        uint64_t tmp = 0;
                                        for (size_t j = 0; j < valsPerArg && i + j < spanSize; j++) 
                                        {
                                            makeCharArgumentType(root, typeOutputI++);
                                            tmp |= static_cast<uint64_t>(spanArgument[i + j])
                                                    << (Argument_CHAR_SIZE * sizeof(Argument) * (valsPerArg - 1 - j));
                                        }
                                        *makeArgument(root, argumentOutputI++) = static_cast<int64_t>(tmp);
                                    }
                                }
                                else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, int16_t>) 
                                {
                                    size_t valsPerArg = sizeof(Argument) / Argument_SHORT_SIZE;
                                    for (size_t i = 0; i < spanSize; i += valsPerArg) 
                                    {
                                        uint64_t tmp = 0;
                                        for (size_t j = 0; j < valsPerArg && i + j < spanSize; j++) 
                                        {
                                            makeShortArgumentType(root, typeOutputI++);
                                            tmp |= static_cast<uint64_t>(spanArgument[i + j])
                                                    << (Argument_SHORT_SIZE * sizeof(Argument) * (valsPerArg - 1 - j));
                                        }
                                        *makeArgument(root, argumentOutputI++) = static_cast<int64_t>(tmp);
                                    }
                                }
                                else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, int32_t>) 
                                {
                                    size_t valsPerArg = sizeof(Argument) / Argument_INT_SIZE;
                                    for (size_t i = 0; i < spanSize; i += valsPerArg) 
                                    {
                                        uint64_t tmp = 0;
                                        for (size_t j = 0; j < valsPerArg && i + j < spanSize; j++) 
                                        {
                                            makeIntArgumentType(root, typeOutputI++);
                                            tmp |= static_cast<uint64_t>(spanArgument[i + j])
                                                    << (Argument_INT_SIZE * sizeof(Argument) * (valsPerArg - 1 - j));
                                        }
                                        *makeArgument(root, argumentOutputI++) = static_cast<int64_t>(tmp);
                                    }
                                }
                                else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, int64_t>) 
                                {
                                    if (spanDict.find(spanI) != spanDict.end()) 
                                    {
                                        auto &dict = spanDict[spanI];
                                        int64_t dictStartI = dictOutputI;
                                        for (auto &entry : dict) 
                                        {
                                            int64_t value = std::get<int64_t>(entry.first);
                                            int32_t &offset = entry.second;
                                            offset = dictOutputI;
                                            *makeLongDictionaryEntry(root, dictOutputI++) = value;
                                        }
                                        size_t argumentSize = getArgumentSizeFromDictSize(dict);
                                        size_t valsPerArg = sizeof(Argument) / argumentSize;
                                        for (size_t i = 0; i < spanSize; i += valsPerArg) 
                                        {
                                            uint64_t tmp = 0;
                                            for (size_t j = 0; j < valsPerArg && i + j < spanSize; j++) 
                                            {
                                                makeLongArgumentType(root, typeOutputI++);
                                                if (argumentSize == Argument_CHAR_SIZE) 
                                                {
                                                    int8_t val = static_cast<int8_t>(dict[DictKey(spanArgument[i + j])]);
                                                    tmp |= static_cast<uint64_t>(val)
                                                            << (argumentSize * sizeof(Argument) * (valsPerArg - 1 - j));
                                                }
                                                else if (argumentSize == Argument_INT_SIZE) 
                                                {
                                                    int32_t val = dict[DictKey(spanArgument[i + j])];
                                                    tmp |= static_cast<uint64_t>(val)
                                                            << (argumentSize * sizeof(Argument) * (valsPerArg - 1 - j));
                                                }
                                            }
                                            *makeArgument(root, argumentOutputI++) = static_cast<int64_t>(tmp);
                                        }
                                        setDictStartAndFlag(root, typeOutputI - spanSize, dictStartI, argumentSize);
                                    }
                                    else {
                                        std::for_each(spanArgument.begin(), spanArgument.end(), [&](auto &arg) {
                                            *makeLongArgument(root, argumentOutputI++, typeOutputI++) = arg;
                                        });
                                    }
                                }
                                else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, float_t>) 
                                {
                                    size_t valsPerArg = sizeof(Argument) / Argument_FLOAT_SIZE;
                                    for (size_t i = 0; i < spanSize; i += valsPerArg) {
                                        uint64_t tmp = 0;
                                        for (size_t j = 0; j < valsPerArg && i + j < spanSize; j++) {
                                            uint32_t rawVal;
                                            std::memcpy(&rawVal, &spanArgument[i + j], sizeof(rawVal));
                                            makeFloatArgumentType(root, typeOutputI++);
                                            tmp |= static_cast<uint64_t>(rawVal)
                                                    << (Argument_FLOAT_SIZE * sizeof(Argument) * (valsPerArg - 1 - j));
                                        }
                                        *makeArgument(root, argumentOutputI++) = static_cast<int64_t>(tmp);
                                    }
                                }
                                else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, double_t>) 
                                {
                                    if (spanDict.find(spanI) != spanDict.end()) {
                                        auto &dict = spanDict[spanI];
                                        int64_t dictStartI = dictOutputI;
                                        for (auto &entry : dict) {
                                            double value = std::get<double>(entry.first);
                                            int32_t &offset = entry.second;
                                            offset = dictOutputI;
                                            *makeDoubleDictionaryEntry(root, dictOutputI++) = value;
                                        }
                                        size_t argumentSize = getArgumentSizeFromDictSize(dict);
                                        size_t valsPerArg = sizeof(Argument) / Argument_INT_SIZE;
                                        for (size_t i = 0; i < spanSize; i += valsPerArg) {
                                            uint64_t tmp = 0;
                                            for (size_t j = 0; j < valsPerArg && i + j < spanSize; j++) {
                                                // NEED DICT ENC LONG TYPE OR BIT ON LONG TYPE
                                                makeDoubleArgumentType(root, typeOutputI++);
                                                if (argumentSize == Argument_CHAR_SIZE) {
                                                    int8_t val =
                                                        static_cast<int8_t>(dict[DictKey(spanArgument[i + j])]);
                                                    tmp |= static_cast<uint64_t>(val)
                                                            << (argumentSize * sizeof(Argument) * (valsPerArg - 1 - j));
                                                }
                                                else if (argumentSize == Argument_INT_SIZE) {
                                                    int32_t val = dict[DictKey(spanArgument[i + j])];
                                                    tmp |= static_cast<uint64_t>(val)
                                                            << (argumentSize * sizeof(Argument) * (valsPerArg - 1 - j));
                                                }
                                            }
                                            *makeArgument(root, argumentOutputI++) = static_cast<int64_t>(tmp);
                                        }
                                        setDictStartAndFlag(root, typeOutputI - spanSize, dictStartI, argumentSize);
                                    }
                                    else {
                                        std::for_each(spanArgument.begin(), spanArgument.end(), [&](auto &arg) {
                                            *makeDoubleArgument(root, argumentOutputI++, typeOutputI++) = arg;
                                        });
                                    }
                                }
                                else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, std::string>) 
                                {
                                    if (spanDict.find(spanI) != spanDict.end()) {
                                        auto &dict = spanDict[spanI];
                                        int64_t dictStartI = dictOutputI;
                                        for (auto &entry : dict) {
                                            std::string value = std::get<std::string>(entry.first);
                                            int32_t &offset = entry.second;
                                            offset = dictOutputI;
                                            auto storedString =
                                                checkMapAndStoreString(value, stringMap, dictEncodeStrings);
                                            *makeStringDictionaryEntry(root, dictOutputI++) = storedString;
                                        }
                                        size_t argumentSize = getArgumentSizeFromDictSize(dict);
                                        size_t valsPerArg = sizeof(Argument) / argumentSize;
                                        for (size_t i = 0; i < spanSize; i += valsPerArg) {
                                            uint64_t tmp = 0;
                                            for (size_t j = 0; j < valsPerArg && i + j < spanSize; j++) {
                                                makeLongArgumentType(root, typeOutputI++);
                                                if (argumentSize == Argument_CHAR_SIZE) {
                                                    int8_t val =
                                                        static_cast<int8_t>(dict[DictKey(spanArgument[i + j])]);
                                                    tmp |= static_cast<uint64_t>(val)
                                                            << (argumentSize * sizeof(Argument) * (valsPerArg - 1 - j));
                                                }
                                                else if (argumentSize == Argument_INT_SIZE) {
                                                    int32_t val = dict[DictKey(spanArgument[i + j])];
                                                    tmp |= static_cast<uint64_t>(val)
                                                            << (argumentSize * sizeof(Argument) * (valsPerArg - 1 - j));
                                                }
                                            }
                                            *makeArgument(root, argumentOutputI++) = static_cast<int64_t>(tmp);
                                        }
                                        setDictStartAndFlag(root, typeOutputI - spanSize, dictStartI, argumentSize);
                                    }
                                    else {
                                        std::for_each(spanArgument.begin(), spanArgument.end(), [&](auto &arg) {
                                            auto storedString =
                                                checkMapAndStoreString(arg, stringMap, dictEncodeStrings);
                                            *makeStringArgument(root, argumentOutputI++, typeOutputI++) = storedString;
                                        });
                                    }
                                }
                                else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, boss::Symbol>) 
                                {
                                    std::for_each(spanArgument.begin(), spanArgument.end(), [&](auto &arg) {
                                        auto storedString =
                                            checkMapAndStoreString(arg.getName(), stringMap, dictEncodeStrings);
                                        *makeSymbolArgument(root, argumentOutputI++, typeOutputI++) = storedString;
                                    });
                                }
                                else {
                                    print_type_name<std::decay_t<decltype(arg0)>>();
                                    throw std::runtime_error("unknown type");
                                }
                                spanI++;

                                if (spanSize >= ArgumentType_RLE_MINIMUM_SIZE) {
                                    setRLEArgumentFlagOrPropagateTypes(root, typeOutputI - spanSize, spanSize);
                                    //  CHECK HERE NEXT
                                }
                            },
                            std::forward<decltype(argument)>(argument));
                    });
            });   // end of for_each input
        
        // recursive flatten with children as new input
        if (!children.empty()) {
            return flattenArguments(
                argumentOutputI, 
                typeOutputI, 
                std::move(children), 
                expressionOutputI, 
                dictOutputI,
                spanDict, 
                spanI, 
                stringMap, 
                dictEncodeStrings
            );
        }
        return argumentOutputI;
    }
    #pragma endregion flatten_arguments

    ////////////////////////////////// Surface Area ////////////////////////////////////

  public:
    explicit SerializedBossExpression(
        boss::Expression &&input, 
        bool dictEncodeStrings = true,
        bool dictEncodeDoublesAndLongs = false
    ) {
        SpanDictionary spanDict;
        if (dictEncodeDoublesAndLongs) 
        {
            spanDict = countUniqueArguments(input);
            root = allocateExpressionTree(
                countArguments(input), 
                countArgumentBytesDict(input, spanDict),
                countExpressions(input), 
                calculateDictionaryBytes(spanDict),
                countStringBytes(input, dictEncodeStrings), 
                allocateFunction);
        }
        else {
            root = allocateExpressionTree(
                countArguments(input), 
                countArgumentBytes(input), 
                countExpressions(input),
                countStringBytes(input, dictEncodeStrings), 
                allocateFunction);
        }
        std::visit(utilities::overload(
            [this, &spanDict, &dictEncodeStrings](boss::ComplexExpression &&input) {
                // count arguments and types 
                uint64_t argumentIterator = 0;
                uint64_t typeIterator = 0;
                uint64_t expressionIterator = 0;
                uint64_t dictIterator = 0;
                auto const childrenTypeCount = countArgumentTypes(input);
                auto const childrenCount = countArgumentsPacked(input, spanDict);
                auto const startChildArgOffset = 1;
                auto const endChildArgOffset = startChildArgOffset + childrenCount;
                auto const startChildTypeOffset = 1;
                auto const endChildTypeOffset = startChildArgOffset + childrenTypeCount;

                auto storedString = storeString(&root, input.getHead().getName().c_str());
                *makeExpression(root, expressionIterator) =
                    PortableBossExpression{storedString, startChildArgOffset, endChildArgOffset, 
                        startChildTypeOffset, endChildTypeOffset};
                *makeExpressionArgument(root, argumentIterator++, typeIterator++) = expressionIterator++;

                auto inputs = std::vector<boss::ComplexExpression>();
                inputs.push_back(std::move(input));

                flattenArguments(
                    argumentIterator, 
                    typeIterator, 
                    std::move(inputs), 
                    expressionIterator, 
                    dictIterator, 
                    spanDict, 
                    dictEncodeStrings
                );
            },
            [this](expressions::atoms::Symbol &&input) {
                auto storedString = storeString(&root, input.getName().c_str());
                *makeSymbolArgument(root, 0) = storedString;
            },
            [this](bool input) { *makeBoolArgument(root, 0) = input; },
            [this](std::int8_t input) { *makeCharArgument(root, 0) = input; },
            [this](std::int16_t input) { *makeShortArgument(root, 0) = input; },
            [this](std::int32_t input) { *makeIntArgument(root, 0) = input; },
            [this](std::int64_t input) { *makeLongArgument(root, 0) = input; },
            [this](std::float_t input) { *makeFloatArgument(root, 0) = input; },
            [this](std::double_t input) { *makeDoubleArgument(root, 0) = input; },
            [](auto &&) { throw std::logic_error("uncountered unknown type during serialization"); }),
        std::move(input));
    }

    explicit SerializedBossExpression(RootExpression *root) : root(root) {}

    // readable output stream for the flattened expression
    #pragma region Add_Index_To_Stream
    static void addIndexToStream(std::ostream &stream, SerializedBossExpression const &expr, size_t index, size_t typeIndex,
                                 int64_t exprIndex, int64_t exprDepth)
    {
        for (auto i = 0; i < exprDepth; i++) {
            stream << "  ";
        }
        auto const &arguments = expr.flattenedArguments();
        auto const &types = expr.flattenedArgumentTypes();
        auto const &expressions = expr.expressionsBuffer();
        auto const &dicts = expr.spanDictionariesBuffer();
        auto const &root = expr.root;

        auto testIndex = typeIndex;
        bool isRLE = (types[testIndex] & ArgumentType_RLE_BIT) != 0u;
        while (!isRLE && testIndex >= 0 && testIndex > typeIndex - 4) {
            testIndex--;
            isRLE |= (types[testIndex] & ArgumentType_RLE_BIT) != 0u;
        }
        auto validTypeIndex = isRLE ? testIndex : typeIndex;
        auto argumentType = static_cast<ArgumentType>(types[validTypeIndex] & ArgumentType_MASK);

        if (exprIndex < 0) {
            stream << "ARG INDEX: " << index << " TYPE INDEX: " << typeIndex << " VALUE: ";
        }
        else {
            stream << "ARG INDEX: " << index << " TYPE INDEX: " << typeIndex << " SUB-EXPR INDEX: " << exprIndex
                   << " VALUE: ";
        }

        switch (argumentType) 
        {
            case ArgumentType::ARGUMENT_TYPE_BOOL:
                stream << arguments[index].asBool << " TYPE: BOOL";
                stream << "\n";
                return;
            case ArgumentType::ARGUMENT_TYPE_CHAR:
                stream << arguments[index].asChar << " TYPE: CHAR";
                stream << "\n";
                return;
            case ArgumentType::ARGUMENT_TYPE_SHORT:
                stream << arguments[index].asShort << " TYPE: SHORT";
                stream << "\n";
                return;
            case ArgumentType::ARGUMENT_TYPE_INT:
                stream << arguments[index].asInt << " TYPE: INT";
                stream << "\n";
                return;
            case ArgumentType::ARGUMENT_TYPE_LONG:
                stream << arguments[index].asLong << " TYPE: LONG";
                stream << "\n";
                return;
            case ArgumentType::ARGUMENT_TYPE_FLOAT:
                stream << arguments[index].asFloat << " TYPE: FLOAT";
                stream << "\n";
                return;
            case ArgumentType::ARGUMENT_TYPE_DOUBLE:
                stream << arguments[index].asDouble << " TYPE: DOUBLE";
                stream << "\n";
                return;
            case ArgumentType::ARGUMENT_TYPE_STRING:
                stream << "( STR_OFFSET[" << arguments[index].asString << "], "
                    << viewString(root, arguments[index].asString) << ")"
                    << " TYPE: STRING";
                stream << "\n";
                return;
            case ArgumentType::ARGUMENT_TYPE_SYMBOL:
                stream << "( STR_OFFSET[" << arguments[index].asString << "], "
                    << boss::Symbol(viewString(root, arguments[index].asString)) << ")"
                    << " TYPE: SYMBOL";
                stream << "\n";
                return;
            case ArgumentType::ARGUMENT_TYPE_BYTE_ARRAY: 
                stream << "UNKNOWN ARG TYPE: " << static_cast<int64_t>(argumentType) << "\n";
                return;
            case ArgumentType::ARGUMENT_TYPE_EXPRESSION:
                // std::cout << "INDEX: " << index << std::endl;
                auto const &expression = expressions[arguments[index].asExpression];
                auto s = boss::Symbol(viewString(root, expression.symbolNameOffset));
                stream << "( EXPR_OFFSET[" << arguments[index].asExpression << "], \n";
                for (auto i = 0; i < exprDepth + 1; i++) {
                    stream << "  ";
                }
                stream << "HEAD: " << s << "\n";
                if (root->expressionCount == 0) {
                    for (auto i = 0; i < exprDepth; i++) {
                        stream << "  ";
                    }
                    stream << ")"
                        << " TYPE: EXPRESSION\n";
                }
                for (auto childI = expression.startChildOffset, childTypeI = expression.startChildTypeOffset;
                    childI < expression.endChildOffset && childTypeI < expression.endChildTypeOffset; childTypeI++) {

                    bool isChildRLE = (types[childTypeI] & ArgumentType_RLE_BIT) != 0u;
                    bool isDictEnc = (types[childTypeI] & ArgumentType_DICT_ENC_BIT) != 0U;

                    if (isChildRLE) {
                        auto const argType = static_cast<ArgumentType>(types[childTypeI] & ArgumentType_MASK);
                        uint32_t spanSize = (static_cast<uint32_t>(types[childTypeI + 4]) << 24) |
                                            (static_cast<uint32_t>(types[childTypeI + 3]) << 16) |
                                            (static_cast<uint32_t>(types[childTypeI + 2]) << 8) |
                                            (static_cast<uint32_t>(types[childTypeI + 1]));
                        uint64_t dictI = 0;
                        size_t dictOffsetArgumentSize = 0;
                        if (isDictEnc) {
                            dictOffsetArgumentSize = (types[childTypeI] & ArgumentType_DICT_ENC_SIZE_BIT) == 0U
                                                        ? Argument_CHAR_SIZE
                                                        : Argument_INT_SIZE;
                            dictI = (static_cast<uint64_t>(types[childTypeI + 12]) << 56) |
                                    (static_cast<uint64_t>(types[childTypeI + 11]) << 48) |
                                    (static_cast<uint64_t>(types[childTypeI + 10]) << 40) |
                                    (static_cast<uint64_t>(types[childTypeI + 9]) << 32) |
                                    (static_cast<uint64_t>(types[childTypeI + 8]) << 24) |
                                    (static_cast<uint64_t>(types[childTypeI + 7]) << 16) |
                                    (static_cast<uint64_t>(types[childTypeI + 6]) << 8) |
                                    (static_cast<uint64_t>(types[childTypeI + 5]));
                        }
                        auto prevChildTypeI = childTypeI;

                        if (argType == ArgumentType::ARGUMENT_TYPE_BOOL) {
                            auto valsPerArg = sizeof(Argument) / Argument_BOOL_SIZE;
                            for (; childTypeI < prevChildTypeI + spanSize; childI++) {
                                int64_t &arg = arguments[childI].asLong;
                                uint64_t tmp = static_cast<uint64_t>(arg);
                                for (int64_t i = valsPerArg - 1; i >= 0 && childTypeI < prevChildTypeI + spanSize;
                                    i--, childTypeI++) {
                                    for (auto j = 0; j < exprDepth + 1; j++) {
                                        stream << "  ";
                                    }
                                    stream << "ARG INDEX: " << childI << " TYPE INDEX: " << childTypeI
                                        << " SUB-EXPR INDEX: " << childTypeI - expression.startChildTypeOffset
                                        << " VALUE: ";
                                    uint8_t val = static_cast<uint8_t>(
                                        (tmp >> (Argument_BOOL_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                    stream << static_cast<bool>(val) << " TYPE: BOOL";
                                    stream << "\n";
                                }
                            }
                        }
                        else if (argType == ArgumentType::ARGUMENT_TYPE_CHAR) {
                            auto valsPerArg = sizeof(Argument) / Argument_CHAR_SIZE;
                            for (; childTypeI < prevChildTypeI + spanSize; childI++) {
                                int64_t &arg = arguments[childI].asLong;
                                uint64_t tmp = static_cast<uint64_t>(arg);
                                for (int64_t i = valsPerArg - 1; i >= 0 && childTypeI < prevChildTypeI + spanSize;
                                    i--, childTypeI++) {
                                    for (auto j = 0; j < exprDepth + 1; j++) {
                                        stream << "  ";
                                    }
                                    stream << "ARG INDEX: " << childI << " TYPE INDEX: " << childTypeI
                                        << " SUB-EXPR INDEX: " << childTypeI - expression.startChildTypeOffset
                                        << " VALUE: ";
                                    uint8_t val = static_cast<uint8_t>(
                                        (tmp >> (Argument_CHAR_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                    stream << static_cast<int32_t>(val) << " TYPE: CHAR";
                                    stream << "\n";
                                }
                            }
                        }
                        else if (argType == ArgumentType::ARGUMENT_TYPE_SHORT) {
                            auto valsPerArg = sizeof(Argument) / Argument_SHORT_SIZE;
                            for (; childTypeI < prevChildTypeI + spanSize; childI++) {
                                int64_t &arg = arguments[childI].asLong;
                                uint64_t tmp = static_cast<uint64_t>(arg);
                                for (int64_t i = valsPerArg - 1; i >= 0 && childTypeI < prevChildTypeI + spanSize;
                                    i--, childTypeI++) {
                                    for (auto j = 0; j < exprDepth + 1; j++) {
                                        stream << "  ";
                                    }
                                    stream << "ARG INDEX: " << childI << " TYPE INDEX: " << childTypeI
                                        << " SUB-EXPR INDEX: " << childTypeI - expression.startChildTypeOffset
                                        << " VALUE: ";
                                    uint16_t val = static_cast<uint16_t>(
                                        (tmp >> (Argument_SHORT_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                    stream << static_cast<int32_t>(val) << " TYPE: SHORT";
                                    stream << "\n";
                                }
                            }
                        }
                        else if (argType == ArgumentType::ARGUMENT_TYPE_INT) {
                            auto valsPerArg = sizeof(Argument) / Argument_INT_SIZE;
                            for (; childTypeI < prevChildTypeI + spanSize; childI++) {
                                int64_t &arg = arguments[childI].asLong;
                                uint64_t tmp = static_cast<uint64_t>(arg);
                                for (int64_t i = valsPerArg - 1; i >= 0 && childTypeI < prevChildTypeI + spanSize;
                                    i--, childTypeI++) {
                                    for (auto j = 0; j < exprDepth + 1; j++) {
                                        stream << "  ";
                                    }
                                    stream << "ARG INDEX: " << childI << " TYPE INDEX: " << childTypeI
                                        << " SUB-EXPR INDEX: " << childTypeI - expression.startChildTypeOffset
                                        << " VALUE: ";
                                    uint32_t val = static_cast<uint32_t>(
                                        (tmp >> (Argument_INT_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                    stream << static_cast<int32_t>(val) << " TYPE: INT";
                                    stream << "\n";
                                }
                            }
                        }
                        else if (argType == ArgumentType::ARGUMENT_TYPE_LONG) {
                            if (isDictEnc) {
                                if (dictOffsetArgumentSize == Argument_CHAR_SIZE) {
                                    size_t valsPerArg = sizeof(Argument) / Argument_CHAR_SIZE;
                                    for (; childTypeI < prevChildTypeI + spanSize; childI++) {
                                        int64_t &arg = arguments[childI].asLong;
                                        uint64_t tmp = static_cast<uint64_t>(arg);
                                        for (int64_t i = valsPerArg - 1; i >= 0 && childTypeI < prevChildTypeI + spanSize;
                                            i--, childTypeI++) {
                                            for (auto j = 0; j < exprDepth + 1; j++) {
                                                stream << "  ";
                                            }
                                            stream << "ARG INDEX: " << childI << " TYPE INDEX: " << childTypeI
                                                << " SUB-EXPR INDEX: " << childTypeI - expression.startChildTypeOffset
                                                << " DICT INDEX (CHAR): ";
                                            uint8_t dictOffset = static_cast<uint8_t>(
                                                (tmp >> (Argument_CHAR_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                            stream << dictI + static_cast<int8_t>(dictOffset) << " VALUE: ";
                                            auto const &arg = dicts[(dictI + static_cast<int8_t>(dictOffset))];
                                            stream << arg.asLong << " TYPE: LONG\n";
                                        }
                                    }
                                }
                                else if (dictOffsetArgumentSize == Argument_INT_SIZE) {
                                    size_t valsPerArg = sizeof(Argument) / Argument_INT_SIZE;
                                    for (; childTypeI < prevChildTypeI + spanSize; childI++) {
                                        int64_t &arg = arguments[childI].asLong;
                                        uint64_t tmp = static_cast<uint64_t>(arg);
                                        for (int64_t i = valsPerArg - 1; i >= 0 && childTypeI < prevChildTypeI + spanSize;
                                            i--, childTypeI++) {
                                            for (auto j = 0; j < exprDepth + 1; j++) {
                                                stream << "  ";
                                            }
                                            stream << "ARG INDEX: " << childI << " TYPE INDEX: " << childTypeI
                                                << " SUB-EXPR INDEX: " << childTypeI - expression.startChildTypeOffset
                                                << " DICT INDEX (INT): ";
                                            uint32_t dictOffset = static_cast<uint32_t>(
                                                (tmp >> (Argument_INT_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                            stream << dictI + static_cast<int32_t>(dictOffset) << " VALUE: ";
                                            auto const &arg = dicts[(dictI + static_cast<int32_t>(dictOffset))];
                                            stream << arg.asLong << " TYPE: LONG\n";
                                        }
                                    }
                                }
                            }
                            else {
                                for (; childTypeI < prevChildTypeI + spanSize; childTypeI++) {
                                    addIndexToStream(stream, expr, childI++, childTypeI,
                                                    childTypeI - expression.startChildTypeOffset, exprDepth + 1);
                                }
                            }
                        }
                        else if (argType == ArgumentType::ARGUMENT_TYPE_FLOAT) {
                            auto valsPerArg = sizeof(Argument) / Argument_FLOAT_SIZE;
                            for (; childTypeI < prevChildTypeI + spanSize; childI++) {
                                int64_t &arg = arguments[childI].asLong;
                                uint64_t tmp = static_cast<uint64_t>(arg);
                                for (int64_t i = valsPerArg - 1; i >= 0 && childTypeI < prevChildTypeI + spanSize;
                                    i--, childTypeI++) {
                                    for (auto j = 0; j < exprDepth + 1; j++) {
                                        stream << "  ";
                                    }
                                    stream << "ARG INDEX: " << childI << " TYPE INDEX: " << childTypeI
                                        << " SUB-EXPR INDEX: " << childTypeI - expression.startChildTypeOffset
                                        << " VALUE: ";
                                    uint32_t val = static_cast<uint32_t>(
                                        (tmp >> (Argument_INT_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                    float realVal;
                                    std::memcpy(&realVal, &val, sizeof(realVal));
                                    stream << static_cast<float>(val) << " TYPE: FLOAT";
                                    stream << "\n";
                                }
                            }
                        }
                        else if (argType == ArgumentType::ARGUMENT_TYPE_DOUBLE) {
                            if (isDictEnc) {
                                if (dictOffsetArgumentSize == Argument_CHAR_SIZE) {
                                    size_t valsPerArg = sizeof(Argument) / Argument_CHAR_SIZE;
                                    for (; childTypeI < prevChildTypeI + spanSize; childI++) {
                                        int64_t &arg = arguments[childI].asLong;
                                        uint64_t tmp = static_cast<uint64_t>(arg);
                                        for (int64_t i = valsPerArg - 1; i >= 0 && childTypeI < prevChildTypeI + spanSize;
                                            i--, childTypeI++) {
                                            for (auto j = 0; j < exprDepth + 1; j++) {
                                                stream << "  ";
                                            }
                                            stream << "ARG INDEX: " << childI << " TYPE INDEX: " << childTypeI
                                                << " SUB-EXPR INDEX: " << childTypeI - expression.startChildTypeOffset
                                                << " DICT INDEX (CHAR): ";
                                            uint8_t dictOffset = static_cast<uint8_t>(
                                                (tmp >> (Argument_CHAR_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                            stream << dictI + static_cast<int8_t>(dictOffset) << " VALUE: ";
                                            auto const &arg = dicts[(dictI + static_cast<int8_t>(dictOffset))];
                                            stream << arg.asDouble << " TYPE: DOUBLE\n";
                                        }
                                    }
                                }
                                else if (dictOffsetArgumentSize == Argument_INT_SIZE) {
                                    size_t valsPerArg = sizeof(Argument) / Argument_INT_SIZE;
                                    for (; childTypeI < prevChildTypeI + spanSize; childI++) {
                                        int64_t &arg = arguments[childI].asLong;
                                        uint64_t tmp = static_cast<uint64_t>(arg);
                                        for (int64_t i = valsPerArg - 1; i >= 0 && childTypeI < prevChildTypeI + spanSize;
                                            i--, childTypeI++) {
                                            for (auto j = 0; j < exprDepth + 1; j++) {
                                                stream << "  ";
                                            }
                                            stream << "ARG INDEX: " << childI << " TYPE INDEX: " << childTypeI
                                                << " SUB-EXPR INDEX: " << childTypeI - expression.startChildTypeOffset
                                                << " DICT INDEX (INT): ";
                                            uint32_t dictOffset = static_cast<uint32_t>(
                                                (tmp >> (Argument_INT_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                            stream << dictI + static_cast<int8_t>(dictOffset) << " VALUE: ";
                                            auto const &arg = dicts[(dictI + static_cast<int32_t>(dictOffset))];
                                            stream << arg.asDouble << " TYPE: DOUBLE\n";
                                        }
                                    }
                                }
                            }
                            else {
                                for (; childTypeI < prevChildTypeI + spanSize; childTypeI++) {
                                    addIndexToStream(stream, expr, childI++, childTypeI,
                                                    childTypeI - expression.startChildTypeOffset, exprDepth + 1);
                                }
                            }
                        }
                        else if (argType == ArgumentType::ARGUMENT_TYPE_STRING) {
                            if (isDictEnc) {
                                if (dictOffsetArgumentSize == Argument_CHAR_SIZE) {
                                    size_t valsPerArg = sizeof(Argument) / Argument_CHAR_SIZE;
                                    for (; childTypeI < prevChildTypeI + spanSize; childI++) {
                                        int64_t &arg = arguments[childI].asLong;
                                        uint64_t tmp = static_cast<uint64_t>(arg);
                                        for (int64_t i = valsPerArg - 1; i >= 0 && childTypeI < prevChildTypeI + spanSize;
                                            i--, childTypeI++) {
                                            for (auto j = 0; j < exprDepth + 1; j++) {
                                                stream << "  ";
                                            }
                                            stream << "ARG INDEX: " << childI << " TYPE INDEX: " << childTypeI
                                                << " SUB-EXPR INDEX: " << childTypeI - expression.startChildTypeOffset
                                                << " DICT INDEX (CHAR): ";
                                            uint8_t dictOffset = static_cast<uint8_t>(
                                                (tmp >> (Argument_CHAR_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                            stream << dictI + static_cast<int8_t>(dictOffset) << " VALUE: ";
                                            auto const &arg = dicts[(dictI + static_cast<int8_t>(dictOffset))];
                                            stream << std::string(viewString(root, arg.asString)) << " TYPE: STRING\n";
                                        }
                                    }
                                }
                                else if (dictOffsetArgumentSize == Argument_INT_SIZE) {
                                    size_t valsPerArg = sizeof(Argument) / Argument_INT_SIZE;
                                    for (; childTypeI < prevChildTypeI + spanSize; childI++) {
                                        int64_t &arg = arguments[childI].asLong;
                                        uint64_t tmp = static_cast<uint64_t>(arg);
                                        for (int64_t i = valsPerArg - 1; i >= 0 && childTypeI < prevChildTypeI + spanSize;
                                            i--, childTypeI++) {
                                            for (auto j = 0; j < exprDepth + 1; j++) {
                                                stream << "  ";
                                            }
                                            stream << "ARG INDEX: " << childI << " TYPE INDEX: " << childTypeI
                                                << " SUB-EXPR INDEX: " << childTypeI - expression.startChildTypeOffset
                                                << " DICT INDEX (INT): ";
                                            uint32_t dictOffset = static_cast<uint32_t>(
                                                (tmp >> (Argument_INT_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                            stream << dictI + static_cast<int8_t>(dictOffset) << " VALUE: ";
                                            auto const &arg = dicts[(dictI + static_cast<int32_t>(dictOffset))];
                                            stream << std::string(viewString(root, arg.asString)) << " TYPE: STRING\n";
                                        }
                                    }
                                }
                            }
                            else {
                                for (; childTypeI < prevChildTypeI + spanSize; childTypeI++) {
                                    addIndexToStream(stream, expr, childI++, childTypeI,
                                                    childTypeI - expression.startChildTypeOffset, exprDepth + 1);
                                }
                            }
                        }
                        else if (argType == ArgumentType::ARGUMENT_TYPE_SYMBOL) {
                            for (; childTypeI < prevChildTypeI + spanSize; childTypeI++) {
                                addIndexToStream(stream, expr, childI++, childTypeI,
                                                childTypeI - expression.startChildTypeOffset, exprDepth + 1);
                            }
                        }
                        --childTypeI;
                        // maybe need to --childI or --childTypeI
                    }
                    else {
                        addIndexToStream(stream, expr, childI++, childTypeI, childTypeI - expression.startChildTypeOffset,
                                        exprDepth + 1);
                    }
                }
                for (auto i = 0; i < exprDepth; i++) {
                    stream << "  ";
                }
                stream << ")"
                    << " TYPE: EXPRESSION";
                stream << "\n";
                return; 
        }
        // if (isRLE) {
        // 	stream << " SPAN";
        // }
        // stream << "\n";
    }

    friend std::ostream &operator<<(std::ostream &stream, SerializedBossExpression const &expr)
    {
        addIndexToStream(stream, expr, 0, 0, -1, 0);
        return stream;
    }
    #pragma endregion Add_Index_To_Stream

    #pragma region Deserialization
    BOSSArgumentPair deserializeArguments(
            uint64_t startChildOffset, 
            uint64_t endChildOffset,
            uint64_t startChildTypeOffset, 
            uint64_t endChildTypeOffset) const
    {
        boss::expressions::ExpressionArguments arguments;
        boss::expressions::ExpressionSpanArguments spanArguments;
        for (auto childTypeIndex = startChildTypeOffset, childArgIndex = startChildOffset;
             childTypeIndex < endChildTypeOffset && childArgIndex < endChildOffset; childTypeIndex++, childArgIndex++) {
            auto const &type = flattenedArgumentTypes()[childTypeIndex];
            auto const &isRLE = (type & ArgumentType_RLE_BIT) != 0U;
            auto const &isDictEnc = (type & ArgumentType_DICT_ENC_BIT) != 0U;

            // std::cout << "TYPE: " << (int64_t)(type & (~ArgumentType_RLE_BIT)) << " isRLE: " <<
            // (int64_t)isRLE << std::endl;

            if (isRLE) {

                auto const argType = static_cast<ArgumentType>(type & ArgumentType_MASK);
                uint32_t size = (static_cast<uint32_t>(flattenedArgumentTypes()[childTypeIndex + 4]) << 24) |
                                (static_cast<uint32_t>(flattenedArgumentTypes()[childTypeIndex + 3]) << 16) |
                                (static_cast<uint32_t>(flattenedArgumentTypes()[childTypeIndex + 2]) << 8) |
                                (static_cast<uint32_t>(flattenedArgumentTypes()[childTypeIndex + 1]));
                uint64_t dictI = 0;
                size_t dictOffsetArgumentSize = 0;
                if (isDictEnc) {
                    dictOffsetArgumentSize =
                        (type & ArgumentType_DICT_ENC_SIZE_BIT) == 0U ? Argument_CHAR_SIZE : Argument_INT_SIZE;
                    dictI = (static_cast<uint64_t>(flattenedArgumentTypes()[childTypeIndex + 12]) << 56) |
                            (static_cast<uint64_t>(flattenedArgumentTypes()[childTypeIndex + 11]) << 48) |
                            (static_cast<uint64_t>(flattenedArgumentTypes()[childTypeIndex + 10]) << 40) |
                            (static_cast<uint64_t>(flattenedArgumentTypes()[childTypeIndex + 9]) << 32) |
                            (static_cast<uint64_t>(flattenedArgumentTypes()[childTypeIndex + 8]) << 24) |
                            (static_cast<uint64_t>(flattenedArgumentTypes()[childTypeIndex + 7]) << 16) |
                            (static_cast<uint64_t>(flattenedArgumentTypes()[childTypeIndex + 6]) << 8) |
                            (static_cast<uint64_t>(flattenedArgumentTypes()[childTypeIndex + 5]));
                }
                auto prevChildTypeIndex = childTypeIndex;

                auto const spanFunctors =
                    std::unordered_map<ArgumentType, std::function<boss::expressions::ExpressionSpanArgument()>>{
                        {ArgumentType::ARGUMENT_TYPE_BOOL,
                         [&] {
                             std::vector<bool> data;
                             data.reserve(size);
                             size_t valsPerArg = sizeof(Argument) / Argument_BOOL_SIZE;
                             for (; childTypeIndex < prevChildTypeIndex + size;) {
                                 int64_t &arg = flattenedArguments()[childArgIndex++].asLong;
                                 uint64_t tmp = static_cast<uint64_t>(arg);
                                 for (int64_t i = valsPerArg - 1; i >= 0 && childTypeIndex < prevChildTypeIndex + size;
                                      i--, childTypeIndex++) {
                                     uint8_t val = static_cast<uint8_t>(
                                         (tmp >> (Argument_BOOL_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                     data.push_back(static_cast<bool>(val));
                                 }
                             }
                             // for(; childTypeIndex < prevChildTypeIndex + size; childTypeIndex++) {
                             //   auto const& arg = flattenedArguments()[childTypeIndex];
                             //   data.push_back(arg.asBool);
                             // }
                             return boss::expressions::Span<bool>(std::move(data));
                         }},
                        {ArgumentType::ARGUMENT_TYPE_CHAR,
                         [&] {
                             std::vector<int8_t> data;
                             data.reserve(size);
                             size_t valsPerArg = sizeof(Argument) / Argument_CHAR_SIZE;
                             for (; childTypeIndex < prevChildTypeIndex + size;) {
                                 int64_t &arg = flattenedArguments()[childArgIndex++].asLong;
                                 uint64_t tmp = static_cast<uint64_t>(arg);
                                 for (int64_t i = valsPerArg - 1; i >= 0 && childTypeIndex < prevChildTypeIndex + size;
                                      i--, childTypeIndex++) {
                                     uint8_t val = static_cast<uint8_t>(
                                         (tmp >> (Argument_CHAR_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                     data.push_back(static_cast<int8_t>(val));
                                 }
                             }
                             // for(; childTypeIndex < prevChildTypeIndex + size; childTypeIndex++) {
                             //   auto const& arg = flattenedArguments()[childTypeIndex];
                             //   data.push_back(arg.asChar);
                             // }
                             return boss::expressions::Span<int8_t>(std::move(data));
                         }},
                        {ArgumentType::ARGUMENT_TYPE_SHORT,
                         [&] {
                             std::vector<int16_t> data;
                             data.reserve(size);
                             size_t valsPerArg = sizeof(Argument) / Argument_SHORT_SIZE;
                             for (; childTypeIndex < prevChildTypeIndex + size;) {
                                 int64_t &arg = flattenedArguments()[childArgIndex++].asLong;
                                 uint64_t tmp = static_cast<uint64_t>(arg);
                                 for (int64_t i = valsPerArg - 1; i >= 0 && childTypeIndex < prevChildTypeIndex + size;
                                      i--, childTypeIndex++) {
                                     uint16_t val = static_cast<uint16_t>(
                                         (tmp >> (Argument_SHORT_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                     data.push_back(static_cast<int16_t>(val));
                                 }
                             }
                             return boss::expressions::Span<int16_t>(std::move(data));
                         }},
                        {ArgumentType::ARGUMENT_TYPE_INT,
                         [&] {
                             std::vector<int32_t> data;
                             data.reserve(size);
                             size_t valsPerArg = sizeof(Argument) / Argument_INT_SIZE;
                             for (; childTypeIndex < prevChildTypeIndex + size;) {
                                 int64_t &arg = flattenedArguments()[childArgIndex++].asLong;
                                 uint64_t tmp = static_cast<uint64_t>(arg);
                                 for (int64_t i = valsPerArg - 1; i >= 0 && childTypeIndex < prevChildTypeIndex + size;
                                      i--, childTypeIndex++) {
                                     uint32_t val = static_cast<uint32_t>(
                                         (tmp >> (Argument_INT_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                     data.push_back(static_cast<int32_t>(val));
                                 }
                             }
                             // for(; childTypeIndex < prevChildTypeIndex + size; childTypeIndex++) {
                             //   auto const& arg = flattenedArguments()[childTypeIndex];
                             //   data.push_back(arg.asInt);
                             // }
                             return boss::expressions::Span<int32_t>(std::move(data));
                         }},
                        {ArgumentType::ARGUMENT_TYPE_LONG,
                         [&] {
                             std::vector<int64_t> data;
                             data.reserve(size);
                             // std::cout << "PREV CHILD TYPE INDEX: " << prevChildTypeIndex << " SPAN
                             // SIZE: " << size << std::endl;
                             if (isDictEnc) {
                                 if (dictOffsetArgumentSize == Argument_CHAR_SIZE) {
                                     size_t valsPerArg = sizeof(Argument) / Argument_CHAR_SIZE;
                                     for (; childTypeIndex < prevChildTypeIndex + size;) {
                                         int64_t &arg = flattenedArguments()[childArgIndex++].asLong;
                                         uint64_t tmp = static_cast<uint64_t>(arg);
                                         for (int64_t i = valsPerArg - 1;
                                              i >= 0 && childTypeIndex < prevChildTypeIndex + size;
                                              i--, childTypeIndex++) {
                                             uint8_t dictOffset = static_cast<uint8_t>(
                                                 (tmp >> (Argument_CHAR_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                             auto const &arg =
                                                 spanDictionariesBuffer()[(dictI + static_cast<int8_t>(dictOffset))];
                                             data.push_back(arg.asLong);
                                         }
                                     }
                                 }
                                 else if (dictOffsetArgumentSize == Argument_INT_SIZE) {
                                     size_t valsPerArg = sizeof(Argument) / Argument_INT_SIZE;
                                     for (; childTypeIndex < prevChildTypeIndex + size;) {
                                         int64_t &arg = flattenedArguments()[childArgIndex++].asLong;
                                         uint64_t tmp = static_cast<uint64_t>(arg);
                                         for (int64_t i = valsPerArg - 1;
                                              i >= 0 && childTypeIndex < prevChildTypeIndex + size;
                                              i--, childTypeIndex++) {
                                             uint32_t dictOffset = static_cast<uint32_t>(
                                                 (tmp >> (Argument_INT_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                             auto const &arg =
                                                 spanDictionariesBuffer()[(dictI + static_cast<int32_t>(dictOffset))];
                                             data.push_back(arg.asLong);
                                         }
                                     }
                                 }
                             }
                             else {
                                 for (; childTypeIndex < prevChildTypeIndex + size; childTypeIndex++, childArgIndex++) {
                                     auto const &arg = flattenedArguments()[childArgIndex];
                                     data.push_back(arg.asLong);
                                 }
                             }
                             return boss::expressions::Span<int64_t>(std::move(data));
                         }},
                        {ArgumentType::ARGUMENT_TYPE_FLOAT,
                         [&] {
                             std::vector<float> data;
                             data.reserve(size);
                             size_t valsPerArg = sizeof(Argument) / Argument_FLOAT_SIZE;
                             for (; childTypeIndex < prevChildTypeIndex + size;) {
                                 int64_t &arg = flattenedArguments()[childArgIndex++].asLong;
                                 uint64_t tmp = static_cast<uint64_t>(arg);
                                 for (int64_t i = valsPerArg - 1; i >= 0 && childTypeIndex < prevChildTypeIndex + size;
                                      i--, childTypeIndex++) {
                                     uint32_t val = static_cast<uint32_t>(
                                         (tmp >> (Argument_FLOAT_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                     float realVal;
                                     std::memcpy(&realVal, &val, sizeof(realVal));
                                     data.push_back(realVal);
                                 }
                             }
                             // for(; childTypeIndex < prevChildTypeIndex + size; childTypeIndex++) {
                             //   auto const& arg = flattenedArguments()[childTypeIndex];
                             //   data.push_back(arg.asFloat);
                             // }
                             return boss::expressions::Span<float>(std::move(data));
                         }},
                        {ArgumentType::ARGUMENT_TYPE_DOUBLE,
                         [&] {
                             std::vector<double_t> data;
                             data.reserve(size);
                             if (isDictEnc) {
                                 if (dictOffsetArgumentSize == Argument_CHAR_SIZE) {
                                     size_t valsPerArg = sizeof(Argument) / Argument_CHAR_SIZE;
                                     for (; childTypeIndex < prevChildTypeIndex + size;) {
                                         int64_t &arg = flattenedArguments()[childArgIndex++].asLong;
                                         uint64_t tmp = static_cast<uint64_t>(arg);
                                         for (int64_t i = valsPerArg - 1;
                                              i >= 0 && childTypeIndex < prevChildTypeIndex + size;
                                              i--, childTypeIndex++) {
                                             uint8_t dictOffset = static_cast<uint8_t>(
                                                 (tmp >> (Argument_CHAR_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                             auto const &arg =
                                                 spanDictionariesBuffer()[(dictI + static_cast<int8_t>(dictOffset))];
                                             data.push_back(arg.asDouble);
                                         }
                                     }
                                 }
                                 else if (dictOffsetArgumentSize == Argument_INT_SIZE) {
                                     size_t valsPerArg = sizeof(Argument) / Argument_INT_SIZE;
                                     for (; childTypeIndex < prevChildTypeIndex + size;) {
                                         int64_t &arg = flattenedArguments()[childArgIndex++].asLong;
                                         uint64_t tmp = static_cast<uint64_t>(arg);
                                         for (int64_t i = valsPerArg - 1;
                                              i >= 0 && childTypeIndex < prevChildTypeIndex + size;
                                              i--, childTypeIndex++) {
                                             uint32_t dictOffset = static_cast<uint32_t>(
                                                 (tmp >> (Argument_INT_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                             auto const &arg =
                                                 spanDictionariesBuffer()[(dictI + static_cast<int32_t>(dictOffset))];
                                             data.push_back(arg.asDouble);
                                         }
                                     }
                                 }
                             }
                             else {
                                 for (; childTypeIndex < prevChildTypeIndex + size; childTypeIndex++, childArgIndex++) {
                                     auto const &arg = flattenedArguments()[childArgIndex];
                                     data.push_back(arg.asDouble);
                                 }
                             }
                             return boss::expressions::Span<double_t>(std::move(data));
                         }},
                        {ArgumentType::ARGUMENT_TYPE_SYMBOL,
                         [&childArgIndex, &childTypeIndex, &prevChildTypeIndex, &size, this] {
                             std::vector<boss::Symbol> data;
                             data.reserve(size);
                             auto spanArgument = boss::expressions::Span<boss::Symbol>();
                             for (; childTypeIndex < prevChildTypeIndex + size; childTypeIndex++, childArgIndex++) {
                                 auto const &arg = flattenedArguments()[childArgIndex];
                                 data.push_back(boss::Symbol(viewString(root, arg.asString)));
                             }
                             return boss::expressions::Span<boss::Symbol>(std::move(data));
                         }},
                        {ArgumentType::ARGUMENT_TYPE_STRING,
                         [&childArgIndex, &childTypeIndex, &prevChildTypeIndex, &size, &isDictEnc,
                          &dictOffsetArgumentSize, &dictI, this] {
                             std::vector<std::string> data;
                             data.reserve(size);
                             if (isDictEnc) {
                                 if (dictOffsetArgumentSize == Argument_CHAR_SIZE) {
                                     size_t valsPerArg = sizeof(Argument) / Argument_CHAR_SIZE;
                                     for (; childTypeIndex < prevChildTypeIndex + size;) {
                                         int64_t &arg = flattenedArguments()[childArgIndex++].asLong;
                                         uint64_t tmp = static_cast<uint64_t>(arg);
                                         for (int64_t i = valsPerArg - 1;
                                              i >= 0 && childTypeIndex < prevChildTypeIndex + size;
                                              i--, childTypeIndex++) {
                                             uint8_t dictOffset = static_cast<uint8_t>(
                                                 (tmp >> (Argument_CHAR_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                             auto const &arg =
                                                 spanDictionariesBuffer()[(dictI + static_cast<int8_t>(dictOffset))];
                                             data.push_back(std::string(viewString(root, arg.asString)));
                                         }
                                     }
                                 }
                                 else if (dictOffsetArgumentSize == Argument_INT_SIZE) {
                                     size_t valsPerArg = sizeof(Argument) / Argument_INT_SIZE;
                                     for (; childTypeIndex < prevChildTypeIndex + size;) {
                                         int64_t &arg = flattenedArguments()[childArgIndex++].asLong;
                                         uint64_t tmp = static_cast<uint64_t>(arg);
                                         for (int64_t i = valsPerArg - 1;
                                              i >= 0 && childTypeIndex < prevChildTypeIndex + size;
                                              i--, childTypeIndex++) {
                                             uint32_t dictOffset = static_cast<uint32_t>(
                                                 (tmp >> (Argument_INT_SIZE * sizeof(Argument) * i)) & 0xFFFFFFFFUL);
                                             auto const &arg =
                                                 spanDictionariesBuffer()[(dictI + static_cast<int32_t>(dictOffset))];
                                             data.push_back(std::string(viewString(root, arg.asString)));
                                         }
                                     }
                                 }
                             }
                             else {
                                 for (; childTypeIndex < prevChildTypeIndex + size; childTypeIndex++, childArgIndex++) {
                                     auto const &arg = flattenedArguments()[childArgIndex];
                                     data.push_back(std::string(viewString(root, arg.asString)));
                                 }
                             }
                             return boss::expressions::Span<std::string>(std::move(data));
                         }}};

                spanArguments.push_back(spanFunctors.at(argType)());
                childTypeIndex--;
                childArgIndex--;
            }
            else {
                auto const &arg = flattenedArguments()[childArgIndex];
                auto const functors = std::unordered_map<ArgumentType, std::function<boss::Expression()>>{
                    {ArgumentType::ARGUMENT_TYPE_BOOL, [&] { return (arg.asBool); }},
                    {ArgumentType::ARGUMENT_TYPE_CHAR, [&] { return (arg.asChar); }},
                    {ArgumentType::ARGUMENT_TYPE_SHORT, [&] { return (arg.asShort); }},
                    {ArgumentType::ARGUMENT_TYPE_INT, [&] { return (arg.asInt); }},
                    {ArgumentType::ARGUMENT_TYPE_LONG, [&] { return (arg.asLong); }},
                    {ArgumentType::ARGUMENT_TYPE_FLOAT, [&] { return (arg.asFloat); }},
                    {ArgumentType::ARGUMENT_TYPE_DOUBLE, [&] { return (arg.asDouble); }},
                    {ArgumentType::ARGUMENT_TYPE_SYMBOL,
                     [&arg, this] { return boss::Symbol(viewString(root, arg.asString)); }},
                    {ArgumentType::ARGUMENT_TYPE_EXPRESSION,
                     [&arg, this]() -> boss::Expression {
                         auto const &expr = expressionsBuffer()[arg.asExpression];
                         auto [args, spanArgs] =
                             deserializeArguments(expr.startChildOffset, expr.endChildOffset, expr.startChildTypeOffset,
                                                  expr.endChildTypeOffset);
                         auto result =
                             boss::expressions::ComplexExpression(boss::Symbol(viewString(root, expr.symbolNameOffset)),
                                                                  {}, std::move(args), std::move(spanArgs));
                         return result;
                     }},
                    {ArgumentType::ARGUMENT_TYPE_STRING,
                     [&arg, this] { return std::string(viewString(root, arg.asString)); }}};
                arguments.push_back(functors.at(type)());
            }
        }
        return std::make_pair(std::move(arguments), std::move(spanArguments));
    }

    template <typename... Types> class variant {
        size_t const *typeTag;
        void *value;

      public:
        variant(size_t const *typeTag, void *value) : typeTag(typeTag), value(value) {}
    };

    #pragma region LazilyDeSerializedBossExpression
    class LazilyDeSerializedBossExpression 
    {
        SerializedBossExpression const &buffer;
        size_t argumentIndex;
        size_t typeIndex;

        template <typename T> T as(Argument const &arg) const;
        template <> bool as<bool>(Argument const &arg) const { return arg.asBool; };
        template <> std::int8_t as<std::int8_t>(Argument const &arg) const { return arg.asChar; };
        template <> std::int16_t as<std::int16_t>(Argument const &arg) const { return arg.asShort; };
        template <> std::int32_t as<std::int32_t>(Argument const &arg) const { return arg.asInt; };
        template <> std::int64_t as<std::int64_t>(Argument const &arg) const { return arg.asLong; };
        template <> std::float_t as<std::float_t>(Argument const &arg) const { return arg.asFloat; };
        template <> std::double_t as<std::double_t>(Argument const &arg) const { return arg.asDouble; };
        template <> std::string as<std::string>(Argument const &arg) const
        {
            return viewString(buffer.root, arg.asString);
        };
        template <> boss::Symbol as<boss::Symbol>(Argument const &arg) const
        {
            return boss::Symbol(viewString(buffer.root, arg.asString));
        };

      public:
        LazilyDeSerializedBossExpression(SerializedBossExpression const &buffer, size_t argumentIndex, size_t typeIndex = 0)
            : buffer(buffer), argumentIndex(argumentIndex), typeIndex(typeIndex == 0 ? argumentIndex : typeIndex)
        {
        }

        size_t getArgumentIndex() const { return argumentIndex; }
        size_t getTypeIndex() const { return typeIndex; }

        bool operator==(boss::Expression const &other) const
        {
            if (other.index() != buffer.flattenedArgumentTypes()[typeIndex]) {
                return false;
            }
            auto const &argument = buffer.flattenedArguments()[argumentIndex];
            return std::visit(
                utilities::overload(
                    [&argument, this](boss::ComplexExpression const &e) {
                        auto expressionPosition = argument.asExpression;
                        assert(expressionPosition < buffer.expressionCount());
                        auto &startChildOffset = buffer.expressionsBuffer()[expressionPosition].startChildOffset;
                        auto &endChildOffset = buffer.expressionsBuffer()[expressionPosition].endChildOffset;
                        auto &startChildTypeOffset =
                            buffer.expressionsBuffer()[expressionPosition].startChildTypeOffset;
                        auto &endChildTypeOffset = buffer.expressionsBuffer()[expressionPosition].endChildTypeOffset;
                        auto numberOfChildrenTypes = endChildTypeOffset - startChildTypeOffset;
                        if (numberOfChildrenTypes != e.getArguments().size()) {
                            return false;
                        }
                        auto result = true;
                        auto argI = 0U;
                        auto typeI = 0U;
                        for (; typeI < e.getDynamicArguments().size(); typeI++, argI++) {
                            auto subExpressionPosition = startChildOffset + argI;
                            auto subExpressionTypePosition = startChildTypeOffset + typeI;
                            result &= (LazilyDeSerializedBossExpression(buffer, subExpressionPosition,
                                                                    subExpressionTypePosition) ==
                                       e.getDynamicArguments().at(typeI));
                        }
                        for (auto j = 0; j < e.getSpanArguments().size(); j++) {
                            std::visit(
                                [&](auto &&typedSpanArg) {
                                    auto subSpanPosition = startChildOffset + argI;
                                    auto subSpanTypePosition = startChildTypeOffset + typeI;
                                    auto currSpan =
                                        (LazilyDeSerializedBossExpression(buffer, subSpanPosition, subSpanTypePosition))
                                            .getCurrentExpressionAsSpan();
                                    result &= std::visit(
                                        [&](auto &&typedCurrSpan) {
                                            if (typedCurrSpan.size() != typedSpanArg.size()) {
                                                return false;
                                            }
                                            using Curr = std::decay_t<decltype(typedCurrSpan)>;
                                            using Other = std::decay_t<decltype(typedSpanArg)>;
                                            if constexpr (!is_same_v<Curr, Other>) {
                                                return false;
                                            }
                                            else {
                                                auto res = true;
                                                for (auto k = 0; k < typedCurrSpan.size(); k++) {
                                                    auto first = typedCurrSpan.at(k);
                                                    auto second = typedSpanArg.at(k);
                                                    res &= first == second;
                                                }
                                                return res;
                                            }
                                        },
                                        currSpan);
                                    typeI += typedSpanArg.size();
                                    const auto &arg0 = typedSpanArg[0];
                                    auto valsPerArg =
                                        sizeof(arg0) > sizeof(Argument) ? 1 : sizeof(Argument) / sizeof(arg0);
                                    argI += (typedSpanArg.size() + valsPerArg - 1) / valsPerArg;
                                },
                                e.getSpanArguments().at(j));
                        }
                        return result;
                    },
                    [&argument, this](auto v) { return as<decltype(v)>(argument) == v; }),
                other);
            ;
        }

        friend std::ostream &operator<<(std::ostream &stream, LazilyDeSerializedBossExpression lazyExpr)
        {
            lazyExpr.buffer.addIndexToStream(stream, lazyExpr.buffer, lazyExpr.argumentIndex, lazyExpr.typeIndex, -1,
                                             0);
            return stream;
        }

        ArgumentType getCurrentExpressionType() const
        {
            auto stopTypeIndex =
                typeIndex < (ArgumentType_RLE_MINIMUM_SIZE - 1) ? 0 : typeIndex - (ArgumentType_RLE_MINIMUM_SIZE - 1);
            auto testIndex = typeIndex;
            bool isRLE = (buffer.flattenedArgumentTypes()[testIndex] & ArgumentType_RLE_BIT) != 0u;
            while (!isRLE && testIndex >= 0 && testIndex > stopTypeIndex) {
                testIndex--;
                isRLE |= (buffer.flattenedArgumentTypes()[testIndex] & ArgumentType_RLE_BIT) != 0u;
            }
            auto validTypeIndex = isRLE ? testIndex : typeIndex;
            auto const &type = buffer.flattenedArgumentTypes()[validTypeIndex];
            return static_cast<ArgumentType>(type & ArgumentType_MASK);
        }

        ArgumentType getCurrentExpressionTypeExact() const
        {
            auto const &type = buffer.flattenedArgumentTypes()[typeIndex];
            return static_cast<ArgumentType>(type & ArgumentType_MASK);
        }

        // ALTER TO CHANGE TYPE OFFSET TOO
        LazilyDeSerializedBossExpression operator()(size_t childOffset, size_t childTypeOffset) const
        {
            auto const &expr = expression();
            // std::cout << "START CHILD OFFSET: " << expr.startChildOffset << std::endl;
            // std::cout << "END CHILD OFFSET: " << expr.endChildOffset << std::endl;
            // std::cout << "START CHILD TYPE OFFSET: " << expr.startChildTypeOffset << std::endl;
            // std::cout << "END CHILD TYPE OFFSET: " << expr.endChildTypeOffset << std::endl;
            assert(childOffset < expr.endChildOffset - expr.startChildOffset);
            assert(childTypeOffset < expr.endChildTypeOffset - expr.startChildTypeOffset);
            return {buffer, expr.startChildOffset + childOffset, expr.startChildTypeOffset + childTypeOffset};
        }

        LazilyDeSerializedBossExpression operator[](size_t childOffset) const
        {
            auto const &expr = expression();
            assert(childOffset < expr.endChildOffset - expr.startChildOffset);
            assert(childOffset < expr.endChildTypeOffset - expr.startChildTypeOffset);
            return {buffer, expr.startChildOffset + childOffset, expr.startChildTypeOffset + childOffset};
        }

        // MAYBE SHOULD USE getCurrentExpressionType()
        LazilyDeSerializedBossExpression operator[](std::string const &keyName) const
        {
            auto const &expr = expression();
            auto const &arguments = buffer.flattenedArguments();
            auto const &argumentTypes = buffer.flattenedArgumentTypes();
            auto const &expressions = buffer.expressionsBuffer();
            for (auto i = expr.startChildOffset, typeI = expr.startChildTypeOffset;
                 i < expr.endChildOffset && typeI < expr.endChildTypeOffset; ++i, ++typeI) {
                if (argumentTypes[typeI] != ArgumentType::ARGUMENT_TYPE_EXPRESSION) {
                    continue;
                }
                auto const &child = expressions[arguments[i].asExpression];
                auto const &key = viewString(buffer.root, child.symbolNameOffset);
                if (std::string_view{key} == keyName) {
                    return {buffer, i};
                }
            }
            throw std::runtime_error(keyName + " not found.");
        }

        // MAYBE SHOULD USE getCurrentExpressionType() as expressions can run
        Expression const &expression() const
        {
            auto const &arguments = buffer.flattenedArguments();
            auto const &argumentTypes = buffer.flattenedArgumentTypes();
            auto const &expressions = buffer.expressionsBuffer();
            assert(argumentTypes[typeIndex] == ArgumentType::ARGUMENT_TYPE_EXPRESSION);
            return expressions[arguments[argumentIndex].asExpression];
        }

        size_t getCurrentExpressionAsExpressionOffset() const
        {
            auto const &arguments = buffer.flattenedArguments();
            auto const &argumentTypes = buffer.flattenedArgumentTypes();
            assert(argumentTypes[typeIndex] == ArgumentType::ARGUMENT_TYPE_EXPRESSION);
            return arguments[argumentIndex].asExpression;
        }

        size_t getCurrentExpressionAsString(bool partOfRLE) const
        {
            auto const &type = getCurrentExpressionType();
            if (!partOfRLE) {
                assert(type == ArgumentType::ARGUMENT_TYPE_STRING || type == ArgumentType::ARGUMENT_TYPE_SYMBOL);
            }
            return buffer.flattenedArguments()[argumentIndex].asString;
        }

        bool currentIsExpression() const
        {
            auto const &argumentType = (buffer.flattenedArgumentTypes()[typeIndex] & ArgumentType_MASK);
            return argumentType == ArgumentType::ARGUMENT_TYPE_EXPRESSION;
        }

        size_t currentIsRLE() const
        {
            auto const &argumentTypes = buffer.flattenedArgumentTypes();
            auto const &type = argumentTypes[typeIndex];
            auto const &isRLE = (type & ArgumentType_RLE_BIT) != 0u;
            if (isRLE) {
                uint32_t size = (static_cast<uint32_t>(argumentTypes[typeIndex + 4]) << 24) |
                                (static_cast<uint32_t>(argumentTypes[typeIndex + 3]) << 16) |
                                (static_cast<uint32_t>(argumentTypes[typeIndex + 2]) << 8) |
                                (static_cast<uint32_t>(argumentTypes[typeIndex + 1]));
                // std::cout << "Size: " << size << std::endl;
                return size;
            }
            return 0;
        }

        std::pair<uint64_t, size_t> currentIsDictionaryEncoded() const
        {
            auto const &argumentTypes = buffer.flattenedArgumentTypes();
            auto const &type = argumentTypes[typeIndex];
            auto const &isDictEnc = (type & ArgumentType_DICT_ENC_BIT) != 0u;
            if (isDictEnc) {
                size_t dictOffsetArgumentSize =
                    (type & ArgumentType_DICT_ENC_SIZE_BIT) == 0u ? Argument_CHAR_SIZE : Argument_INT_SIZE;
                uint64_t dictI = (static_cast<uint64_t>(argumentTypes[typeIndex + 12]) << 56) |
                                 (static_cast<uint64_t>(argumentTypes[typeIndex + 11]) << 48) |
                                 (static_cast<uint64_t>(argumentTypes[typeIndex + 10]) << 40) |
                                 (static_cast<uint64_t>(argumentTypes[typeIndex + 9]) << 32) |
                                 (static_cast<uint64_t>(argumentTypes[typeIndex + 8]) << 24) |
                                 (static_cast<uint64_t>(argumentTypes[typeIndex + 7]) << 16) |
                                 (static_cast<uint64_t>(argumentTypes[typeIndex + 6]) << 8) |
                                 (static_cast<uint64_t>(argumentTypes[typeIndex + 5]));
                return {dictI, dictOffsetArgumentSize};
            }
            return {0, 0};
        }

        boss::Symbol getCurrentExpressionHead() const
        {
            auto const &expr = expression();
            return boss::Symbol(viewString(buffer.root, expr.symbolNameOffset));
        }

        template <typename IntT> static inline IntT extractField(uint64_t tmp, size_t shiftAmt)
        {
            return static_cast<IntT>((tmp >> shiftAmt) & 0xFFFFFFFFUL);
        }

        template <typename T, typename U>
        boss::Span<T> getCurrentExpressionAsSpanWithIndices(const std::vector<U> &indices) const
        {
            auto const &arguments = buffer.flattenedArguments();
            auto const &expr = expression();
            auto const &startChildOffset = expr.startChildOffset;

            const size_t n = indices.size();
            constexpr size_t valsPerArg = sizeof(T) > sizeof(Argument) ? 1 : sizeof(Argument) / sizeof(T);
            constexpr size_t shiftAmt = 
                sizeof(T) > sizeof(Argument) 
                ? sizeof(Argument) * sizeof(Argument)   // NOLINT(bugprone-sizeof-expression)
                : sizeof(Argument) * sizeof(T);         // NOLINT(bugprone-sizeof-expression)

            constexpr size_t valsPerArgMask = valsPerArg - 1;
            constexpr size_t valsPerArgShift = [] {
                size_t s = 0;
                size_t v = valsPerArg;
                while ((v >>= 1) > 0)
                    ++s;
                return s;
            }();

            if constexpr (std::is_same_v<T, boss::Symbol>) {
                std::vector<T> data;
                data.reserve(n);
                for (size_t i = 0; i < n; i++) {
                    const auto &index = indices[i];
                    size_t childOffset = index >> valsPerArgShift;
                    int64_t inArgI = valsPerArg - 1 - (index & valsPerArgMask);

                    auto &arg = arguments[startChildOffset + childOffset];
                    uint64_t tmp = static_cast<uint64_t>(arg.asLong);
                    data.emplace_back(viewString(buffer.root, arg.asString));
                }
                return boss::expressions::Span<T>(std::move(data));
            }
            else {
                std::vector<T> data(n);

                for (size_t i = 0; i < n; i++) {
                    const auto &index = indices[i];
                    size_t childOffset = index >> valsPerArgShift;
                    int64_t inArgI = valsPerArg - 1 - (index & valsPerArgMask);

                    auto &arg = arguments[startChildOffset + childOffset];
                    uint64_t tmp = static_cast<uint64_t>(arg.asLong);

                    if constexpr (std::is_same_v<T, bool>) {
                        data[i] = static_cast<bool>(extractField<uint8_t>(tmp, shiftAmt * inArgI));
                    }
                    else if constexpr (std::is_same_v<T, int8_t>) {
                        data[i] = static_cast<int8_t>(extractField<uint8_t>(tmp, shiftAmt * inArgI));
                    }
                    else if constexpr (std::is_same_v<T, int16_t>) {
                        data[i] = static_cast<int16_t>(extractField<uint16_t>(tmp, shiftAmt * inArgI));
                    }
                    else if constexpr (std::is_same_v<T, int32_t>) {
                        data[i] = static_cast<int32_t>(extractField<uint32_t>(tmp, shiftAmt * inArgI));
                    }
                    else if constexpr (std::is_same_v<T, float_t>) {
                        uint32_t val = extractField<uint32_t>(tmp, shiftAmt * inArgI);
                        union {
                            uint32_t i;
                            float f;
                        } u;
                        u.i = val;
                        data[i] = u.f;
                    }
                    else if constexpr (std::is_same_v<T, int64_t>) {
                        data[i] = arg.asLong;
                    }
                    else if constexpr (std::is_same_v<T, double_t>) {
                        data[i] = arg.asDouble;
                    }
                    else if constexpr (std::is_same_v<T, std::string>) {
                        data[i] = std::string(viewString(buffer.root, arg.asString));
                    }
                    else {
                        static_assert(sizeof(T) == 0, "Unsupported type in getCurrentExpressionAsSpanWithIndices<T>()");
                    }
                }
                return boss::expressions::Span<T>(std::move(data));
            }
        }

        template <typename T>
        boss::expressions::ExpressionSpanArgument
        getCurrentExpressionAsSpanWithIndices(ArgumentType type, const std::vector<T> &indices) const
        {
            switch (type) {
                case ArgumentType::ARGUMENT_TYPE_BOOL:
                    return getCurrentExpressionAsSpanWithIndices<bool, T>(indices);
                case ArgumentType::ARGUMENT_TYPE_CHAR:
                    return getCurrentExpressionAsSpanWithIndices<int8_t, T>(indices);
                case ArgumentType::ARGUMENT_TYPE_SHORT:
                    return getCurrentExpressionAsSpanWithIndices<int16_t, T>(indices);
                case ArgumentType::ARGUMENT_TYPE_INT:
                    return getCurrentExpressionAsSpanWithIndices<int32_t, T>(indices);
                case ArgumentType::ARGUMENT_TYPE_LONG:
                    return getCurrentExpressionAsSpanWithIndices<int64_t, T>(indices);
                case ArgumentType::ARGUMENT_TYPE_FLOAT:
                    return getCurrentExpressionAsSpanWithIndices<float_t, T>(indices);
                case ArgumentType::ARGUMENT_TYPE_DOUBLE:
                    return getCurrentExpressionAsSpanWithIndices<double_t, T>(indices);
                case ArgumentType::ARGUMENT_TYPE_STRING:
                    return getCurrentExpressionAsSpanWithIndices<std::string, T>(indices);
                case ArgumentType::ARGUMENT_TYPE_SYMBOL:
                    return getCurrentExpressionAsSpanWithIndices<boss::Symbol, T>(indices);
                case ArgumentType::ARGUMENT_TYPE_EXPRESSION:
                    break;
                default: 
                    throw std::runtime_error("Invalid type found in getCurrentExpressionAsSpanWithIndices");
            }
            throw std::runtime_error("Invalid type in getCurrentExpressionAsSpanWithIndices");
        }

        boss::expressions::ExpressionSpanArgument getCurrentExpressionAsSpanWithTypeAndSize(
            ArgumentType type, size_t size) const
        {
            auto const &arguments = buffer.flattenedArguments();
            auto const spanFunctors =
                std::unordered_map<ArgumentType, std::function<boss::expressions::ExpressionSpanArgument()>>{
                    {ArgumentType::ARGUMENT_TYPE_BOOL,
                     [&] {
                        std::vector<bool> data(size);
                        constexpr size_t valsPerArg = sizeof(Argument) / Argument_BOOL_SIZE;
                        constexpr size_t shiftAmt = sizeof(Argument) * Argument_BOOL_SIZE;
                        auto tempI = 0;
                        for (size_t i = 0; i < size; tempI++) {
                            int64_t &arg = arguments[argumentIndex + tempI].asLong;
                            uint64_t tmp = static_cast<uint64_t>(arg);
                            for (int64_t j = valsPerArg - 1; j >= 0 && i < size; j--, i++) {
                                uint8_t val = static_cast<uint8_t>((tmp >> (shiftAmt * j)) & 0xFFFFFFFFUL);
                                data[i] = static_cast<bool>(val);
                            }
                        }
                        // for(size_t i = 0; i < size; i++) {
                        //   auto const& arg = arguments[argumentIndex + i];
                        //   data.push_back(arg.asBool);
                        // }
                        return boss::expressions::Span<bool>(std::move(data));
                     }},
                    {ArgumentType::ARGUMENT_TYPE_CHAR,
                     [&] {
                         auto base64 = &arguments[argumentIndex];
                         auto base = reinterpret_cast<int8_t *>(base64);
                         return boss::expressions::Span<int8_t>(base, size, nullptr);
                     }},
                    {ArgumentType::ARGUMENT_TYPE_SHORT,
                     [&] {
                         auto base64 = &arguments[argumentIndex];
                         auto base = reinterpret_cast<int16_t *>(base64);
                         return boss::expressions::Span<int16_t>(base, size, nullptr);
                     }},
                    {ArgumentType::ARGUMENT_TYPE_INT,
                     [&] {
                         auto base64 = &arguments[argumentIndex];
                         auto base = reinterpret_cast<int32_t *>(base64);
                         return boss::expressions::Span<int32_t>(base, size, nullptr);
                     }},
                    {ArgumentType::ARGUMENT_TYPE_LONG,
                     [&] {
                         auto base64 = &arguments[argumentIndex];
                         auto base = reinterpret_cast<int64_t *>(base64);
                         return boss::expressions::Span<int64_t>(base, size, nullptr);
                     }},
                    {ArgumentType::ARGUMENT_TYPE_FLOAT,
                     [&] {
                         auto base64 = &arguments[argumentIndex];
                         auto base = reinterpret_cast<float_t *>(base64);
                         return boss::expressions::Span<float_t>(base, size, nullptr);
                     }},
                    {ArgumentType::ARGUMENT_TYPE_DOUBLE,
                     [&]{
                         auto base64 = &arguments[argumentIndex];
                         auto base = reinterpret_cast<double_t *>(base64);
                         return boss::expressions::Span<double_t>(base, size, nullptr);
                     }},
                    {ArgumentType::ARGUMENT_TYPE_STRING,
                     [&] {
                         std::vector<std::string> data(size);
                         for (size_t i = 0; i < size; i++) {
                             auto const &arg = arguments[argumentIndex + i];
                             data[i] = std::string(viewString(buffer.root, arg.asString));
                         }
                         return boss::expressions::Span<std::string>(std::move(data));
                     }},
                    {ArgumentType::ARGUMENT_TYPE_SYMBOL, [&] {
                         std::vector<boss::Symbol> data;
                         data.reserve(size);
                         for (size_t i = 0; i < size; i++) {
                             auto const &arg = arguments[argumentIndex + i];
                             data.emplace_back(viewString(buffer.root, arg.asString));
                         }
                         return boss::expressions::Span<boss::Symbol>(std::move(data));
                     }}};
            return spanFunctors.at(type)();
        }

        boss::expressions::ExpressionSpanArgument getCurrentExpressionAsSpanWithTypeAndSizeWithCopy(ArgumentType type,
                                                                                                    size_t size) const
        {
            auto const &arguments = buffer.flattenedArguments();
            auto const spanFunctors =
                std::unordered_map<ArgumentType, std::function<boss::expressions::ExpressionSpanArgument()>>{
                    {ArgumentType::ARGUMENT_TYPE_BOOL,
                     [&] {
                         std::vector<bool> data(size);
                         constexpr size_t valsPerArg = sizeof(Argument) / Argument_BOOL_SIZE;
                         constexpr size_t shiftAmt = sizeof(Argument) * Argument_BOOL_SIZE;
                         auto tempI = 0;
                         for (size_t i = 0; i < size; tempI++) {
                             int64_t &arg = arguments[argumentIndex + tempI].asLong;
                             uint64_t tmp = static_cast<uint64_t>(arg);
                             for (int64_t j = valsPerArg - 1; j >= 0 && i < size; j--, i++) {
                                 uint8_t val = static_cast<uint8_t>((tmp >> (shiftAmt * j)) & 0xFFFFFFFFUL);
                                 data[i] = static_cast<bool>(val);
                             }
                         }
                         // for(size_t i = 0; i < size; i++) {
                         //   auto const& arg = arguments[argumentIndex + i];
                         //   data.push_back(arg.asBool);
                         // }
                         return boss::expressions::Span<bool>(std::move(data));
                     }},
                    {ArgumentType::ARGUMENT_TYPE_CHAR,
                     [&] {
                         std::vector<int8_t> data(size);
                         constexpr size_t valsPerArg = sizeof(Argument) / Argument_CHAR_SIZE;
                         constexpr size_t shiftAmt = sizeof(Argument) * Argument_CHAR_SIZE;
                         auto tempI = 0;
                         for (size_t i = 0; i < size; tempI++) {
                             int64_t &arg = arguments[argumentIndex + tempI].asLong;
                             uint64_t tmp = static_cast<uint64_t>(arg);
                             for (int64_t j = valsPerArg - 1; j >= 0 && i < size; j--, i++) {
                                 uint8_t val = static_cast<uint8_t>((tmp >> (shiftAmt * j)) & 0xFFFFFFFFUL);
                                 data[i] = static_cast<int8_t>(val);
                             }
                         }
                         return boss::expressions::Span<int8_t>(std::move(data));
                     }},
                    {ArgumentType::ARGUMENT_TYPE_SHORT,
                     [&] {
                         std::vector<int16_t> data(size);
                         constexpr size_t valsPerArg = sizeof(Argument) / Argument_SHORT_SIZE;
                         constexpr size_t shiftAmt = sizeof(Argument) * Argument_SHORT_SIZE;
                         auto tempI = 0;
                         for (size_t i = 0; i < size; tempI++) {
                             int64_t &arg = arguments[argumentIndex + tempI].asLong;
                             uint64_t tmp = static_cast<uint64_t>(arg);
                             for (int64_t j = valsPerArg - 1; j >= 0 && i < size; j--, i++) {
                                 uint16_t val = static_cast<uint16_t>((tmp >> (shiftAmt * j)) & 0xFFFFFFFFUL);
                                 data[i] = static_cast<int16_t>(val);
                             }
                         }
                         return boss::expressions::Span<int16_t>(std::move(data));
                     }},
                    {ArgumentType::ARGUMENT_TYPE_INT,
                     [&] {
                         std::vector<int32_t> data(size);
                         constexpr size_t valsPerArg = sizeof(Argument) / Argument_INT_SIZE;
                         constexpr size_t shiftAmt = sizeof(Argument) * Argument_INT_SIZE;
                         auto tempI = 0;
                         for (size_t i = 0; i < size; tempI++) {
                             int64_t &arg = arguments[argumentIndex + tempI].asLong;
                             uint64_t tmp = static_cast<uint64_t>(arg);
                             for (int64_t j = valsPerArg - 1; j >= 0 && i < size; j--, i++) {
                                 uint32_t val = static_cast<uint32_t>((tmp >> (shiftAmt * j)) & 0xFFFFFFFFUL);
                                 data[i] = static_cast<int32_t>(val);
                             }
                         }
                         return boss::expressions::Span<int32_t>(std::move(data));
                     }},
                    {ArgumentType::ARGUMENT_TYPE_LONG,
                     [&] {
                         std::vector<int64_t> data(size);
                         for (size_t i = 0; i < size; i++) {
                             auto const &arg = arguments[argumentIndex + i];
                             data[i] = arg.asLong;
                         }
                         return boss::expressions::Span<int64_t>(std::move(data));
                     }},
                    {ArgumentType::ARGUMENT_TYPE_FLOAT,
                     [&] {
                         std::vector<float_t> data(size);
                         constexpr size_t valsPerArg = sizeof(Argument) / Argument_FLOAT_SIZE;
                         constexpr size_t shiftAmt = sizeof(Argument) * Argument_FLOAT_SIZE;
                         auto tempI = 0;
                         for (size_t i = 0; i < size; tempI++) {
                             int64_t &arg = arguments[argumentIndex + tempI].asLong;
                             uint64_t tmp = static_cast<uint64_t>(arg);
                             for (int64_t j = valsPerArg - 1; j >= 0 && i < size; j--, i++) {
                                 uint32_t val = static_cast<uint32_t>((tmp >> (shiftAmt * j)) & 0xFFFFFFFFUL);
                                 union {
                                     uint32_t i;
                                     float f;
                                 } u;
                                 u.i = val;
                                 // float realVal;
                                 // std::memcpy(&realVal, &val, sizeof(realVal));
                                 data[i] = u.f;
                             }
                         }
                         // for(size_t i = 0; i < size; i++) {
                         //   auto const& arg = arguments[argumentIndex + i];
                         //   data[i] = arg.asFloat);
                         // }
                         return boss::expressions::Span<float_t>(std::move(data));
                     }},
                    {ArgumentType::ARGUMENT_TYPE_DOUBLE,
                     [&] {
                         std::vector<double_t> data(size);
                         for (size_t i = 0; i < size; i++) {
                             auto const &arg = arguments[argumentIndex + i];
                             data[i] = arg.asDouble;
                         }
                         return boss::expressions::Span<double_t>(std::move(data));
                     }},
                    {ArgumentType::ARGUMENT_TYPE_STRING,
                     [&] {
                         std::vector<std::string> data(size);
                         for (size_t i = 0; i < size; i++) {
                             auto const &arg = arguments[argumentIndex + i];
                             data[i] = std::string(viewString(buffer.root, arg.asString));
                         }
                         return boss::expressions::Span<std::string>(std::move(data));
                     }},
                    {ArgumentType::ARGUMENT_TYPE_SYMBOL, [&] {
                         std::vector<boss::Symbol> data;
                         data.reserve(size);
                         for (size_t i = 0; i < size; i++) {
                             auto const &arg = arguments[argumentIndex + i];
                             data.emplace_back(viewString(buffer.root, arg.asString));
                         }
                         return boss::expressions::Span<boss::Symbol>(std::move(data));
                     }}};
            return spanFunctors.at(type)();
        }

        boss::expressions::ExpressionSpanArgument
        getCurrentExpressionAsDictEncodedSpanWithTypeAndSize(ArgumentType type, size_t size, uint64_t dictI,
                                                             size_t dictOffsetArgumentSize) const
        {
            auto const &arguments = buffer.flattenedArguments();
            auto const &dicts = buffer.spanDictionariesBuffer();
            auto const spanFunctors =
                std::unordered_map<ArgumentType, std::function<boss::expressions::ExpressionSpanArgument()>>{
                    {ArgumentType::ARGUMENT_TYPE_LONG,
                     [&] {
                         std::vector<int64_t> data(size);
                         if (dictOffsetArgumentSize == Argument_CHAR_SIZE) {
                             constexpr size_t valsPerArg = sizeof(Argument) / Argument_CHAR_SIZE;
                             constexpr size_t shiftAmt = sizeof(Argument) * Argument_CHAR_SIZE;
                             auto tempI = 0;
                             for (size_t i = 0; i < size; tempI++) {
                                 int64_t &arg = arguments[argumentIndex + tempI].asLong;
                                 uint64_t tmp = static_cast<uint64_t>(arg);
                                 for (int64_t j = valsPerArg - 1; j >= 0 && i < size; j--, i++) {
                                     uint8_t dictOffset = static_cast<uint8_t>((tmp >> (shiftAmt * j)) & 0xFFFFFFFFUL);
                                     auto const &arg = dicts[(dictI + static_cast<int8_t>(dictOffset))];
                                     data[i] = arg.asLong;
                                 }
                             }
                         }
                         else if (dictOffsetArgumentSize == Argument_INT_SIZE) {
                             constexpr size_t valsPerArg = sizeof(Argument) / Argument_INT_SIZE;
                             constexpr size_t shiftAmt = sizeof(Argument) * Argument_INT_SIZE;
                             auto tempI = 0;
                             for (size_t i = 0; i < size; tempI++) {
                                 int64_t &arg = arguments[argumentIndex + tempI].asLong;
                                 uint64_t tmp = static_cast<uint64_t>(arg);
                                 for (int64_t j = valsPerArg - 1; j >= 0 && i < size; j--, i++) {
                                     uint32_t dictOffset =
                                         static_cast<uint32_t>((tmp >> (shiftAmt * j)) & 0xFFFFFFFFUL);
                                     auto const &arg = dicts[(dictI + static_cast<int32_t>(dictOffset))];
                                     data[i] = arg.asLong;
                                 }
                             }
                         }
                         return boss::expressions::Span<int64_t>(std::move(data));
                     }},
                    {ArgumentType::ARGUMENT_TYPE_DOUBLE,
                     [&] {
                         std::vector<double> data(size);
                         if (dictOffsetArgumentSize == Argument_CHAR_SIZE) {
                             constexpr size_t valsPerArg = sizeof(Argument) / Argument_CHAR_SIZE;
                             constexpr size_t shiftAmt = sizeof(Argument) * Argument_CHAR_SIZE;
                             auto tempI = 0;
                             for (size_t i = 0; i < size; tempI++) {
                                 int64_t &arg = arguments[argumentIndex + tempI].asLong;
                                 uint64_t tmp = static_cast<uint64_t>(arg);
                                 for (int64_t j = valsPerArg - 1; j >= 0 && i < size; j--, i++) {
                                     uint8_t dictOffset = static_cast<uint8_t>((tmp >> (shiftAmt * j)) & 0xFFFFFFFFUL);
                                     auto const &arg = dicts[(dictI + static_cast<int8_t>(dictOffset))];
                                     data[i] = arg.asDouble;
                                 }
                             }
                         }
                         else if (dictOffsetArgumentSize == Argument_INT_SIZE) {
                             constexpr size_t valsPerArg = sizeof(Argument) / Argument_INT_SIZE;
                             constexpr size_t shiftAmt = sizeof(Argument) * Argument_INT_SIZE;
                             auto tempI = 0;
                             for (size_t i = 0; i < size; tempI++) {
                                 int64_t &arg = arguments[argumentIndex + tempI].asLong;
                                 uint64_t tmp = static_cast<uint64_t>(arg);
                                 for (int64_t j = valsPerArg - 1; j >= 0 && i < size; j--, i++) {
                                     uint32_t dictOffset =
                                         static_cast<uint32_t>((tmp >> (shiftAmt * j)) & 0xFFFFFFFFUL);
                                     auto const &arg = dicts[(dictI + static_cast<int32_t>(dictOffset))];
                                     data[i] = arg.asDouble;
                                 }
                             }
                         }
                         return boss::expressions::Span<double>(std::move(data));
                     }},
                    {ArgumentType::ARGUMENT_TYPE_STRING, [&] {
                         std::vector<std::string> data(size);
                         if (dictOffsetArgumentSize == Argument_CHAR_SIZE) {
                             constexpr size_t valsPerArg = sizeof(Argument) / Argument_CHAR_SIZE;
                             constexpr size_t shiftAmt = sizeof(Argument) * Argument_CHAR_SIZE;
                             auto tempI = 0;
                             for (size_t i = 0; i < size; tempI++) {
                                 int64_t &arg = arguments[argumentIndex + tempI].asLong;
                                 uint64_t tmp = static_cast<uint64_t>(arg);
                                 for (int64_t j = valsPerArg - 1; j >= 0 && i < size; j--, i++) {
                                     uint8_t dictOffset = static_cast<uint8_t>((tmp >> (shiftAmt * j)) & 0xFFFFFFFFUL);
                                     auto const &arg = dicts[(dictI + static_cast<int8_t>(dictOffset))];
                                     data[i] = std::string(viewString(buffer.root, arg.asString));
                                 }
                             }
                         }
                         else if (dictOffsetArgumentSize == Argument_INT_SIZE) {
                             constexpr size_t valsPerArg = sizeof(Argument) / Argument_INT_SIZE;
                             constexpr size_t shiftAmt = sizeof(Argument) * Argument_INT_SIZE;
                             auto tempI = 0;
                             for (size_t i = 0; i < size; tempI++) {
                                 int64_t &arg = arguments[argumentIndex + tempI].asLong;
                                 uint64_t tmp = static_cast<uint64_t>(arg);
                                 for (int64_t j = valsPerArg - 1; j >= 0 && i < size; j--, i++) {
                                     uint32_t dictOffset =
                                         static_cast<uint32_t>((tmp >> (shiftAmt * j)) & 0xFFFFFFFFUL);
                                     auto const &arg = dicts[(dictI + static_cast<int32_t>(dictOffset))];
                                     data[i] = std::string(viewString(buffer.root, arg.asString));
                                 }
                             }
                         }
                         return boss::expressions::Span<std::string>(std::move(data));
                     }}};
            return spanFunctors.at(type)();
        }

        boss::expressions::ExpressionSpanArgument getCurrentExpressionAsSpan() const
        {
            size_t size = currentIsRLE();
            assert(size != 0);
            auto [dictI, dictOffsetArgSize] = currentIsDictionaryEncoded();
            auto const &type = getCurrentExpressionType();
            if (dictOffsetArgSize == Argument_CHAR_SIZE || dictOffsetArgSize == Argument_INT_SIZE) {
                return std::move(
                    getCurrentExpressionAsDictEncodedSpanWithTypeAndSize(type, size, dictI, dictOffsetArgSize));
            }
            return std::move(getCurrentExpressionAsSpanWithTypeAndSize(type, size));
        }

        boss::expressions::ExpressionSpanArgument getCurrentExpressionAsSpanWithCopy() const
        {
            size_t size = currentIsRLE();
            assert(size != 0);
            auto [dictI, dictOffsetArgSize] = currentIsDictionaryEncoded();
            auto const &type = getCurrentExpressionType();
            if (dictOffsetArgSize == Argument_CHAR_SIZE || dictOffsetArgSize == Argument_INT_SIZE) {
                return std::move(
                    getCurrentExpressionAsDictEncodedSpanWithTypeAndSize(type, size, dictI, dictOffsetArgSize));
            }
            return std::move(getCurrentExpressionAsSpanWithTypeAndSizeWithCopy(type, size));
        }

        template <typename T> inline T getCurrentExpressionInSpanAtAs(size_t spanArgI) const
        {
            auto &argument = buffer.flattenedArguments()[argumentIndex];
            uint64_t tmp = static_cast<uint64_t>(argument.asLong);

            if constexpr (std::is_same_v<T, bool>) {
                constexpr size_t valsPerArg = sizeof(Argument) / Argument_BOOL_SIZE;
                constexpr size_t shiftAmt = sizeof(Argument) * Argument_BOOL_SIZE;
                int64_t inArgI = valsPerArg - 1 - (spanArgI % valsPerArg);
                return static_cast<bool>(extractField<uint8_t>(tmp, shiftAmt * inArgI));
                // uint32_t val = static_cast<uint8_t>((shiftAmt * inArgI) & 0xFFFFFFFFUL);
                // return static_cast<bool>(val);
            }
            else if constexpr (std::is_same_v<T, int8_t>) {
                constexpr size_t valsPerArg = sizeof(Argument) / Argument_CHAR_SIZE;
                constexpr size_t shiftAmt = sizeof(Argument) * Argument_CHAR_SIZE;
                int64_t inArgI = valsPerArg - 1 - (spanArgI % valsPerArg);
                return static_cast<int8_t>(extractField<uint8_t>(tmp, shiftAmt * inArgI));
            }
            else if constexpr (std::is_same_v<T, int16_t>) {
                constexpr size_t valsPerArg = sizeof(Argument) / Argument_SHORT_SIZE;
                constexpr size_t shiftAmt = sizeof(Argument) * Argument_SHORT_SIZE;
                int64_t inArgI = valsPerArg - 1 - (spanArgI % valsPerArg);
                return static_cast<int16_t>(extractField<uint16_t>(tmp, shiftAmt * inArgI));
            }
            else if constexpr (std::is_same_v<T, int32_t>) {
                constexpr size_t valsPerArg = sizeof(Argument) / Argument_INT_SIZE;
                constexpr size_t shiftAmt = sizeof(Argument) * Argument_INT_SIZE;
                int64_t inArgI = valsPerArg - 1 - (spanArgI % valsPerArg);
                return static_cast<int32_t>(extractField<uint32_t>(tmp, shiftAmt * inArgI));
            }
            else if constexpr (std::is_same_v<T, int64_t>) {
                return argument.asLong;
            }
            else if constexpr (std::is_same_v<T, float_t>) {
                constexpr size_t valsPerArg = sizeof(Argument) / Argument_FLOAT_SIZE;
                constexpr size_t shiftAmt = sizeof(Argument) * Argument_FLOAT_SIZE;
                int64_t inArgI = valsPerArg - 1 - (spanArgI % valsPerArg);
                uint32_t val = extractField<uint32_t>(tmp, shiftAmt * inArgI);
                union {
                    int32_t i;
                    float f;
                } u;
                u.i = val;
                return u.f;
            }
            else if constexpr (std::is_same_v<T, double_t>) {
                return argument.asDouble;
            }
            else if constexpr (std::is_same_v<T, std::string>) {
                return viewString(buffer.root, argument.asString);
            }
            else if constexpr (std::is_same_v<T, boss::Symbol>) {
                return boss::Symbol(viewString(buffer.root, argument.asString));
            }
            else {
                static_assert(sizeof(T) == 0, "Unsupported type passes to getCurrentExpressionInSpanAtAs<T>()");
            }
        }

        inline boss::Expression getCurrentExpressionInSpanAtAs(size_t spanArgI, ArgumentType argumentType) const
        {
            // std::cout << "ARGI: " << argumentIndex << " TYPEI: " << typeIndex << " ARG TYPE: " <<
            // static_cast<int32_t>(argumentType) << std::endl;
            switch (argumentType) {
                case ArgumentType::ARGUMENT_TYPE_BOOL:
                    return getCurrentExpressionInSpanAtAs<bool>(spanArgI);
                case ArgumentType::ARGUMENT_TYPE_CHAR:
                    return getCurrentExpressionInSpanAtAs<int8_t>(spanArgI);
                case ArgumentType::ARGUMENT_TYPE_SHORT:
                    return getCurrentExpressionInSpanAtAs<int16_t>(spanArgI);
                case ArgumentType::ARGUMENT_TYPE_INT:
                    return getCurrentExpressionInSpanAtAs<int32_t>(spanArgI);
                case ArgumentType::ARGUMENT_TYPE_LONG:
                    return getCurrentExpressionInSpanAtAs<int64_t>(spanArgI);
                case ArgumentType::ARGUMENT_TYPE_FLOAT:
                    return getCurrentExpressionInSpanAtAs<float_t>(spanArgI);
                case ArgumentType::ARGUMENT_TYPE_DOUBLE:
                    return getCurrentExpressionInSpanAtAs<double_t>(spanArgI);
                case ArgumentType::ARGUMENT_TYPE_STRING:
                    return getCurrentExpressionInSpanAtAs<std::string>(spanArgI);
                case ArgumentType::ARGUMENT_TYPE_SYMBOL:
                    return getCurrentExpressionInSpanAtAs<boss::Symbol>(spanArgI);
                case ArgumentType::ARGUMENT_TYPE_EXPRESSION:
                    break;
                case ArgumentType::ARGUMENT_TYPE_BYTE_ARRAY: 
                    throw std::runtime_error("Invalid argument type in getCurrentExpressionInSpanAtAs");
            }
            return "ErrorDeserialisingExpressionInSpan"_(
                "ArgumentIndex"_(static_cast<int64_t>(argumentIndex)), "TypeIndex"_(static_cast<int64_t>(typeIndex)),
                "InSpanIndex"_(static_cast<int64_t>(spanArgI)), "ArgumentType"_(static_cast<int64_t>(argumentType)));
        }

        template <typename T>
        T getCurrentExpressionInDictEncodedSpanAtAs(size_t spanArgI, uint64_t dictI,
                                                    size_t dictOffsetArgumentSize) const
        {
            auto &argument = buffer.flattenedArguments()[argumentIndex];
            uint64_t tmp = static_cast<uint64_t>(argument.asLong);
            size_t valsPerArg = sizeof(Argument) / dictOffsetArgumentSize;
            int64_t inArgI = valsPerArg - 1 - (spanArgI % valsPerArg);

            int32_t dictOffset;
            if (dictOffsetArgumentSize == Argument_CHAR_SIZE) {
                uint8_t val =
                    static_cast<uint8_t>((tmp >> (dictOffsetArgumentSize * sizeof(Argument) * inArgI)) & 0xFFFFFFFFUL);
                dictOffset = static_cast<int32_t>(static_cast<int8_t>(val));
            }
            else if (dictOffsetArgumentSize == Argument_CHAR_SIZE) {
                uint32_t val =
                    static_cast<uint32_t>((tmp >> (dictOffsetArgumentSize * sizeof(Argument) * inArgI)) & 0xFFFFFFFFUL);
                dictOffset = static_cast<int32_t>(val);
            }
            auto &dictArg = buffer.spanDictionariesBuffer()[(dictI + dictOffset)];
            if constexpr (std::is_same_v<T, int64_t>) {
                return dictArg.asLong;
            }
            else if constexpr (std::is_same_v<T, double_t>) {
                return dictArg.asDouble;
            }
            else if constexpr (std::is_same_v<T, std::string>) {
                return viewString(buffer.root, dictArg.asString);
            }
            else {
                static_assert(sizeof(T) == 0,
                              "Unsupported type passes to getCurrentExpressionInDictEncodedSpanAtAs<T>()");
            }
        }

        boss::Expression getCurrentExpressionInDictEncodedSpanAtAs(size_t spanArgI, uint64_t dictI,
                                                                   size_t dictOffsetArgumentSize,
                                                                   ArgumentType argumentType) const
        {
            // std::cout << "ARGI: " << argumentIndex << " TYPEI: " << typeIndex << std::endl;
            auto &argument = buffer.flattenedArguments()[argumentIndex];
            uint64_t tmp = static_cast<uint64_t>(argument.asLong);
            size_t valsPerArg = sizeof(Argument) / dictOffsetArgumentSize;
            int64_t inArgI = valsPerArg - 1 - (spanArgI % valsPerArg);

            int32_t dictOffset;
            if (dictOffsetArgumentSize == Argument_CHAR_SIZE) {
                uint8_t val =
                    static_cast<uint8_t>((tmp >> (dictOffsetArgumentSize * sizeof(Argument) * inArgI)) & 0xFFFFFFFFUL);
                dictOffset = static_cast<int32_t>(static_cast<int8_t>(val));
            }
            else if (dictOffsetArgumentSize == Argument_CHAR_SIZE) {
                uint32_t val =
                    static_cast<uint32_t>((tmp >> (dictOffsetArgumentSize * sizeof(Argument) * inArgI)) & 0xFFFFFFFFUL);
                dictOffset = static_cast<int32_t>(val);
            }

            switch (argumentType) {
                case ArgumentType::ARGUMENT_TYPE_LONG:
                    return getCurrentExpressionInDictEncodedSpanAtAs<int64_t>(spanArgI, dictI, dictOffsetArgumentSize);
                case ArgumentType::ARGUMENT_TYPE_DOUBLE:
                    return getCurrentExpressionInDictEncodedSpanAtAs<double_t>(spanArgI, dictI, dictOffsetArgumentSize);
                case ArgumentType::ARGUMENT_TYPE_STRING:
                    return getCurrentExpressionInDictEncodedSpanAtAs<std::string>(spanArgI, dictI, dictOffsetArgumentSize);
                case ArgumentType::ARGUMENT_TYPE_BOOL:
                case ArgumentType::ARGUMENT_TYPE_CHAR:
                case ArgumentType::ARGUMENT_TYPE_SHORT:
                case ArgumentType::ARGUMENT_TYPE_INT:
                case ArgumentType::ARGUMENT_TYPE_FLOAT:
                case ArgumentType::ARGUMENT_TYPE_SYMBOL:
                case ArgumentType::ARGUMENT_TYPE_EXPRESSION:
                    break;
                case ArgumentType::ARGUMENT_TYPE_BYTE_ARRAY:
                    throw std::runtime_error("Invalid argument type in getCurrentExpressionInDictEncodedSpanAtAs");
            }
            return "ErrorDeserialisingExpressionInDictEncodedSpan"_(
                "ArgumentIndex"_(static_cast<int64_t>(argumentIndex)), "TypeIndex"_(static_cast<int64_t>(typeIndex)),
                "InSpanIndex"_(static_cast<int64_t>(spanArgI)), "DictIndex"_(static_cast<int64_t>(dictI)),
                "InDictIndex"_(static_cast<int64_t>(dictOffset)), "ArgumentType"_(static_cast<int64_t>(argumentType)));
        }

        boss::Expression getCurrentExpressionInSpanAt(size_t spanArgI) const
        {
            auto argumentType = getCurrentExpressionType();
            return getCurrentExpressionInSpanAtAs(spanArgI, argumentType);
        }

        boss::Expression getCurrentExpressionInDictEncodedSpanAt(size_t spanArgI, uint64_t dictI,
                                                                 size_t dictOffsetArgSize) const
        {
            auto argumentType = getCurrentExpressionType();
            return getCurrentExpressionInDictEncodedSpanAtAs(spanArgI, dictI, dictOffsetArgSize, argumentType);
        }

        template <typename T> T getCurrentExpressionAs() const
        {
            auto &argument = buffer.flattenedArguments()[argumentIndex];
            if constexpr (std::is_same_v<T, bool>) {
                return argument.asBool;
            }
            else if constexpr (std::is_same_v<T, int8_t>) {
                return argument.asChar;
            }
            else if constexpr (std::is_same_v<T, int16_t>) {
                return argument.asShort;
            }
            else if constexpr (std::is_same_v<T, int32_t>) {
                return argument.asInt;
            }
            else if constexpr (std::is_same_v<T, int64_t>) {
                return argument.asLong;
            }
            else if constexpr (std::is_same_v<T, float_t>) {
                return argument.asFloat;
            }
            else if constexpr (std::is_same_v<T, double_t>) {
                return argument.asDouble;
            }
            else if constexpr (std::is_same_v<T, std::string>) {
                return viewString(buffer.root, argument.asString);
            }
            else if constexpr (std::is_same_v<T, boss::Symbol>) {
                return boss::Symbol(viewString(buffer.root, argument.asString));
            }
            else if constexpr (std::is_same_v<T, boss::Expression>) {
                auto const &expr = expression();
                auto s = boss::Symbol(viewString(buffer.root, expr.symbolNameOffset));
                if (buffer.expressionCount() == 0) {
                    return s;
                }
                auto [args, spanArgs] = buffer.deserializeArguments(expr.startChildOffset, expr.endChildOffset,
                                                                    expr.startChildTypeOffset, expr.endChildTypeOffset);
                auto result = boss::ComplexExpression{s, {}, std::move(args), std::move(spanArgs)};
                return result;
            }
            else {
                static_assert(sizeof(T) == 0, "Unsupported type passes to getCurrentExpressionAs<T>()");
            }
        }

        boss::Expression getCurrentExpressionAs(ArgumentType argumentType) const
        {
            switch (argumentType) {
                case ArgumentType::ARGUMENT_TYPE_BOOL:
                    return getCurrentExpressionAs<bool>();
                case ArgumentType::ARGUMENT_TYPE_CHAR:
                    return getCurrentExpressionAs<int8_t>();
                case ArgumentType::ARGUMENT_TYPE_SHORT:
                    return getCurrentExpressionAs<int16_t>();
                case ArgumentType::ARGUMENT_TYPE_INT:
                    return getCurrentExpressionAs<int32_t>();
                case ArgumentType::ARGUMENT_TYPE_LONG:
                    return getCurrentExpressionAs<int64_t>();
                case ArgumentType::ARGUMENT_TYPE_FLOAT:
                    return getCurrentExpressionAs<float_t>();
                case ArgumentType::ARGUMENT_TYPE_DOUBLE:
                    return getCurrentExpressionAs<double_t>();
                case ArgumentType::ARGUMENT_TYPE_STRING:
                    return getCurrentExpressionAs<std::string>();
                case ArgumentType::ARGUMENT_TYPE_SYMBOL:
                    return getCurrentExpressionAs<boss::Symbol>();
                case ArgumentType::ARGUMENT_TYPE_EXPRESSION:
                    return getCurrentExpressionAs<boss::Expression>();
                case ArgumentType::ARGUMENT_TYPE_BYTE_ARRAY:
                    throw std::runtime_error("Invalid argument type in getCurrentExpressionAs");
            }
            return "ErrorDeserialisingExpression"_("ArgumentIndex"_(static_cast<int64_t>(argumentIndex)),
                                                   "TypeIndex"_(static_cast<int64_t>(typeIndex)),
                                                   "ArgumentType"_(static_cast<int64_t>(argumentType)));
        }

        // could use * operator for this
        // should this be && qualified?
        boss::Expression getCurrentExpression() const
        {
            auto argumentType = getCurrentExpressionType();
            return getCurrentExpressionAs(argumentType);
        }

        template <typename T> class Iterator {
          public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = T;
            using difference_type = std::ptrdiff_t;
            using pointer = T *;
            using reference = T &;

            Iterator(SerializedBossExpression const &buffer, size_t argumentIndex)
                : buffer(buffer), arguments(buffer.flattenedArguments()),
                  argumentTypes(buffer.flattenedArgumentTypes()), argumentIndex(argumentIndex),
                  validIndexEnd(argumentIndex)
            {
                updateValidIndexEnd();
            }
            virtual ~Iterator() = default;

            Iterator(Iterator &&) noexcept = default;
            Iterator(Iterator const &) = delete;
            Iterator &operator=(Iterator &&) noexcept = default;
            Iterator &operator=(Iterator const &) = delete;

            Iterator operator++(int) { return Iterator(buffer.root, incrementIndex(1)); }
            Iterator &operator++()
            {
                incrementIndex(1);
                return *this;
            }

            bool isValid() { return argumentIndex < validIndexEnd; }

            T &operator*() const
            {
                if constexpr (std::is_same_v<T, int32_t>) {
                    return arguments[argumentIndex].asInt;
                }
                else if constexpr (std::is_same_v<T, int64_t>) {
                    return arguments[argumentIndex].asLong;
                }
                else if constexpr (std::is_same_v<T, float_t>) {
                    return arguments[argumentIndex].asFloat;
                }
                else if constexpr (std::is_same_v<T, double_t>) {
                    return arguments[argumentIndex].asDouble;
                }
                else {
                    throw std::runtime_error("non-numerical types not yet implemented");
                }
            }

            T *operator->() const { return &operator*(); }

            Iterator operator+(std::ptrdiff_t v) const { return incrementIndex(v); }
            bool operator==(const Iterator &rhs) const { return argumentIndex == rhs.argumentIndex; }
            bool operator!=(const Iterator &rhs) const { return argumentIndex != rhs.argumentIndex; }

          private:
            SerializedBossExpression const &buffer;
            Argument *arguments;
            ArgumentType *argumentTypes;
            size_t argumentIndex;
            size_t validIndexEnd;

            size_t incrementIndex(std::ptrdiff_t increment)
            {
                argumentIndex += increment;
                updateValidIndexEnd();
                return argumentIndex;
            }

            void updateValidIndexEnd()
            {
                if (argumentIndex >= validIndexEnd) {
                    if ((argumentTypes[argumentIndex] & ArgumentType_RLE_BIT) != 0U) {
                        if ((argumentTypes[argumentIndex] & ~ArgumentType_RLE_BIT) == expectedArgumentType()) {
                            uint32_t size = (static_cast<uint32_t>(argumentTypes[argumentIndex + 4]) << 24) |
                                            (static_cast<uint32_t>(argumentTypes[argumentIndex + 3]) << 16) |
                                            (static_cast<uint32_t>(argumentTypes[argumentIndex + 2]) << 8) |
                                            (static_cast<uint32_t>(argumentTypes[argumentIndex + 1]));
                            validIndexEnd = argumentIndex + size;
                        }
                    }
                    else {
                        if (argumentTypes[argumentIndex] == expectedArgumentType()) {
                            validIndexEnd = argumentIndex + 1;
                        }
                    }
                }
            }

            constexpr ArgumentType expectedArgumentType()
            {
                if constexpr (std::is_same_v<T, int32_t>) {
                    return ArgumentType::ARGUMENT_TYPE_INT;
                }
                else if constexpr (std::is_same_v<T, int64_t>) {
                    return ArgumentType::ARGUMENT_TYPE_LONG;
                }
                else if constexpr (std::is_same_v<T, float_t>) {
                    return ArgumentType::ARGUMENT_TYPE_FLOAT;
                }
                else if constexpr (std::is_same_v<T, double_t>) {
                    return ArgumentType::ARGUMENT_TYPE_DOUBLE;
                }
                else if constexpr (std::is_same_v<T, std::string>) {
                    return ArgumentType::ARGUMENT_TYPE_STRING;
                }
            }
        };

        template <typename T> Iterator<T> begin() { return Iterator<T>(buffer, expression().startChildOffset); }
        template <typename T> Iterator<T> end() { return Iterator<T>(buffer, expression().endChildOffset); }

      private:
    };

    LazilyDeSerializedBossExpression lazilyDeserialize() & { return {*this, 0, 0}; };
    #pragma endregion LazilyDeSerializedBossExpression

    boss::Expression deserialize() &&
    {
        switch (flattenedArgumentTypes()[0]) {
        case ArgumentType::ARGUMENT_TYPE_BOOL:
            return flattenedArguments()[0].asBool;
        case ArgumentType::ARGUMENT_TYPE_CHAR:
            return flattenedArguments()[0].asChar;
        case ArgumentType::ARGUMENT_TYPE_SHORT:
            return flattenedArguments()[0].asShort;
        case ArgumentType::ARGUMENT_TYPE_INT:
            return flattenedArguments()[0].asInt;
        case ArgumentType::ARGUMENT_TYPE_LONG:
            return flattenedArguments()[0].asLong;
        case ArgumentType::ARGUMENT_TYPE_FLOAT:
            return flattenedArguments()[0].asFloat;
        case ArgumentType::ARGUMENT_TYPE_DOUBLE:
            return flattenedArguments()[0].asDouble;
        case ArgumentType::ARGUMENT_TYPE_STRING:
            return viewString(root, flattenedArguments()[0].asString);
        case ArgumentType::ARGUMENT_TYPE_SYMBOL:
            return boss::Symbol(viewString(root, flattenedArguments()[0].asString));
        case ArgumentType::ARGUMENT_TYPE_EXPRESSION:
            // std::cout << "ROOT METADATA: " << std::endl;
            // std::cout << "  argumentCount: " << root->argumentCount << std::endl;
            // std::cout << "  argumentBytesCount: " << root->argumentBytesCount << std::endl;
            // std::cout << "  expressionCount: " << root->expressionCount << std::endl;
            // std::cout << "  argumentDictionaryBytesCount: " << root->argumentDictionaryBytesCount
            // << std::endl; std::cout << "  stringArgumentsFillIndex: " <<
            // root->stringArgumentsFillIndex << std::endl; std::cout << "  originalAddress: " <<
            // root->originalAddress << std::endl; std::cout << "ROOT: " << root << std::endl;
            // std::cout << "ARGS: " << flattenedArguments() << std::endl;
            // std::cout << "TYPES: " << flattenedArgumentTypes() << std::endl;
            // std::cout << "EXPRS: " << expressionsBuffer() << std::endl;
            auto const &expr = expressionsBuffer()[0];
            auto s = boss::Symbol(viewString(root, expr.symbolNameOffset));
            if (root->expressionCount == 0) {
                return s;
            }
            auto [args, spanArgs] = deserializeArguments(expr.startChildOffset, expr.endChildOffset,
                                                         expr.startChildTypeOffset, expr.endChildTypeOffset);
            auto result = boss::ComplexExpression{s, {}, std::move(args), std::move(spanArgs)};
            return result;
        }
    };
    #pragma endregion Deserialization

    ///////////////////////////// Compressor helpers /////////////////////////////

    #pragma region Compress_Boss_Expression

    #pragma region handle_compressed_expression

    template <typename From, typename To>
    std::vector<To>&& reinterpretVec(std::vector<From>&& v) {
        static_assert(sizeof(From) == sizeof(To));
        return reinterpret_cast<std::vector<To>&&>(v);
    }

    // dynamic arguments of the listExpression to include column metadata
    boss::ComplexExpression handleColumnMetadata(
        ColumnMetaData &columnMetadata
    ) {
        boss::expressions::ExpressionArguments newDynamicArguments;
        newDynamicArguments.push_back(boss::Symbol(columnMetadata.columnName));
        newDynamicArguments.push_back(static_cast<int64_t>(columnMetadata.numberOfValues));
        newDynamicArguments.push_back(static_cast<int64_t>(columnMetadata.totalUncompressedSize));
        newDynamicArguments.push_back(static_cast<int64_t>(columnMetadata.totalCompressedSize));
        newDynamicArguments.push_back(static_cast<int32_t>(columnMetadata.physicalType));
        newDynamicArguments.push_back(static_cast<int32_t>(columnMetadata.encodingType));


        boss::expressions::ExpressionArguments compressionTypeArguments;
        for (const auto& compressionType : columnMetadata.compressionTypes) 
        {
            compressionTypeArguments.push_back(static_cast<int64_t>(compressionType));
        }
        boss::expressions::ComplexExpression compressionTypes(
            boss::Symbol("compressionTypes"),
            std::tuple<>{}, 
            std::move(compressionTypeArguments), 
            {}
        );
        newDynamicArguments.push_back(std::move(compressionTypes));

        boss::expressions::ExpressionArguments pagesArguments;
        for (const auto& pageHeader : columnMetadata.pageHeaders) 
        {
            boss::expressions::ExpressionArguments pageArguments;

            pageArguments.push_back(static_cast<int32_t>(pageHeader.pageType));
            pageArguments.push_back(static_cast<int64_t>(pageHeader.numberOfValues));
            pageArguments.push_back(static_cast<int64_t>(pageHeader.firstRowIndex));
            pageArguments.push_back(static_cast<int64_t>(pageHeader.uncompressedPageSize));
            pageArguments.push_back(static_cast<int64_t>(pageHeader.compressedPageSize));
            
            switch (columnMetadata.physicalType) 
            {
                case PhysicalType::INT64:
                    pageArguments.push_back(pageHeader.pageStatistics.minInt.value());
                    pageArguments.push_back(pageHeader.pageStatistics.maxInt.value());
                    break;

                case PhysicalType::DOUBLE:
                    pageArguments.push_back(pageHeader.pageStatistics.minDouble.value());
                    pageArguments.push_back(pageHeader.pageStatistics.maxDouble.value());
                    break;

                case PhysicalType::BYTE_ARRAY:
                    pageArguments.push_back(pageHeader.pageStatistics.minString.value());
                    pageArguments.push_back(pageHeader.pageStatistics.maxString.value());
                    break;

                default:
                    throw std::runtime_error("Unsupported physical type for minValue");
            }
            pageArguments.push_back(pageHeader.pageStatistics.nullCount);
            pageArguments.push_back(pageHeader.pageStatistics.distinctCount);

            pageArguments.push_back(pageHeader.isDictionaryPage);

            boss::expressions::ExpressionSpanArguments pageSpans;
            boss::Span<int8_t> span = boss::Span<int8_t>(
                reinterpretVec<uint8_t, int8_t>(std::move(pageHeader.byteArray))
            );
            pageSpans.push_back(std::move(span));

            boss::ComplexExpression pageExpression(
                boss::Symbol("PageHeader"),
                std::tuple<>{}, // no static args
                std::move(pageArguments),
                std::move(pageSpans)
            );

            pagesArguments.push_back(std::move(pageExpression));
        }

        boss::expressions::ComplexExpression pages(
            boss::Symbol("pages"),
            std::tuple<>{}, 
            std::move(pagesArguments), 
            {}
        );
        newDynamicArguments.push_back(std::move(pages));

        return boss::ComplexExpression(
            boss::Symbol("page"), 
            {}, 
            std::move(newDynamicArguments), 
            {} 
        );
    }

    //  1. collect spans of the same data type to form a single span
    //  2. for each typed span, perform compression
    //  3. rewrites the compressed metadata and byte data into the columnExpression
    void handleColumnExpression(
        boss::ComplexExpression &&columnExpression, 
        CompressionPipeline &pipeline
    ) {
        auto [head, statics, dynamics, spans] = std::move(columnExpression).decompose();
        std::transform(
            std::make_move_iterator(dynamics.begin()), 
            std::make_move_iterator(dynamics.end()), 
            dynamics.begin(),
            [&](auto &&list) 
            {
                if (!std::holds_alternative<boss::ComplexExpression>(list)) return; 

                auto [lHead, lStatics, lDynamics, lSpans] = std::move(std::get<boss::ComplexExpression>(list)).decompose();
                std::vector<bool> boolData;
                std::vector<int8_t> charData;
                std::vector<int32_t> intData;
                std::vector<int64_t> longData;
                std::vector<float> floatData;
                std::vector<double> doubleData;
                std::vector<std::string> stringData;
                std::vector<boss::Symbol> symbolData;
                std::for_each(
                    lSpans.begin(), 
                    lSpans.end(), 
                    [&](auto const &spanArg) 
                    {
                        if (std::holds_alternative<boss::Span<bool>>(spanArg)) {
                            // std::cout << "BOOL" << std::endl;
                            auto const &typedSpan = std::get<boss::Span<bool>>(spanArg);
                            for (size_t i = 0; i < typedSpan.size(); i++) {
                                boolData.push_back(typedSpan[i]);
                            }
                        }
                        else if (std::holds_alternative<boss::Span<bool const>>(spanArg)) {
                            // std::cout << "BOOL" << std::endl;
                            auto const &typedSpan = std::get<boss::Span<bool const>>(spanArg);
                            for (size_t i = 0; i < typedSpan.size(); i++) {
                                boolData.push_back(typedSpan[i]);
                            }
                        }
                        else if (std::holds_alternative<boss::Span<int8_t>>(spanArg)) {
                            // std::cout << "CHAR" << std::endl;
                            auto const &typedSpan = std::get<boss::Span<int8_t>>(spanArg);
                            for (size_t i = 0; i < typedSpan.size(); i++) {
                                charData.push_back(typedSpan[i]);
                            }
                        }
                        else if (std::holds_alternative<boss::Span<int8_t const>>(spanArg)) {
                            // std::cout << "CHAR" << std::endl;
                            auto const &typedSpan = std::get<boss::Span<int8_t const>>(spanArg);
                            for (size_t i = 0; i < typedSpan.size(); i++) {
                                charData.push_back(typedSpan[i]);
                            }
                        }
                        else if (std::holds_alternative<boss::Span<int32_t>>(spanArg)) {
                            auto const &typedSpan = std::get<boss::Span<int32_t>>(spanArg);
                            for (size_t i = 0; i < typedSpan.size(); i++) {
                                intData.push_back(typedSpan[i]);
                            }
                        }
                        else if (std::holds_alternative<boss::Span<int32_t const>>(spanArg)) {
                            auto const &typedSpan = std::get<boss::Span<int32_t const>>(spanArg);
                            for (size_t i = 0; i < typedSpan.size(); i++) {
                                intData.push_back(typedSpan[i]);
                            }
                        }
                        else if (std::holds_alternative<boss::Span<int64_t>>(spanArg)) {
                            auto const &typedSpan = std::get<boss::Span<int64_t>>(spanArg);
                            for (size_t i = 0; i < typedSpan.size(); i++) {
                                longData.push_back(typedSpan[i]);
                            }
                        }
                        else if (std::holds_alternative<boss::Span<int64_t const>>(spanArg)) {
                            auto const &typedSpan = std::get<boss::Span<int64_t const>>(spanArg);
                            for (size_t i = 0; i < typedSpan.size(); i++) {
                                longData.push_back(typedSpan[i]);
                            }
                        }
                        else if (std::holds_alternative<boss::Span<float>>(spanArg)) {
                            auto const &typedSpan = std::get<boss::Span<float>>(spanArg);
                            for (size_t i = 0; i < typedSpan.size(); i++) {
                                floatData.push_back(typedSpan[i]);
                            }
                        }
                        else if (std::holds_alternative<boss::Span<float const>>(spanArg)) {
                            auto const &typedSpan = std::get<boss::Span<float const>>(spanArg);
                            for (size_t i = 0; i < typedSpan.size(); i++) {
                                floatData.push_back(typedSpan[i]);
                            }
                        }
                        else if (std::holds_alternative<boss::Span<double>>(spanArg)) {
                            auto const &typedSpan = std::get<boss::Span<double>>(spanArg);
                            for (size_t i = 0; i < typedSpan.size(); i++) {
                                doubleData.push_back(typedSpan[i]);
                            }
                        }
                        else if (std::holds_alternative<boss::Span<double const>>(spanArg)) {
                            auto const &typedSpan = std::get<boss::Span<double const>>(spanArg);
                            for (size_t i = 0; i < typedSpan.size(); i++) {
                                doubleData.push_back(typedSpan[i]);
                            }
                        }
                        else if (std::holds_alternative<boss::Span<std::string>>(spanArg)) {
                            // std::cout << "STRING" << std::endl;
                            auto const &typedSpan = std::get<boss::Span<std::string>>(spanArg);
                            for (size_t i = 0; i < typedSpan.size(); i++) {
                                stringData.push_back(typedSpan[i]);
                            }
                        }
                        else if (std::holds_alternative<boss::Span<std::string const>>(spanArg)) {
                            // std::cout << "STRING" << std::endl;
                            auto const &typedSpan = std::get<boss::Span<std::string const>>(spanArg);
                            for (size_t i = 0; i < typedSpan.size(); i++) {
                                stringData.push_back(typedSpan[i]);
                            }
                        }
                        else if (std::holds_alternative<boss::Span<boss::Symbol>>(spanArg)) {
                            auto const &typedSpan = std::get<boss::Span<boss::Symbol>>(spanArg);
                            for (size_t i = 0; i < typedSpan.size(); i++) {
                                symbolData.push_back(typedSpan[i]);
                            }
                        }
                        else if (std::holds_alternative<boss::Span<boss::Symbol const>>(spanArg)) {
                            auto const &typedSpan = std::get<boss::Span<boss::Symbol const>>(spanArg);
                            for (size_t i = 0; i < typedSpan.size(); i++) {
                                symbolData.push_back(typedSpan[i]);
                            }
                        }
                        else {
                            // std::cout << "WHAT" << std::endl;
                        }
                    }
                );

                boss::expressions::ExpressionSpanArguments typedSpans;
                if (boolData.size() > 0) {
                    typedSpans.push_back(boss::Span<bool>(std::move(boolData)));
                }
                else if (charData.size() > 0) {
                    typedSpans.push_back(boss::Span<int8_t>(std::move(charData)));
                }
                else if (intData.size() > 0) {
                    typedSpans.push_back(boss::Span<int32_t>(std::move(intData)));
                }
                else if (longData.size() > 0) {
                    typedSpans.push_back(boss::Span<int64_t>(std::move(longData)));
                }
                else if (floatData.size() > 0) {
                    typedSpans.push_back(boss::Span<float>(std::move(floatData)));
                }
                else if (doubleData.size() > 0) {
                    typedSpans.push_back(boss::Span<double>(std::move(doubleData)));
                }
                else if (stringData.size() > 0) {
                    typedSpans.push_back(boss::Span<std::string>(std::move(stringData)));
                }
                else if (symbolData.size() > 0) {
                    typedSpans.push_back(boss::Span<boss::Symbol>(std::move(symbolData)));
                }

                boss::expressions::ExpressionArguments compressedArguments;
                for (auto& typedSpan : typedSpans) 
                {
                    std::vector<std::vector<uint8_t>> encodedData;
                    ColumnMetaData columnMetaData;

                    std::visit([&](auto&& span) 
                    {
                        using T = std::decay_t<decltype(span)>;
                        if constexpr (std::is_same_v<T, boss::Span<int64_t>>) 
                        {
                            encodedData = encodeIntColumn(
                                std::vector<int64_t>(span.begin(), span.end()), 
                                columnMetaData
                            );
                        } 
                        else if constexpr (std::is_same_v<T, boss::Span<double>>) 
                        {
                            encodedData = encodeDoubleColumn(
                                std::vector<double>(span.begin(), span.end()), 
                                columnMetaData
                            );
                        } 
                        else if constexpr (std::is_same_v<T, boss::Span<std::string>>) 
                        {
                            encodedData = encodeStringColumn(
                                std::vector<std::string>(span.begin(), span.end()), 
                                columnMetaData
                            );
                        }
                    }, typedSpan);

                    for (size_t i = 0; i < encodedData.size(); ++i)
                    {
                        Result<std::vector<uint8_t>> compressedData = pipeline.compress(
                            encodedData[i]
                        );
                        if (!compressedData.success())
                        {
                            throw std::runtime_error(
                                "Compression failed: " + compressedData.getError()
                            );
                        }
                        columnMetaData.pageHeaders[i].compressedPageSize = compressedData.getValue().size();
                        columnMetaData.pageHeaders[i].byteArray = std::move(compressedData.getValue());
                        columnMetaData.compressionTypes = pipeline.getPipeline();
                    }

                    boss::ComplexExpression newDynamicArgument = handleColumnMetadata(columnMetaData); 
                    compressedArguments.push_back(std::move(newDynamicArgument)); 
                }

                list = boss::ComplexExpression(
                    std::move(lHead),
                    std::move(lStatics),
                    std::move(compressedArguments),
                    {}
                );
            }
        );
    }
    #pragma endregion handle_compressed_expression

    #pragma region compressor_counters

    /*  new counter function: 
     *
     *  1. finds expressions that matches the pipeline map
     *   - splits their spans into pages
     *   - encodes and compresses the pages
     *   - stores the resulting bytes and span metadata in processedSpans
     *
     *  2. counts everything 
     *   - updates the new argument/expression counts / byte counts
     *   - recursively counts arguments in static / dynamic arguments
    */

    template <typename TupleLike, uint64_t... Is>
    void countArgumentsInTuple(
        TupleLike &&tuple, 
        std::index_sequence<Is...> /*unused*/, 
        std::unordered_map<std::string, CompressionPipeline*> &compressionPipelineMap, 
        uint64_t &argumentCount, 
        uint64_t &argumentBytesCount,
        uint64_t &expressionCount,
        uint64_t &stringBytesCount
    ) {
        std::apply(
            [&](auto const&... argument) {
                (..., ([&] {
                    countArguments(
                        argument,
                        compressionPipelineMap,
                        argumentCount,
                        argumentBytesCount,
                        expressionCount,
                        stringBytesCount
                    );
                }()));
            },
            std::forward<TupleLike>(tuple)
        );
    };

    void countArguments(
        boss::Expression &&input, 
        std::unordered_map<std::string, CompressionPipeline*> &compressionPipelineMap, 
        uint64_t &argumentCount, 
        uint64_t &argumentBytesCount,
        uint64_t &expressionCount,
        uint64_t &stringBytesCount, 
        bool preprocessedExpression = false
    ) {
        std::visit(
            [&compressionPipelineMap, &argumentCount, &argumentBytesCount, 
             &expressionCount, &stringBytesCount, &preprocessedExpression, this](auto &input) 
            {
                if constexpr (std::is_same_v<std::decay_t<decltype(input)>, boss::ComplexExpression>)
                {
                    // without compression: simply count everything
                    // or with compression, but pre-processed
                    if (compressionPipelineMap.find(input.getHead().getName()) == compressionPipelineMap.end()
                        || preprocessedExpression
                    ) {
                        // count head & span arguments
                        argumentCount += 1
                            + std::accumulate(
                                input.getSpanArguments().begin(),
                                input.getSpanArguments().end(), 
                                uint64_t(0),
                                [](uint64_t runningSum, auto const &argument) -> uint64_t {
                                    return runningSum + std::visit(
                                            [&](auto const &argument) -> uint64_t { return argument.size(); },
                                            std::forward<decltype(argument)>(argument));
                                });

                        argumentBytesCount += 
                            static_cast<uint64_t>(sizeof(Argument))   // head bytes
                            + std::accumulate(                        // span bytes
                                input.getSpanArguments().begin(), 
                                input.getSpanArguments().end(), 
                                uint64_t(0),
                                [](auto runningSum, auto const &argument) -> uint64_t 
                                {
                                    return runningSum + std::visit(
                                        [&](auto const &spanArgument) -> uint64_t {
                                            uint64_t spanBytes = 0;
                                            uint64_t spanSize = spanArgument.size();
                                            auto const &arg0 = spanArgument[0];
                                            if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, bool> ||
                                                        std::is_same_v<std::decay_t<decltype(arg0)>, std::_Bit_reference>) 
                                            {
                                                spanBytes = spanSize * static_cast<uint64_t>(sizeof(bool));
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, int8_t>) 
                                            {
                                                spanBytes = spanSize * static_cast<uint64_t>(sizeof(int8_t));
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, int16_t>) 
                                            {
                                                spanBytes = spanSize * static_cast<uint64_t>(sizeof(int16_t));
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, int32_t>) 
                                            {
                                                spanBytes = spanSize * static_cast<uint64_t>(sizeof(int32_t));
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, int64_t>) 
                                            {
                                                spanBytes = spanSize * static_cast<uint64_t>(sizeof(int64_t));
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, float_t>) 
                                            {
                                                spanBytes = spanSize * static_cast<uint64_t>(sizeof(float_t));
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, double_t>) 
                                            {
                                                spanBytes = spanSize * static_cast<uint64_t>(sizeof(double_t));
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, std::string>) 
                                            {
                                                spanBytes = spanSize * static_cast<uint64_t>(sizeof(Argument));
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, boss::Symbol>) 
                                            {
                                                spanBytes = spanSize * static_cast<uint64_t>(sizeof(Argument));
                                            }
                                            else 
                                            {
                                                print_type_name<std::decay_t<decltype(arg0)>>();
                                                throw std::runtime_error("unknown type in span");
                                            }
                                            // std::cout << "SPAN BYTES: " << spanBytes <<
                                            // std::endl; std::cout << "ROUNDED SPAN BYTES: "
                                            // << ((spanBytes + sizeof(Argument) - 1) &
                                            // -sizeof(Argument)) << std::endl;
                                            return (spanBytes + static_cast<uint64_t>(sizeof(Argument)) - 1) &
                                                    -(static_cast<uint64_t>(sizeof(Argument)));
                                        },
                                        std::forward<decltype(argument)>(argument));
                                });
                                
                        expressionCount ++;

                        stringBytesCount += 
                            (input.getHead().getName().size() + 1)   // head bytes
                            + std::accumulate(                       // span bytes
                                input.getSpanArguments().begin(), 
                                input.getSpanArguments().end(), 
                                uint64_t(0),
                                [&](size_t runningSum, auto const &argument) -> uint64_t {
                                    return runningSum + std::visit(
                                        [&](auto const &spanArgument) -> uint64_t 
                                        {
                                            if constexpr (std::is_same_v<std::decay_t<decltype(spanArgument)>, 
                                                                         boss::Span<std::string>>) 
                                            {
                                                return std::accumulate(
                                                    spanArgument.begin(), 
                                                    spanArgument.end(), uint64_t(0),
                                                    [&](uint64_t innerRunningSum, auto const &stringArgument) -> uint64_t {
                                                        return innerRunningSum + (strlen(stringArgument.c_str()) + 1);
                                                    });
                                            }
                                            else if constexpr (std::is_same_v<std::decay_t<decltype(spanArgument)>, 
                                                                              boss::Span<boss::Symbol>>) 
                                            {
                                                return std::accumulate(
                                                    spanArgument.begin(), 
                                                    spanArgument.end(), uint64_t(0),
                                                    [&](uint64_t innerRunningSum, auto const &stringArgument) -> uint64_t {
                                                        return innerRunningSum + (strlen(stringArgument.getName().c_str()) + 1);
                                                    });
                                            }
                                            return 0;
                                        },
                                        std::forward<decltype(argument)>(argument));
                                });

                        // count static arguments
                        countArgumentsInTuple(
                            input.getStaticArguments(),
                            std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(input.getStaticArguments())>>>(),
                            compressionPipelineMap, 
                            argumentCount,
                            argumentBytesCount,
                            expressionCount,
                            stringBytesCount
                        ); 

                        // recursively count dynamic arguments
                        for (const auto& dynamicArgument : input.getDynamicArguments()) 
                        {
                            countArguments(
                                dynamicArgument, 
                                compressionPipelineMap, 
                                argumentCount, 
                                argumentBytesCount, 
                                expressionCount, 
                                stringBytesCount
                            );
                        } 
                        return; 
                    }

                    // with compression, and not compressed yet
                    CompressionPipeline pipeline = compressionPipelineMap[input.getHead().getName()];
                    this->handleColumnExpression(
                        input, 
                        pipeline
                    ); 
                    countArguments(
                        input, 
                        compressionPipelineMap, 
                        argumentCount, 
                        argumentBytesCount, 
                        expressionCount, 
                        stringBytesCount, 
                        true  // preprocessedExpression
                    );
                    return; 
                }

                // non-complex expressions
                argumentCount ++;
                argumentBytesCount += static_cast<uint64_t>(sizeof(Argument));
                // expressionCount += 0; 
                if constexpr (std::is_same_v<std::decay_t<decltype(input)>, boss::Symbol>) 
                {
                    stringBytesCount += (strlen(input.getName().c_str()) + 1);
                }
                else if constexpr (std::is_same_v<std::decay_t<decltype(input)>, std::string>) {
                    stringBytesCount += (strlen(input.c_str()) + 1);
                }
            },
            input);
    }

    #pragma endregion compressor_counters

    explicit SerializedBossExpression(
        boss::Expression &&input, 
        std::unordered_map<std::string, CompressionPipeline*> &compressionPipelineMap, 
        bool dictEncodeStrings = true,
        bool dictEncodeDoublesAndLongs = false
    ) {
        SpanDictionary spanDict;
        // if (dictEncodeDoublesAndLongs) 
        // {
        //     spanDict = countUniqueArguments(input);
        //     root = allocateExpressionTree(
        //         countArguments(input), 
        //         countArgumentBytesDict(input, spanDict),
        //         countExpressions(input), 
        //         calculateDictionaryBytes(spanDict),
        //         countStringBytes(input, dictEncodeStrings), 
        //         allocateFunction);
        // }

        uint64_t argumentCount = 0; 
        uint64_t argumentBytesCount = 0; 
        uint64_t expressionCount = 0;
        uint64_t stringBytesCount = 0;

        countArguments(
            input, 
            compressionPipelineMap, 
            argumentCount, 
            argumentBytesCount, 
            expressionCount, 
            stringBytesCount
        );

        root = allocateExpressionTree(
            argumentCount, 
            argumentBytesCount, 
            expressionCount,
            stringBytesCount, 
            allocateFunction
        );

        std::visit(utilities::overload(
            [this, &spanDict, &dictEncodeStrings](boss::ComplexExpression &&input) {
                // count arguments and types 
                uint64_t argumentIterator = 0;
                uint64_t typeIterator = 0;
                uint64_t expressionIterator = 0;
                uint64_t dictIterator = 0;
                auto const childrenTypeCount = countArgumentTypes(input);
                auto const childrenCount = countArgumentsPacked(input, spanDict);
                auto const startChildArgOffset = 1;
                auto const endChildArgOffset = startChildArgOffset + childrenCount;
                auto const startChildTypeOffset = 1;
                auto const endChildTypeOffset = startChildArgOffset + childrenTypeCount;

                auto storedString = storeString(
                    &root, 
                    input.getHead().getName().c_str()
                );
                *makeExpression(root, expressionIterator) =
                    PortableBossExpression{storedString, startChildArgOffset, endChildArgOffset, 
                        startChildTypeOffset, endChildTypeOffset};
                *makeExpressionArgument(root, argumentIterator++, typeIterator++) = expressionIterator++;

                auto inputs = std::vector<boss::ComplexExpression>();
                inputs.push_back(std::move(input));

                flattenArguments(
                    argumentIterator, 
                    typeIterator, 
                    std::move(inputs), 
                    expressionIterator, 
                    dictIterator, 
                    spanDict, 
                    dictEncodeStrings
                );
            },
            [this](expressions::atoms::Symbol &&input) {
                auto storedString = storeString(&root, input.getName().c_str());
                *makeSymbolArgument(root, 0) = storedString;
            },
            [this](bool input) { *makeBoolArgument(root, 0) = input; },
            [this](std::int8_t input) { *makeCharArgument(root, 0) = input; },
            [this](std::int16_t input) { *makeShortArgument(root, 0) = input; },
            [this](std::int32_t input) { *makeIntArgument(root, 0) = input; },
            [this](std::int64_t input) { *makeLongArgument(root, 0) = input; },
            [this](std::float_t input) { *makeFloatArgument(root, 0) = input; },
            [this](std::double_t input) { *makeDoubleArgument(root, 0) = input; },
            [](auto &&) { throw std::logic_error("uncountered unknown type during serialization"); }),
        std::move(input));
    }

    #pragma endregion Compress_Boss_Expression

    //////////////////////////// Ownership management ////////////////////////////

    RootExpression *extractRoot() &&
    {
        auto *root = this->root;
        this->root = nullptr;
        return root;
    };

    SerializedBossExpression(SerializedBossExpression &&) noexcept = default;
    SerializedBossExpression(SerializedBossExpression const &) = delete;
    SerializedBossExpression &operator=(SerializedBossExpression &&) noexcept = default;
    SerializedBossExpression &operator=(SerializedBossExpression const &) = delete;
    ~SerializedBossExpression()
    {
        if (freeFunction)
            freeExpressionTree(root, freeFunction);
    }
};

// NOLINTEND(cppcoreguidelines-pro-type-union-access)
} // namespace boss::serialization