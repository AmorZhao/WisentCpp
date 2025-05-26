#pragma once
#include <stdexcept>
#include <string>
#include <vector>
#include <optional>  // C++17: std::optional 
#include <variant>   // C++17: std::variant
#include <sys/resource.h>
#include <iostream>
#include "../../Include/json.h"
#include "../../Include/rapidcsv.h"

using json = nlohmann::json;  

static rapidcsv::Document openCsvFile(std::string const &filepath)
{
    try {
        rapidcsv::Document doc(
            filepath,
            rapidcsv::LabelParams(),
            rapidcsv::SeparatorParams(),
            rapidcsv::ConverterParams(),
            rapidcsv::LineReaderParams()
        );
        return doc;
    } 
    catch (const std::exception &e) {
        std::cerr << "Error opening CSV file: " << e.what() << std::endl;
        throw;
    }
}

template <typename T>
static std::vector<std::optional<T>> loadCsvData(
    rapidcsv::Document const &doc,
    std::string const &columnName
) {
    std::vector<std::optional<T>> column;
    try {
        auto numRows = doc.GetRowCount();
        column.reserve(numRows);
        auto columnIndex = doc.GetColumnIdx(columnName);

        for (auto rowIndex = 0L; rowIndex < numRows; ++rowIndex)
        {
            column.emplace_back(doc.GetCell<std::optional<T>>(
                columnIndex,
                rowIndex,
                [](std::string const &str, std::optional<T> &val)
                {
                    if (!std::is_same<T, std::string>::value)
                    {
                        if (str.empty())
                        {
                            val = {}; 
                            return;
                        }
                    }
                    if (std::is_same<T, int64_t>::value)
                    {
                        size_t pos;
                        val = json(std::stol(str, &pos));
                        if (pos != str.length())
                        {
                            throw std::invalid_argument("failed to convert " + str + " into int");
                        }
                    }
                    else if (std::is_same<T, double_t>::value)
                    {
                        size_t pos;
                        val = json(std::stod(str, &pos));
                        if (pos != str.length())
                        {
                            throw std::invalid_argument("failed to convert " + str + " into double_t");
                        }
                    }
                    else {
                        val = json(str);
                    }
                }
            ));
        }
    }
    catch (std::invalid_argument const &e)
    {
        // load function will try again with different type
        // std::cerr << "Error opening CSV file: " << e.what() << std::endl;
        return {}; 
    }
    return std::move(column);
}

template <typename T>
static json loadCsvDataToJson(
    rapidcsv::Document const &doc,
    std::string const &columnName
) {
    json column(json::value_t::array);
    try {
        auto numRows = doc.GetRowCount();
        auto columnIndex = doc.GetColumnIdx(columnName);

        for (auto rowIndex = 0L; rowIndex < numRows; ++rowIndex)
        {
            column.push_back(doc.GetCell<json>(
                columnIndex,
                rowIndex,
                [](std::string const &str, json &val)
                {
                    if (!std::is_same<T, std::string>::value)
                    {
                        if (str.empty())
                        {
                            val = json{};
                            return;
                        }
                    }
                    if (std::is_same<T, double_t>::value)
                    {
                        size_t pos;
                        val = std::stod(str, &pos);
                        if (pos != str.length())
                        {
                            throw std::invalid_argument("failed to convert " + str + " into double");
                        }
                    }
                    else if (std::is_same<T, int64_t>::value)
                    {
                        size_t pos;
                        val = std::stol(str, &pos);
                        if (pos != str.length())
                        {
                            throw std::invalid_argument("failed to convert " + str + " into int");
                        }
                    }
                    else
                    {
                        val = str; 
                    }
                }
            ));
        }
    }
    catch (std::invalid_argument const &e)
    {
        // load function will try again with different type
        // std::cerr << "Error opening CSV file: " << e.what() << std::endl;
        return json{}; 
    }
    return std::move(column);
}

enum class DataType { 
    Int, 
    Double, 
    String, 
    None 
};

// struct LoadedColumn 
// {
//     DataType type = DataType::None;
//     std::vector<int64_t> intData;
//     std::vector<double> doubleData;
//     std::vector<std::string> stringData;
// };

using ColumnDataType = std::variant<
    std::vector<int64_t>, 
    std::vector<double>, 
    std::vector<std::string>
>;

static std::optional<ColumnDataType> tryLoadColumn(
    const rapidcsv::Document& doc, 
    const std::string& columnName
) {
    std::vector<std::optional<int64_t>> columnInt = loadCsvData<int64_t>(doc, columnName);
    if (!columnInt.empty()) {
        std::vector<int64_t> intData;
        intData.reserve(columnInt.size());
        for (const auto& val : columnInt) {
            if (val) intData.push_back(*val);
        }
        return intData;
    }

    std::vector<std::optional<double>> columnDouble = loadCsvData<double>(doc, columnName);
    if (!columnDouble.empty()) {
        std::vector<double> doubleData;
        doubleData.reserve(columnDouble.size());
        for (const auto& val : columnDouble) {
            if (val) doubleData.push_back(*val);
        }
        return doubleData;
    }

    std::vector<std::optional<std::string>> columnString = loadCsvData<std::string>(doc, columnName);
    if (!columnString.empty()) {
        std::vector<std::string> stringData;
        stringData.reserve(columnString.size());
        for (const auto& val : columnString) {
            if (val) stringData.push_back(*val);
        }
        return stringData;
    }

    return std::nullopt;
}



// static LoadedColumn tryLoadColumn(
//     const rapidcsv::Document& doc, 
//     const std::string& columnName
// ) {
//     LoadedColumn result;

//     std::vector<std::optional<long>> columnInt = loadCsvData<int64_t>(doc, columnName);
//     if (!columnInt.empty()) 
//     {
//         result.type = DataType::Int;
//         result.intData.reserve(columnInt.size());
//         for (const std::optional<long>& val : columnInt) 
//         {
//             if (val) result.intData.push_back(*val);
//         }
//         return result;
//     }

//     std::vector<std::optional<double>> columnDouble = loadCsvData<double_t>(doc, columnName);
//     if (!columnDouble.empty()) 
//     {
//         result.type = DataType::Double;
//         result.doubleData.reserve(columnDouble.size());
//         for (const std::optional<double>& val : columnDouble) 
//         {
//             if (val) result.doubleData.push_back(*val);
//         }
//         return result;
//     }

//     std::vector<std::optional<std::string>> columnString = loadCsvData<std::string>(doc, columnName);
//     if (!columnString.empty()) 
//     {
//         result.type = DataType::String;
//         result.stringData.reserve(columnString.size());
//         for (const std::optional<std::string>& val : columnString) 
//         {
//             if (val) result.stringData.push_back(*val);
//         }
//         return result;
//     }

//     return result;
// }