#include "nonceserver/http/ApiController.h"

namespace nonceserver {

namespace {

HttpResponse MapStatus(const Status& status) {
    int httpStatus = 500;
    switch (status.code()) {
        case ErrorCode::InvalidArgument: httpStatus = 400; break;
        case ErrorCode::NotFound: httpStatus = 404; break;
        case ErrorCode::Conflict: httpStatus = 409; break;
        case ErrorCode::NotImplemented: httpStatus = 501; break;
        case ErrorCode::Unavailable: httpStatus = 503; break;
        case ErrorCode::StorageFailure: httpStatus = 500; break;
        case ErrorCode::CryptoFailure: httpStatus = 500; break;
        case ErrorCode::Internal: httpStatus = 500; break;
        case ErrorCode::Ok: httpStatus = 200; break;
    }
    return HttpResponse::Error(httpStatus, ToString(status.code()), status.message());
}

bool ParseBody(const HttpRequest& request, Json& out, HttpResponse& error) {
    if (request.body.empty()) {
        error = HttpResponse::Error(400, "bad_request", "request body is required");
        return false;
    }
    std::string parseError;
    const auto parsed = Json::parse(request.body, &parseError);
    if (!parsed || !parsed->isObject()) {
        error = HttpResponse::Error(400, "bad_request", "invalid JSON body: " + parseError);
        return false;
    }
    out = *parsed;
    return true;
}

}  // namespace

void ApiController::RegisterRoutes(Router& router) {
    router.Add("POST", "/v1/commitments",
               [this](const HttpRequest& r, const RouteParams& p) { return HandleCreateCommitment(r, p); });
    router.Add("GET", "/v1/commitments/issued",
               [this](const HttpRequest& r, const RouteParams& p) { return HandleListIssued(r, p); });
    router.Add("GET", "/v1/commitments/{commitmentId}",
               [this](const HttpRequest& r, const RouteParams& p) { return HandleGetCommitment(r, p); });
    router.Add("GET", "/v1/chain/head",
               [this](const HttpRequest& r, const RouteParams& p) { return HandleChainHead(r, p); });
    router.Add("GET", "/v1/chain/proof/{sequence}",
               [this](const HttpRequest& r, const RouteParams& p) { return HandleChainProof(r, p); });
    router.Add("POST", "/v1/draws",
               [this](const HttpRequest& r, const RouteParams& p) { return HandleDraws(r, p); });
}

HttpResponse ApiController::HandleCreateCommitment(const HttpRequest& request,
                                                   const RouteParams&) {
    Json body;
    HttpResponse error;
    if (!ParseBody(request, body, error)) return error;

    const CommitmentInput input = CommitmentInput::FromJson(body);
    const auto result = service_.CreateCommitment(input);
    if (!result.ok()) return MapStatus(result.status());
    return HttpResponse::Json(result.value().ToJson(), 200);
}

HttpResponse ApiController::HandleGetCommitment(const HttpRequest&, const RouteParams& params) {
    const auto result = service_.GetCommitment(params.Get("commitmentId"));
    if (!result.ok()) return MapStatus(result.status());
    return HttpResponse::Json(result.value().ToRecordJson(), 200);
}

HttpResponse ApiController::HandleListIssued(const HttpRequest& request, const RouteParams&) {
    int64_t fromSequence = 0;
    if (const std::string value = request.Query("fromSequence"); !value.empty()) {
        try {
            fromSequence = std::stoll(value);
        } catch (...) {
            return HttpResponse::Error(400, "bad_request", "fromSequence must be an integer");
        }
    }
    std::size_t limit = 0;
    if (const std::string value = request.Query("limit"); !value.empty()) {
        try {
            limit = static_cast<std::size_t>(std::stoull(value));
        } catch (...) {
            return HttpResponse::Error(400, "bad_request", "limit must be an integer");
        }
    }

    const auto result = service_.ListIssued(fromSequence, limit);
    if (!result.ok()) return MapStatus(result.status());

    Json records(Json::Array{});
    for (const auto& record : result.value()) {
        records.push_back(record.ToRecordJson());
    }
    Json response(Json::Object{});
    response["chainHead"] = service_.GetChainHead().ToJson();
    response["commitments"] = std::move(records);
    return HttpResponse::Json(response, 200);
}

HttpResponse ApiController::HandleChainHead(const HttpRequest&, const RouteParams&) {
    return HttpResponse::Json(service_.GetChainHead().ToJson(), 200);
}

HttpResponse ApiController::HandleChainProof(const HttpRequest&, const RouteParams&) {
    // TODO(§4.5): 生成某条记录的链上包含证明（Merkle/前缀证明）。
    return HttpResponse::Error(501, "not_implemented", "chain inclusion proof is not implemented");
}

HttpResponse ApiController::HandleDraws(const HttpRequest& request, const RouteParams&) {
    Json body;
    HttpResponse error;
    if (!ParseBody(request, body, error)) return error;

    DrawDisclosure disclosure;
    disclosure.protocolVersion = static_cast<int>(body["protocolVersion"].asInt64(1));
    disclosure.commitmentId = body["commitmentId"].asString();
    disclosure.proofId = body["proofId"].asString();
    disclosure.proofHash = body["proofHash"].asString();
    if (const Json* consumed = body.find("consumedAtUtc"); consumed && consumed->isString()) {
        if (const auto parsed = ParseRfc3339(consumed->asString())) {
            disclosure.consumedAtUtc = *parsed;
        }
    }

    const Status status = service_.RecordConsumption(disclosure);
    if (!status.ok()) return MapStatus(status);

    Json response(Json::Object{});
    response["status"] = "recorded";
    response["commitmentId"] = disclosure.commitmentId;
    return HttpResponse::Json(response, 200);
}

}  // namespace nonceserver
