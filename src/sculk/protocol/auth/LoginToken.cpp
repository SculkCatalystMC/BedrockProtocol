// Copyright © 2026 SculkCatalystMC. All rights reserved.
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#include "sculk/protocol/auth/LoginToken.hpp"
#include "../ssl/ES384.hpp"
#include "../ssl/RS256.hpp"
#include <format>
#include <sculk/reflection/jsonc/reflection.hpp>

namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE {

#define SCULK_LOGIN_TOKEN_CHECK_HEADER(FIELD)                                                                          \
    if (!mHeader.FIELD.has_value()) {                                                                                  \
        return error_utils::makeError("Login token does not contain a valid '" #FIELD "' header field");               \
    }

#define SCULK_LOGIN_TOKEN_CHECK_PAYLOAD(FIELD)                                                                         \
    if (!mPayload.FIELD.has_value()) {                                                                                 \
        return error_utils::makeError("Login token does not contain a valid '" #FIELD "' payload field");              \
    }

#define SCULK_LOGIN_TOKEN_SERIALIZE_OPTION_INIT()                                                                      \
    constexpr reflection::jsonc::options options {                                                                     \
        .indent = -1, .allow_trailing_comma = false, .enum_cast_case_sensitive = true                                  \
    }

#define SCULK_LOGIN_TOKEN_CREATE_JSON(PART, DATA)                                                                      \
    jsonc::json PART##Json = jsonc::json::object();                                                                    \
    const auto& PART       = DATA;

#define SCULK_LOGIN_TOKEN_SERIALIZE(PART, FIELD)                                                                       \
    auto FIELD         = reflection::jsonc::serialize<false, false>(PART.FIELD, options);                              \
    PART##Json[#FIELD] = FIELD;


#define SCULK_LOGIN_TOKEN_SERIALIZE_OPTIONAL(PART, FIELD)                                                              \
    if (!PART.FIELD.has_value()) {                                                                                     \
        return error_utils::makeError("Login token " #PART " does not contain a valid '" #FIELD "' field");            \
    }                                                                                                                  \
    auto FIELD         = reflection::jsonc::serialize<false, false>(*PART.FIELD, options);                             \
    PART##Json[#FIELD] = FIELD;


#define SCULK_LOGIN_TOKEN_PARSE_JSON(PART, RAW)                                                                        \
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
#define SCULK_LOGIN_TOKEN_DESERIALIZE_REQUIRED(PART, FIELD)                                                            \
    if (!PART##Json.contains(#FIELD)) {                                                                                \
        return error_utils::makeError("Login token " #PART " JSON does not contain a valid field '" #FIELD "'");       \
    }                                                                                                                  \
    if (auto status = reflection::jsonc::deserialize<false, false>(PART.FIELD, PART##Json[#FIELD], options);           \
        !status) {                                                                                                     \
        return error_utils::makeError(                                                                                 \
            std::format("Failed to deserialize login token {} field '{}': {}", #PART, #FIELD, status.error())          \
        );                                                                                                             \
    }

#define SCULK_LOGIN_TOKEN_DESERIALIZE_OPTIONAL(PART, FIELD)                                                            \
    if (PART##Json.contains(#FIELD)) {                                                                                 \
        if (auto status = reflection::jsonc::deserialize<false, false>(PART.FIELD, PART##Json[#FIELD], options);       \
            !status) {                                                                                                 \
            return error_utils::makeError(                                                                             \
                std::format("Failed to deserialize login token {} field '{}': {}", #PART, #FIELD, status.error())      \
            );                                                                                                         \
        }                                                                                                              \
    }
#else
#define SCULK_LOGIN_TOKEN_DESERIALIZE_REQUIRED(PART, FIELD)                                                            \
    if (!PART##Json.contains(#FIELD)) {                                                                                \
        return error_utils::makeError("Login token " #PART " JSON does not contain a valid field '" #FIELD "'");       \
    }                                                                                                                  \
    if (!reflection::jsonc::deserialize<false, false>(PART.FIELD, PART##Json[#FIELD], options)) {                      \
        return error_utils::makeError("Failed to deserialize login token " #PART " field '" #FIELD "'");               \
    }

#define SCULK_LOGIN_TOKEN_DESERIALIZE_OPTIONAL(PART, FIELD)                                                            \
    if (PART##Json.contains(#FIELD)) {                                                                                 \
        if (!reflection::jsonc::deserialize<false, false>(PART.FIELD, PART##Json[#FIELD], options)) {                  \
            return error_utils::makeError("Failed to deserialize login token " #PART " field '" #FIELD "'");           \
        }                                                                                                              \
    }
#endif

Result<> LoginToken::verifyOnline(const AuthenticationKeyManager& authenticationKeyManager) const {
    if (isEmpty()) {
        return error_utils::makeError("Login token is empty");
    }

    std::string signingInput = std::format("{}.{}", mRawHeader, mRawPayload);

    auto timeNow = std::chrono::system_clock::now();
    auto leeway  = authenticationKeyManager.getLeeway();

    auto minExpCount = std::chrono::duration_cast<std::chrono::seconds>((timeNow - leeway).time_since_epoch()).count();
    if (mPayload.exp < minExpCount) {
        return error_utils::makeError("Login token has expired");
    }

    if (mPayload.aud != "api://auth-minecraft-services/multiplayer") {
        return error_utils::makeError("Login token audience is invalid");
    }

    if (mHeader.alg != "RS256") {
        return error_utils::makeError("Unsupported algorithm in login token header for full authentication");
    }

    SCULK_LOGIN_TOKEN_CHECK_HEADER(kid);
    if (!mHeader.typ || *mHeader.typ != "JWT") {
        return error_utils::makeError("Login token 'typ' header field is empty or invalid");
    }

    SCULK_LOGIN_TOKEN_CHECK_PAYLOAD(sub);
    SCULK_LOGIN_TOKEN_CHECK_PAYLOAD(ipt);

    SCULK_LOGIN_TOKEN_CHECK_PAYLOAD(iat);
    auto maxNowCount = std::chrono::duration_cast<std::chrono::seconds>((timeNow + leeway).time_since_epoch()).count();
    if (*mPayload.iat > maxNowCount) {
        return error_utils::makeError("Login token 'iat' claim is in the future");
    }

    SCULK_LOGIN_TOKEN_CHECK_PAYLOAD(tid);
    if (*mPayload.tid != authenticationKeyManager.getExpectedPlayFabTitle()) {
        return error_utils::makeError("Login token 'tid' claim does not match expected PlayFab title ID");
    }

    SCULK_LOGIN_TOKEN_CHECK_PAYLOAD(pfcd);

    SCULK_LOGIN_TOKEN_CHECK_PAYLOAD(iss);
    if (*mPayload.iss != authenticationKeyManager.getExpectedIssuer()) {
        return error_utils::makeError("Login token 'iss' claim is invalid");
    }

    auto keyId = authenticationKeyManager.getPublicKeyPemByKeyId(*mHeader.kid);
    if (!keyId) {
        return error_utils::makeError("No Mojang public key found for key ID in login token header");
    }

    if (!rs256::verifyRS256Signature(signingInput, mSignature, *keyId)) {
        return error_utils::makeError("Failed to verify login token signature");
    }

    return {};
}

Result<> LoginToken::verifySelfSigned(std::chrono::seconds leeway) const {
    if (isEmpty()) {
        return error_utils::makeError("Login token is empty");
    }

    std::string signingInput = std::format("{}.{}", mRawHeader, mRawPayload);

    auto timeNow = std::chrono::system_clock::now();

    auto minExpCount = std::chrono::duration_cast<std::chrono::seconds>((timeNow - leeway).time_since_epoch()).count();
    if (mPayload.exp < minExpCount) {
        return error_utils::makeError("Login token has expired");
    }

    if (mPayload.aud != "api://auth-minecraft-services/multiplayer") {
        return error_utils::makeError("Login token audience is invalid");
    }

    if (mHeader.alg != "ES384") {
        return error_utils::makeError("Unsupported algorithm in login token header for self-signed authentication");
    }

    SCULK_LOGIN_TOKEN_CHECK_HEADER(x5u);
    SCULK_LOGIN_TOKEN_CHECK_PAYLOAD(leguuid);
    SCULK_LOGIN_TOKEN_CHECK_PAYLOAD(nid);
    SCULK_LOGIN_TOKEN_CHECK_PAYLOAD(nname);
    SCULK_LOGIN_TOKEN_CHECK_PAYLOAD(pid);
    SCULK_LOGIN_TOKEN_CHECK_PAYLOAD(pname);

    if (*mHeader.x5u != mPayload.cpk) {
        return error_utils::makeError("Login token 'x5u' header field does not match 'cpk' payload field");
    }

    if (!es384::verifyES384Signature(signingInput, mSignature, *mHeader.x5u)) {
        return error_utils::makeError("Failed to verify login token signature");
    }

    return {};
}

Result<> LoginToken::selfSign(const PemKeyPair& clientKeyPair) {
    SCULK_LOGIN_TOKEN_SERIALIZE_OPTION_INIT();

    mHeader.alg  = "ES384";
    mHeader.x5u  = pem_helper::stripPemMarkersAndCompact(clientKeyPair.mPublicKeyPem);
    mPayload.cpk = *mHeader.x5u;

    auto expTime = std::chrono::system_clock::now() + std::chrono::years(1);
    mPayload.exp = std::chrono::duration_cast<std::chrono::seconds>((expTime).time_since_epoch()).count();

    SCULK_LOGIN_TOKEN_CREATE_JSON(header, mHeader);
    SCULK_LOGIN_TOKEN_SERIALIZE(header, alg);
    SCULK_LOGIN_TOKEN_SERIALIZE_OPTIONAL(header, x5u);
    mRawHeader = base64url::encode(headerJson.dump(-1));

    mPayload.cpk = *mHeader.x5u;
    SCULK_LOGIN_TOKEN_CREATE_JSON(payload, mPayload);
    SCULK_LOGIN_TOKEN_SERIALIZE(payload, mid);
    SCULK_LOGIN_TOKEN_SERIALIZE(payload, cpk);
    SCULK_LOGIN_TOKEN_SERIALIZE(payload, ap);
    SCULK_LOGIN_TOKEN_SERIALIZE(payload, xid);
    SCULK_LOGIN_TOKEN_SERIALIZE(payload, xname);
    SCULK_LOGIN_TOKEN_SERIALIZE(payload, exp);
    SCULK_LOGIN_TOKEN_SERIALIZE(payload, aud);
    SCULK_LOGIN_TOKEN_SERIALIZE_OPTIONAL(payload, leguuid);
    SCULK_LOGIN_TOKEN_SERIALIZE_OPTIONAL(payload, nid);
    SCULK_LOGIN_TOKEN_SERIALIZE_OPTIONAL(payload, nname);
    SCULK_LOGIN_TOKEN_SERIALIZE_OPTIONAL(payload, pid);
    SCULK_LOGIN_TOKEN_SERIALIZE_OPTIONAL(payload, pname);
    mRawPayload = base64url::encode(payloadJson.dump(-1));

    auto signingInput = std::format("{}.{}", mRawHeader, mRawPayload);
    if (!es384::signES384Signature(signingInput, clientKeyPair.mPrivateKeyPem, mSignature)) {
        return error_utils::makeError("Failed to sign login token with ES384");
    }
    return {};
}

std::string LoginToken::toString() const {
    return isEmpty() ? "" : std::format("{}.{}.{}", mRawHeader, mRawPayload, mSignature);
}

Result<LoginToken> LoginToken::fromString(std::string_view rawLoginToken) {
    if (rawLoginToken.empty()) {
        return LoginToken{};
    }

    SCULK_LOGIN_TOKEN_SERIALIZE_OPTION_INIT();

    const auto first = rawLoginToken.find('.');
    const auto last  = rawLoginToken.rfind('.');

    if (first == std::string::npos || last == std::string::npos || first == last) {
        return error_utils::makeError("Invalid login token format: expected three parts separated by dots");
    }

    auto   rawHeader = rawLoginToken.substr(0, first);
    Header header{};
    SCULK_LOGIN_TOKEN_PARSE_JSON(header, rawHeader);
    SCULK_LOGIN_TOKEN_DESERIALIZE_REQUIRED(header, alg);
    SCULK_LOGIN_TOKEN_DESERIALIZE_OPTIONAL(header, kid);
    SCULK_LOGIN_TOKEN_DESERIALIZE_OPTIONAL(header, x5u);
    SCULK_LOGIN_TOKEN_DESERIALIZE_OPTIONAL(header, typ);

    auto    rawPayload = rawLoginToken.substr(first + 1, last - first - 1);
    Payload payload{};
    SCULK_LOGIN_TOKEN_PARSE_JSON(payload, rawPayload);
    SCULK_LOGIN_TOKEN_DESERIALIZE_OPTIONAL(payload, sub);
    SCULK_LOGIN_TOKEN_DESERIALIZE_OPTIONAL(payload, ipt);
    SCULK_LOGIN_TOKEN_DESERIALIZE_OPTIONAL(payload, iat);
    SCULK_LOGIN_TOKEN_DESERIALIZE_REQUIRED(payload, mid);
    SCULK_LOGIN_TOKEN_DESERIALIZE_OPTIONAL(payload, tid);
    SCULK_LOGIN_TOKEN_DESERIALIZE_OPTIONAL(payload, pfcd);
    SCULK_LOGIN_TOKEN_DESERIALIZE_REQUIRED(payload, cpk);
    SCULK_LOGIN_TOKEN_DESERIALIZE_REQUIRED(payload, ap);
    SCULK_LOGIN_TOKEN_DESERIALIZE_REQUIRED(payload, xid);
    SCULK_LOGIN_TOKEN_DESERIALIZE_REQUIRED(payload, xname);
    SCULK_LOGIN_TOKEN_DESERIALIZE_REQUIRED(payload, exp);
    SCULK_LOGIN_TOKEN_DESERIALIZE_OPTIONAL(payload, iss);
    SCULK_LOGIN_TOKEN_DESERIALIZE_REQUIRED(payload, aud);
    SCULK_LOGIN_TOKEN_DESERIALIZE_OPTIONAL(payload, leguuid);
    SCULK_LOGIN_TOKEN_DESERIALIZE_OPTIONAL(payload, nid);
    SCULK_LOGIN_TOKEN_DESERIALIZE_OPTIONAL(payload, nname);
    SCULK_LOGIN_TOKEN_DESERIALIZE_OPTIONAL(payload, pid);
    SCULK_LOGIN_TOKEN_DESERIALIZE_OPTIONAL(payload, pname);

    auto signature = rawLoginToken.substr(last + 1);

    return LoginToken{
        .mRawHeader  = std::string(rawHeader),
        .mHeader     = std::move(header),
        .mRawPayload = std::string(rawPayload),
        .mPayload    = std::move(payload),
        .mSignature  = std::string(signature),
    };
}

} // namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE
