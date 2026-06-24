// Copyright © 2026 SculkCatalystMC. All rights reserved.
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#include "sculk/protocol/codec/inventory/transaction/InventoryTransactionSource.hpp"

namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE {

void InventoryTransactionSource::write(BinaryStream& stream) const {
    stream.writeEnum(mType, &BinaryStream::writeUnsignedVarInt);
    stream.writeBool(true); // unknown flag, should always be true
    stream.writeOptional(mContainerId, [&](BinaryStream& stream, const std::uint8_t& value) {
        stream.writeByte(value);
    });
    stream.writeBool(true); // unknown flag, should always be true
    stream.writeOptional(mBitFlags, [&](BinaryStream& stream, std::uint32_t const& value) {
        stream.writeUnsignedVarInt(value);
    });
}

Result<> InventoryTransactionSource::read(ReadOnlyBinaryStream& stream) {
    _SCULK_READ(stream.readEnum(mType, &ReadOnlyBinaryStream::readUnsignedVarInt));

    bool unknownFlag1{};
    _SCULK_READ(stream.readBool(unknownFlag1));
    if (!unknownFlag1) {
        return error_utils::makeError("Expected container id");
    }

    _SCULK_READ(stream.readOptional(mContainerId, [&](ReadOnlyBinaryStream& stream, std::uint8_t& value) {
        return stream.readByte(value);
    }));

    bool unknownFlag2{};
    _SCULK_READ(stream.readBool(unknownFlag2));
    if (!unknownFlag2) {
        return error_utils::makeError("Expected bit flags");
    }

    return stream.readOptional(mBitFlags, [&](ReadOnlyBinaryStream& stream, std::uint32_t& value) {
        return stream.readUnsignedVarInt(value);
    });
}

void InventoryTransactionSource::writeLegacy(BinaryStream& stream) const {
    stream.writeEnum(mType, &BinaryStream::writeUnsignedVarInt);
    switch (mType) {
    case InventoryTransactionSourceType::ContainerInventory:
    case InventoryTransactionSourceType::NonImplementedFeatureTODO:
        stream.writeVarInt(mContainerId.value_or(0));
        break;
    case InventoryTransactionSourceType::WorldInteraction:
        stream.writeUnsignedVarInt(mBitFlags.value_or(0));
        break;
    default:
        break;
    }
}

Result<> InventoryTransactionSource::readLegacy(ReadOnlyBinaryStream& stream) {
    _SCULK_READ(stream.readEnum(mType, &ReadOnlyBinaryStream::readUnsignedVarInt));
    switch (mType) {
    case InventoryTransactionSourceType::ContainerInventory:
    case InventoryTransactionSourceType::NonImplementedFeatureTODO: {
        int containerId{};
        _SCULK_READ(stream.readVarInt(containerId));
        mContainerId.emplace() = static_cast<std::uint8_t>(containerId);
        return {};
    }
    case InventoryTransactionSourceType::WorldInteraction: {
        return stream.readUnsignedVarInt(mBitFlags.emplace());
    }
    default:
        return {};
    }
}

} // namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE