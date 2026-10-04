#include "nonceserver/json/Json.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace nonceserver {

Json::Json() : value_(nullptr) {}
Json::Json(std::nullptr_t) : value_(nullptr) {}
Json::Json(bool value) : value_(value) {}
Json::Json(int value) : value_(static_cast<int64_t>(value)) {}
Json::Json(unsigned int value) : value_(static_cast<uint64_t>(value)) {}
Json::Json(long value) : value_(static_cast<int64_t>(value)) {}
Json::Json(unsigned long value) : value_(static_cast<uint64_t>(value)) {}
Json::Json(long long value) : value_(static_cast<int64_t>(value)) {}
Json::Json(unsigned long long value) : value_(static_cast<uint64_t>(value)) {}
Json::Json(double value) : value_(value) {}
Json::Json(const char* value) : value_(std::string(value ? value : "")) {}
Json::Json(const std::string& value) : value_(value) {}
Json::Json(std::string&& value) : value_(std::move(value)) {}
Json::Json(const Array& value) : value_(value) {}
Json::Json(Array&& value) : value_(std::move(value)) {}
Json::Json(const Object& value) : value_(value) {}
Json::Json(Object&& value) : value_(std::move(value)) {}

Json::Type Json::type() const {
    switch (value_.index()) {
        case 0: return Type::Null;
        case 1: return Type::Bool;
        case 2: return Type::Int;
        case 3: return Type::UInt;
        case 4: return Type::Double;
        case 5: return Type::String;
        case 6: return Type::Array;
        case 7: return Type::Object;
    }
    return Type::Null;
}

bool Json::isNull() const { return std::holds_alternative<std::nullptr_t>(value_); }
bool Json::isBool() const { return std::holds_alternative<bool>(value_); }
bool Json::isNumber() const {
    return std::holds_alternative<int64_t>(value_) || std::holds_alternative<uint64_t>(value_) ||
           std::holds_alternative<double>(value_);
}
bool Json::isString() const { return std::holds_alternative<std::string>(value_); }
bool Json::isArray() const { return std::holds_alternative<Array>(value_); }
bool Json::isObject() const { return std::holds_alternative<Object>(value_); }

std::size_t Json::size() const {
    if (const auto* arr = std::get_if<Array>(&value_)) return arr->size();
    if (const auto* obj = std::get_if<Object>(&value_)) return obj->size();
    return 0;
}

bool Json::contains(std::string_view key) const {
    const auto* obj = std::get_if<Object>(&value_);
    return obj && obj->find(key) != obj->end();
}

const Json* Json::find(std::string_view key) const {
    const auto* obj = std::get_if<Object>(&value_);
    if (!obj) return nullptr;
    const auto it = obj->find(key);
    return it == obj->end() ? nullptr : &it->second;
}

Json& Json::operator[](std::string_view key) {
    if (!isObject()) value_ = Object{};
    auto& obj = std::get<Object>(value_);
    return obj[std::string(key)];
}

void Json::set(std::string key, Json value) {
    if (!isObject()) value_ = Object{};
    std::get<Object>(value_)[std::move(key)] = std::move(value);
}

const Json::Object& Json::object() const {
    static const Object kEmpty;
    const auto* obj = std::get_if<Object>(&value_);
    return obj ? *obj : kEmpty;
}

const Json::Array& Json::array() const {
    static const Array kEmpty;
    const auto* arr = std::get_if<Array>(&value_);
    return arr ? *arr : kEmpty;
}

Json::Array& Json::array() {
    if (!isArray()) value_ = Array{};
    return std::get<Array>(value_);
}

void Json::push_back(Json value) {
    if (!isArray()) value_ = Array{};
    std::get<Array>(value_).push_back(std::move(value));
}

const Json& Json::at(std::size_t index) const {
    static const Json kNull;
    const auto* arr = std::get_if<Array>(&value_);
    if (!arr || index >= arr->size()) return kNull;
    return (*arr)[index];
}

std::string Json::asString(std::string fallback) const {
    if (const auto* s = std::get_if<std::string>(&value_)) return *s;
    return fallback;
}

int64_t Json::asInt64(int64_t fallback) const {
    if (const auto* v = std::get_if<int64_t>(&value_)) return *v;
    if (const auto* u = std::get_if<uint64_t>(&value_)) return static_cast<int64_t>(*u);
    if (const auto* d = std::get_if<double>(&value_)) return static_cast<int64_t>(*d);
    return fallback;
}

uint64_t Json::asUInt64(uint64_t fallback) const {
    if (const auto* u = std::get_if<uint64_t>(&value_)) return *u;
    if (const auto* v = std::get_if<int64_t>(&value_)) return static_cast<uint64_t>(*v);
    if (const auto* d = std::get_if<double>(&value_)) return static_cast<uint64_t>(*d);
    return fallback;
}

double Json::asDouble(double fallback) const {
    if (const auto* d = std::get_if<double>(&value_)) return *d;
    if (const auto* v = std::get_if<int64_t>(&value_)) return static_cast<double>(*v);
    if (const auto* u = std::get_if<uint64_t>(&value_)) return static_cast<double>(*u);
    return fallback;
}

bool Json::asBool(bool fallback) const {
    if (const auto* b = std::get_if<bool>(&value_)) return *b;
    return fallback;
}

namespace {

void EncodeUtf8(uint32_t codePoint, std::string& out) {
    if (codePoint <= 0x7F) {
        out.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

void DumpString(std::string_view value, std::string& out) {
    out.push_back('"');
    for (unsigned char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                    out += buffer;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
}

std::string FormatDouble(double value) {
    if (std::isfinite(value) && value == std::floor(value) && std::fabs(value) < 1e15) {
        return std::to_string(static_cast<long long>(value));
    }
    std::ostringstream os;
    os << std::setprecision(17) << value;
    return os.str();
}

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    bool parse(Json& out) {
        skipWhitespace();
        if (!parseValue(out)) return false;
        skipWhitespace();
        if (position_ != text_.size()) {
            error_ = "trailing characters after JSON value";
            return false;
        }
        return true;
    }

    const std::string& error() const { return error_; }

private:
    bool fail(std::string message) {
        if (error_.empty()) error_ = std::move(message);
        return false;
    }

    void skipWhitespace() {
        while (position_ < text_.size()) {
            const char c = text_[position_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++position_;
            } else {
                break;
            }
        }
    }

    bool parseValue(Json& out) {
        if (position_ >= text_.size()) return fail("unexpected end of input");
        switch (text_[position_]) {
            case '{': return parseObject(out);
            case '[': return parseArray(out);
            case '"': {
                std::string value;
                if (!parseString(value)) return false;
                out = Json(std::move(value));
                return true;
            }
            case 't':
                if (text_.substr(position_, 4) == "true") {
                    position_ += 4;
                    out = Json(true);
                    return true;
                }
                return fail("invalid literal");
            case 'f':
                if (text_.substr(position_, 5) == "false") {
                    position_ += 5;
                    out = Json(false);
                    return true;
                }
                return fail("invalid literal");
            case 'n':
                if (text_.substr(position_, 4) == "null") {
                    position_ += 4;
                    out = Json(nullptr);
                    return true;
                }
                return fail("invalid literal");
            default: return parseNumber(out);
        }
    }

    bool parseObject(Json& out) {
        ++position_;  // '{'
        Json::Object object;
        skipWhitespace();
        if (position_ < text_.size() && text_[position_] == '}') {
            ++position_;
            out = Json(std::move(object));
            return true;
        }
        while (true) {
            skipWhitespace();
            if (position_ >= text_.size() || text_[position_] != '"') return fail("expected object key");
            std::string key;
            if (!parseString(key)) return false;
            skipWhitespace();
            if (position_ >= text_.size() || text_[position_] != ':') return fail("expected ':'");
            ++position_;
            skipWhitespace();
            Json value;
            if (!parseValue(value)) return false;
            object[std::move(key)] = std::move(value);
            skipWhitespace();
            if (position_ >= text_.size()) return fail("unterminated object");
            if (text_[position_] == ',') {
                ++position_;
                continue;
            }
            if (text_[position_] == '}') {
                ++position_;
                out = Json(std::move(object));
                return true;
            }
            return fail("expected ',' or '}'");
        }
    }

    bool parseArray(Json& out) {
        ++position_;  // '['
        Json::Array array;
        skipWhitespace();
        if (position_ < text_.size() && text_[position_] == ']') {
            ++position_;
            out = Json(std::move(array));
            return true;
        }
        while (true) {
            skipWhitespace();
            Json value;
            if (!parseValue(value)) return false;
            array.push_back(std::move(value));
            skipWhitespace();
            if (position_ >= text_.size()) return fail("unterminated array");
            if (text_[position_] == ',') {
                ++position_;
                continue;
            }
            if (text_[position_] == ']') {
                ++position_;
                out = Json(std::move(array));
                return true;
            }
            return fail("expected ',' or ']'");
        }
    }

    bool parseString(std::string& out) {
        ++position_;  // opening quote
        out.clear();
        while (position_ < text_.size()) {
            const char c = text_[position_++];
            if (c == '"') return true;
            if (c == '\\') {
                if (position_ >= text_.size()) return fail("unterminated escape");
                const char esc = text_[position_++];
                switch (esc) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        uint32_t codePoint = 0;
                        if (!parseHex4(codePoint)) return false;
                        if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {
                            if (position_ + 1 < text_.size() && text_[position_] == '\\' &&
                                text_[position_ + 1] == 'u') {
                                position_ += 2;
                                uint32_t low = 0;
                                if (!parseHex4(low)) return false;
                                if (low >= 0xDC00 && low <= 0xDFFF) {
                                    codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                                } else {
                                    EncodeUtf8(codePoint, out);
                                    codePoint = low;
                                }
                            }
                        }
                        EncodeUtf8(codePoint, out);
                        break;
                    }
                    default: return fail("invalid escape");
                }
            } else {
                out.push_back(c);
            }
        }
        return fail("unterminated string");
    }

    bool parseHex4(uint32_t& value) {
        if (position_ + 4 > text_.size()) return fail("invalid \\u escape");
        value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text_[position_++];
            value <<= 4;
            if (c >= '0' && c <= '9') value |= static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') value |= static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= static_cast<uint32_t>(c - 'A' + 10);
            else return fail("invalid hex digit in \\u escape");
        }
        return true;
    }

    bool parseNumber(Json& out) {
        const std::size_t start = position_;
        if (position_ < text_.size() && text_[position_] == '-') ++position_;
        if (position_ >= text_.size() || text_[position_] < '0' || text_[position_] > '9') {
            return fail("invalid number");
        }
        while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
            ++position_;
        }
        bool isInteger = true;
        if (position_ < text_.size() && text_[position_] == '.') {
            isInteger = false;
            ++position_;
            while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
                ++position_;
            }
        }
        if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
            isInteger = false;
            ++position_;
            if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-')) {
                ++position_;
            }
            while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
                ++position_;
            }
        }
        const std::string token(text_.substr(start, position_ - start));
        try {
            if (isInteger && !token.empty() && token[0] != '-') {
                const unsigned long long u = std::stoull(token);
                if (u <= static_cast<unsigned long long>(INT64_MAX)) {
                    out = Json(static_cast<int64_t>(u));
                } else {
                    out = Json(u);
                }
            } else if (isInteger) {
                out = Json(static_cast<int64_t>(std::stoll(token)));
            } else {
                out = Json(std::stod(token));
            }
        } catch (const std::exception&) {
            return fail("number out of range");
        }
        return true;
    }

    std::string_view text_;
    std::size_t position_ = 0;
    std::string error_;
};

}  // namespace

std::string Json::dump(int indent) const {
    std::string out;
    dumpTo(out, indent, 0);
    return out;
}

void Json::dumpTo(std::string& out, int indent, int depth) const {
    const bool pretty = indent >= 0;
    switch (type()) {
        case Type::Null:
            out += "null";
            break;
        case Type::Bool:
            out += std::get<bool>(value_) ? "true" : "false";
            break;
        case Type::Int:
            out += std::to_string(std::get<int64_t>(value_));
            break;
        case Type::UInt:
            out += std::to_string(std::get<uint64_t>(value_));
            break;
        case Type::Double:
            out += FormatDouble(std::get<double>(value_));
            break;
        case Type::String:
            DumpString(std::get<std::string>(value_), out);
            break;
        case Type::Array: {
            const auto& array = std::get<Array>(value_);
            if (array.empty()) {
                out += "[]";
                break;
            }
            out.push_back('[');
            for (std::size_t i = 0; i < array.size(); ++i) {
                if (pretty) out += "\n" + std::string((depth + 1) * indent, ' ');
                array[i].dumpTo(out, indent, depth + 1);
                if (i + 1 < array.size()) out.push_back(',');
            }
            if (pretty) out += "\n" + std::string(depth * indent, ' ');
            out.push_back(']');
            break;
        }
        case Type::Object: {
            const auto& object = std::get<Object>(value_);
            if (object.empty()) {
                out += "{}";
                break;
            }
            out.push_back('{');
            std::size_t index = 0;
            for (const auto& [key, value] : object) {
                if (pretty) out += "\n" + std::string((depth + 1) * indent, ' ');
                DumpString(key, out);
                out += pretty ? ": " : ":";
                value.dumpTo(out, indent, depth + 1);
                if (++index < object.size()) out.push_back(',');
            }
            if (pretty) out += "\n" + std::string(depth * indent, ' ');
            out.push_back('}');
            break;
        }
    }
}

std::optional<Json> Json::parse(std::string_view text, std::string* error) {
    Json result;
    Parser parser(text);
    if (!parser.parse(result)) {
        if (error) *error = parser.error();
        return std::nullopt;
    }
    return result;
}

}  // namespace nonceserver
