#include "nonceserver/crypto/Ecdsa.h"

#include "nonceserver/common/Base64.h"
#include "nonceserver/common/Hex.h"
#include "nonceserver/crypto/Sha256.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace nonceserver {

namespace {

bool Succeeded(NTSTATUS status) { return status >= 0; }

BCRYPT_ALG_HANDLE EcdsaAlgorithm() {
    static struct Provider {
        BCRYPT_ALG_HANDLE handle = nullptr;
        Provider() {
            if (!Succeeded(
                    BCryptOpenAlgorithmProvider(&handle, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0))) {
                throw std::runtime_error("BCryptOpenAlgorithmProvider(ECDSA_P256) failed");
            }
        }
        ~Provider() {
            if (handle) BCryptCloseAlgorithmProvider(handle, 0);
        }
    } provider;
    return provider.handle;
}

// CNG 输出 r||s（各 32 字节）-> DER SEQUENCE(INTEGER r, INTEGER s)
std::vector<uint8_t> RawSignatureToDer(const std::vector<uint8_t>& raw) {
    const std::size_t half = raw.size() / 2;
    auto encodeInteger = [](const uint8_t* data, std::size_t length) {
        std::size_t start = 0;
        while (start + 1 < length && data[start] == 0) ++start;
        std::vector<uint8_t> out;
        if (data[start] & 0x80) out.push_back(0x00);
        out.insert(out.end(), data + start, data + length);
        return out;
    };

    const std::vector<uint8_t> r = encodeInteger(raw.data(), half);
    const std::vector<uint8_t> s = encodeInteger(raw.data() + half, half);

    std::vector<uint8_t> body;
    body.push_back(0x02);
    body.push_back(static_cast<uint8_t>(r.size()));
    body.insert(body.end(), r.begin(), r.end());
    body.push_back(0x02);
    body.push_back(static_cast<uint8_t>(s.size()));
    body.insert(body.end(), s.begin(), s.end());

    std::vector<uint8_t> der;
    der.push_back(0x30);
    if (body.size() < 128) {
        der.push_back(static_cast<uint8_t>(body.size()));
    } else {
        der.push_back(0x81);
        der.push_back(static_cast<uint8_t>(body.size()));
    }
    der.insert(der.end(), body.begin(), body.end());
    return der;
}

// DER SEQUENCE(INTEGER r, INTEGER s) -> r||s（定长 half 字节，左补零）
std::optional<std::vector<uint8_t>> DerSignatureToRaw(const std::vector<uint8_t>& der,
                                                      std::size_t half) {
    if (der.size() < 2 || der[0] != 0x30) return std::nullopt;
    std::size_t index = 1;
    std::size_t length = der[index++];
    if (length & 0x80) {
        const std::size_t count = length & 0x7F;
        if (count == 0 || index + count > der.size()) return std::nullopt;
        length = 0;
        for (std::size_t i = 0; i < count; ++i) length = (length << 8) | der[index++];
    }
    if (index + length > der.size()) return std::nullopt;

    auto readInteger = [&](std::vector<uint8_t>& out) -> bool {
        if (index >= der.size() || der[index++] != 0x02) return false;
        if (index >= der.size()) return false;
        std::size_t len = der[index++];
        if (len & 0x80) {
            const std::size_t count = len & 0x7F;
            if (count == 0 || index + count > der.size()) return false;
            len = 0;
            for (std::size_t i = 0; i < count; ++i) len = (len << 8) | der[index++];
        }
        if (index + len > der.size()) return false;
        std::vector<uint8_t> value(der.begin() + static_cast<std::ptrdiff_t>(index),
                                   der.begin() + static_cast<std::ptrdiff_t>(index + len));
        index += len;
        while (!value.empty() && value.front() == 0) value.erase(value.begin());
        if (value.size() > half) return false;
        out.assign(half - value.size(), 0);
        out.insert(out.end(), value.begin(), value.end());
        return true;
    };

    std::vector<uint8_t> r, s;
    if (!readInteger(r) || !readInteger(s)) return std::nullopt;
    r.insert(r.end(), s.begin(), s.end());
    return r;
}

}  // namespace

EcdsaP256Key::~EcdsaP256Key() {
    auto handle = static_cast<BCRYPT_KEY_HANDLE>(handle_);
    if (handle) BCryptDestroyKey(handle);
}

EcdsaP256Key::EcdsaP256Key(EcdsaP256Key&& other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
}

EcdsaP256Key& EcdsaP256Key::operator=(EcdsaP256Key&& other) noexcept {
    if (this != &other) {
        auto handle = static_cast<BCRYPT_KEY_HANDLE>(handle_);
        if (handle) BCryptDestroyKey(handle);
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }
    return *this;
}

EcdsaP256Key EcdsaP256Key::Generate() {
    BCRYPT_KEY_HANDLE handle = nullptr;
    if (!Succeeded(BCryptGenerateKeyPair(EcdsaAlgorithm(), &handle, 256, 0)) ||
        !Succeeded(BCryptFinalizeKeyPair(handle, 0))) {
        if (handle) BCryptDestroyKey(handle);
        throw std::runtime_error("BCryptGenerateKeyPair(ECDSA_P256) failed");
    }
    EcdsaP256Key key;
    key.handle_ = handle;
    return key;
}

Result<EcdsaP256Key> EcdsaP256Key::ImportPrivateBlob(const std::vector<uint8_t>& blob) {
    BCRYPT_KEY_HANDLE handle = nullptr;
    if (!Succeeded(BCryptImportKeyPair(EcdsaAlgorithm(), nullptr, BCRYPT_ECCPRIVATE_BLOB, &handle,
                                       const_cast<PUCHAR>(blob.data()), static_cast<ULONG>(blob.size()),
                                       0))) {
        return Status::Error(ErrorCode::CryptoFailure, "import ECDSA private blob failed");
    }
    EcdsaP256Key key;
    key.handle_ = handle;
    return key;
}

Result<EcdsaP256Key> EcdsaP256Key::ImportPublicBlob(const std::vector<uint8_t>& blob) {
    BCRYPT_KEY_HANDLE handle = nullptr;
    if (!Succeeded(BCryptImportKeyPair(EcdsaAlgorithm(), nullptr, BCRYPT_ECCPUBLIC_BLOB, &handle,
                                       const_cast<PUCHAR>(blob.data()), static_cast<ULONG>(blob.size()),
                                       0))) {
        return Status::Error(ErrorCode::CryptoFailure, "import ECDSA public blob failed");
    }
    EcdsaP256Key key;
    key.handle_ = handle;
    return key;
}

std::vector<uint8_t> EcdsaP256Key::ExportPrivateBlob() const {
    auto handle = static_cast<BCRYPT_KEY_HANDLE>(handle_);
    ULONG size = 0;
    BCryptExportKey(handle, nullptr, BCRYPT_ECCPRIVATE_BLOB, nullptr, 0, &size, 0);
    std::vector<uint8_t> blob(size);
    if (size > 0) {
        BCryptExportKey(handle, nullptr, BCRYPT_ECCPRIVATE_BLOB, blob.data(), size, &size, 0);
        blob.resize(size);
    }
    return blob;
}

std::vector<uint8_t> EcdsaP256Key::ExportPublicBlob() const {
    auto handle = static_cast<BCRYPT_KEY_HANDLE>(handle_);
    ULONG size = 0;
    BCryptExportKey(handle, nullptr, BCRYPT_ECCPUBLIC_BLOB, nullptr, 0, &size, 0);
    std::vector<uint8_t> blob(size);
    if (size > 0) {
        BCryptExportKey(handle, nullptr, BCRYPT_ECCPUBLIC_BLOB, blob.data(), size, &size, 0);
        blob.resize(size);
    }
    return blob;
}

std::string EcdsaP256Key::KeyId() const {
    const auto publicBlob = ExportPublicBlob();
    const auto digest = Sha256(publicBlob);
    return ToHex(digest.data(), digest.size());
}

Result<std::vector<uint8_t>> EcdsaP256Key::SignHashDer(const uint8_t* hash,
                                                       std::size_t hashLength) const {
    auto handle = static_cast<BCRYPT_KEY_HANDLE>(handle_);
    if (!handle) return Status::Error(ErrorCode::CryptoFailure, "signing key is not initialized");

    ULONG size = 0;
    if (!Succeeded(BCryptSignHash(handle, nullptr, const_cast<PUCHAR>(hash),
                                  static_cast<ULONG>(hashLength), nullptr, 0, &size, 0))) {
        return Status::Error(ErrorCode::CryptoFailure, "BCryptSignHash size query failed");
    }
    std::vector<uint8_t> raw(size);
    if (!Succeeded(BCryptSignHash(handle, nullptr, const_cast<PUCHAR>(hash),
                                  static_cast<ULONG>(hashLength), raw.data(), size, &size, 0))) {
        return Status::Error(ErrorCode::CryptoFailure, "BCryptSignHash failed");
    }
    raw.resize(size);
    return RawSignatureToDer(raw);
}

Result<std::vector<uint8_t>> EcdsaP256Key::SignDer(const uint8_t* data,
                                                   std::size_t length) const {
    const auto digest = Sha256(data, length);
    return SignHashDer(digest.data(), digest.size());
}

Status VerifyEcdsaP256Hash(const std::vector<uint8_t>& publicBlob, const uint8_t* hash,
                           std::size_t hashLength, const std::vector<uint8_t>& derSignature) {
    const auto raw = DerSignatureToRaw(derSignature, 32);
    if (!raw) return Status::Error(ErrorCode::InvalidArgument, "malformed DER signature");

    BCRYPT_KEY_HANDLE key = nullptr;
    if (!Succeeded(BCryptImportKeyPair(EcdsaAlgorithm(), nullptr, BCRYPT_ECCPUBLIC_BLOB, &key,
                                       const_cast<PUCHAR>(publicBlob.data()),
                                       static_cast<ULONG>(publicBlob.size()), 0))) {
        return Status::Error(ErrorCode::CryptoFailure, "import public key failed");
    }
    const NTSTATUS status =
        BCryptVerifySignature(key, nullptr, const_cast<PUCHAR>(hash),
                              static_cast<ULONG>(hashLength), const_cast<PUCHAR>(raw->data()),
                              static_cast<ULONG>(raw->size()), 0);
    BCryptDestroyKey(key);
    if (!Succeeded(status)) {
        return Status::Error(ErrorCode::CryptoFailure, "signature verification failed");
    }
    return Status::Ok();
}

Status VerifyEcdsaP256(const std::vector<uint8_t>& publicBlob, const uint8_t* data,
                       std::size_t length, const std::vector<uint8_t>& derSignature) {
    const auto digest = Sha256(data, length);
    return VerifyEcdsaP256Hash(publicBlob, digest.data(), digest.size(), derSignature);
}

Result<EcdsaP256Key> LoadOrCreateSigningKey(const std::string& path, bool* createdOut) {
    if (createdOut) *createdOut = false;
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) {
            return Status::Error(ErrorCode::StorageFailure, "cannot open signing key file: " + path);
        }
        std::ostringstream buffer;
        buffer << stream.rdbuf();
        const auto decoded = Base64UrlDecode(buffer.str());
        if (!decoded) {
            return Status::Error(ErrorCode::InvalidArgument, "signing key file is not valid base64url");
        }
        return EcdsaP256Key::ImportPrivateBlob(*decoded);
    }

    EcdsaP256Key key = EcdsaP256Key::Generate();
    const auto blob = key.ExportPrivateBlob();
    const std::string encoded = Base64UrlEncode(blob);

    const std::filesystem::path filePath(path);
    if (filePath.has_parent_path()) {
        std::filesystem::create_directories(filePath.parent_path(), ec);
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        return Status::Error(ErrorCode::StorageFailure, "cannot create signing key file: " + path);
    }
    stream << encoded;
    stream.flush();
    if (!stream) {
        return Status::Error(ErrorCode::StorageFailure, "failed writing signing key file: " + path);
    }
    if (createdOut) *createdOut = true;
    return key;
}

}  // namespace nonceserver
