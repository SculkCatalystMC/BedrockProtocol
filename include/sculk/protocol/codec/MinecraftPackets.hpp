// Copyright © 2026 SculkCatalystMC. All rights reserved.
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#pragma once
#include "MinecraftPacketIds.hpp"
#include "packet/IPacket.hpp"
#include "sculk/protocol/utility/BinaryStream.hpp"
#include "sculk/protocol/utility/ReadOnlyBinaryStream.hpp"
#include "sculk/protocol/utility/Result.hpp"
#include <memory>

namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE {

class MinecraftPackets {
public:
    struct PacketHeader {
        MinecraftPacketIds mPacketId{};
        std::uint8_t       mSenderSubClientId{};
        std::uint8_t       mTargetSubClientId{};
    };

public:
    [[nodiscard]] static Result<std::unique_ptr<IPacket>> createPacket(MinecraftPacketIds packetId);

    [[nodiscard]] static Result<std::unique_ptr<IPacket>> createPacket(const PacketHeader& header);

    [[nodiscard]] static Result<std::unique_ptr<IPacket>> readAndCreatePacketFromStream(ReadOnlyBinaryStream& stream);

    [[nodiscard]] static Result<std::unique_ptr<IPacket>>
    readAndCreatePacketFromBuffer(std::span<const std::byte> buffer);

    [[nodiscard]] static Result<std::unique_ptr<IPacket>>
    readAndCreatePacketFromHeader(const PacketHeader& header, ReadOnlyBinaryStream& stream);

    [[nodiscard]] static Result<PacketHeader> readPacketHeader(ReadOnlyBinaryStream& stream);

    static void writePacketHeader(BinaryStream& stream, const PacketHeader& header);

    [[nodiscard]] static Result<> readPacket(IPacket& packet, ReadOnlyBinaryStream& stream);
};

} // namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE
