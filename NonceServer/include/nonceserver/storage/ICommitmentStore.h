// ICommitmentStore.h: 只可追加的承诺链存储抽象（规范 §3、§7、§11）。
#pragma once

#include "nonceserver/common/Result.h"
#include "nonceserver/domain/Commitment.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace nonceserver {

// 存储层不得存在 UPDATE/DELETE 路径；consumed 通过追加“消费披露”记录实现。
class ICommitmentStore {
public:
    virtual ~ICommitmentStore() = default;

    virtual Status Initialize() = 0;

    // 在锁内分配 sequence 与 prevHash，调用 finalize 计算 commitmentHash/recordHash，
    // 完成“先落盘（含 fsync）再返回”。同一 clientId+clientNonce 已存在时返回既有记录以实现幂等。
    using Finalize = std::function<Status(CommitmentRecord&)>;
    virtual Result<CommitmentRecord> Append(CommitmentRecord record, const Finalize& finalize) = 0;

    virtual Result<CommitmentRecord> GetById(const std::string& commitmentId) const = 0;
    virtual Result<CommitmentRecord> FindByClientNonce(const std::string& clientId,
                                                       const std::string& clientNonce) const = 0;
    virtual Result<std::vector<CommitmentRecord>> ListBySequence(int64_t fromSequence,
                                                                 std::size_t limit) const = 0;
    virtual ChainHead Head() const = 0;
    virtual Status MarkConsumed(const std::string& commitmentId,
                                const Consumption& consumption) = 0;
    virtual std::size_t Count() const = 0;
};

}  // namespace nonceserver
