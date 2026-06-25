// Copyright © 2026 SculkCatalystMC. All rights reserved.
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#include "sculk/protocol/auth/ConnectionRequest.hpp"
#include "../ssl/Base64Url.hpp"
#include "../ssl/PemHelper.hpp"
#include "sculk/protocol/utility/BinaryStream.hpp"
#include "sculk/protocol/utility/ReadOnlyBinaryStream.hpp"
#include <limits>
#include <random>
#include <sculk/reflection/jsonc/reflection.hpp>

namespace sculk::reflection::jsonc {
template <>
struct serializer<sculk::protocol::AuthenticationType> {
    static int to_signed(const sculk::protocol::AuthenticationType& t) { return static_cast<int>(t); }
    static std::optional<sculk::protocol::AuthenticationType> from_signed(int v) {
        if (v < static_cast<int>(sculk::protocol::AuthenticationType::Full)
            || v > static_cast<int>(sculk::protocol::AuthenticationType::SelfSigned)) {
            return std::nullopt;
        }
        return static_cast<sculk::protocol::AuthenticationType>(v);
    }
};
} // namespace sculk::reflection::jsonc

namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE {

#ifdef SCULK_PROTOCOL_ENABLE_DETAIL_ERRORS
#define SCULK_CONNECTION_REQUEST_ERROR(STATUS, MESSAGE)                                                                \
    error_utils::makeError(std::format(MESSAGE ": {}", STATUS.error().mMessage))
#else
#define SCULK_CONNECTION_REQUEST_ERROR(STATUS, MESSAGE) error_utils::makeError(MESSAGE);
#endif

std::optional<std::string> ConnectionRequest::getXboxLiveID() const {
    std::string xuid{};
    if (!mLoginToken.isEmpty() && mLoginToken.mHeader.alg == "RS256") {
        xuid = mLoginToken.mPayload.xid;
    }
    if (mLegacyCertificateChain && mLegacyCertificateChain->mClientCertificate.has_value()
        && mLegacyCertificateChain->mMojangCertificate.has_value()) {
        xuid = mLegacyCertificateChain->mLoginCertificate.mPayload.extraData->XUID;
    }
    if (!xuid.empty() && Identity::validateXuid(xuid)) {
        return xuid;
    }
    return std::nullopt;
}

std::string ConnectionRequest::getXboxLiveName() const {
    if (!mLoginToken.isEmpty()) {
        return mLoginToken.mPayload.xname;
    }
    if (mLegacyCertificateChain) {
        return mLegacyCertificateChain->mLoginCertificate.mPayload.extraData->displayName;
    }
    return {};
}

Identity ConnectionRequest::getIdentity() const {
    auto xuid = getXboxLiveID();
    if (xuid && Identity::validateXuid(*xuid)) {
        return Identity::fromXuid(*xuid);
    }
    auto selfSignedId = mClientProperties.mPayload.mSelfSignedId;
    if (Identity::validateString(selfSignedId)) {
        return Identity::fromString(selfSignedId);
    }
    return {};
}

std::string ConnectionRequest::getPlayFabID() const {
    if (!mLoginToken.isEmpty()) {
        return mLoginToken.mPayload.mid;
    }
    return mClientProperties.mPayload.mPlayFabId;
}

std::string ConnectionRequest::getClientPublicKey() const { return mClientProperties.mHeader.x5u; }

Result<ConnectionRequest::VerificationStatus> ConnectionRequest::verify(
    const AuthenticationKeyManager& authenticationKeyManager,
    bool                            onlineMode,
    bool                            allowLegacy
) const {
    if (onlineMode) {
        return _verifyOnline(authenticationKeyManager, allowLegacy);
    }

    auto onlineStatus = _verifyOnline(authenticationKeyManager, allowLegacy);
    if (onlineStatus) {
        return *onlineStatus;
    }

    return _verifySelfSigned(authenticationKeyManager.getLeeway(), allowLegacy);
}

Result<ConnectionRequest::VerificationStatus>
ConnectionRequest::_verifyOnline(const AuthenticationKeyManager& authenticationKeyManager, bool allowLegacy) const {
    if (mLoginToken.verifyOnline(authenticationKeyManager)) {
        if (mClientProperties.verify(mLoginToken.getClientPublicKey())) {
            return VerificationStatus::Online;
        }
        return error_utils::makeError("client properties verification failed with login token public key");
    }

    if (allowLegacy && mLegacyCertificateChain) {
        if (mLegacyCertificateChain->verifyOnline(authenticationKeyManager.getLeeway())) {
            if (mClientProperties.verify(mLegacyCertificateChain->mLoginCertificate.mHeader.x5u)) {
                return VerificationStatus::LegacyOnline_Deprecated;
            }
            return error_utils::makeError(
                "client properties verification failed with legacy certificate chain public key"
            );
        }
        return error_utils::makeError("legacy certificate chain verification failed");
    }

    return error_utils::makeError("login token verification failed");
}

Result<ConnectionRequest::VerificationStatus>
ConnectionRequest::_verifySelfSigned(std::chrono::seconds leeway, bool allowLegacy) const {
    if (mLoginToken.verifySelfSigned(leeway)) {
        if (mClientProperties.verify(mLoginToken.getClientPublicKey())) {
            return VerificationStatus::SelfSigned;
        }
        return error_utils::makeError("client properties verification failed with self-signed login token public key");
    }

    if (allowLegacy && mLegacyCertificateChain) {
        if (mLegacyCertificateChain->verifySelfSigned(leeway)) {
            if (mClientProperties.verify(mLegacyCertificateChain->mLoginCertificate.mHeader.x5u)) {
                return VerificationStatus::LegacySelfSigned_Deprecated;
            }
            return error_utils::makeError(
                "client properties verification failed with legacy self-signed certificate chain public key"
            );
        }
        return error_utils::makeError("legacy certificate chain self-signed verification failed");
    }

    return error_utils::makeError("login token self-signed verification failed");
}

namespace {

template <typename T = std::int64_t>
inline T randomInt() {
    thread_local std::mt19937_64     generator(std::random_device{}());
    std::uniform_int_distribution<T> dist(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
    return dist(generator);
}

inline Identity randomIdentity() {
    return Identity{.mHighBits = randomInt<std::uint64_t>(), .mLowBits = randomInt<std::uint64_t>()};
}

inline void ensureAndFillLoginToken(ConnectionRequest& request) {
    auto& payload = request.mLoginToken.mPayload;

    payload.mid = request.getPlayFabID();
    payload.ap  = 0; // Unknown, seems to be 0
    if (payload.xid.empty() && request.mLegacyCertificateChain) {
        payload.xid = request.mLegacyCertificateChain->mLoginCertificate.mPayload.extraData->XUID;
    }
    if (payload.xname.empty()) {
        payload.xname = request.getXboxLiveName();
    }
    payload.aud = "api://auth-minecraft-services/multiplayer";
    if (!payload.leguuid) {
        payload.leguuid = randomIdentity().toString(); // Unknown field, seems to be a random UUID
    }
    payload.nid   = ""; // Unknown field, seems to be empty
    payload.nname = ""; // Unknown field, seems to be empty
    payload.pid   = ""; // Unknown field, seems to be empty
    payload.pname = ""; // Unknown field, seems to be empty

    request.mClientProperties.mPayload.mPlayFabId = payload.mid;
}

inline void ensureAndFillLegacyChain(ConnectionRequest& request) {
    if (!request.mLegacyCertificateChain) {
        request.mLegacyCertificateChain = LegacyCertificateChain{
            .mLoginCertificate = Certificate{
                .mPayload = {
                    .randomNonce = randomInt(),
                    .iss         = "Mojang",
                    .iat         = 0,
                    .extraData   = Certificate::ExtraData{
                          .identity    = "", // Empty identity
                          .displayName = request.getXboxLiveName(),
                          .XUID        = "",      // Empty XUID
                          .sandBoxId   = "RETAIL" // Always "RETAIL"
                    }
                }
            }
        };
    } else {
        auto& extraData    = *request.mLegacyCertificateChain->mLoginCertificate.mPayload.extraData;
        extraData.identity = "";
        extraData.XUID     = "";
    }
}

} // namespace

Result<> ConnectionRequest::selfSign(const PemKeyPair& clientKeyPair, bool includeLegacyChain) {
    mAuthenticationType = AuthenticationType::SelfSigned;

    ensureAndFillLoginToken(*this);
    if (auto status = mLoginToken.selfSign(clientKeyPair); !status) {
        return SCULK_CONNECTION_REQUEST_ERROR(status, "Login token signing failed");
    }

    if (auto status = mClientProperties.sign(clientKeyPair); !status) {
        return SCULK_CONNECTION_REQUEST_ERROR(status, "Client properties signing failed");
    }

    if (!includeLegacyChain) {
        return {};
    }

    ensureAndFillLegacyChain(*this);
    if (auto status = mLegacyCertificateChain->selfSign(clientKeyPair); !status) {
        return SCULK_CONNECTION_REQUEST_ERROR(status, "Legacy certificate chain signing failed");
    }

    return {};
}

std::string ConnectionRequest::toString() const {
    std::vector<std::byte> buffer{};
    BinaryStream           stream(buffer);

    jsonc::json authJson = {
        {"AuthenticationType", static_cast<int>(mAuthenticationType)},
        {"Token",              mLoginToken.toString()               }
    };
    if (mLegacyCertificateChain) {
        authJson["Certificate"] = mLegacyCertificateChain->toString();
    }

    auto authJsonStr = authJson.dump(-1);
    authJsonStr.push_back('\n');
    stream.writeLongString(authJsonStr);
    stream.writeLongString(mClientProperties.toString());

    std::string result{};
    result.resize_and_overwrite(buffer.size(), [&buffer](char* data, std::size_t size) {
        std::memcpy(data, buffer.data(), size);
        return size;
    });

    return result;
}

#ifdef SCULK_PROTOCOL_ENABLE_DETAIL_ERRORS
#define SCULK_CONNECTION_REQUEST_DESERIALIZE_REQUIRED(FIELD_NAME, VALUE)                                               \
    if (!authJson.contains(FIELD_NAME)) {                                                                              \
        return error_utils::makeError("Authentication JSON does not contain a valid '" FIELD_NAME "' field");          \
    }                                                                                                                  \
    if (auto status = reflection::jsonc::deserialize<false, false>(VALUE, authJson[FIELD_NAME], options); !status) {   \
        return error_utils::makeError(                                                                                 \
            std::format("Failed to deserialize '{}' field in authentication JSON: {}", FIELD_NAME, status.error())     \
        );                                                                                                             \
    }

#define SCULK_CONNECTION_REQUEST_DESERIALIZE_OPTIONAL(FIELD_NAME, VALUE)                                               \
    if (authJson.contains(FIELD_NAME)) {                                                                               \
        if (auto status = reflection::jsonc::deserialize<false, false>(VALUE, authJson[FIELD_NAME], options);          \
            !status) {                                                                                                 \
            return error_utils::makeError(                                                                             \
                std::format("Failed to deserialize '{}' field in authentication JSON: {}", FIELD_NAME, status.error()) \
            );                                                                                                         \
        }                                                                                                              \
    }
#else
#define SCULK_CONNECTION_REQUEST_DESERIALIZE_REQUIRED(FIELD_NAME, VALUE)                                               \
    if (!authJson.contains(FIELD_NAME)) {                                                                              \
        return error_utils::makeError("Authentication JSON does not contain a valid '" FIELD_NAME "' field");          \
    }                                                                                                                  \
    if (!reflection::jsonc::deserialize<false, false>(VALUE, authJson[FIELD_NAME], options)) {                         \
        return error_utils::makeError("Failed to deserialize '" FIELD_NAME "' field in authentication JSON");          \
    }

#define SCULK_CONNECTION_REQUEST_DESERIALIZE_OPTIONAL(FIELD_NAME, VALUE)                                               \
    if (authJson.contains(FIELD_NAME)) {                                                                               \
        if (!reflection::jsonc::deserialize<false, false>(VALUE, authJson[FIELD_NAME], options)) {                     \
            return error_utils::makeError("Failed to deserialize '" FIELD_NAME "' field in authentication JSON");      \
        }                                                                                                              \
    }
#endif

Result<ConnectionRequest> ConnectionRequest::fromString(std::string_view rawRequest) {
    ReadOnlyBinaryStream stream(rawRequest);

    std::string authJsonStr{};
    if (!stream.readLongString(authJsonStr)) {
#ifdef SCULK_PROTOCOL_ENABLE_DETAIL_ERRORS
        return error_utils::makeError(
            std::format(
                "Read ConnectionRequest authentication JSON overflowed: mReadPointer={}, size={}",
                stream.getPosition(),
                stream.size()
            )
        );
#else
        return error_utils::makeError("Read ConnectionRequest authentication JSON overflowed");
#endif
    }

    auto authJsonOpt = jsonc::json::parse(authJsonStr);
    if (!authJsonOpt) {
        return error_utils::makeError("Failed to parse authentication JSON from ConnectionRequest");
    }

    const auto&                          authJson = *authJsonOpt;
    constexpr reflection::jsonc::options options{
        .indent                   = -1,
        .allow_trailing_comma     = false,
        .enum_cast_case_sensitive = true
    };

    AuthenticationType authType{};
    SCULK_CONNECTION_REQUEST_DESERIALIZE_REQUIRED("AuthenticationType", authType);
    std::optional<std::string> legacyCertificate{};
    SCULK_CONNECTION_REQUEST_DESERIALIZE_OPTIONAL("Certificate", legacyCertificate);
    std::string loginToken{};
    SCULK_CONNECTION_REQUEST_DESERIALIZE_REQUIRED("Token", loginToken);

    std::string clientProperties{};
    if (!stream.readLongString(clientProperties)) {
#ifdef SCULK_PROTOCOL_ENABLE_DETAIL_ERRORS
        return error_utils::makeError(
            std::format(
                "Read ConnectionRequest client properties overflowed: mReadPointer={}, size={}",
                stream.getPosition(),
                stream.size()
            )
        );
#else
        return error_utils::makeError("Read ConnectionRequest client properties overflowed");
#endif
    }

    return ConnectionRequest::create(authType, legacyCertificate, loginToken, clientProperties);
}

Result<ConnectionRequest> ConnectionRequest::create(
    AuthenticationType                authenticationType,
    const std::optional<std::string>& legacyCertificateChainString,
    std::string_view                  loginTokenString,
    std::string_view                  clientPropertiesString
) {
    std::optional<LegacyCertificateChain> legacyCertificateChain{};
    if (legacyCertificateChainString && *legacyCertificateChainString != "{\"chain\":[\"..\"]}\n") {
        auto certChain = LegacyCertificateChain::fromString(*legacyCertificateChainString);
        if (!certChain) {
#ifdef SCULK_PROTOCOL_ENABLE_DETAIL_ERRORS
            return error_utils::makeError(
                std::format("Failed to parse legacy certificate chain: {}", certChain.error().mMessage)
            );
#else
            return error_utils::makeError("Failed to parse legacy certificate chain");
#endif
        }
        legacyCertificateChain = std::move(*certChain);
    }

    auto loginToken = LoginToken::fromString(loginTokenString);
    if (!loginToken) {
#ifdef SCULK_PROTOCOL_ENABLE_DETAIL_ERRORS
        return error_utils::makeError(std::format("Failed to parse login token: {}", loginToken.error().mMessage));
#else
        return error_utils::makeError("Failed to parse login token");
#endif
    }

    if (!legacyCertificateChain && loginToken->isEmpty()) {
        return error_utils::makeError("ConnectionRequest must have either a legacy certificate chain or a login token");
    }

    auto clientProperties = ClientProperties::fromString(clientPropertiesString);
    if (!clientProperties) {
#ifdef SCULK_PROTOCOL_ENABLE_DETAIL_ERRORS
        return error_utils::makeError(
            std::format("Failed to parse client properties: {}", clientProperties.error().mMessage)
        );
#else
        return error_utils::makeError("Failed to parse client properties");
#endif
    }

    return ConnectionRequest{
        .mAuthenticationType     = authenticationType,
        .mLegacyCertificateChain = std::move(legacyCertificateChain),
        .mLoginToken             = std::move(*loginToken),
        .mClientProperties       = std::move(*clientProperties)
    };
}

} // namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE
