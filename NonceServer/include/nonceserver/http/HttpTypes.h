// HttpTypes.h: HTTP 请求/响应数据结构。
#pragma once

#include "nonceserver/json/Json.h"

#include <map>
#include <string>
#include <string_view>

namespace nonceserver {

struct HttpRequest {
    std::string method;
    std::string target;
    std::string path;
    std::map<std::string, std::string> query;
    std::map<std::string, std::string> headers;  // 键统一小写
    std::string body;

    std::string Header(std::string_view name) const;
    std::string Query(std::string_view name, std::string fallback = {}) const;
};

struct HttpResponse {
    int status = 200;
    std::map<std::string, std::string> headers;
    std::string body;

    static HttpResponse Json(const Json& body, int status = 200);
    static HttpResponse Text(std::string body, int status = 200,
                             std::string contentType = "text/plain; charset=utf-8");
    static HttpResponse Error(int status, std::string code, std::string message);
};

const char* ReasonPhrase(int status);

}  // namespace nonceserver
