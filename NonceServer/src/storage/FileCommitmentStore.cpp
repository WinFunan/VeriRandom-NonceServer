#include "nonceserver/storage/FileCommitmentStore.h"

#include "nonceserver/canonical/CanonicalHash.h"
#include "nonceserver/log/Logger.h"

#include <io.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace nonceserver {

namespace {

constexpr const char* kGenesisPrevHash =
    "0000000000000000000000000000000000000000000000000000000000000000";

Status FlushAndSync(std::FILE* file) {
    if (std::fflush(file) != 0) {
        return Status::Error(ErrorCode::StorageFailure, "fflush failed");
    }
    if (_commit(_fileno(file)) != 0) {
        return Status::Error(ErrorCode::StorageFailure, "fsync (_commit) failed");
    }
    return Status::Ok();
}

}  // namespace

std::string StoreOptions::CommitmentsPath() const {
    return (std::filesystem::path(directory) / commitmentsFileName).string();
}

std::string StoreOptions::DisclosuresPath() const {
    return (std::filesystem::path(directory) / disclosuresFileName).string();
}

FileCommitmentStore::FileCommitmentStore(StoreOptions options) : options_(std::move(options)) {}

FileCommitmentStore::~FileCommitmentStore() {
    if (commitmentsFile_) std::fclose(commitmentsFile_);
    if (disclosuresFile_) std::fclose(disclosuresFile_);
}

Status FileCommitmentStore::Initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::error_code ec;
    std::filesystem::create_directories(options_.directory, ec);
    if (ec) {
        return Status::Error(ErrorCode::StorageFailure,
                             "cannot create storage directory: " + options_.directory);
    }

    Status loaded = LoadExisting();
    if (!loaded.ok()) return loaded;

    commitmentsFile_ = std::fopen(options_.CommitmentsPath().c_str(), "ab");
    if (!commitmentsFile_) {
        return Status::Error(ErrorCode::StorageFailure,
                             "cannot open commitments log: " + options_.CommitmentsPath());
    }
    disclosuresFile_ = std::fopen(options_.DisclosuresPath().c_str(), "ab");
    if (!disclosuresFile_) {
        return Status::Error(ErrorCode::StorageFailure,
                             "cannot open disclosures log: " + options_.DisclosuresPath());
    }

    LogInfo("commitment store initialized: " + options_.directory + " (" +
            std::to_string(records_.size()) + " records)");
    return Status::Ok();
}

Status FileCommitmentStore::LoadExisting() {
    records_.clear();
    byId_.clear();
    byClientNonce_.clear();
    chainHeadHash_.clear();

    const std::string commitmentsPath = options_.CommitmentsPath();
    std::error_code ec;
    if (std::filesystem::exists(commitmentsPath, ec)) {
        std::ifstream stream(commitmentsPath, std::ios::binary);
        if (!stream) {
            return Status::Error(ErrorCode::StorageFailure,
                                 "cannot read commitments log: " + commitmentsPath);
        }
        std::string line;
        while (std::getline(stream, line)) {
            if (line.empty()) continue;
            std::string error;
            const auto parsed = Json::parse(line, &error);
            if (!parsed || !parsed->isObject()) {
                return Status::Error(ErrorCode::StorageFailure,
                                     "corrupt commitment record: " + error);
            }
            CommitmentRecord record = CommitmentRecord::FromRecordJson(*parsed);
            const int64_t expected = static_cast<int64_t>(records_.size());
            if (record.sequence != expected) {
                return Status::Error(ErrorCode::StorageFailure,
                                     "commitment chain sequence gap at " + std::to_string(expected));
            }
            const std::string prevHash =
                records_.empty() ? std::string(kGenesisPrevHash) : records_.back().recordHash;
            if (record.prevHash != prevHash) {
                return Status::Error(ErrorCode::StorageFailure,
                                     "commitment chain broken at sequence " +
                                         std::to_string(record.sequence));
            }
            const std::string recomputed =
                ComputeRecordHash(record.sequence, record.prevHash, record.commitmentHash);
            if (recomputed != record.recordHash) {
                return Status::Error(ErrorCode::StorageFailure,
                                     "commitment record hash mismatch at sequence " +
                                         std::to_string(record.sequence));
            }
            byId_[record.commitmentId] = records_.size();
            if (!record.clientNonce.empty()) {
                byClientNonce_[IdempotencyKey(record.clientId, record.clientNonce)] = records_.size();
            }
            chainHeadHash_ = record.recordHash;
            records_.push_back(std::move(record));
        }
    }

    // 应用消费披露（单独文件，保证承诺日志只追加）。
    const std::string disclosuresPath = options_.DisclosuresPath();
    if (std::filesystem::exists(disclosuresPath, ec)) {
        std::ifstream stream(disclosuresPath, std::ios::binary);
        if (!stream) {
            return Status::Error(ErrorCode::StorageFailure,
                                 "cannot read disclosures log: " + disclosuresPath);
        }
        std::string line;
        while (std::getline(stream, line)) {
            if (line.empty()) continue;
            std::string error;
            auto parsed = Json::parse(line, &error);
            if (!parsed || !parsed->isObject()) {
                return Status::Error(ErrorCode::StorageFailure,
                                     "corrupt disclosure record: " + error);
            }
            const std::string commitmentId = (*parsed)["commitmentId"].asString();
            const auto it = byId_.find(commitmentId);
            if (it != byId_.end()) {
                records_[it->second].consumed = Consumption::FromJson(*parsed);
            }
        }
    }
    return Status::Ok();
}

std::string FileCommitmentStore::IdempotencyKey(const std::string& clientId,
                                                const std::string& clientNonce) {
    return clientId + '\x1F' + clientNonce;
}

Status FileCommitmentStore::AppendLine(std::FILE* file, const std::string& line) {
    const std::string payload = line + "\n";
    if (std::fwrite(payload.data(), 1, payload.size(), file) != payload.size()) {
        return Status::Error(ErrorCode::StorageFailure, "write failed");
    }
    return FlushAndSync(file);
}

Result<CommitmentRecord> FileCommitmentStore::Append(CommitmentRecord record,
                                                     const Finalize& finalize) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!record.clientNonce.empty()) {
        const auto it = byClientNonce_.find(IdempotencyKey(record.clientId, record.clientNonce));
        if (it != byClientNonce_.end()) {
            return records_[it->second];  // 幂等：返回同一承诺
        }
    }

    record.sequence = static_cast<int64_t>(records_.size());
    record.prevHash = records_.empty() ? std::string(kGenesisPrevHash) : records_.back().recordHash;

    const Status finalized = finalize(record);
    if (!finalized.ok()) return finalized;

    const Status written = AppendLine(commitmentsFile_, record.ToRecordJson().dump());
    if (!written.ok()) return written;

    byId_[record.commitmentId] = records_.size();
    if (!record.clientNonce.empty()) {
        byClientNonce_[IdempotencyKey(record.clientId, record.clientNonce)] = records_.size();
    }
    chainHeadHash_ = record.recordHash;
    records_.push_back(record);
    return record;
}

Result<CommitmentRecord> FileCommitmentStore::GetById(const std::string& commitmentId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = byId_.find(commitmentId);
    if (it == byId_.end()) {
        return Status::Error(ErrorCode::NotFound, "commitment not found: " + commitmentId);
    }
    return records_[it->second];
}

Result<CommitmentRecord> FileCommitmentStore::FindByClientNonce(
    const std::string& clientId, const std::string& clientNonce) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = byClientNonce_.find(IdempotencyKey(clientId, clientNonce));
    if (it == byClientNonce_.end()) {
        return Status::Error(ErrorCode::NotFound, "commitment not found for client nonce");
    }
    return records_[it->second];
}

Result<std::vector<CommitmentRecord>> FileCommitmentStore::ListBySequence(int64_t fromSequence,
                                                                          std::size_t limit) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CommitmentRecord> out;
    if (limit == 0 || records_.empty()) return out;
    if (fromSequence < 0) fromSequence = 0;
    const std::size_t start = static_cast<std::size_t>(fromSequence);
    for (std::size_t index = start; index < records_.size() && out.size() < limit; ++index) {
        out.push_back(records_[index]);
    }
    return out;
}

ChainHead FileCommitmentStore::Head() const {
    std::lock_guard<std::mutex> lock(mutex_);
    ChainHead head;
    if (!records_.empty()) {
        head.sequence = records_.back().sequence;
        head.recordHash = records_.back().recordHash;
    }
    return head;
}

Status FileCommitmentStore::MarkConsumed(const std::string& commitmentId,
                                         const Consumption& consumption) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = byId_.find(commitmentId);
    if (it == byId_.end()) {
        return Status::Error(ErrorCode::NotFound, "commitment not found: " + commitmentId);
    }
    CommitmentRecord& record = records_[it->second];
    if (record.consumed) {
        if (record.consumed->proofId == consumption.proofId) return Status::Ok();
        return Status::Error(ErrorCode::Conflict,
                             "commitment already consumed by a different proof");
    }

    Json disclosure(Json::Object{});
    disclosure["commitmentId"] = commitmentId;
    disclosure["proofId"] = consumption.proofId;
    disclosure["proofHash"] = consumption.proofHash;
    disclosure["reportedAtUtc"] = FormatRfc3339(consumption.reportedAtUtc);

    const Status written = AppendLine(disclosuresFile_, disclosure.dump());
    if (!written.ok()) return written;

    record.consumed = consumption;
    return Status::Ok();
}

std::size_t FileCommitmentStore::Count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return records_.size();
}

}  // namespace nonceserver
