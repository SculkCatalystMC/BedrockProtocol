// Copyright © 2026 SculkCatalystMC. All rights reserved.
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#include "sculk/protocol/connection/HandShakeToken.hpp"
#include "../ssl/ES384.hpp"
#include "../ssl/RS256.hpp"
#include <cctype>
#include <format>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sculk/reflection/jsonc/reflection.hpp>

namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE {

#define SCULK_HANDSHAKE_TOKEN_SERIALIZE_OPTION_INIT()                                                                  \
    static reflection::jsonc::options options {                                                                        \
        .indent = -1, .allow_trailing_comma = false, .enum_cast_case_sensitive = true                                  \
    }

#define SCULK_HANDSHAKE_TOKEN_CREATE_JSON(PART, DATA)                                                                  \
    jsonc::json PART##Json = jsonc::json::object();                                                                    \
    const auto& PART       = DATA;

#define SCULK_HANDSHAKE_TOKEN_SERIALIZE(PART, FIELD)                                                                   \
    auto FIELD         = reflection::jsonc::serialize<false, false>(PART.FIELD, options);                              \
    PART##Json[#FIELD] = FIELD;


#define SCULK_HANDSHAKE_TOKEN_PARSE_JSON(PART, RAW)                                                                    \
    auto PART##JsonStr = base64url::decodeChecked(RAW);                                                                \
    if (!PART##JsonStr) {                                                                                              \
        return error_utils::makeError("Failed to decode login token " #PART);                                          \
    }                                                                                                                  \
    auto PART##JsonOpt = jsonc::json::parse(*PART##JsonStr);                                                           \
    if (!PART##JsonOpt) {                                                                                              \
        return error_utils::makeError("Failed to parse login token " #PART " JSON");                                   \
    }                                                                                                                  \
    const auto& PART##Json = *PART##JsonOpt;

#ifdef SCULK_PROTOCOL_ENABLE_DETAIL_ERRORS
#define SCULK_HANDSHAKE_TOKEN_DESERIALIZE(PART, FIELD)                                                                 \
    if (!PART##Json.contains(#FIELD)) {                                                                                \
        return error_utils::makeError("Login token " #PART " JSON does not contain a valid field '" #FIELD "'");       \
    }                                                                                                                  \
    if (auto status = reflection::jsonc::deserialize<false, false>(PART.FIELD, PART##Json[#FIELD], options);           \
        !status) {                                                                                                     \
        return error_utils::makeError(                                                                                 \
            std::format("Failed to deserialize login token {} field '{}': {}", #PART, #FIELD, status.error())          \
        );                                                                                                             \
    }
#else
#define SCULK_HANDSHAKE_TOKEN_DESERIALIZE(PART, FIELD)                                                                 \
    if (!PART##Json.contains(#FIELD)) {                                                                                \
        return error_utils::makeError("Login token " #PART " JSON does not contain a valid field '" #FIELD "'");       \
    }                                                                                                                  \
    if (!reflection::jsonc::deserialize<false, false>(PART.FIELD, PART##Json[#FIELD], options)) {                      \
        return error_utils::makeError("Failed to deserialize login token " #PART " field '" #FIELD "'");               \
    }
#endif

Result<> HandShakeToken::verify() const {
    std::string signingInput = std::format("{}.{}", mRawHeader, mRawPayload);

    if (mHeader.alg != "ES384") {
        return error_utils::makeError("Unsupported algorithm in handshake token header");
    }

    if (!es384::verifyES384Signature(signingInput, mSignature, mHeader.x5u)) {
        return error_utils::makeError("Failed to verify login token signature");
    }
    return {};
}

Result<> HandShakeToken::sign(const PemKeyPair& localKeyPair) {
    SCULK_HANDSHAKE_TOKEN_SERIALIZE_OPTION_INIT();

    mHeader.alg = "ES384";
    mHeader.x5u = pem_helper::stripPemMarkersAndCompact(localKeyPair.mPublicKeyPem);

    SCULK_HANDSHAKE_TOKEN_CREATE_JSON(header, mHeader);
    SCULK_HANDSHAKE_TOKEN_SERIALIZE(header, alg);
    SCULK_HANDSHAKE_TOKEN_SERIALIZE(header, x5u);
    mRawHeader = base64url::encode(headerJson.dump(-1));

    SCULK_HANDSHAKE_TOKEN_CREATE_JSON(payload, mPayload);
    SCULK_HANDSHAKE_TOKEN_SERIALIZE(payload, salt);
    mRawPayload = base64url::encode(payloadJson.dump(-1));

    auto signingInput = std::format("{}.{}", mRawHeader, mRawPayload);
    if (!es384::signES384Signature(signingInput, localKeyPair.mPrivateKeyPem, mSignature)) {
        return error_utils::makeError("Failed to sign login token with ES384");
    }
    return {};
}

Result<HandShakeToken> HandShakeToken::fromString(std::string_view rawLoginToken) {
    SCULK_HANDSHAKE_TOKEN_SERIALIZE_OPTION_INIT();

    const auto first = rawLoginToken.find('.');
    const auto last  = rawLoginToken.rfind('.');

    if (first == std::string::npos || last == std::string::npos || first == last) {
        return error_utils::makeError("Invalid login token format: expected three parts separated by dots");
    }

    auto   rawHeader = rawLoginToken.substr(0, first);
    Header header{};
    SCULK_HANDSHAKE_TOKEN_PARSE_JSON(header, rawHeader);
    SCULK_HANDSHAKE_TOKEN_DESERIALIZE(header, alg);
    SCULK_HANDSHAKE_TOKEN_DESERIALIZE(header, x5u);

    auto    rawPayload = rawLoginToken.substr(first + 1, last - first - 1);
    Payload payload{};
    SCULK_HANDSHAKE_TOKEN_PARSE_JSON(payload, rawPayload);
    SCULK_HANDSHAKE_TOKEN_DESERIALIZE(payload, salt);

    auto signature = rawLoginToken.substr(last + 1);

    return HandShakeToken{
        .mRawHeader  = std::string(rawHeader),
        .mHeader     = std::move(header),
        .mRawPayload = std::string(rawPayload),
        .mPayload    = std::move(payload),
        .mSignature  = std::string(signature),
    };
}

namespace {

std::string base64Encode(std::span<const std::byte> data) {
    if (data.empty()) {
        return {};
    }

    const auto  encodedSize = 4 * ((data.size() + 2) / 3);
    std::string encoded(encodedSize, '\0');

    const auto* input = reinterpret_cast<const unsigned char*>(data.data());
    const auto  written =
        EVP_EncodeBlock(reinterpret_cast<unsigned char*>(encoded.data()), input, static_cast<int>(data.size()));

    if (written < 0) {
        return {};
    }

    encoded.resize(static_cast<std::size_t>(written));
    return encoded;
}

std::vector<std::byte> base64Decode(std::string_view data) {
    if (data.empty()) {
        return {};
    }

    std::string compact;
    compact.reserve(data.size());
    for (char ch : data) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            compact.push_back(ch);
        }
    }

    if (compact.empty() || (compact.size() % 4) != 0) {
        return {};
    }

    std::vector<unsigned char> decodedBuffer((compact.size() / 4) * 3);
    const auto                 written = EVP_DecodeBlock(
        decodedBuffer.data(),
        reinterpret_cast<const unsigned char*>(compact.data()),
        static_cast<int>(compact.size())
    );

    if (written < 0) {
        return {};
    }

    std::size_t decodedSize = static_cast<std::size_t>(written);
    if (!compact.empty() && compact.back() == '=') {
        --decodedSize;
        if (compact.size() > 1 && compact[compact.size() - 2] == '=') {
            --decodedSize;
        }
    }

    std::vector<std::byte> decoded(decodedSize);
    for (std::size_t i = 0; i < decodedSize; ++i) {
        decoded[i] = static_cast<std::byte>(decodedBuffer[i]);
    }
    return decoded;
}

} // namespace

std::string HandShakeToken::toString() const { return std::format("{}.{}.{}", mRawHeader, mRawPayload, mSignature); }

void HandShakeToken::setSaltBytes(std::span<const std::byte> salt) { mPayload.salt = base64Encode(salt); }

std::vector<std::byte> HandShakeToken::getSaltBytes() const { return base64Decode(mPayload.salt); }

std::vector<std::byte> HandShakeToken::randomSalt() {
    constexpr std::size_t  saltSize = 8;
    std::vector<std::byte> salt(saltSize);

    auto* output = reinterpret_cast<unsigned char*>(salt.data());
    if (RAND_bytes(output, static_cast<int>(salt.size())) != 1) {
        return {};
    }

    return salt;
}

Result<HandShakeToken> HandShakeToken::random(const PemKeyPair& localKeyPair) {
    HandShakeToken token{};
    token.setSaltBytes(randomSalt());
    if (!token.sign(localKeyPair)) {
        return error_utils::makeError("Failed to sign handshake token");
    }
    return token;
}

} // namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE
