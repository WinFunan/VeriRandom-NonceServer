#include "nonceserver/canonical/CanonicalHash.h"

#include "nonceserver/common/Hex.h"
#include "nonceserver/crypto/Sha256.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace nonceserver {

namespace {

// 以 '\n' 连接各行，末尾不加换行。
std::string JoinLines(const std::vector<std::string>& lines) {
    std::string out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i != 0) out.push_back('\n');
        out += lines[i];
    }
    return out;
}

std::string Sha256Hex(std::string_view text) {
    const auto digest = Sha256(text);
    return ToHex(digest.data(), digest.size());
}

}  // namespace

std::string ComputeInputDigest(const CommitmentInput& input) {
    // 字段按“字段名”升序、以 name=value 形式排列（规范 §5）。
    std::vector<std::pair<std::string, std::string>> fields = {
        {"algorithmProfile", input.algorithmProfile},
        {"count", std::to_string(input.count)},
        {"drawKind", input.drawKind},
        {"rosterDigest", input.rosterDigest},
        {"rosterSize", std::to_string(input.rosterSize)},
        {"samplingMode", input.samplingMode},
    };
    std::sort(fields.begin(), fields.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    std::vector<std::string> lines;
    lines.reserve(fields.size() + 1);
    lines.emplace_back("VeriRandomCommitmentInput/v1");
    for (const auto& [name, value] : fields) {
        lines.push_back(name + "=" + value);
    }
    return Sha256Hex(JoinLines(lines));
}

std::string ComputeCommitmentHash(const CommitmentRecord& record) {
    const std::vector<std::string> lines = {
        "VeriRandomCommitment/v1",
        record.commitmentId,
        std::to_string(record.sequence),
        FormatRfc3339(record.issuedAtUtc),
        record.inputDigest,
        record.nonce,
        record.assignedPulse.source,
        record.assignedPulse.SourceIdentity(),
        record.assignedPulse.RoundIdentity(),
        FormatRfc3339(record.assignedPulse.publishedAt),
    };
    return Sha256Hex(JoinLines(lines));
}

std::string ComputeRecordHash(int64_t sequence, const std::string& prevHash,
                              const std::string& commitmentHash) {
    const std::vector<std::string> lines = {
        "VeriRandomCommitmentChain/v1",
        std::to_string(sequence),
        prevHash,
        commitmentHash,
    };
    return Sha256Hex(JoinLines(lines));
}

std::string ComputeResponseSigningProjection(const std::string& commitmentHash,
                                             const std::string& chainHeadRecordHash,
                                             const std::string& serverKeyId) {
    const std::vector<std::string> lines = {
        "VeriRandomResponse/v1",
        commitmentHash,
        chainHeadRecordHash,
        serverKeyId,
    };
    return JoinLines(lines);
}

}  // namespace nonceserver
