// Json.h: 项目自包含的最小 JSON 值类型（解析 + 序列化）。
// 不引入第三方依赖；支持 int64/uint64 以保全 sequence、round 等整数精度。
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace nonceserver {

class Json {
public:
    using Array = std::vector<Json>;
    using Object = std::map<std::string, Json, std::less<>>;

    enum class Type { Null, Bool, Int, UInt, Double, String, Array, Object };

    Json();
    Json(std::nullptr_t);
    Json(bool value);
    Json(int value);
    Json(unsigned int value);
    Json(long value);
    Json(unsigned long value);
    Json(long long value);
    Json(unsigned long long value);
    Json(double value);
    Json(const char* value);
    Json(const std::string& value);
    Json(std::string&& value);
    Json(const Array& value);
    Json(Array&& value);
    Json(const Object& value);
    Json(Object&& value);

    Type type() const;
    bool isNull() const;
    bool isBool() const;
    bool isNumber() const;
    bool isString() const;
    bool isArray() const;
    bool isObject() const;
    std::size_t size() const;

    // 对象访问
    bool contains(std::string_view key) const;
    const Json* find(std::string_view key) const;
    Json& operator[](std::string_view key);  // 不存在时创建（必要时把自身转为对象）
    void set(std::string key, Json value);
    const Object& object() const;

    // 数组访问
    const Array& array() const;
    Array& array();
    void push_back(Json value);
    const Json& at(std::size_t index) const;

    // 标量访问（类型不符时返回 fallback）
    std::string asString(std::string fallback = {}) const;
    int64_t asInt64(int64_t fallback = 0) const;
    uint64_t asUInt64(uint64_t fallback = 0) const;
    double asDouble(double fallback = 0.0) const;
    bool asBool(bool fallback = false) const;

    // indent < 0 为紧凑输出；否则按层级缩进。
    std::string dump(int indent = -1) const;
    static std::optional<Json> parse(std::string_view text, std::string* error = nullptr);

private:
    void dumpTo(std::string& out, int indent, int depth) const;

    std::variant<std::nullptr_t, bool, int64_t, uint64_t, double, std::string, Array, Object>
        value_;
};

}  // namespace nonceserver
