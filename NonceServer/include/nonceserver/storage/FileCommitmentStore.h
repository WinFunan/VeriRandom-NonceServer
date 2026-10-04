// FileCommitmentStore.h: 基于追加日志（JSON Lines）的存储实现。
// 每次追加后 flush + fsync，确保“先落盘再响应”。消费披露单独追加到另一文件。
#pragma once

#include "nonceserver/storage/ICommitmentStore.h"

#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace nonceserver {

struct StoreOptions {
    std::string directory;
    std::string commitmentsFileName = "commitments.log";
    std::string disclosuresFileName = "consumed.log";

    std::string CommitmentsPath() const;
    std::string DisclosuresPath() const;
};

class FileCommitmentStore : public ICommitmentStore {
public:
    explicit FileCommitmentStore(StoreOptions options);
    ~FileCommitmentStore() override;

    FileCommitmentStore(const FileCommitmentStore&) = delete;
    FileCommitmentStore& operator=(const FileCommitmentStore&) = delete;

    Status Initialize() override;
    Result<CommitmentRecord> Append(CommitmentRecord record, const Finalize& finalize) override;

    Result<CommitmentRecord> GetById(const std::string& commitmentId) const override;
    Result<CommitmentRecord> FindByClientNonce(const std::string& clientId,
                                               const std::string& clientNonce) const override;
    Result<std::vector<CommitmentRecord>> ListBySequence(int64_t fromSequence,
                                                         std::size_t limit) const override;
    ChainHead Head() const override;
    Status MarkConsumed(const std::string& commitmentId, const Consumption& consumption) override;
    std::size_t Count() const override;

private:
    static std::string IdempotencyKey(const std::string& clientId, const std::string& clientNonce);
    Status AppendLine(std::FILE* file, const std::string& line);
    Status LoadExisting();

    StoreOptions options_;
    mutable std::mutex mutex_;
    std::vector<CommitmentRecord> records_;  // 下标即 sequence
    std::unordered_map<std::string, std::size_t> byId_;
    std::unordered_map<std::string, std::size_t> byClientNonce_;
    std::string chainHeadHash_;
    std::FILE* commitmentsFile_ = nullptr;
    std::FILE* disclosuresFile_ = nullptr;
};

}  // namespace nonceserver
