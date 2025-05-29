#include "BsonSerializer/BsonSerializer.hpp"
#include "WisentSerializer/WisentHelpers.hpp"
#include "WisentSerializer/WisentSerializer.hpp"
#include "WisentCompressor/CompressionPipeline.hpp"
#include "WisentCompressor/WisentCompressor.hpp"
// #include "WisentParser/WisentParser.hpp"
#include "ServerHelpers.hpp"
#include <chrono>
#include <iostream>
#include <string>

int main(int argc, char **argv)
{
    int httpPort = 8000;
    httplib::Server svr;
    svr.Get("/serialize", [&](const httplib::Request &req, httplib::Response &res) 
    {
        std::string filename;
        std::string filepath;
        std::string csvPrefix;
        bool disableRLE = false;
        bool disableCsvHandling = false;
        parseReqestParams(
            req.params, 
            filename, 
            filepath, 
            csvPrefix,
            disableRLE, 
            disableCsvHandling
        );

        const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
        Result<WisentRootExpression*> serializeResult = wisent::serializer::load(
            filepath, 
            filename, 
            csvPrefix, 
            disableRLE,
            disableCsvHandling
        );
        const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();

        handleResponse(
            res, 
            serializeResult, 
            start, 
            end
        );
        return;
    });

    svr.Post("/serialize", [&](const httplib::Request &req, httplib::Response &res) 
    {
        std::string filename;
        std::string filepath;
        std::string csvPrefix; 
        bool disableRLE = false;
        bool disableCsvHandling = false;
        parseReqestParams(
            req.params, 
            filename, 
            filepath, 
            csvPrefix,
            disableRLE, 
            disableCsvHandling
        );

        const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
        Result<WisentRootExpression*> serializeResult = makeResult<WisentRootExpression*>(nullptr); 
        // Result<WisentRootExpression*> serializeResult = wisent::serializer::load(
        //     filename, 
        //     filepath, 
        //     csvPrefix, 
        //     disableRLE,
        //     disableCsvHandling
        // );
        const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();

        handleResponse(
            res, 
            serializeResult, 
            start, 
            end
        );
        return;
    });

    svr.Post("/compress", [&](const httplib::Request &req, httplib::Response &res) 
    {
        std::string filename;
        std::string filepath;
        std::string csvPrefix; 
        bool disableRLE = false;
        bool disableCsvHandling = false;
        parseReqestParams(
            req.params, 
            filename, 
            filepath, 
            csvPrefix,
            disableRLE, 
            disableCsvHandling
        );

        Result<std::unordered_map<std::string, CompressionPipeline*>> CompressionPipelineMapResult; 
        parseCompressionPipeline(
            req.body, 
            CompressionPipelineMapResult
        );
        if (!CompressionPipelineMapResult.success()) 
        {
            res.status = httplib::BadRequest_400; 
            res.set_content(CompressionPipelineMapResult.getError(), "text/plain");
            return;
        }
        
        const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
        Result<WisentRootExpression*> compressResult = wisent::compressor::CompressAndLoadJson(
            filename, 
            filepath, 
            csvPrefix, 
            CompressionPipelineMapResult.value.value(), 
            disableRLE,
            disableCsvHandling
        );
        const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();

        handleResponse(
            res, 
            compressResult, 
            start, 
            end
        );
        return;
    });

    svr.Get("/stop", [&](const httplib::Request & /*req*/, httplib::Response & /*res*/) 
    { 
        svr.stop(); 
    });

    std::cout << "Server running on port " << httpPort << "..." << std::endl;

    svr.listen("0.0.0.0", httpPort);
    return 0;
}