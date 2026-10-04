// Router.h: 极简方法 + 路径模板路由（支持 /v1/commitments/{commitmentId}）。
#pragma once

#include "nonceserver/http/HttpTypes.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace nonceserver {

struct RouteParams {
    std::map<std::string, std::string> values;

    std::string Get(std::string_view name) const;
    int64_t GetInt64(std::string_view name, int64_t fallback = 0) const;
};

using RouteHandler = std::function<HttpResponse(const HttpRequest&, const RouteParams&)>;

enum class DispatchResult { Matched, NotFound, MethodNotAllowed };

class Router {
public:
    void Add(std::string method, std::string pattern, RouteHandler handler);
    DispatchResult Dispatch(const HttpRequest& request, HttpResponse& response) const;

private:
    struct Route {
        std::string method;
        std::vector<std::string> segments;
        std::vector<bool> isParam;
        RouteHandler handler;
    };

    static std::vector<std::string> SplitPath(std::string_view path);
    std::vector<Route> routes_;
};

}  // namespace nonceserver
