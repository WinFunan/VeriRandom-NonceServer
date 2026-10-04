#include "nonceserver/common/Result.h"

namespace nonceserver {

const char* ToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::Ok: return "ok";
        case ErrorCode::InvalidArgument: return "invalid_argument";
        case ErrorCode::NotFound: return "not_found";
        case ErrorCode::Conflict: return "conflict";
        case ErrorCode::StorageFailure: return "storage_failure";
        case ErrorCode::CryptoFailure: return "crypto_failure";
        case ErrorCode::Unavailable: return "unavailable";
        case ErrorCode::NotImplemented: return "not_implemented";
        case ErrorCode::Internal: return "internal";
    }
    return "unknown";
}

}  // namespace nonceserver
