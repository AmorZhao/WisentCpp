#pragma once
#include <string>
#include <vector>
#include <boost/optional.hpp>  // C++17: std::optional 
#include <sys/resource.h>
#include <iostream>
#include "../../Include/json.h"
#include "../../Include/rapidcsv.h"

using json = nlohmann::json;  

static rapidcsv::Document openCsvFile(std::string const &filepath)
{
    struct rusage usage;
    // getrusage(RUSAGE_SELF, &usage);
    // std::cout << "Memory usage: " << usage.ru_maxrss << " KB" << std::endl;
    try {
        rapidcsv::Document doc(
            filepath,
            rapidcsv::LabelParams(),
            rapidcsv::SeparatorParams(),
            rapidcsv::ConverterParams(),
            rapidcsv::LineReaderParams()
        );
        // getrusage(RUSAGE_SELF, &usage);
        // std::cout << "Memory usage: " << usage.ru_maxrss << " KB" << std::endl;
        // auto rows = doc.GetRowCount();
        // auto cols = doc.GetColumnCount();
        // std::cout << "Rows: " << rows << ", Columns: " << cols << std::endl;
        return doc;
    } catch (const std::exception &e) {
        std::cerr << "Error opening CSV file: " << e.what() << std::endl;
        throw;
    }
}

template <typename T>
static std::vector<boost::optional<T>> loadCsvData(
    rapidcsv::Document const &doc,
    std::string const &columnName
) {
    std::vector<boost::optional<T>> column;
    try {
        auto numRows = doc.GetRowCount();
        column.reserve(numRows);
        auto columnIndex = doc.GetColumnIdx(columnName);

        for (auto rowIndex = 0L; rowIndex < numRows; ++rowIndex)
        {
            column.emplace_back(doc.GetCell<boost::optional<T>>(
                columnIndex,
                rowIndex,
                [](std::string const &str, boost::optional<T> &val)
                {
                    // Handle empty strings if T is not std::string
                    if (!std::is_same<T, std::string>::value)
                    {
                        if (str.empty())
                        {
                            val = {}; 
                            return;
                        }
                    }
                    // Convert string to integer
                    if (std::is_same<T, int64_t>::value)
                    {
                        size_t pos;
                        val = json(std::stol(str, &pos));
                        if (pos != str.length())
                        {
                            throw std::invalid_argument("failed to convert the whole string");
                        }
                    }
                    // Convert string to double
                    else if (std::is_same<T, double_t>::value)
                    {
                        size_t pos;
                        val = json(std::stod(str, &pos));
                        if (pos != str.length())
                        {
                            throw std::invalid_argument("failed to convert the whole string");
                        }
                    }
                    // Default: store string as-is
                    else
                    {
                        val = json(str); // Store string as JSON
                    }
                }
            ));
        }
    }
    catch (std::invalid_argument const & /*e*/)
    {
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
                    // Handle empty strings if T is not std::string
                    if (!std::is_same<T, std::string>::value)
                    {
                        if (str.empty())
                        {
                            val = json{};
                            return;
                        }
                    }
                    // Convert string to integer
                    if (std::is_same<T, int64_t>::value)
                    {
                        size_t pos;
                        val = std::stol(str, &pos);
                        if (pos != str.length())
                        {
                            throw std::invalid_argument("failed to convert the whole string");
                        }
                    }
                    // Convert string to double
                    else if (std::is_same<T, double_t>::value)
                    {
                        size_t pos;
                        val = std::stod(str, &pos);
                        if (pos != str.length())
                        {
                            throw std::invalid_argument("failed to convert the whole string");
                        }
                    }
                    // Default: store string as-is
                    else
                    {
                        val = str; 
                    }
                }
            ));
        }
    }
    catch (std::invalid_argument const & /*e*/)
    {
        return json{}; // Return empty JSON array in case of errors
    }
    return std::move(column);
}
