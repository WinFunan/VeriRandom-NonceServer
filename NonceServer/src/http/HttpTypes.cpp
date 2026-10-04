#include "nonceserver/http/HttpTypes.h"

#include <algorithm>
#include <cctype>

namespace nonceserver {

namespace {
std::string ToLower(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}
}  // namespace

std::string HttpRequest::Header(std::string_view name) const {
    const auto it = headers.find(ToLower(name));
    return it == headers.end() ? std::string() : it->second;
}

std::string HttpRequest::Query(std::string_view name, std::string fallback) const {
    const auto it = query.find(std::string(name));
    return it == query.end() ? std::move(fallback) : it->second;
}

HttpResponse HttpResponse::Json(const nonceserver::Json& body, int status) {
    HttpResponse response;
    response.status = status;
    response.headers["Content-Type"] = "application/json; charset=utf-8";
    response.body = body.dump();
    return response;
}

HttpResponse HttpResponse::Text(std::string body, int status, std::string contentType) {
    HttpResponse response;
    response.status = status;
    response.headers["Content-Type"] = std::move(contentType);
    response.body = std::move(body);
    return response;
}

HttpResponse HttpResponse::Error(int status, std::string code, std::string message) {
    nonceserver::Json json(nonceserver::Json::Object{});
    json["error"] = std::move(code);
    json["message"] = std::move(message);
    return Json(json, status);
}

const char* ReasonPhrase(int status) {
    switch (status) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 415: return "Unsupported Media Type";
        case 422: return "Unprocessable Entity";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 503: return "Service Unavailable";
        default: return "Unknown";
    }
}

}  // namespace nonceserver
