#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

namespace http {

using Handler = std::function<void(const HttpRequest&, HttpResponse&)>;

struct RouteKey {
    Method method;
    std::string path;
    bool operator==(const RouteKey& other) const { return method == other.method && path == other.path; }
};

struct RouteKeyHash {
    std::size_t operator()(const RouteKey& k) const {
        return (static_cast<std::size_t>(k.method) << 32) ^ std::hash<std::string>()(k.path);
    }
};

class Router {
public:
    void get(const std::string& path, Handler h) { add(Method::GET, path, std::move(h)); }
    void post(const std::string& path, Handler h) { add(Method::POST, path, std::move(h)); }
    void add(Method m, const std::string& path, Handler h) {
        routes_[RouteKey{m, path}] = std::move(h);
    }
    bool route(const HttpRequest& req, HttpResponse& resp) const;

    // simple static file serving under a prefix mapping to a root directory
    void set_static(const std::string& url_prefix, const std::string& dir_root) {
        static_prefix_ = url_prefix; static_root_ = dir_root;
    }

private:
    std::unordered_map<RouteKey, Handler, RouteKeyHash> routes_;
    std::string static_prefix_;
    std::string static_root_;
};

} // namespace http

