// CanonicalHash.h: 规范哈希（规范 §5、§7）。
//
// 所有跨端一致的规范编码都集中在本模块，便于与客户端逐字节对齐与审计。
// 约定（与规范 §5 一致）：
//   - 每个逻辑部分占一行，行内不注入换行，行间以 '\n' 分隔；
//   - 域分隔标签作为第一行（VeriRandomCommitment.../v1）；
//   - 时间为 RFC3339 UTC、固定 3 位毫秒。
// 若客户端实现采用不同的连接方式，只需修改本模块。
#pragma once

#include "nonceserver/domain/Commitment.h"

#include <cstdint>
#include <string>

namespace nonceserver {

std::string ComputeInputDigest(const CommitmentInput& input);
std::string ComputeCommitmentHash(const CommitmentRecord& record);
std::string ComputeRecordHash(int64_t sequence, const std::string& prevHash,
                              const std::string& commitmentHash);

// 响应签名投影：覆盖 commitmentHash + chainHead.recordHash + serverKeyId（规范 §5）。
std::string ComputeResponseSigningProjection(const std::string& commitmentHash,
                                             const std::string& chainHeadRecordHash,
                                             const std::string& serverKeyId);

}  // namespace nonceserver
