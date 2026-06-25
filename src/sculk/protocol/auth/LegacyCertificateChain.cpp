// Copyright © 2026 SculkCatalystMC. All rights reserved.
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#include "sculk/protocol/auth/LegacyCertificateChain.hpp"
#include "../ssl/ES384.hpp"
#include "sculk/reflection/jsonc/reflection.hpp"
#include <format>

namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE {

constexpr std::string_view LEGACY_MOJANG_PUBLIC_KEY_PEM =
    "MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAECRXueJeTDqNRRgJi/vlRufByu/2G0i2Ebt6YMar5QX/R0DIIyrJMcUpruK4QveTfJSTp3Shlq4Gk34cD/"
    "4GUWwkv0DVuzeuB+tXija7HBxii03NHDbPAD0AKnLr2wdAp";

#define SCULK_CERTIFICATE_SERIALIZE_OPTION_INIT()                                                                      \
    constexpr reflection::jsonc::options options {                                                                     \
        .indent = -1, .allow_trailing_comma = false, .enum_cast_case_sensitive = true                                  \
    }

#define SCULK_CERTIFICATE_CREATE_JSON(PART, DATA)                                                                      \
    jsonc::json PART##Json = jsonc::json::object();                                                                    \
    const auto& PART       = DATA;

#define SCULK_CERTIFICATE_SERIALIZE(PART, FIELD)                                                                       \
    auto FIELD = reflection::jsonc::serialize<false, false>(PART.FIELD, options);                                      \
    if (!FIELD.is_null()) {                                                                                            \
        PART##Json[#FIELD] = FIELD;                                                                                    \
    }

#define SCULK_CERTIFICATE_PARSE_JSON(PART, RAW)                                                                        \
    auto PART##JsonStr = base64url::decodeChecked(RAW);                                                                \
    if (!PART##JsonStr) {                                                                                              \
        return error_utils::makeError("Failed to decode certificate " #PART);                                          \
    }                                                                                                                  \
    auto PART##JsonOpt = jsonc::json::parse(*PART##JsonStr);                                                           \
    if (!PART##JsonOpt) {                                                                                              \
        return error_utils::makeError("Failed to parse certificate " #PART " JSON");                                   \
    }                                                                                                                  \
    const auto& PART##Json = *PART##JsonOpt;

#ifdef SCULK_PROTOCOL_ENABLE_DETAIL_ERRORS
#define SCULK_CERTIFICATE_DESERIALIZE_REQUIRED(PART, FIELD)                                                            \
    if (!PART##Json.contains(#FIELD)) {                                                                                \
        return error_utils::makeError("Certificate JSON does not contain a valid field '" #FIELD "'");                 \
    }                                                                                                                  \
    if (auto status = reflection::jsonc::deserialize<false, false>(PART.FIELD, PART##Json[#FIELD], options);           \
        !status) {                                                                                                     \
        return error_utils::makeError(                                                                                 \
            std::format("Failed to deserialize certificate {} field '{}': {}", #PART, #FIELD, status.error())          \
        );                                                                                                             \
    }

#define SCULK_CERTIFICATE_DESERIALIZE_OPTIONAL(PART, FIELD)                                                            \
    if (PART##Json.contains(#FIELD)) {                                                                                 \
        if (auto status = reflection::jsonc::deserialize<false, false>(PART.FIELD, PART##Json[#FIELD], options);       \
            !status) {                                                                                                 \
            return error_utils::makeError(                                                                             \
                std::format("Failed to deserialize certificate {} field '{}': {}", #PART, #FIELD, status.error())      \
            );                                                                                                         \
        }                                                                                                              \
    }
#else
#define SCULK_CERTIFICATE_DESERIALIZE_REQUIRED(PART, FIELD)                                                            \
    if (!PART##Json.contains(#FIELD)) {                                                                                \
        return error_utils::makeError("Certificate JSON does not contain a valid field '" #FIELD "'");                 \
    }                                                                                                                  \
    if (!reflection::jsonc::deserialize<false, false>(PART.FIELD, PART##Json[#FIELD], options)) {                      \
        return error_utils::makeError("Failed to deserialize certificate " #PART " field '" #FIELD "'");               \
    }

#define SCULK_CERTIFICATE_DESERIALIZE_OPTIONAL(PART, FIELD)                                                            \
    if (PART##Json.contains(#FIELD)) {                                                                                 \
        if (!reflection::jsonc::deserialize<false, false>(PART.FIELD, PART##Json[#FIELD], options)) {                  \
            return error_utils::makeError("Failed to deserialize certificate " #PART " field '" #FIELD "'");           \
        }                                                                                                              \
    }
#endif

bool Certificate::checkTimeValidity(std::chrono::seconds leeway, std::chrono::system_clock::time_point now) const {
    auto nowSec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    if (mPayload.nbf > nowSec + leeway.count()) {
        return false;
    }
    if (nowSec - leeway.count() > mPayload.exp) {
        return false;
    }
    if (mPayload.iat && *mPayload.iat > nowSec + leeway.count()) {
        return false;
    }
    return true;
}

bool Certificate::checkIssuer(std::string_view expectedIssuer) const {
    return mPayload.iss && *mPayload.iss == expectedIssuer;
}

std::string Certificate::toString() const { return std::format("{}.{}.{}", mRawHeader, mRawPayload, mSignature); }

bool Certificate::verify(std::string_view publicKeyPem) const {
    std::string signingInput = std::format("{}.{}", mRawHeader, mRawPayload);
    return es384::verifyES384Signature(signingInput, mSignature, publicKeyPem);
}

bool Certificate::sign(std::string_view privateKeyPem, std::chrono::system_clock::time_point now) {
    mHeader.alg = "ES384";
    mPayload.exp =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch() + std::chrono::hours(24)).count();
    mPayload.nbf = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    if (mPayload.iat.has_value()) {
        mPayload.iat = mPayload.nbf;
    }

    SCULK_CERTIFICATE_SERIALIZE_OPTION_INIT();

    SCULK_CERTIFICATE_CREATE_JSON(header, mHeader);
    SCULK_CERTIFICATE_SERIALIZE(header, alg);
    SCULK_CERTIFICATE_SERIALIZE(header, x5u);
    SCULK_CERTIFICATE_SERIALIZE(header, x5t);
    mRawHeader = base64url::encode(headerJson.dump(-1));

    SCULK_CERTIFICATE_CREATE_JSON(payload, mPayload);
    SCULK_CERTIFICATE_SERIALIZE(payload, nbf);
    SCULK_CERTIFICATE_SERIALIZE(payload, exp);
    SCULK_CERTIFICATE_SERIALIZE(payload, identityPublicKey);
    SCULK_CERTIFICATE_SERIALIZE(payload, certificateAuthority);
    SCULK_CERTIFICATE_SERIALIZE(payload, randomNonce);
    SCULK_CERTIFICATE_SERIALIZE(payload, iss);
    SCULK_CERTIFICATE_SERIALIZE(payload, iat);
    SCULK_CERTIFICATE_SERIALIZE(payload, extraData);
    mRawPayload = base64url::encode(payloadJson.dump(-1));

    std::string signingInput = std::format("{}.{}", mRawHeader, mRawPayload);
    return es384::signES384Signature(signingInput, privateKeyPem, mSignature);
}

Result<Certificate> Certificate::fromString(std::string_view certificateStr) {
    SCULK_CERTIFICATE_SERIALIZE_OPTION_INIT();

    const auto first = certificateStr.find('.');
    const auto last  = certificateStr.rfind('.');

    if (first == std::string::npos || last == std::string::npos || first == last) {
        return error_utils::makeError("Invalid certificate format: expected three parts separated by dots");
    }

    auto   rawHeader = certificateStr.substr(0, first);
    Header header{};
    SCULK_CERTIFICATE_PARSE_JSON(header, rawHeader);
    SCULK_CERTIFICATE_DESERIALIZE_REQUIRED(header, alg);
    if (header.alg != "ES384") {
        return error_utils::makeError("certificate signing algorithm must be ES384");
    }
    SCULK_CERTIFICATE_DESERIALIZE_REQUIRED(header, x5u);
    SCULK_CERTIFICATE_DESERIALIZE_OPTIONAL(header, x5t);

    auto    rawPayload = certificateStr.substr(first + 1, last - first - 1);
    Payload payload{};
    SCULK_CERTIFICATE_PARSE_JSON(payload, rawPayload);
    SCULK_CERTIFICATE_DESERIALIZE_REQUIRED(payload, nbf);
    SCULK_CERTIFICATE_DESERIALIZE_REQUIRED(payload, exp);
    SCULK_CERTIFICATE_DESERIALIZE_REQUIRED(payload, identityPublicKey);
    SCULK_CERTIFICATE_DESERIALIZE_OPTIONAL(payload, certificateAuthority);
    SCULK_CERTIFICATE_DESERIALIZE_OPTIONAL(payload, randomNonce);
    SCULK_CERTIFICATE_DESERIALIZE_OPTIONAL(payload, iss);
    SCULK_CERTIFICATE_DESERIALIZE_OPTIONAL(payload, iat);
    SCULK_CERTIFICATE_DESERIALIZE_OPTIONAL(payload, extraData);

    auto signature = certificateStr.substr(last + 1);

    return Certificate{
        .mRawHeader  = std::string(rawHeader),
        .mHeader     = std::move(header),
        .mRawPayload = std::string(rawPayload),
        .mPayload    = std::move(payload),
        .mSignature  = std::string(signature)
    };
}


std::string LegacyCertificateChain::toString() const {
    jsonc::json certChainJson = {
        {"chain", jsonc::json::array({})}
    };
    if (mClientCertificate) {
        certChainJson["chain"].push_back(mClientCertificate->toString());
    }
    if (mMojangCertificate) {
        certChainJson["chain"].push_back(mMojangCertificate->toString());
    }
    certChainJson["chain"].push_back(mLoginCertificate.toString());
    auto certChainJsonStr = certChainJson.dump(-1);
    certChainJsonStr.push_back('\n');
    return certChainJsonStr;
}

Result<> LegacyCertificateChain::verifyOnline(std::chrono::seconds leeway) const {
    if (!mClientCertificate.has_value() || !mMojangCertificate.has_value()) {
        return error_utils::makeError("Legacy certificate chain must contain both client and Mojang certificates");
    }

    auto now = std::chrono::system_clock::now();

    if (mClientCertificate->mHeader.x5u != mLoginCertificate.mPayload.identityPublicKey) {
        return error_utils::makeError("Client certificate does not match login certificate");
    }
    if (!mClientCertificate->checkTimeValidity(leeway, now)) {
        return error_utils::makeError("Client certificate time validity check failed");
    }
    if (!mClientCertificate->verify(mClientCertificate->mHeader.x5u)) {
        return error_utils::makeError("Client certificate signature verification failed");
    }

    if (mMojangCertificate->mHeader.x5u != mClientCertificate->mPayload.identityPublicKey) {
        return error_utils::makeError("Mojang certificate does not match client certificate");
    }
    if (!mMojangCertificate->checkTimeValidity(leeway, now)) {
        return error_utils::makeError("Mojang certificate time validity check failed");
    }
    if (!mMojangCertificate->checkIssuer("Mojang")) {
        return error_utils::makeError("Mojang certificate issuer check failed");
    }
    if (mMojangCertificate->mHeader.x5u == LEGACY_MOJANG_PUBLIC_KEY_PEM) {
        return error_utils::makeError("Mojang public key mismatch`");
    }
    if (!mMojangCertificate->verify(LEGACY_MOJANG_PUBLIC_KEY_PEM)) {
        return error_utils::makeError("Mojang certificate signature verification failed");
    }

    if (mLoginCertificate.mHeader.x5u != mMojangCertificate->mPayload.identityPublicKey) {
        return error_utils::makeError("Login certificate does not match mojang certificate");
    }
    if (!mLoginCertificate.checkTimeValidity(leeway, now)) {
        return error_utils::makeError("Login certificate time validity check failed");
    }
    if (!mLoginCertificate.checkIssuer("Mojang")) {
        return error_utils::makeError("Login certificate issuer check failed");
    }
    if (!mLoginCertificate.verify(mLoginCertificate.mHeader.x5u)) {
        return error_utils::makeError("Login certificate signature verification failed");
    }

    return {};
}

Result<> LegacyCertificateChain::verifySelfSigned(std::chrono::seconds leeway) const {
    if (mClientCertificate.has_value() || mMojangCertificate.has_value()) {
        return error_utils::makeError(
            "Self-signed legacy certificate chain should only contains a login certificate, client and Mojang "
            "certificates should not be present"
        );
    }

    if (mLoginCertificate.mHeader.x5u != mLoginCertificate.mPayload.identityPublicKey) {
        return error_utils::makeError("Login certificate does not match itself");
    }
    if (!mLoginCertificate.checkTimeValidity(leeway, std::chrono::system_clock::now())) {
        return error_utils::makeError("Login certificate time validity check failed");
    }
    if (!mLoginCertificate.verify(mLoginCertificate.mHeader.x5u)) {
        return error_utils::makeError("Login certificate signature verification failed");
    }
    return {};
}

Result<> LegacyCertificateChain::selfSign(const PemKeyPair& clientKeyPair) {
    mClientCertificate.reset();
    mMojangCertificate.reset();

    mLoginCertificate.mHeader.x5u                = pem_helper::stripPemMarkersAndCompact(clientKeyPair.mPublicKeyPem);
    mLoginCertificate.mPayload.identityPublicKey = pem_helper::stripPemMarkersAndCompact(clientKeyPair.mPublicKeyPem);
    if (!mLoginCertificate.sign(clientKeyPair.mPrivateKeyPem, std::chrono::system_clock::now())) {
        return error_utils::makeError("Failed to sign login certificate");
    }

    return {};
}

#ifdef SCULK_PROTOCOL_ENABLE_DETAIL_ERRORS
#define SCULK_CERTIFICATE_PARSE(PART, INDEX)                                                                           \
    if (!certJson["chain"][INDEX].is_string()) {                                                                       \
        return error_utils::makeError("Certificate JSON 'chain' field must be an array of strings");                   \
    }                                                                                                                  \
    auto PART##CertStr = certJson["chain"][INDEX].get<std::string>();                                                  \
    auto PART##CertOpt = Certificate::fromString(PART##CertStr);                                                       \
    if (!PART##CertOpt) {                                                                                              \
        return error_utils::makeError(                                                                                 \
            std::format("Failed to parse {} certificate: {}", #PART, PART##CertOpt.error().mMessage)                   \
        );                                                                                                             \
    }                                                                                                                  \
    auto PART##Cert = *PART##CertOpt;
#else
#define SCULK_CERTIFICATE_PARSE(PART, INDEX)                                                                           \
    if (!certJson["chain"][INDEX].is_string()) {                                                                       \
        return error_utils::makeError("Certificate JSON 'chain' field must be an array of strings");                   \
    }                                                                                                                  \
    auto PART##CertStr = certJson["chain"][INDEX].get<std::string>();                                                  \
    auto PART##CertOpt = Certificate::fromString(PART##CertStr);                                                       \
    if (!PART##CertOpt) {                                                                                              \
        return error_utils::makeError("Failed to parse " #PART " certificate");                                        \
    }                                                                                                                  \
    auto PART##Cert = *PART##CertOpt;
#endif

#define SCULK_CERTIFICATE_CHECK_HEADER(PART, FIELD)                                                                    \
    if (!PART##Cert.mHeader.FIELD.has_value()) {                                                                       \
        return error_utils::makeError(#PART " certificate does not contain a valid '" #FIELD "' header field");        \
    }

#define SCULK_CERTIFICATE_CHECK_PAYLOAD(PART, FIELD)                                                                   \
    if (!PART##Cert.mPayload.FIELD.has_value()) {                                                                      \
        return error_utils::makeError(#PART " certificate does not contain a valid '" #FIELD "' payload field");       \
    }

Result<LegacyCertificateChain> LegacyCertificateChain::fromString(std::string_view certificateChainJsonStr) {
    auto certJsonOpt = jsonc::json::parse(certificateChainJsonStr);
    if (!certJsonOpt) {
        return error_utils::makeError("Failed to parse certificate chain JSON");
    }

    const auto& certJson = *certJsonOpt;
    if (!certJson.contains("chain") || !certJson["chain"].is_array()) {
        return error_utils::makeError("Certificate JSON does not contain a valid 'chain' field");
    }

    std::size_t certCount = certJson["chain"].size();
    if (certCount == 1) {
        SCULK_CERTIFICATE_PARSE(login, 0);
        SCULK_CERTIFICATE_CHECK_PAYLOAD(login, extraData);
        SCULK_CERTIFICATE_CHECK_PAYLOAD(login, randomNonce);
        SCULK_CERTIFICATE_CHECK_PAYLOAD(login, iss);
        SCULK_CERTIFICATE_CHECK_PAYLOAD(login, iat);
        return LegacyCertificateChain{.mLoginCertificate = std::move(*loginCertOpt)};
    } else if (certCount == 3) {
        SCULK_CERTIFICATE_PARSE(client, 0);
        SCULK_CERTIFICATE_CHECK_PAYLOAD(client, certificateAuthority);
        SCULK_CERTIFICATE_PARSE(mojang, 1);
        SCULK_CERTIFICATE_CHECK_HEADER(mojang, x5t);
        SCULK_CERTIFICATE_CHECK_PAYLOAD(mojang, certificateAuthority);
        SCULK_CERTIFICATE_CHECK_PAYLOAD(mojang, randomNonce);
        SCULK_CERTIFICATE_CHECK_PAYLOAD(mojang, iss);
        SCULK_CERTIFICATE_CHECK_PAYLOAD(mojang, iat);
        SCULK_CERTIFICATE_PARSE(login, 2);
        SCULK_CERTIFICATE_CHECK_HEADER(login, x5t);
        SCULK_CERTIFICATE_CHECK_PAYLOAD(login, extraData);
        SCULK_CERTIFICATE_CHECK_PAYLOAD(login, randomNonce);
        SCULK_CERTIFICATE_CHECK_PAYLOAD(login, iss);
        SCULK_CERTIFICATE_CHECK_PAYLOAD(login, iat);
        return LegacyCertificateChain{
            .mClientCertificate = std::move(*clientCertOpt),
            .mMojangCertificate = std::move(*mojangCertOpt),
            .mLoginCertificate  = std::move(*loginCertOpt)
        };
    }
    return error_utils::makeError("Certificate JSON 'chain' field must contain either 1 or 3 certificates");
}

} // namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE
