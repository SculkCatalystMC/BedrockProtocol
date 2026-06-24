// Copyright © 2026 SculkCatalystMC. All rights reserved.
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#pragma once
#include "InventoryTransaction.hpp"
#include "sculk/protocol/codec/inventory/item/NetworkItemStackDescriptor.hpp"
#include "sculk/protocol/codec/level/block/BlockPos.hpp"
#include "sculk/protocol/codec/math/Vec3.hpp"
#include "sculk/protocol/utility/Enum.hpp"
#include <cstdint>

namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE {

class ItemUseInventoryTransaction {
public:
    enum class ActionType : std::uint8_t {
        Place       = 0,
        Use         = 1,
        Destroy     = 2,
        UseAsAttack = 3,
    };

    enum class ClientCooldownState : std::uint8_t {
        Off = 0,
        On  = 1,
    };

    enum class PredictedResult : std::uint8_t {
        Failure = 0,
        Success = 1,
    };

    enum class TriggerType : std::uint8_t {
        Unknown        = 0,
        PlayerInput    = 1,
        SimulationTick = 2,
    };

public:
    InventoryTransaction       mTransaction{};
    ActionType                 mActionType{};
    TriggerType                mTriggerType{};
    BlockPos                   mPos{};
    std::uint32_t              mTargetBlockId{};
    std::uint8_t               mFace{};
    std::int32_t               mSlot{};
    NetworkItemStackDescriptor mItem{};
    Vec3                       mFromPos{};
    Vec3                       mClickPos{};
    PredictedResult            mClientPredictedResult{};
    ClientCooldownState        mClientCooldownState{};

public:
    void write(BinaryStream& stream) const;

    [[nodiscard]] Result<> read(ReadOnlyBinaryStream& stream);

    void writeLegacy(BinaryStream& stream) const;

    [[nodiscard]] Result<> readLegacy(ReadOnlyBinaryStream& stream);
};

} // namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE

SCULK_PROTOCOL_ENUM_RANGE(ItemUseInventoryTransaction::ActionType, 0, 3)
SCULK_PROTOCOL_ENUM_RANGE(ItemUseInventoryTransaction::ClientCooldownState, 0, 1)
SCULK_PROTOCOL_ENUM_RANGE(ItemUseInventoryTransaction::PredictedResult, 0, 1)
SCULK_PROTOCOL_ENUM_RANGE(ItemUseInventoryTransaction::TriggerType, 0, 2)
