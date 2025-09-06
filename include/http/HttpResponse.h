#pragma once
#include <string>
#include <unordered_map>

namespace http {

struct HttpResponse {
    int status{200};
    std::string reason{"OK"};
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    void set_content_type(const std::string& type) { headers["Content-Type"] = type; }
    void set_header(const std::string& key, const std::string& value) {
        headers[key] = value;
    }
    void set_keep_alive(bool on);
    std::string to_string() const;
};

} // namespace http

