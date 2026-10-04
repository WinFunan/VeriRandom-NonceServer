#include "nonceserver/tls/TlsFactory.h"

#include "nonceserver/common/Hex.h"
#include "nonceserver/log/Logger.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define SECURITY_WIN32 1

#include <winsock2.h>
#include <windows.h>
#include <wincrypt.h>
#include <schannel.h>
#include <security.h>
#include <sspi.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace nonceserver {

namespace {

bool Succeeded(SECURITY_STATUS status) { return status >= 0; }

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) return std::wstring();
    const int length =
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
    return wide;
}

std::string NormalizeHex(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (std::isxdigit(static_cast<unsigned char>(c))) {
            out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
    }
    return out;
}

std::string CertHashHex(PCCERT_CONTEXT cert, DWORD property) {
    DWORD size = 0;
    if (!CertGetCertificateContextProperty(cert, property, nullptr, &size) || size == 0) {
        return std::string();
    }
    std::vector<uint8_t> buffer(size);
    if (!CertGetCertificateContextProperty(cert, property, buffer.data(), &size)) {
        return std::string();
    }
    buffer.resize(size);
    return ToHex(buffer);
}

bool HasPrivateKey(PCCERT_CONTEXT cert) {
    DWORD size = 0;
    return CertGetCertificateContextProperty(cert, CERT_KEY_PROV_INFO_PROP_ID, nullptr, &size) &&
           size > 0;
}

DWORD StoreLocationFlag(const std::string& location) {
    return location == "CurrentUser" ? CERT_SYSTEM_STORE_CURRENT_USER
                                     : CERT_SYSTEM_STORE_LOCAL_MACHINE;
}

// 从 Windows 证书存储按指纹或主题查找带私钥的服务端证书（返回需 CertFreeCertificateContext）。
PCCERT_CONTEXT FindServerCertificate(const TlsConfig& config, std::string* outThumbprint) {
    const std::wstring storeName = Utf8ToWide(config.certificateStoreName);
    HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0,
                                     StoreLocationFlag(config.certificateStoreLocation) |
                                         CERT_STORE_READONLY_FLAG,
                                     storeName.c_str());
    if (!store) {
        LogError("cannot open certificate store " + config.certificateStoreLocation + "\\" +
                 config.certificateStoreName);
        return nullptr;
    }

    PCCERT_CONTEXT found = nullptr;

    if (!config.certificateThumbprint.empty()) {
        const std::string normalized = NormalizeHex(config.certificateThumbprint);
        const auto bytes = FromHex(normalized);
        if (bytes) {
            CRYPT_HASH_BLOB hash{};
            hash.cbData = static_cast<DWORD>(bytes->size());
            hash.pbData = const_cast<BYTE*>(bytes->data());
            found = CertFindCertificateInStore(store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0,
                                               CERT_FIND_HASH, &hash, nullptr);
        }
    } else if (!config.certificateSubject.empty()) {
        const std::wstring subject = Utf8ToWide(config.certificateSubject);
        PCCERT_CONTEXT candidate = nullptr;
        while ((candidate = CertFindCertificateInStore(store,
                                                       X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0,
                                                       CERT_FIND_SUBJECT_STR, subject.c_str(),
                                                       candidate)) != nullptr) {
            if (HasPrivateKey(candidate)) {
                found = CertDuplicateCertificateContext(candidate);
                break;
            }
        }
    }

    if (found && !HasPrivateKey(found)) {
        CertFreeCertificateContext(found);
        found = nullptr;
    }
    if (found && outThumbprint) {
        *outThumbprint = CertHashHex(found, CERT_SHA1_HASH_PROP_ID);
    }

    CertCloseStore(store, 0);
    return found;
}

struct TlsCredential {
    CredHandle handle{};
    bool valid = false;

    void Reset() {
        if (valid) {
            FreeCredentialsHandle(&handle);
            valid = false;
        }
    }
    ~TlsCredential() { Reset(); }
};

using TlsCredentialPtr = std::shared_ptr<TlsCredential>;

Status AcquireCredential(PCCERT_CONTEXT cert, TlsCredential& credential) {
    SCHANNEL_CRED schannelCred{};
    schannelCred.dwVersion = SCHANNEL_CRED_VERSION;
    schannelCred.grbitEnabledProtocols = 0;  // 使用系统默认协议
    schannelCred.dwFlags = SCH_USE_STRONG_CRYPTO;
    schannelCred.cCreds = 1;
    schannelCred.paCred = &cert;

    TimeStamp expiry{};
    const SECURITY_STATUS status = AcquireCredentialsHandleW(
        nullptr, const_cast<SEC_WCHAR*>(UNISP_NAME_W), SECPKG_CRED_INBOUND, nullptr,
        &schannelCred, nullptr, nullptr, &credential.handle, &expiry);
    if (!Succeeded(status)) {
        return Status::Error(ErrorCode::CryptoFailure,
                             "AcquireCredentialsHandle failed: " + std::to_string(status));
    }
    credential.valid = true;
    return Status::Ok();
}

bool SendAll(SOCKET socket, const uint8_t* data, std::size_t length) {
    std::size_t sent = 0;
    while (sent < length) {
        const int chunk = static_cast<int>(std::min<std::size_t>(length - sent, 0x7FFFFFFF));
        const int written = send(socket, reinterpret_cast<const char*>(data + sent), chunk, 0);
        if (written == SOCKET_ERROR) return false;
        sent += static_cast<std::size_t>(written);
    }
    return true;
}

bool RecvInto(SOCKET socket, std::vector<uint8_t>& target) {
    uint8_t buffer[16384];
    const int received = recv(socket, reinterpret_cast<char*>(buffer), sizeof(buffer), 0);
    if (received <= 0) return false;
    target.insert(target.end(), buffer, buffer + received);
    return true;
}

// 一个 TLS 服务端会话：Schannel 握手 + 流式加解密。
class TlsSession {
public:
    TlsSession(SOCKET socket, TlsCredentialPtr credential)
        : socket_(socket), credential_(std::move(credential)) {}

    ~TlsSession() { Shutdown(); }

    Status Handshake() {
        ULONG requestFlags = ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT |
                             ASC_REQ_CONFIDENTIALITY | ASC_REQ_EXTENDED_ERROR | ASC_REQ_STREAM |
                             ASC_REQ_ALLOCATE_MEMORY;
        bool contextInitialized = false;

        for (;;) {
            if (encrypted_.empty()) {
                if (!RecvInto(socket_, encrypted_)) {
                    return Status::Error(ErrorCode::Unavailable, "connection closed during handshake");
                }
            }

            SecBuffer inBuffers[2];
            inBuffers[0].BufferType = SECBUFFER_TOKEN;
            inBuffers[0].pvBuffer = encrypted_.empty() ? nullptr : encrypted_.data();
            inBuffers[0].cbBuffer = static_cast<ULONG>(encrypted_.size());
            inBuffers[1].BufferType = SECBUFFER_EMPTY;
            inBuffers[1].pvBuffer = nullptr;
            inBuffers[1].cbBuffer = 0;
            SecBufferDesc inDesc{SECBUFFER_VERSION, 2, inBuffers};

            SecBuffer outBuffer;
            outBuffer.BufferType = SECBUFFER_TOKEN;
            outBuffer.pvBuffer = nullptr;
            outBuffer.cbBuffer = 0;
            SecBufferDesc outDesc{SECBUFFER_VERSION, 1, &outBuffer};

            ULONG outFlags = 0;
            const SECURITY_STATUS status = AcceptSecurityContext(
                &credential_->handle, contextInitialized ? &context_ : nullptr, &inDesc,
                requestFlags, SECURITY_NATIVE_DREP, &context_, &outDesc, &outFlags, nullptr);
            contextInitialized = true;
            haveContext_ = true;

            if (outBuffer.pvBuffer) {
                if (outBuffer.cbBuffer > 0) {
                    SendAll(socket_, static_cast<const uint8_t*>(outBuffer.pvBuffer),
                            outBuffer.cbBuffer);
                }
                if (outFlags & ASC_RET_ALLOCATED_MEMORY) {
                    FreeContextBuffer(outBuffer.pvBuffer);
                }
            }

            if (status == SEC_E_INCOMPLETE_MESSAGE) {
                // 输入不足：保留已收到的密文，继续接收。
                if (!RecvInto(socket_, encrypted_)) {
                    return Status::Error(ErrorCode::Unavailable,
                                         "connection closed during handshake");
                }
                continue;
            }

            if (inBuffers[1].BufferType == SECBUFFER_EXTRA && inBuffers[1].cbBuffer > 0) {
                const std::size_t keep = inBuffers[1].cbBuffer;
                std::vector<uint8_t> extra(encrypted_.end() - keep, encrypted_.end());
                encrypted_.swap(extra);
            } else {
                encrypted_.clear();
            }

            if (status == SEC_I_CONTINUE_NEEDED) {
                continue;
            }
            if (status == SEC_I_COMPLETE_NEEDED || status == SEC_I_COMPLETE_AND_CONTINUE) {
                if (CompleteAuthToken(&context_, &outDesc) != SEC_E_OK) {
                    return Status::Error(ErrorCode::CryptoFailure, "CompleteAuthToken failed");
                }
                if (status == SEC_I_COMPLETE_AND_CONTINUE) continue;
            }
            if (status == SEC_E_OK) {
                if (!Succeeded(QueryContextAttributes(&context_, SECPKG_ATTR_STREAM_SIZES, &sizes_))) {
                    return Status::Error(ErrorCode::CryptoFailure, "QueryContextAttributes failed");
                }
                established_ = true;
                return Status::Ok();
            }
            return Status::Error(ErrorCode::CryptoFailure,
                                 "AcceptSecurityContext failed: " + std::to_string(status));
        }
    }

    Result<std::size_t> Read(uint8_t* buffer, std::size_t length) {
        if (decryptedOffset_ >= decrypted_.size()) {
            const Status status = DecryptNext();
            if (!status.ok()) {
                if (status.code() == ErrorCode::Unavailable) return std::size_t{0};
                return status;
            }
        }
        const std::size_t available = decrypted_.size() - decryptedOffset_;
        const std::size_t copied = std::min(available, length);
        std::memcpy(buffer, decrypted_.data() + decryptedOffset_, copied);
        decryptedOffset_ += copied;
        if (decryptedOffset_ >= decrypted_.size()) {
            decrypted_.clear();
            decryptedOffset_ = 0;
        }
        return copied;
    }

    Status Write(const uint8_t* data, std::size_t length) {
        std::size_t offset = 0;
        while (offset < length) {
            const std::size_t chunk = std::min<std::size_t>(length - offset, sizes_.cbMaximumMessage);
            const std::size_t total = sizes_.cbHeader + chunk + sizes_.cbTrailer;
            std::vector<uint8_t> buffer(total);
            std::memcpy(buffer.data() + sizes_.cbHeader, data + offset, chunk);

            SecBuffer buffers[4];
            buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
            buffers[0].pvBuffer = buffer.data();
            buffers[0].cbBuffer = sizes_.cbHeader;
            buffers[1].BufferType = SECBUFFER_DATA;
            buffers[1].pvBuffer = buffer.data() + sizes_.cbHeader;
            buffers[1].cbBuffer = static_cast<ULONG>(chunk);
            buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
            buffers[2].pvBuffer = buffer.data() + sizes_.cbHeader + chunk;
            buffers[2].cbBuffer = sizes_.cbTrailer;
            buffers[3].BufferType = SECBUFFER_EMPTY;
            buffers[3].pvBuffer = nullptr;
            buffers[3].cbBuffer = 0;
            SecBufferDesc desc{SECBUFFER_VERSION, 4, buffers};

            if (EncryptMessage(&context_, 0, &desc, 0) != SEC_E_OK) {
                return Status::Error(ErrorCode::CryptoFailure, "EncryptMessage failed");
            }
            const std::size_t encryptedLength =
                buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer;
            if (!SendAll(socket_, buffer.data(), encryptedLength)) {
                return Status::Error(ErrorCode::Unavailable, "send failed");
            }
            offset += chunk;
        }
        return Status::Ok();
    }

private:
    Status DecryptNext() {
        decrypted_.clear();
        decryptedOffset_ = 0;

        for (;;) {
            if (encrypted_.empty()) {
                if (!RecvInto(socket_, encrypted_)) {
                    return Status::Error(ErrorCode::Unavailable, "connection closed");
                }
            }

            SecBuffer buffers[4];
            buffers[0].BufferType = SECBUFFER_DATA;
            buffers[0].pvBuffer = encrypted_.data();
            buffers[0].cbBuffer = static_cast<ULONG>(encrypted_.size());
            for (int i = 1; i < 4; ++i) {
                buffers[i].BufferType = SECBUFFER_EMPTY;
                buffers[i].pvBuffer = nullptr;
                buffers[i].cbBuffer = 0;
            }
            SecBufferDesc desc{SECBUFFER_VERSION, 4, buffers};

            const SECURITY_STATUS status = DecryptMessage(&context_, &desc, 0, nullptr);
            if (status == SEC_E_INCOMPLETE_MESSAGE) {
                if (!RecvInto(socket_, encrypted_)) {
                    return Status::Error(ErrorCode::Unavailable, "connection closed");
                }
                continue;
            }
            if (status == SEC_I_CONTEXT_EXPIRED) {
                return Status::Error(ErrorCode::Unavailable, "tls connection closed");
            }
            if (status == SEC_I_RENEGOTIATE) {
                return Status::Error(ErrorCode::NotImplemented, "tls renegotiation not supported");
            }
            if (status != SEC_E_OK) {
                return Status::Error(ErrorCode::CryptoFailure,
                                     "DecryptMessage failed: " + std::to_string(status));
            }

            bool haveData = false;
            std::size_t extraBytes = 0;
            for (const auto& buffer : buffers) {
                if (buffer.BufferType == SECBUFFER_DATA && buffer.cbBuffer > 0) {
                    const auto* begin = static_cast<const uint8_t*>(buffer.pvBuffer);
                    decrypted_.assign(begin, begin + buffer.cbBuffer);
                    haveData = true;
                } else if (buffer.BufferType == SECBUFFER_EXTRA) {
                    extraBytes = buffer.cbBuffer;
                }
            }
            if (extraBytes > 0) {
                std::vector<uint8_t> extra(encrypted_.end() - extraBytes, encrypted_.end());
                encrypted_.swap(extra);
            } else {
                encrypted_.clear();
            }
            if (haveData) return Status::Ok();
        }
    }

    void Shutdown() {
        if (!haveContext_) return;
        DWORD shutdownType = SCHANNEL_SHUTDOWN;
        SecBuffer buffer;
        buffer.BufferType = SECBUFFER_TOKEN;
        buffer.pvBuffer = &shutdownType;
        buffer.cbBuffer = sizeof(shutdownType);
        SecBufferDesc desc{SECBUFFER_VERSION, 1, &buffer};
        if (ApplyControlToken(&context_, &desc) == SEC_E_OK && credential_) {
            SecBuffer out;
            out.BufferType = SECBUFFER_TOKEN;
            out.pvBuffer = nullptr;
            out.cbBuffer = 0;
            SecBufferDesc outDesc{SECBUFFER_VERSION, 1, &out};
            ULONG flags = 0;
            TimeStamp expiry{};
            if (Succeeded(AcceptSecurityContext(&credential_->handle, &context_, nullptr,
                                                ASC_REQ_ALLOCATE_MEMORY, SECURITY_NATIVE_DREP,
                                                &context_, &outDesc, &flags, &expiry)) &&
                out.pvBuffer) {
                if (out.cbBuffer > 0) {
                    SendAll(socket_, static_cast<const uint8_t*>(out.pvBuffer), out.cbBuffer);
                }
                if (flags & ASC_RET_ALLOCATED_MEMORY) FreeContextBuffer(out.pvBuffer);
            }
        }
        DeleteSecurityContext(&context_);
        haveContext_ = false;
    }

    SOCKET socket_;
    TlsCredentialPtr credential_;
    CtxtHandle context_{};
    bool haveContext_ = false;
    bool established_ = false;
    SecPkgContext_StreamSizes sizes_{};
    std::vector<uint8_t> encrypted_;
    std::vector<uint8_t> decrypted_;
    std::size_t decryptedOffset_ = 0;
};

class SchannelTransport : public ITransport {
public:
    SchannelTransport(SOCKET socket, std::unique_ptr<TlsSession> session)
        : socket_(socket), session_(std::move(session)) {}

    ~SchannelTransport() override {
        session_.reset();
        if (socket_ != INVALID_SOCKET) closesocket(socket_);
    }

    Result<std::size_t> Read(uint8_t* buffer, std::size_t length) override {
        return session_->Read(buffer, length);
    }

    Status Write(const uint8_t* data, std::size_t length) override {
        return session_->Write(data, length);
    }

private:
    SOCKET socket_;
    std::unique_ptr<TlsSession> session_;
};

class SchannelTlsContext : public ITlsContext {
public:
    explicit SchannelTlsContext(TlsConfig config) : config_(std::move(config)) {}

    Status Initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        return EnsureCredentialLocked();
    }

    Result<std::unique_ptr<ITransport>> WrapServerSocket(std::uintptr_t socket) override {
        TlsCredentialPtr credential;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            // 每次连接刷新，以拾取自动续期后的新证书。
            const Status status = EnsureCredentialLocked();
            if (!status.ok()) return status;
            credential = credential_;
        }

        auto session = std::make_unique<TlsSession>(static_cast<SOCKET>(socket), credential);
        const Status handshake = session->Handshake();
        if (!handshake.ok()) return handshake;
        return std::unique_ptr<ITransport>(
            std::make_unique<SchannelTransport>(static_cast<SOCKET>(socket), std::move(session)));
    }

private:
    Status EnsureCredentialLocked() {
        std::string thumbprint;
        PCCERT_CONTEXT cert = FindServerCertificate(config_, &thumbprint);
        if (!cert) {
            return Status::Error(
                ErrorCode::NotFound,
                "no matching server certificate with private key in " +
                    config_.certificateStoreLocation + "\\" + config_.certificateStoreName);
        }

        if (credential_ && !thumbprint.empty() && thumbprint == currentThumbprint_) {
            CertFreeCertificateContext(cert);
            return Status::Ok();
        }

        auto credential = std::make_shared<TlsCredential>();
        const Status status = AcquireCredential(cert, *credential);
        CertFreeCertificateContext(cert);
        if (!status.ok()) return status;

        credential_ = std::move(credential);
        currentThumbprint_ = thumbprint;
        LogInfo("TLS certificate loaded, SHA-1 thumbprint: " + thumbprint);
        return Status::Ok();
    }

    TlsConfig config_;
    std::mutex mutex_;
    TlsCredentialPtr credential_;
    std::string currentThumbprint_;
};

}  // namespace

std::shared_ptr<ITlsContext> CreateSchannelTlsContext(const TlsConfig& config) {
    return std::make_shared<SchannelTlsContext>(config);
}

}  // namespace nonceserver
