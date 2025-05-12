#include "WisentParser.hpp"
#include "../WisentSerializer/WisentHelpers.h"
#include "../Helpers/ISharedMemorySegment.hpp"
#include <iostream>
#include <vector>
#include <memory>
#include <fcntl.h>
#include <unistd.h>

const size_t BytesPerLong = 8;
class Expression 
{
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

class Symbol 
{
    public:
    std::string name;

    Symbol(const std::string& name) : name(name) {}

    std::string toString() const {
        return "'" + name;
    }
};

std::string readString(size_t offset, const std::string& strings) 
{
    size_t length = strings.find('\0', offset) - offset;
    return strings.substr(offset, length);
}

std::shared_ptr<Symbol> readSymbol(size_t offset, const std::string& strings) 
{
    std::string str = readString(offset, strings);
    return std::make_shared<Symbol>(str);
}

std::shared_ptr<void> readArgumentWithType(WisentArgumentType WisentArgumentType, size_t offset, const std::string& args, const std::string& WisentArgumentTypes, const std::string& exprs, const std::string& strings);

std::shared_ptr<void> readExpression(
    size_t offset, 
    const std::string& args, 
    const std::string& WisentArgumentTypes, 
    const std::string& exprs, 
    const std::string& strings) 
{
    size_t head, startChild, endChild;
    std::memcpy(&head, &exprs[offset * 24], sizeof(size_t));
    std::memcpy(&startChild, &exprs[offset * 24 + BytesPerLong], sizeof(size_t));
    std::memcpy(&endChild, &exprs[offset * 24 + 16], sizeof(size_t));

    std::string headStr = readSymbol(head, strings)->name;
    auto expr = std::make_shared<Expression>(headStr);

    for (size_t i = startChild; i < endChild; ++i) {
        size_t WisentArgumentType;
        std::memcpy(&WisentArgumentType, &WisentArgumentTypes[i * BytesPerLong], sizeof(size_t));
        expr->args.push_back(readArgumentWithType(static_cast<enum WisentArgumentType>(WisentArgumentType), i, args, WisentArgumentTypes, exprs, strings));
    }

    return expr;
}

std::string readArgument(
    size_t offset,
    const std::string& args, 
    const std::string& WisentArgumentTypes, 
    const std::string& exprs, 
    const std::string& strings) 
{
    WisentArgumentType argumentType;
    std::memcpy(
        &argumentType, 
        &WisentArgumentTypes[offset * BytesPerLong], 
        sizeof(size_t)
    );

    auto argument = readArgumentWithType(
        static_cast<enum WisentArgumentType>(argumentType), 
        offset, 
        args, 
        WisentArgumentTypes, 
        exprs, 
        strings
    );

    if (argumentType == WisentArgumentType::ARGUMENT_TYPE_EXPRESSION) {
        return  std::static_pointer_cast<Expression>(argument)->toString();
    } 
    else if (argumentType == WisentArgumentType::ARGUMENT_TYPE_SYMBOL) {
        return std::static_pointer_cast<Symbol>(argument)->toString();
    } 
    else if (argumentType == WisentArgumentType::ARGUMENT_TYPE_STRING) {
        return *std::static_pointer_cast<std::string>(argument);
    } 
    else if (argumentType == WisentArgumentType::ARGUMENT_TYPE_LONG) {
        return std::to_string(*std::static_pointer_cast<double>(argument));
    } 
    else if (argumentType == WisentArgumentType::ARGUMENT_TYPE_DOUBLE) {
        return std::to_string(*std::static_pointer_cast<size_t>(argument));
    } 
    else if (argumentType == WisentArgumentType::ARGUMENT_TYPE_BOOL) {
        return *std::static_pointer_cast<bool>(argument) ? "true" : "false";
    }

    return "";
}

std::shared_ptr<void> readArgumentWithType(
    WisentArgumentType WisentArgumentType, 
    size_t offset, 
    const std::string& args, 
    const std::string& WisentArgumentTypes, 
    const std::string& exprs, 
    const std::string& strings) 
{
    switch (WisentArgumentType) 
    {
        case WisentArgumentType::ARGUMENT_TYPE_BOOL: {
            bool value;
            std::memcpy(&value, &args[offset * BytesPerLong], sizeof(bool));
            return std::make_shared<bool>(value);
        }
        case WisentArgumentType::ARGUMENT_TYPE_LONG: {
            size_t value;
            std::memcpy(&value, &args[offset * BytesPerLong], sizeof(size_t));
            return std::make_shared<size_t>(value);
        }
        case WisentArgumentType::ARGUMENT_TYPE_DOUBLE: {
            double value;
            std::memcpy(&value, &args[offset * BytesPerLong], sizeof(double));
            return std::make_shared<double>(value);
        }
        case WisentArgumentType::ARGUMENT_TYPE_STRING: {
            size_t index;
            std::memcpy(&index, &args[offset * BytesPerLong], sizeof(size_t));
            return std::make_shared<std::string>(readString(index, strings));
        }
        case WisentArgumentType::ARGUMENT_TYPE_SYMBOL: {
            size_t index;
            std::memcpy(&index, &args[offset * BytesPerLong], sizeof(size_t));
            return readSymbol(index, strings);
        }
        case WisentArgumentType::ARGUMENT_TYPE_EXPRESSION: {
            size_t index;
            std::memcpy(&index, &args[offset * BytesPerLong], sizeof(size_t));
            return readExpression(index, args, WisentArgumentTypes, exprs, strings);
        }
        default:
            return nullptr;
    }
}

std::string deserialize(const std::string& buffer) 
{
    size_t argumentCount, exprCount;
    std::memcpy(&argumentCount, buffer.data(), sizeof(size_t));
    std::memcpy(&exprCount, buffer.data() + sizeof(size_t), sizeof(size_t));

    size_t offset = 32; 
    size_t argumentVectorSize = argumentCount * BytesPerLong; 
    std::string argumentVector(buffer.data() + offset, argumentVectorSize);
    
    offset += argumentVectorSize;
    size_t typeBytefieldSize = argumentCount * BytesPerLong;
    std::string typeBytefield(buffer.data() + offset, typeBytefieldSize);
    
    offset += typeBytefieldSize;
    size_t structureVectorSize = exprCount * BytesPerLong * 3;
    std::string structureVector(buffer.data() + offset, exprCount * 24);

    offset += exprCount * 24;
    std::string stringBuffer(buffer.data() + offset, buffer.size() - offset);

    return readArgument(0, argumentVector, typeBytefield, structureVector, stringBuffer);
}

std::string wisent::parser::parse(
    std::string const& sharedMemoryName)
{
    ISharedMemorySegment *sharedMemory = SharedMemorySegments::createOrGetMemorySegment(sharedMemoryName);
    if (!sharedMemory->isLoaded()) 
    {
        std::cerr << "Can't parse wisent file: Shared memory segment is not loaded." << std::endl;
        return "";
    }
    auto baseAddress = sharedMemory->getBaseAddress();
    auto size = sharedMemory->getSize();

    std::string buffer(static_cast<char*>(baseAddress), size);
    // auto parseResult = deserialize(buffer);

    return "parseResult";
}

std::string wisent::parser::query(const std::string& query) 
{
    return "not implemented";
}