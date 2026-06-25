// Copyright © 2026 SculkCatalystMC. All rights reserved.
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#include "sculk/protocol/auth/ClientProperties.hpp"
#include "../ssl/ES384.hpp"
#include <format>
#include <sculk/reflection/jsonc/reflection.hpp>

namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE {

namespace {
constexpr std::string_view removeLeadingCharacter(std::string_view key) {
    if (key.starts_with("m")) {
        key.remove_prefix(1);
    }
    return key;
}
} // namespace

#define SCULK_CLIENT_PROPERTIES_SERIALIZE_OPTION_INIT()                                                                \
    constexpr reflection::jsonc::options options {                                                                     \
        .indent = -1, .allow_trailing_comma = false, .enum_cast_case_sensitive = true                                  \
    }

#define SCULK_CLIENT_PROPERTIES_CREATE_JSON(PART, DATA)                                                                \
    jsonc::json PART##Json = jsonc::json::object();                                                                    \
    const auto& PART       = DATA;

#define SCULK_CLIENT_PROPERTIES_SERIALIZE(PART, FIELD)                                                                 \
    auto FIELD = reflection::jsonc::serialize<false, false>(PART.FIELD, removeLeadingCharacter, options);              \
    PART##Json[removeLeadingCharacter(#FIELD)] = FIELD;


#define SCULK_CLIENT_PROPERTIES_PARSE_JSON(PART, RAW)                                                                  \
    auto PART##JsonStr = base64url::decodeChecked(RAW);                                                                \
    if (!PART##JsonStr) {                                                                                              \
        return error_utils::makeError("Failed to decode client properties " #PART);                                    \
    }                                                                                                                  \
    auto PART##JsonOpt = jsonc::json::parse(*PART##JsonStr);                                                           \
    if (!PART##JsonOpt) {                                                                                              \
        return error_utils::makeError("Failed to parse client properties " #PART " JSON");                             \
    }                                                                                                                  \
    const auto& PART##Json = *PART##JsonOpt;

#ifdef SCULK_PROTOCOL_ENABLE_DETAIL_ERRORS
#define SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(PART, FIELD)                                                      \
    if (!PART##Json.contains(removeLeadingCharacter(#FIELD))) {                                                        \
        return error_utils::makeError("Client properties " #PART " JSON does not contain a valid field '" #FIELD "'"); \
    }                                                                                                                  \
    if (auto status = reflection::jsonc::deserialize<false, false>(                                                    \
            PART.FIELD,                                                                                                \
            PART##Json[removeLeadingCharacter(#FIELD)],                                                                \
            removeLeadingCharacter,                                                                                    \
            options                                                                                                    \
        );                                                                                                             \
        !status) {                                                                                                     \
        return error_utils::makeError(                                                                                 \
            std::format("Failed to deserialize client properties {} field '{}': {}", #PART, #FIELD, status.error())    \
        );                                                                                                             \
    }

#define SCULK_CLIENT_PROPERTIES_DESERIALIZE_OPTIONAL(PART, FIELD)                                                      \
    if (PART##Json.contains(removeLeadingCharacter(#FIELD))) {                                                         \
        if (auto status = reflection::jsonc::deserialize<false, false>(                                                \
                PART.FIELD,                                                                                            \
                PART##Json[removeLeadingCharacter(#FIELD)],                                                            \
                removeLeadingCharacter,                                                                                \
                options                                                                                                \
            );                                                                                                         \
            !status) {                                                                                                 \
            return error_utils::makeError(                                                                             \
                std::format(                                                                                           \
                    "Failed to deserialize client properties {} field '{}': {}",                                       \
                    #PART,                                                                                             \
                    #FIELD,                                                                                            \
                    status.error()                                                                                     \
                )                                                                                                      \
            );                                                                                                         \
        }                                                                                                              \
    }
#else
#define SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(PART, FIELD)                                                      \
    if (!PART##Json.contains(removeLeadingCharacter(#FIELD))) {                                                        \
        return error_utils::makeError("Client properties " #PART " JSON does not contain a valid field '" #FIELD "'"); \
    }                                                                                                                  \
    if (!reflection::jsonc::deserialize<false, false>(                                                                 \
            PART.FIELD,                                                                                                \
            PART##Json[removeLeadingCharacter(#FIELD)],                                                                \
            removeLeadingCharacter,                                                                                    \
            options                                                                                                    \
        )) {                                                                                                           \
        return error_utils::makeError("Failed to deserialize client properties " #PART " field '" #FIELD "'");         \
    }

#define SCULK_CLIENT_PROPERTIES_DESERIALIZE_OPTIONAL(PART, FIELD)                                                      \
    if (PART##Json.contains(removeLeadingCharacter(#FIELD))) {                                                         \
        if (!reflection::jsonc::deserialize<false, false>(                                                             \
                PART.FIELD,                                                                                            \
                PART##Json[removeLeadingCharacter(#FIELD)],                                                            \
                removeLeadingCharacter,                                                                                \
                options                                                                                                \
            )) {                                                                                                       \
            return error_utils::makeError("Failed to deserialize client properties " #PART " field '" #FIELD "'");     \
        }                                                                                                              \
    }
#endif


Result<> ClientProperties::verify(std::string_view publicKeyPem) const {
    std::string signingInput = std::format("{}.{}", mRawHeader, mRawPayload);
    if (!es384::verifyES384Signature(signingInput, mSignature, publicKeyPem)) {
        return error_utils::makeError("Failed to verify client properties signature");
    }
    return {};
}

Result<> ClientProperties::sign(const PemKeyPair& clientKeyPair) {
    SCULK_CLIENT_PROPERTIES_SERIALIZE_OPTION_INIT();

    mHeader.alg = "ES384";
    mHeader.x5u = pem_helper::stripPemMarkersAndCompact(clientKeyPair.mPublicKeyPem);

    SCULK_CLIENT_PROPERTIES_CREATE_JSON(header, mHeader);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(header, alg);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(header, x5u);
    mRawHeader = base64url::encode(headerJson.dump(-1));

    SCULK_CLIENT_PROPERTIES_CREATE_JSON(payload, mPayload);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mAnimatedImageData);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mArmSize);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mCapeData);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mCapeId);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mCapeImageHeight);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mCapeImageWidth);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mCapeOnClassicSkin);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mClientRandomId);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mCompatibleWithClientSideChunkGen);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mCurrentInputMode);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mDefaultInputMode);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mDeviceId);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mDeviceModel);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mDeviceOS);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mFilterProfanity);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mGameVersion);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mGraphicsMode);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mGuiScale);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mIsEditorMode);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mIsPartyLeader);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mLanguageCode);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mMaxViewDistance);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mMemoryTier);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mOverrideSkin);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mPartyId);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mPersonaPieces);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mPersonaSkin);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mPieceTintColors);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mPlatformOfflineId);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mPlatformOnlineId);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mPlatformType);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mPlayFabId);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mPremiumSkin);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mSelfSignedId);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mServerAddress);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mSkinAnimationData);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mSkinColor);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mSkinData);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mSkinGeometryData);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mSkinGeometryDataEngineVersion);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mSkinId);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mSkinImageHeight);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mSkinImageWidth);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mSkinResourcePatch);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mThirdPartyName);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mTrustedSkin);
    SCULK_CLIENT_PROPERTIES_SERIALIZE(payload, mUIProfile);
    mRawPayload = base64url::encode(payloadJson.dump(-1));

    std::string signingInput = std::format("{}.{}", mRawHeader, mRawPayload);
    if (!es384::signES384Signature(signingInput, clientKeyPair.mPrivateKeyPem, mSignature)) {
        return error_utils::makeError("Failed to sign client properties");
    }
    return {};
}

std::string ClientProperties::toString() const { return std::format("{}.{}.{}", mRawHeader, mRawPayload, mSignature); }

Result<ClientProperties> ClientProperties::fromString(std::string_view rawClientProperties) {
    SCULK_CLIENT_PROPERTIES_SERIALIZE_OPTION_INIT();

    const auto first = rawClientProperties.find('.');
    const auto last  = rawClientProperties.rfind('.');

    if (first == std::string::npos || last == std::string::npos || first == last) {
        return error_utils::makeError("Invalid client properties format: expected three parts separated by dots");
    }

    auto   rawHeader = rawClientProperties.substr(0, first);
    Header header{};
    SCULK_CLIENT_PROPERTIES_PARSE_JSON(header, rawHeader);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(header, alg);
    if (header.alg != "ES384") {
        return error_utils::makeError("certificate signing algorithm must be ES384");
    }
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(header, x5u);

    auto    rawPayload = rawClientProperties.substr(first + 1, last - first - 1);
    Payload payload{};
    SCULK_CLIENT_PROPERTIES_PARSE_JSON(payload, rawPayload);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mAnimatedImageData);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mArmSize);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mCapeData);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mCapeId);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mCapeImageHeight);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mCapeImageWidth);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mCapeOnClassicSkin);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mClientRandomId);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mCompatibleWithClientSideChunkGen);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mCurrentInputMode);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mDefaultInputMode);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mDeviceId);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mDeviceModel);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mDeviceOS);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_OPTIONAL(payload, mFilterProfanity);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mGameVersion);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mGraphicsMode);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mGuiScale);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_OPTIONAL(payload, mIsEditorMode);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_OPTIONAL(payload, mIsPartyLeader);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mLanguageCode);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mMaxViewDistance);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mMemoryTier);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mOverrideSkin);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_OPTIONAL(payload, mPartyId);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mPersonaPieces);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mPersonaSkin);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mPieceTintColors);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mPlatformOfflineId);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mPlatformOnlineId);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mPlatformType);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_OPTIONAL(payload, mPlayFabId);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mPremiumSkin);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mSelfSignedId);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mServerAddress);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mSkinAnimationData);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mSkinColor);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mSkinData);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mSkinGeometryData);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mSkinGeometryDataEngineVersion);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mSkinId);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mSkinImageHeight);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mSkinImageWidth);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mSkinResourcePatch);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mThirdPartyName);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mTrustedSkin);
    SCULK_CLIENT_PROPERTIES_DESERIALIZE_REQUIRED(payload, mUIProfile);

    auto signature = rawClientProperties.substr(last + 1);

    return ClientProperties{
        .mRawHeader  = std::string(rawHeader),
        .mHeader     = std::move(header),
        .mRawPayload = std::string(rawPayload),
        .mPayload    = std::move(payload),
        .mSignature  = std::string(signature)
    };
}

} // namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE