#include "nonceserver/http/Router.h"

namespace nonceserver {

std::string RouteParams::Get(std::string_view name) const {
    const auto it = values.find(std::string(name));
    return it == values.end() ? std::string() : it->second;
}

int64_t RouteParams::GetInt64(std::string_view name, int64_t fallback) const {
    const std::string value = Get(name);
    if (value.empty()) return fallback;
    try {
        return std::stoll(value);
    } catch (...) {
        return fallback;
    }
}

std::vector<std::string> Router::SplitPath(std::string_view path) {
    std::vector<std::string> segments;
    std::size_t index = 0;
    // 跳过首个 '/'，按 '/' 切分。
    if (index < path.size() && path[index] == '/') ++index;
    while (index < path.size()) {
        std::size_t next = path.find('/', index);
        if (next == std::string_view::npos) next = path.size();
        segments.emplace_back(path.substr(index, next - index));
        index = next + 1;
    }
    return segments;
}

void Router::Add(std::string method, std::string pattern, RouteHandler handler) {
    Route route;
    route.method = std::move(method);
    route.handler = std::move(handler);

    for (auto& segment : SplitPath(pattern)) {
        if (segment.size() >= 2 && segment.front() == '{' && segment.back() == '}') {
            route.isParam.push_back(true);
            route.segments.push_back(segment.substr(1, segment.size() - 2));
        } else {
            route.isParam.push_back(false);
            route.segments.push_back(std::move(segment));
        }
    }
    routes_.push_back(std::move(route));
}

DispatchResult Router::Dispatch(const HttpRequest& request, HttpResponse& response) const {
    const std::vector<std::string> pathSegments = SplitPath(request.path);
    bool pathMatched = false;

    for (const auto& route : routes_) {
        if (route.segments.size() != pathSegments.size()) continue;

        RouteParams params;
        bool matched = true;
        for (std::size_t i = 0; i < route.segments.size(); ++i) {
            if (route.isParam[i]) {
                params.values[route.segments[i]] = pathSegments[i];
            } else if (route.segments[i] != pathSegments[i]) {
                matched = false;
                break;
            }
        }
        if (!matched) continue;

        pathMatched = true;
        if (route.method != request.method) continue;

        response = route.handler(request, params);
        return DispatchResult::Matched;
    }

    return pathMatched ? DispatchResult::MethodNotAllowed : DispatchResult::NotFound;
}

}  // namespace nonceserver
