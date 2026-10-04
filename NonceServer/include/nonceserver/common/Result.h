// Status.h: 统一的错误码与结果封装，避免跨层抛异常。
#pragma once

#include <optional>
#include <string>
#include <utility>

namespace nonceserver {

enum class ErrorCode {
    Ok = 0,
    InvalidArgument,
    NotFound,
    Conflict,
    StorageFailure,
    CryptoFailure,
    Unavailable,
    NotImplemented,
    Internal,
};

const char* ToString(ErrorCode code);

// 轻量状态对象：既表示成功也表示失败原因。
class Status {
public:
    Status() = default;
    Status(ErrorCode code, std::string message)
        : code_(code), message_(std::move(message)) {}

    static Status Ok() { return Status(); }
    static Status Error(ErrorCode code, std::string message) {
        return Status(code, std::move(message));
    }

    bool ok() const { return code_ == ErrorCode::Ok; }
    ErrorCode code() const { return code_; }
    const std::string& message() const { return message_; }

private:
    ErrorCode code_ = ErrorCode::Ok;
    std::string message_;
};

// 带值的结果：失败时携带 Status，成功时携带 T。
template <typename T>
class Result {
public:
    Result(T value) : value_(std::move(value)) {}
    Result(Status status) : status_(std::move(status)) {}

    bool ok() const { return value_.has_value(); }
    const Status& status() const { return status_; }

    T& value() { return *value_; }
    const T& value() const { return *value_; }

    T value_or(T fallback) const { return value_ ? *value_ : std::move(fallback); }

private:
    std::optional<T> value_;
    Status status_;
};

}  // namespace nonceserver
