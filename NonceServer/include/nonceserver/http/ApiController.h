// ApiController.h: 将规范 §4 的 HTTP 端点映射到 CommitmentService。
#pragma once

#include "nonceserver/http/HttpTypes.h"
#include "nonceserver/http/Router.h"
#include "nonceserver/service/CommitmentService.h"

namespace nonceserver {

class ApiController {
public:
    explicit ApiController(CommitmentService& service) : service_(service) {}

    void RegisterRoutes(Router& router);

private:
    HttpResponse HandleCreateCommitment(const HttpRequest& request, const RouteParams& params);
    HttpResponse HandleGetCommitment(const HttpRequest& request, const RouteParams& params);
    HttpResponse HandleListIssued(const HttpRequest& request, const RouteParams& params);
    HttpResponse HandleChainHead(const HttpRequest& request, const RouteParams& params);
    HttpResponse HandleChainProof(const HttpRequest& request, const RouteParams& params);
    HttpResponse HandleDraws(const HttpRequest& request, const RouteParams& params);

    CommitmentService& service_;
};

}  // namespace nonceserver
