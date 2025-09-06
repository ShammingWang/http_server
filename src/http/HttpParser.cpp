#include "http/HttpParser.h"
#include "http/HttpResponse.h"
#include "http/Router.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace http {

static inline std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e-1]))) --e;
    return s.substr(b, e - b);
}

Method parse_method(const std::string& s) {
    if (s == "GET") return Method::GET;
    if (s == "POST") return Method::POST;
    if (s == "PUT") return Method::PUT;
    if (s == "DELETE") return Method::DELETE_;
    if (s == "HEAD") return Method::HEAD;
    if (s == "OPTIONS") return Method::OPTIONS;
    if (s == "PATCH") return Method::PATCH;
    return Method::UNKNOWN;
}

bool HttpRequest::keep_alive() const {
    auto it = headers.find("Connection");
    if (it != headers.end()) {
        std::string v = it->second; std::transform(v.begin(), v.end(), v.begin(), ::tolower);
        if (v == "close") return false;
        if (v == "keep-alive") return true;
    }
    // HTTP/1.1 default keep-alive
    return version == "HTTP/1.1";
}

HttpParser::HttpParser() = default;

bool HttpParser::parse_request_line(const std::string& line) {
    std::istringstream iss(line);
    std::string method, uri, version;
    if (!(iss >> method >> uri >> version)) return false;
    req_.method = parse_method(method);
    req_.uri = uri;
    req_.version = version;
    // split path and query
    auto pos = uri.find('?');
    if (pos == std::string::npos) req_.path = uri; else { req_.path = uri.substr(0, pos); req_.query = uri.substr(pos+1); }
    return true;
}

bool HttpParser::parse_header_line(const std::string& line) {
    auto pos = line.find(':');
    if (pos == std::string::npos) return false;
    auto key = trim(line.substr(0, pos));
    auto val = trim(line.substr(pos+1));
    req_.headers[key] = val;
    return true;
}

bool HttpParser::parse(const std::string& data) {
    // naive parsing: expect CRLF line endings
    size_t i = consumed_;
    auto read_line = [&](std::string& out)->bool{
        size_t j = data.find("\r\n", i);
        if (j == std::string::npos) return false;
        out = data.substr(i, j - i);
        i = j + 2;
        return true;
    };

    if (state_ == State::REQUEST_LINE) {
        std::string line;
        if (!read_line(line)) return false;
        if (!parse_request_line(line)) { state_ = State::ERROR; return false; }
        state_ = State::HEADERS;
    }

    if (state_ == State::HEADERS) {
        std::string line;
        while (true) {
            if (!read_line(line)) { consumed_ = i; return false; }
            if (line.empty()) break; // end headers
            if (!parse_header_line(line)) { state_ = State::ERROR; return false; }
        }
        // body?
        if (auto it = req_.headers.find("Content-Length"); it != req_.headers.end()) {
            expected_body_len_ = static_cast<size_t>(std::stoul(it->second));
            state_ = expected_body_len_ > 0 ? State::BODY : State::COMPLETE;
        } else {
            expected_body_len_ = 0;
            state_ = State::COMPLETE;
        }
    }

    if (state_ == State::BODY) {
        size_t remain = data.size() - i;
        if (remain < expected_body_len_) { consumed_ = i; return false; }
        req_.body = data.substr(i, expected_body_len_);
        i += expected_body_len_;
        state_ = State::COMPLETE;
    }

    consumed_ = i;
    return state_ == State::COMPLETE;
}

void HttpParser::reset() {
    state_ = State::REQUEST_LINE;
    req_ = HttpRequest{};
    expected_body_len_ = 0;
    consumed_ = 0;
}

void HttpResponse::set_keep_alive(bool on) {
    headers["Connection"] = on ? "keep-alive" : "close";
}

static std::string status_reason(int status) {
    switch (status) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "";
    }
}

std::string HttpResponse::to_string() const {
    std::string out;
    std::string r = reason.empty() ? status_reason(status) : reason;
    out += "HTTP/1.1 " + std::to_string(status) + " " + r + "\r\n";
    // ensure Content-Length
    bool has_len = false;
    for (auto const& kv : headers) {
        if (kv.first == "Content-Length") has_len = true;
    }
    if (!has_len) {
        out += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    }
    for (auto const& kv : headers) {
        out += kv.first + ": " + kv.second + "\r\n";
    }
    out += "\r\n";
    out += body;
    return out;
}

bool Router::route(const HttpRequest& req, HttpResponse& resp) const {
    auto it = routes_.find(RouteKey{req.method, req.path});
    if (it != routes_.end()) { it->second(req, resp); return true; } // 拿到这处理函数it->second并调用
    // static files
    if (!static_prefix_.empty() && !static_root_.empty()) {
        if (req.path.rfind(static_prefix_, 0) == 0) {
            std::string rel = req.path.substr(static_prefix_.size());
            if (!rel.empty() && rel[0] == '/') rel.erase(0,1);
            std::string full = static_root_ + "/" + rel;
            // very basic path guard
            if (full.find("..") != std::string::npos) {
                resp.status = 400; resp.reason = "Bad Request"; resp.body = "Bad path"; return true;
            }
            // read file
            FILE* f = fopen(full.c_str(), "rb");
            if (!f) { resp.status = 404; resp.reason = "Not Found"; resp.body = "Not Found"; return true; }
            std::string data;
            char buf[8192]; size_t n;
            while ((n = fread(buf,1,sizeof(buf),f))>0) data.append(buf, buf+n);
            fclose(f);
            resp.status = 200; resp.reason = "OK"; resp.body = std::move(data);
            // naive content-type by extension
            auto lower = [](std::string s){ for (auto& ch : s) ch = (char)std::tolower((unsigned char)ch); return s; };
            std::string ext = lower(full);

            if      (ext.rfind(".html") != std::string::npos) resp.set_content_type("text/html; charset=utf-8");
            else if (ext.rfind(".json") != std::string::npos) resp.set_content_type("application/json");
            else if (ext.rfind(".css")  != std::string::npos) resp.set_content_type("text/css");
            else if (ext.rfind(".js")   != std::string::npos) resp.set_content_type("application/javascript");
            else if (ext.rfind(".pdf")  != std::string::npos) {
                resp.set_content_type("application/pdf");
                resp.set_header("Content-Disposition", "inline"); // 确保内联预览而非下载
            }
            //（常见图片/字体）
            else if (ext.rfind(".png")  != std::string::npos) resp.set_content_type("image/png");
            else if (ext.rfind(".jpg")  != std::string::npos || ext.rfind(".jpeg") != std::string::npos) resp.set_content_type("image/jpeg");
            else if (ext.rfind(".svg")  != std::string::npos) resp.set_content_type("image/svg+xml");
            else if (ext.rfind(".woff2")!= std::string::npos) resp.set_content_type("font/woff2");
            else resp.set_content_type("application/octet-stream");

            return true;
        }
    }
    return false;
}

} // namespace http

