#pragma once
#include <string>
#include <optional>
#include "http/HttpRequest.h"


namespace http {

// Very simple HTTP/1.1 parser: request-line + headers + optional body by Content-Length
class HttpParser {
public:
    enum class State { REQUEST_LINE, HEADERS, BODY, COMPLETE, ERROR };

    HttpParser();
    // Feed data and try to parse one request. Returns true when a full request is parsed.
    bool parse(const std::string& data);

    bool complete() const { return state_ == State::COMPLETE; }
    bool error() const { return state_ == State::ERROR; }
    const HttpRequest& request() const { return req_; }
    void reset();

private:
    State state_{State::REQUEST_LINE};
    HttpRequest req_;
    size_t expected_body_len_{0};
    size_t consumed_{0}; // how many chars of the input have been consumed

    bool parse_request_line(const std::string& line);
    bool parse_header_line(const std::string& line);
};

} // namespace http

