// Ecdsa.h: 基于 Windows CNG 的 ECDSA P-256 + SHA-256 签名/校验。
// 规范要求签名为 DER 编码；CNG 原生输出 r||s（P1363），本模块负责转换。
#pragma once

#include "nonceserver/common/Result.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nonceserver {

// 持有 BCRYPT_KEY_HANDLE 的 RAII 包装（可移动、不可复制）。
class EcdsaP256Key {
public:
    EcdsaP256Key() = default;
    ~EcdsaP256Key();
    EcdsaP256Key(EcdsaP256Key&& other) noexcept;
    EcdsaP256Key& operator=(EcdsaP256Key&& other) noexcept;
    EcdsaP256Key(const EcdsaP256Key&) = delete;
    EcdsaP256Key& operator=(const EcdsaP256Key&) = delete;

    bool valid() const { return handle_ != nullptr; }

    static EcdsaP256Key Generate();

    // 导入 CNG ECC 私钥/公钥 BLOB（BCRYPT_ECCPRIVATE_BLOB / BCRYPT_ECCPUBLIC_BLOB）。
    static Result<EcdsaP256Key> ImportPrivateBlob(const std::vector<uint8_t>& blob);
    static Result<EcdsaP256Key> ImportPublicBlob(const std::vector<uint8_t>& blob);

    std::vector<uint8_t> ExportPrivateBlob() const;
    std::vector<uint8_t> ExportPublicBlob() const;

    // 服务端密钥标识：SHA-256(公钥 BLOB) 的十六进制。
    std::string KeyId() const;

    // 对任意消息计算 SHA-256 后签名。
    Result<std::vector<uint8_t>> SignDer(const uint8_t* data, std::size_t length) const;
    // 对已算好的 SHA-256 摘要签名。
    Result<std::vector<uint8_t>> SignHashDer(const uint8_t* hash, std::size_t hashLength) const;

private:
    void* handle_ = nullptr;  // BCRYPT_KEY_HANDLE

    friend Status VerifyEcdsaP256Hash(const std::vector<uint8_t>&, const uint8_t*, std::size_t,
                                      const std::vector<uint8_t>&);
    friend Status VerifyEcdsaP256(const std::vector<uint8_t>&, const uint8_t*, std::size_t,
                                  const std::vector<uint8_t>&);
};

// 使用公钥 BLOB 校验 DER 签名。data 版本内部做 SHA-256。
Status VerifyEcdsaP256(const std::vector<uint8_t>& publicBlob, const uint8_t* data,
                       std::size_t length, const std::vector<uint8_t>& derSignature);
Status VerifyEcdsaP256Hash(const std::vector<uint8_t>& publicBlob, const uint8_t* hash,
                           std::size_t hashLength, const std::vector<uint8_t>& derSignature);

// 从文件加载签名密钥（base64url 编码的私钥 BLOB）；不存在则生成并写入。
// createdOut 非空时，若本次新建则置为 true。
Result<EcdsaP256Key> LoadOrCreateSigningKey(const std::string& path, bool* createdOut = nullptr);

}  // namespace nonceserver
