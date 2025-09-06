#include "server/Server.h"
#include "http/Router.h"
#include "http/HttpResponse.h"
#include <iostream>

int main() {
    http::Router router;
    router.get("/hello", [](const http::HttpRequest& req, http::HttpResponse& resp){
        (void)req; // 消除req未使用的警告
        resp.set_content_type("application/json");
        resp.body = R"({"message":"Hello, C++ Web Server!"})";
    });
    router.set_static("/static", "static");
    
    net::Server server(8080);
    server.set_router(&router);
    if (!server.listen_and_serve()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }
    return 0;
}
