#pragma once
#include <optional>
#include <string>
#include <vector>

template<typename T>
struct Result 
{
    std::optional<T> value;
    std::optional<std::string> error; 
    std::vector<std::string> warnings; 

    bool success() const 
    { 
        return value.has_value() && !error.has_value(); 
    }

    bool hasWarning() const 
    { 
        return !warnings.empty(); 
    }

    void setValue(const T& val) 
    { 
        value = val; 
    }

    void setError(const std::string& errorMessage) 
    { 
        error = errorMessage; 
    }

    void addWarning(const std::string& warningMessage) 
    { 
        warnings.push_back(warningMessage); 
    }

    std::string getError() const 
    { 
        return error.value_or("No error"); 
    }
};


template<typename T>
static Result<T> makeError(const std::string& errorMessage) 
{
    Result<T> result;
    result.setError(errorMessage);
    return result;
}; 

template<typename T>
static Result<T> makeResult(const T& value, Result<T>* result = nullptr) 
{
    if (result != nullptr) {
        result->setValue(value);
        return *result;
    }
    Result<T> newResult;
    newResult.setValue(value);
    return newResult;
};