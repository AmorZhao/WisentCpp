#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <numeric>

enum WisentArgumentType {
    ARGUMENT_TYPE_LONG = 1,
    ARGUMENT_TYPE_DOUBLE = 2,
    ARGUMENT_TYPE_STRING = 3,
    ARGUMENT_TYPE_SYMBOL = 4,
    ARGUMENT_TYPE_EXPRESSION = 5
};

struct WisentExpression {
    int startChildOffset;
    int endChildOffset;
    int symbolNameOffset;
};

struct Buffer {
    char *input;

    int expressionCount() const
    {
        return *reinterpret_cast<int64_t *>(input + sizeof(int64_t)); 
    }

    int argumentCount() const { return *reinterpret_cast<int64_t *>(input); }

    std::vector<int64_t> flattenedArguments() const
    {
        int count = argumentCount();
        std::vector<int64_t> result(count);
        std::memcpy(result.data(), input + sizeof(int64_t) * 2, count * sizeof(int64_t));
        return result;
    }

    std::vector<WisentArgumentType> flattenedArgumentTypes() const
    {
        int count = argumentCount();
        std::vector<WisentArgumentType> result(count);
        for (int i = 0; i < count; ++i) {
            result[i] = static_cast<WisentArgumentType>(flattenedArguments()[i] & 0x7F);
        }
        return result;
    }

    std::vector<WisentExpression> expressions() const
    {
        int count = expressionCount();
        std::vector<WisentExpression> result(count);
        return result;
    }

    char *stringBuffer() const { return input + expressionCount() * sizeof(WisentExpression); }
};

struct ComplexExpression {
    int argumentIndex;
    const Buffer &input;

    std::vector<std::pair<WisentArgumentType, std::vector<int>>> arguments() const
    {
        WisentExpression e = input.expressions()[input.flattenedArguments()[argumentIndex]];
        int length =
            e.startChildOffset + ((input.flattenedArgumentTypes()[e.startChildOffset] & 0x80) != 0
                                      ? input.flattenedArgumentTypes()[e.startChildOffset + 1]
                                      : 0);
        std::vector<std::pair<WisentArgumentType, std::vector<int>>> result;
        int state = e.startChildOffset;
        while (state != e.endChildOffset) {
            WisentArgumentType type = input.flattenedArgumentTypes()[state];
            std::vector<int> range(length);
            std::iota(range.begin(), range.end(), state);
            result.emplace_back(type, range);
            state = std::max(state + 1, length);
        }
        return result;
    }

    std::string head() const
    {
        char *ptr = input.stringBuffer() +
                    input.expressions()[input.flattenedArguments()[argumentIndex]].symbolNameOffset;
        return std::string(ptr);
    }
};

enum Atom { Int64, Double, String, Complex, Symbol };

struct SerializedExpression {
    int argumentIndex;
    const Buffer &input;

    WisentArgumentType type() const
    {
        return static_cast<WisentArgumentType>(input.flattenedArgumentTypes()[argumentIndex] &
                                               0x7F);
    }

    Atom asAtom() const
    {
        switch (type()) {
        case ARGUMENT_TYPE_STRING: 
            return Atom::String;
        case ARGUMENT_TYPE_SYMBOL:
            return Atom::Symbol;
        case ARGUMENT_TYPE_LONG:
            return Atom::Int64;
        case ARGUMENT_TYPE_DOUBLE:
            return Atom::Double;
        case ARGUMENT_TYPE_EXPRESSION:
            return Atom::Complex;
        default:
            return Atom::String;
        }
    }

    std::vector<ComplexExpression> arguments() const
    {
        if (asAtom() == Atom::Complex) {
            return {ComplexExpression{argumentIndex, input}};
        }
        return {};
    }

    int64_t asInt() const { return input.flattenedArguments()[argumentIndex]; }

    double asDouble() const
    {
        return *reinterpret_cast<const double *>(input.flattenedArguments().data() + argumentIndex);
    }
};

int64_t sumUp(const std::vector<int> &run)
{
    int64_t result = 0;
    for (int value : run) {
        result += value;
    }
    return result;
}

double sumUpFloat(const std::vector<int> &run)
{
    double result = 0.0;
    for (int value : run) {
        result += static_cast<double>(value); 
    }
    return result;
}

int64_t processInts(const std::vector<std::pair<WisentArgumentType, std::vector<int>>> &arguments)
{
    int64_t agg = 0;
    for (const auto &arg : arguments) {
        if (arg.first == ARGUMENT_TYPE_LONG) {
            agg += sumUp(arg.second);
        }
        else if (arg.first == ARGUMENT_TYPE_DOUBLE) {
            agg += static_cast<int64_t>(sumUpFloat(arg.second));
        }
    }
    return agg;
}

