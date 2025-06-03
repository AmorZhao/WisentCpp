#include "../../WisentSerializer/BossHelpers/BossSerializedExpression.hpp"
#include "../CompressionPipeline.hpp"
#include "../CompressionHelpers/Algorithms.hpp"

/*
 *  BossCompressedExpression inherits from boss::serialization::SerializedExpression
 *  and makes the following changes: 
 *   - CountArguments functions takes into account the number of arguments *after* compression 
 *   - FlattenedArguments function splits, encode and compress the span vectors 
 *     as specified in the compression pipeline map
 */

using namespace boss::serialization;

template <
    void *(*allocateFunction)(size_t) = std::malloc,
    void *(*reallocateFunction)(void *, size_t) = std::realloc,
    void (*freeFunction)(void *) = std::free
>
struct BossCompressedExpression : SerializedExpression<allocateFunction, reallocateFunction, freeFunction>
{
    using BOSSArgumentPair =
        std::pair<boss::expressions::ExpressionArguments, boss::expressions::ExpressionSpanArguments>;

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

    #pragma region counter_functions

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
    static void countUniqueArgumentsInTuple(SpanDictionary &dict, size_t &spanI, TupleLike const &tuple,
                                            std::index_sequence<Is...> /*unused*/)
    {
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

    static bool countUniqueArgumentsAtLevel(boss::Expression const &input, SpanDictionary &dict, size_t &spanI,
                                            int64_t level)
    {
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

    static void countUniqueArgumentsStaticsAndSpans(boss::Expression const &input, SpanDictionary &dict, size_t &spanI)
    {
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

    static bool countArgumentBytesDictAtLevel(boss::Expression const &input, uint64_t &count, SpanDictionary &dict,
                                              size_t &spanI, int64_t level)
    {
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

    static uint64_t countArgumentBytesDictStaticsAndSpans(boss::Expression const &input, SpanDictionary &dict,
                                                          size_t &spanI)
    {
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
            boss::utilities::overload(
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
    static uint64_t countStringBytesInTuple(std::unordered_set<std::string> &stringSet, bool dictEncodeStrings,
                                            TupleLike const &tuple, std::index_sequence<Is...> /*unused*/)
    {
        return (countStringBytes(std::get<Is>(tuple), stringSet, dictEncodeStrings) + ... + 0);
    };

    static uint64_t countStringBytes(boss::Expression const &input, bool dictEncodeStrings = true)
    {
        std::unordered_set<std::string> stringSet;
        return 1 + countStringBytes(input, stringSet, dictEncodeStrings);
    }

    static uint64_t countStringBytes(boss::Expression const &input, std::unordered_set<std::string> &stringSet,
                                     bool dictEncodeStrings)
    {
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
                            return runningSum +
                                    std::visit(
                                        [&](auto const &argument) -> uint64_t {
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

    #pragma endregion counter_functions

    //////////////////////////////// Compression Helpers ///////////////////////////////

    #pragma region convert_spans_to_single_span
    boss::ComplexExpression convertSpansToSingleSpan(boss::ComplexExpression &&e)
    {
        auto [head, statics, dynamics, spans] = std::move(e).decompose();
        std::transform(
            std::make_move_iterator(dynamics.begin()), std::make_move_iterator(dynamics.end()), dynamics.begin(),
            [](auto &&arg) {
                auto [cHead, cStatics, cDynamics, cSpans] = std::move(std::get<boss::ComplexExpression>(arg)).decompose();
                std::transform(
                    std::make_move_iterator(cDynamics.begin()), 
                    std::make_move_iterator(cDynamics.end()),
                    cDynamics.begin(), 
                    [](auto &&lArg) 
                    {
                        auto [lHead, lStatics, lDynamics, lSpans] = std::move(std::get<boss::ComplexExpression>(lArg)).decompose();
                        boss::expressions::ExpressionSpanArguments newSpanArgs;
                        std::vector<bool> boolData;
                        std::vector<int8_t> charData;
                        std::vector<int32_t> intData;
                        std::vector<int64_t> longData;
                        std::vector<float> floatData;
                        std::vector<double> doubleData;
                        std::vector<std::string> stringData;
                        std::vector<boss::Symbol> symbolData;
                        std::for_each(lSpans.begin(), lSpans.end(), [&](auto const &spanArg) {
                            if (std::holds_alternative<boss::Span<bool>>(spanArg)) {
                                // std::cout << "BOOL" << std::endl;
                                auto const &typedSpan = std::get<boss::Span<bool>>(spanArg);
                                for (size_t i = 0; i < typedSpan.size(); i++) {
                                    boolData.push_back(typedSpan[i]);
                                }
                            }
                            else if (std::holds_alternative<boss::Span<bool const>>(spanArg)) {
                                auto const &typedSpan = std::get<boss::Span<bool const>>(spanArg);
                                for (size_t i = 0; i < typedSpan.size(); i++) {
                                    boolData.push_back(typedSpan[i]);
                                }
                            }
                            else if (std::holds_alternative<boss::Span<int8_t>>(spanArg)) {
                                auto const &typedSpan = std::get<boss::Span<int8_t>>(spanArg);
                                for (size_t i = 0; i < typedSpan.size(); i++) {
                                    charData.push_back(typedSpan[i]);
                                }
                            }
                            else if (std::holds_alternative<boss::Span<int8_t const>>(spanArg)) {
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
                        });
                        if (boolData.size() > 0) {
                            newSpanArgs.push_back(boss::Span<bool>(std::move(boolData)));
                        }
                        else if (charData.size() > 0) {
                            newSpanArgs.push_back(boss::Span<int8_t>(std::move(charData)));
                        }
                        else if (intData.size() > 0) {
                            newSpanArgs.push_back(boss::Span<int32_t>(std::move(intData)));
                        }
                        else if (longData.size() > 0) {
                            newSpanArgs.push_back(boss::Span<int64_t>(std::move(longData)));
                        }
                        else if (floatData.size() > 0) {
                            newSpanArgs.push_back(boss::Span<float>(std::move(floatData)));
                        }
                        else if (doubleData.size() > 0) {
                            newSpanArgs.push_back(boss::Span<double>(std::move(doubleData)));
                        }
                        else if (stringData.size() > 0) {
                            newSpanArgs.push_back(boss::Span<std::string>(std::move(stringData)));
                        }
                        else if (symbolData.size() > 0) {
                            newSpanArgs.push_back(boss::Span<boss::Symbol>(std::move(symbolData)));
                        }
                        return boss::ComplexExpression(std::move(lHead), std::move(lStatics),
                                                        std::move(lDynamics), std::move(newSpanArgs));
                    });
                return boss::ComplexExpression(std::move(cHead), std::move(cStatics), std::move(cDynamics),
                                            std::move(cSpans));
            });
        return boss::ComplexExpression(std::move(head), std::move(statics), std::move(dynamics), std::move(spans));
    }
    #pragma endregion convert_spans_to_single_span

    //////////////////////////////// Flatten Arguments /////////////////////////////////

    #pragma region flatten_arguments

    size_t checkMapAndStoreString(const std::string &key, std::unordered_map<std::string, size_t> &stringMap,
                                  bool dictEncodeStrings)
    {
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

    uint64_t countArgumentsPacked(boss::ComplexExpression const &expression, SpanDictionary &spanDict, size_t spanIInput)
    {
        size_t spanI = spanIInput;
        uint64_t staticsCount = std::tuple_size_v<std::decay_t<decltype(expression.getStaticArguments())>>;
        uint64_t dynamicsCount = expression.getDynamicArguments().size();

        uint64_t spansCount = std::accumulate(
            expression.getSpanArguments().begin(), expression.getSpanArguments().end(), uint64_t(0),
            [&spanDict, &spanI](uint64_t runningSum, auto const &spanArg) -> uint64_t {
                return runningSum + std::visit(
                        [&](auto const &spanArgument) -> uint64_t {
                            uint64_t spanSize = spanArgument.size();
                            auto const &arg0 = spanArgument[0];
                            uint64_t valsPerArg = static_cast<uint64_t>(
                                sizeof(arg0) > sizeof(Argument) ? 1 : sizeof(Argument) / sizeof(arg0));
                            if (spanDict.find(spanI) != spanDict.end()) {
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
    void flattenArgumentsInTuple(TupleLike &&tuple, std::index_sequence<Is...> /*unused*/, uint64_t &argumentOutputI,
                                 uint64_t &typeOutputI, uint64_t &dictOutputI, SpanDictionary &spanDict, size_t &spanI,
                                 std::unordered_map<std::string, size_t> &stringMap, bool dictEncodeStrings)
    {
        (flattenArguments(std::get<Is>(tuple), argumentOutputI, typeOutputI, dictOutputI, spanDict, spanI, stringMap,
                          dictEncodeStrings),
         ...);
    };

    // assuming RLE encode for now
    uint64_t flattenArguments(uint64_t argumentOutputI, uint64_t typeOutputI,
                              std::vector<boss::ComplexExpression> &&inputs, uint64_t &expressionOutputI,
                              uint64_t dictOutputI, SpanDictionary &spanDict, bool dictEncodeStrings = true)
    {
        std::unordered_map<std::string, size_t> stringMap;
        size_t spanI = 0;
        return flattenArguments(argumentOutputI, typeOutputI, std::move(inputs), expressionOutputI, dictOutputI,
                                spanDict, spanI, stringMap, dictEncodeStrings);
    }

    uint64_t flattenArguments(uint64_t argumentOutputI, uint64_t typeOutputI,
                              std::vector<boss::ComplexExpression> &&inputs, uint64_t &expressionOutputI,
                              uint64_t dictOutputI, SpanDictionary &spanDict, size_t &spanI,
                              std::unordered_map<std::string, size_t> &stringMap, bool dictEncodeStrings)
    {
        auto const nextLayerTypeOffset = typeOutputI + std::accumulate(
                                        inputs.begin(), inputs.end(), 0, 
                                        [this](auto count, auto const &expression) {
                                            return count + countArgumentTypes(expression);
                                        });
        auto const nextLayerOffset = argumentOutputI + std::accumulate(
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
                    argumentOutputI, typeOutputI, dictOutputI, 
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
                             &dictEncodeStrings, &spanDict, spanI](auto &&argument) 
                            {
                                if constexpr (boss::expressions::generic::isComplexExpression<decltype(argument)>) 
                                {
                                    auto const childrenCount = countArgumentsPacked(argument, spanDict, spanI);
                                    auto const childrenTypeCount = countArgumentTypes(argument);
                                    auto const startChildArgOffset = nextLayerOffset + childrenCountRunningSum;
                                    auto const endChildArgOffset = nextLayerOffset + childrenCountRunningSum + childrenCount;
                                    auto const startChildTypeOffset = nextLayerTypeOffset + childrenTypeCountRunningSum;
                                    auto const endChildTypeOffset = nextLayerTypeOffset + childrenTypeCountRunningSum + childrenTypeCount;
                                    
                                    // std::cout << "HEAD: " << argument.getHead().getName() <<
                                    // std::endl; std::cout << "  argOutput: " << argumentOutputI <<
                                    // std::endl; std::cout << "  typeOutput: " << typeOutputI <<
                                    // std::endl; std::cout << "  exprOutput: " << expressionOutputI
                                    // << std::endl; std::cout << "  startChildArgOffset: " <<
                                    // startChildArgOffset << std::endl; std::cout << "
                                    // endChildArgOffset: " << endChildArgOffset << std::endl;
                                    // std::cout << "  startChildArgTypeOffset: " <<
                                    // startChildTypeOffset << std::endl; std::cout << "
                                    // endChildArgTypeOffset: " << endChildTypeOffset << std::endl;

                                    auto storedString = checkMapAndStoreString(argument.getHead().getName(), stringMap,
                                                                               dictEncodeStrings);
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
                    [this, &argumentOutputI, &typeOutputI, &dictOutputI, &spanDict, &spanI, &stringMap, &dictEncodeStrings](auto &&argument) {
                        std::visit(
                            [&](auto &&spanArgument) {
                                auto spanSize = spanArgument.size();
                                auto const &arg0 = spanArgument[0];
                                if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, bool> ||
                                                std::is_same_v<std::decay_t<decltype(arg0)>, std::_Bit_reference>) 
                                {
                                    size_t valsPerArg = sizeof(Argument) / Argument_BOOL_SIZE;
                                    for (size_t i = 0; i < spanSize; i += valsPerArg) {
                                        uint64_t tmp = 0;
                                        for (size_t j = 0; j < valsPerArg && i + j < spanSize; j++) {
                                            makeBoolArgumentType(root, typeOutputI++);
                                            tmp |= static_cast<uint64_t>(spanArgument[i + j])
                                                    << (Argument_BOOL_SIZE * sizeof(Argument) * (valsPerArg - 1 - j));
                                        }
                                        *makeArgument(root, argumentOutputI++) = static_cast<int64_t>(tmp);
                                    }
                                    // std::for_each(spanArgument.begin(), spanArgument.end(),
                                    // [&](auto arg) {
                                    //   *makeBoolArgument(root, argumentOutputI++) = arg;
                                    // });
                                }
                                else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, int8_t>) 
                                {
                                    size_t valsPerArg = sizeof(Argument) / Argument_CHAR_SIZE;
                                    for (size_t i = 0; i < spanSize; i += valsPerArg) {
                                        uint64_t tmp = 0;
                                        for (size_t j = 0; j < valsPerArg && i + j < spanSize; j++) {
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
                                    for (size_t i = 0; i < spanSize; i += valsPerArg) {
                                        uint64_t tmp = 0;
                                        for (size_t j = 0; j < valsPerArg && i + j < spanSize; j++) {
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
                                    for (size_t i = 0; i < spanSize; i += valsPerArg) {
                                        uint64_t tmp = 0;
                                        for (size_t j = 0; j < valsPerArg && i + j < spanSize; j++) {
                                            makeIntArgumentType(root, typeOutputI++);
                                            tmp |= static_cast<uint64_t>(spanArgument[i + j])
                                                    << (Argument_INT_SIZE * sizeof(Argument) * (valsPerArg - 1 - j));
                                        }
                                        *makeArgument(root, argumentOutputI++) = static_cast<int64_t>(tmp);
                                    }
                                }
                                else if constexpr (std::is_same_v<std::decay_t<decltype(arg0)>, int64_t>) 
                                {
                                    if (spanDict.find(spanI) != spanDict.end()) {
                                        auto &dict = spanDict[spanI];
                                        int64_t dictStartI = dictOutputI;
                                        for (auto &entry : dict) {
                                            int64_t value = std::get<int64_t>(entry.first);
                                            int32_t &offset = entry.second;
                                            offset = dictOutputI;
                                            *makeLongDictionaryEntry(root, dictOutputI++) = value;
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
                                    // std::for_each(spanArgument.begin(), spanArgument.end(),
                                    // [&](auto& arg) {
                                    //   *makeFloatArgument(root, argumentOutputI++) = arg;
                                    // });
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
            return flattenArguments(argumentOutputI, typeOutputI, std::move(children), expressionOutputI, dictOutputI,
                                    spanDict, spanI, stringMap, dictEncodeStrings);
        }
        return argumentOutputI;
    }
    #pragma endregion flatten_arguments

  public: 
    explicit BossCompressedExpression(
        boss::Expression &&input, 
        std::unordered_map<std::string, CompressionPipeline*> &compressionPipelineMap,
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
        std::visit(boss::utilities::overload(
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
            [this](boss::expressions::atoms::Symbol &&input) {
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

    explicit BossCompressedExpression(RootExpression *root) : root(root) {}
};