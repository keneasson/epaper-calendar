/*
 * HTTPClient API Simulation for Native Builds
 * Reads API responses from local JSON files.
 */

#ifndef HTTPCLIENT_SIM_H
#define HTTPCLIENT_SIM_H

#include "Arduino.h"
#include <fstream>
#include <sstream>

// HTTP status codes
#define HTTP_CODE_OK 200
#define HTTP_CODE_NOT_FOUND 404
#define HTTP_CODE_INTERNAL_SERVER_ERROR 500

class HTTPClient {
public:
    HTTPClient() : timeout_(10000) {}

    void begin(const char* url) {
        url_ = url;
        printf("[SIM] HTTP begin: %s\n", url);
    }

    void begin(const String& url) {
        begin(url.c_str());
    }

    void end() {
        url_.clear();
    }

    void setTimeout(int timeout) {
        timeout_ = timeout;
    }

    void addHeader(const char* name, const String& value) {
        printf("[SIM] HTTP header: %s: %s\n", name, value.c_str());
    }

    int GET() {
        printf("[SIM] HTTP GET request\n");

        // Try to read from simulation/api_response.json
        std::ifstream file("simulation/api_response.json");
        if (!file.is_open()) {
            // Try alternate path (when running from project root)
            file.open("api_response.json");
        }

        if (!file.is_open()) {
            printf("[SIM] No api_response.json found, using default response\n");
            // Use default response
            response_ = R"({
  "service_date": "Sunday, December 1, 2024",
  "service_time": "11:00 AM",
  "events": [
    {
      "time": "9:30 AM",
      "title": "Sunday School",
      "location": "Fellowship Hall"
    },
    {
      "time": "11:00 AM",
      "title": "Worship Service",
      "location": "Main Sanctuary",
      "speaker": "Pastor John Smith"
    },
    {
      "time": "6:00 PM",
      "title": "Evening Prayer",
      "location": "Chapel"
    }
  ]
})";
            return HTTP_CODE_OK;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        response_ = buffer.str();
        file.close();

        printf("[SIM] Loaded response from file (%zu bytes)\n", response_.length());
        return HTTP_CODE_OK;
    }

    String getString() {
        return String(response_.c_str());
    }

    const char* errorToString(int error) {
        switch (error) {
            case HTTP_CODE_NOT_FOUND: return "Not Found";
            case HTTP_CODE_INTERNAL_SERVER_ERROR: return "Internal Server Error";
            default: return "Unknown Error";
        }
    }

private:
    std::string url_;
    std::string response_;
    int timeout_;
};

#endif // HTTPCLIENT_SIM_H
