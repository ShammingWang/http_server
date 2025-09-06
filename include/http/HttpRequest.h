#pragma once
#include <string>
#include <unordered_map>

namespace http {

enum class Method { GET, POST, PUT, DELETE_, HEAD, OPTIONS, PATCH, UNKNOWN };

struct HttpRequest {
    Method method{Method::UNKNOWN};
    std::string uri;
    std::string path;
    std::string query;
    std::string version{"HTTP/1.1"};
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    bool keep_alive() const;
};

Method parse_method(const std::string& s);

} // namespace http
