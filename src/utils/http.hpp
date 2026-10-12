#pragma once
#include <string>
#include <vector>

struct HttpResponse {
    long statusCode;
    std::string body;

    bool ok() const {
        return statusCode >= 200 && statusCode < 300;
    }
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    HttpResponse post(const std::string& url,
                      const std::vector<std::string>& headers,
                      const std::string& body,
                      long timeoutSec = 30);
};