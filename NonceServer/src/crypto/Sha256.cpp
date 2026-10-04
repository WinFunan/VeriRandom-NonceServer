#include "nonceserver/crypto/Sha256.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>

#include <stdexcept>

namespace nonceserver {

namespace {

bool Succeeded(NTSTATUS status) { return status >= 0; }

BCRYPT_ALG_HANDLE Sha256Algorithm() {
    static struct Provider {
        BCRYPT_ALG_HANDLE handle = nullptr;
        Provider() {
            if (!Succeeded(BCryptOpenAlgorithmProvider(&handle, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
                throw std::runtime_error("BCryptOpenAlgorithmProvider(SHA256) failed");
            }
        }
        ~Provider() {
            if (handle) BCryptCloseAlgorithmProvider(handle, 0);
        }
    } provider;
    return provider.handle;
}

}  // namespace

Sha256Hasher::Sha256Hasher() {
    BCRYPT_HASH_HANDLE handle = nullptr;
    if (!Succeeded(BCryptCreateHash(Sha256Algorithm(), &handle, nullptr, 0, nullptr, 0, 0))) {
        throw std::runtime_error("BCryptCreateHash failed");
    }
    handle_ = handle;
}

Sha256Hasher::~Sha256Hasher() {
    auto handle = static_cast<BCRYPT_HASH_HANDLE>(handle_);
    if (handle) BCryptDestroyHash(handle);
}

void Sha256Hasher::Update(const uint8_t* data, std::size_t length) {
    auto handle = static_cast<BCRYPT_HASH_HANDLE>(handle_);
    const uint8_t* cursor = data;
    std::size_t remaining = length;
    while (remaining > 0) {
        const ULONG chunk = static_cast<ULONG>(remaining > 0xFFFFFFFFull ? 0xFFFFFFFFull : remaining);
        if (!Succeeded(BCryptHashData(handle, const_cast<PUCHAR>(cursor), chunk, 0))) {
            throw std::runtime_error("BCryptHashData failed");
        }
        cursor += chunk;
        remaining -= chunk;
    }
}

void Sha256Hasher::Update(std::string_view data) {
    Update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

Sha256Digest Sha256Hasher::Final() {
    Sha256Digest digest{};
    auto handle = static_cast<BCRYPT_HASH_HANDLE>(handle_);
    if (!Succeeded(BCryptFinishHash(handle, digest.data(), static_cast<ULONG>(digest.size()), 0))) {
        throw std::runtime_error("BCryptFinishHash failed");
    }
    return digest;
}

Sha256Digest Sha256(const uint8_t* data, std::size_t length) {
    Sha256Hasher hasher;
    hasher.Update(data, length);
    return hasher.Final();
}

Sha256Digest Sha256(std::string_view data) {
    return Sha256(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

Sha256Digest Sha256(const std::vector<uint8_t>& data) {
    return Sha256(data.data(), data.size());
}

}  // namespace nonceserver
