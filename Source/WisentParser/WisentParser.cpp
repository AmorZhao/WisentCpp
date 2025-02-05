#include "WisentParser.hpp"
#include "../Helpers/CsvLoading.hpp"
#include "../Helpers/SharedMemorySegment.hpp"
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <memory>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

class Expression {
public:
    std::string head;
    std::vector<std::shared_ptr<void>> args;

    Expression(const std::string& head) : head(head) {}

    std::string toString() const {
        std::string result = "('" + head;
        for (const auto& arg : args) {
            result += " " + std::to_string(reinterpret_cast<uintptr_t>(arg.get()));
        }
        result += ")";
        return result;
    }
};

class Symbol {
public:
    std::string name;

    Symbol(const std::string& name) : name(name) {}

    std::string toString() const {
        return "'" + name;
    }
};

std::string readString(size_t offset, const std::string& strings) {
    size_t length = strings.find('\0', offset) - offset;
    return strings.substr(offset, length);
}

std::shared_ptr<Symbol> readSymbol(size_t offset, const std::string& strings) {
    std::string str = readString(offset, strings);
    return std::make_shared<Symbol>(str);
}

std::shared_ptr<void> readArgWithType(WisentArgumentType WisentArgumentType, size_t offset, const std::string& args, const std::string& WisentArgumentTypes, const std::string& exprs, const std::string& strings);

std::shared_ptr<void> readExpression(size_t offset, const std::string& args, const std::string& WisentArgumentTypes, const std::string& exprs, const std::string& strings) {
    size_t head, startChild, endChild;
    std::memcpy(&head, &exprs[offset * 24], sizeof(size_t));
    std::memcpy(&startChild, &exprs[offset * 24 + 8], sizeof(size_t));
    std::memcpy(&endChild, &exprs[offset * 24 + 16], sizeof(size_t));

    std::string headStr = readSymbol(head, strings)->name;
    auto expr = std::make_shared<Expression>(headStr);

    for (size_t i = startChild; i < endChild; ++i) {
        size_t WisentArgumentType;
        std::memcpy(&WisentArgumentType, &WisentArgumentTypes[i * 8], sizeof(size_t));
        expr->args.push_back(readArgWithType(static_cast<enum WisentArgumentType>(WisentArgumentType), i, args, WisentArgumentTypes, exprs, strings));
    }

    return expr;
}

std::shared_ptr<void> readArgWithType(WisentArgumentType WisentArgumentType, size_t offset, const std::string& args, const std::string& WisentArgumentTypes, const std::string& exprs, const std::string& strings) {
    switch (WisentArgumentType) 
    {
        case WisentArgumentType::ARGUMENT_TYPE_BOOL: {
            bool value;
            std::memcpy(&value, &args[offset * 8], sizeof(bool));
            return std::make_shared<bool>(value);
        }
        case WisentArgumentType::ARGUMENT_TYPE_LONG: {
            size_t value;
            std::memcpy(&value, &args[offset * 8], sizeof(size_t));
            return std::make_shared<size_t>(value);
        }
        case WisentArgumentType::ARGUMENT_TYPE_DOUBLE: {
            double value;
            std::memcpy(&value, &args[offset * 8], sizeof(double));
            return std::make_shared<double>(value);
        }
        case WisentArgumentType::ARGUMENT_TYPE_STRING: {
            size_t index;
            std::memcpy(&index, &args[offset * 8], sizeof(size_t));
            return std::make_shared<std::string>(readString(index, strings));
        }
        case WisentArgumentType::ARGUMENT_TYPE_SYMBOL: {
            size_t index;
            std::memcpy(&index, &args[offset * 8], sizeof(size_t));
            return readSymbol(index, strings);
        }
        case WisentArgumentType::ARGUMENT_TYPE_EXPRESSION: {
            size_t index;
            std::memcpy(&index, &args[offset * 8], sizeof(size_t));
            return readExpression(index, args, WisentArgumentTypes, exprs, strings);
        }
        default:
            return nullptr;
    }
}

std::string deserialize(const std::string& buffer) 
{
    size_t argCount, exprCount;
    std::memcpy(&argCount, buffer.data(), sizeof(size_t));
    std::memcpy(&exprCount, buffer.data() + sizeof(size_t), sizeof(size_t));

    size_t offset = 32; 
    std::string args(buffer.data() + offset, argCount * 8);
    offset += argCount * 8;
    std::string WisentArgumentTypes(buffer.data() + offset, argCount * 8);
    offset += argCount * 8;
    std::string exprs(buffer.data() + offset, exprCount * 24);
    offset += exprCount * 24;
    std::string strings(buffer.data() + offset, buffer.size() - offset);

    auto result = readArgWithType(WisentArgumentType::ARGUMENT_TYPE_EXPRESSION, 0, args, WisentArgumentTypes, exprs, strings);

    // Assuming addr and sb are declared and initialized somewhere in the code
    extern void* addr;
    extern struct stat sb;
    munmap(addr, sb.st_size);

    if (auto expr = std::dynamic_pointer_cast<Expression>(result)) 
    {
        return expr->toString();
    }

    return "";
}

std::string wisent::parser::query(const std::string& query) 
{
    return "not implemented";
}

std::string wisent::parser::parse(std::string const& sharedMemoryName)
{
    auto& sharedMemory = createOrGetMemorySegment(sharedMemoryName);
    if (!sharedMemory.loaded()) 
    {
        std::cerr << "Can't parse wisent file: Shared memory segment is not loaded." << std::endl;
        return "";
    }
    std::string buffer(static_cast<char*>(sharedMemory.baseAddress()), sharedMemory.size());
    auto parseResult = deserialize(buffer);
    return parseResult;
}