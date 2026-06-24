// Copyright © 2026 SculkCatalystMC. All rights reserved.
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#include "sculk/protocol/codec/MinecraftPackets.hpp"
#include "sculk/protocol/codec/packet/ActorEventPacket.hpp"
#include "sculk/protocol/codec/packet/ActorPickRequestPacket.hpp"
#include "sculk/protocol/codec/packet/AddActorPacket.hpp"
#include "sculk/protocol/codec/packet/AddBehaviorTreePacket.hpp"
#include "sculk/protocol/codec/packet/AddItemActorPacket.hpp"
#include "sculk/protocol/codec/packet/AddPaintingPacket.hpp"
#include "sculk/protocol/codec/packet/AddPlayerPacket.hpp"
#include "sculk/protocol/codec/packet/AddVolumeEntityPacket.hpp"
#include "sculk/protocol/codec/packet/AgentActionEventPacket.hpp"
#include "sculk/protocol/codec/packet/AgentAnimationPacket.hpp"
#include "sculk/protocol/codec/packet/AnimateEntityPacket.hpp"
#include "sculk/protocol/codec/packet/AnimatePacket.hpp"
#include "sculk/protocol/codec/packet/AnvilDamagePacket.hpp"
#include "sculk/protocol/codec/packet/AutomationClientConnectPacket.hpp"
#include "sculk/protocol/codec/packet/AvailableActorIdentifiersPacket.hpp"
#include "sculk/protocol/codec/packet/AvailableCommandsPacket.hpp"
#include "sculk/protocol/codec/packet/AwardAchievementPacket.hpp"
#include "sculk/protocol/codec/packet/BiomeDefinitionListPacket.hpp"
#include "sculk/protocol/codec/packet/BlockActorDataPacket.hpp"
#include "sculk/protocol/codec/packet/BlockEventPacket.hpp"
#include "sculk/protocol/codec/packet/BlockPickRequestPacket.hpp"
#include "sculk/protocol/codec/packet/BookEditPacket.hpp"
#include "sculk/protocol/codec/packet/BossEventPacket.hpp"
#include "sculk/protocol/codec/packet/CameraAimAssistActorPriorityPacket.hpp"
#include "sculk/protocol/codec/packet/CameraAimAssistPacket.hpp"
#include "sculk/protocol/codec/packet/CameraAimAssistPresetsPacket.hpp"
#include "sculk/protocol/codec/packet/CameraInstructionPacket.hpp"
#include "sculk/protocol/codec/packet/CameraPacket.hpp"
#include "sculk/protocol/codec/packet/CameraPresetsPacket.hpp"
#include "sculk/protocol/codec/packet/CameraShakePacket.hpp"
#include "sculk/protocol/codec/packet/CameraSplinePacket.hpp"
#include "sculk/protocol/codec/packet/ChangeDimensionPacket.hpp"
#include "sculk/protocol/codec/packet/ChangeMobPropertyPacket.hpp"
#include "sculk/protocol/codec/packet/ChunkRadiusUpdatedPacket.hpp"
#include "sculk/protocol/codec/packet/ClientCacheBlobStatusPacket.hpp"
#include "sculk/protocol/codec/packet/ClientCacheMissResponsePacket.hpp"
#include "sculk/protocol/codec/packet/ClientCacheStatusPacket.hpp"
#include "sculk/protocol/codec/packet/ClientCameraAimAssistPacket.hpp"
#include "sculk/protocol/codec/packet/ClientMovementPredictionSyncPacket.hpp"
#include "sculk/protocol/codec/packet/ClientToServerHandshakePacket.hpp"
#include "sculk/protocol/codec/packet/ClientboundAttributeLayerSyncPacket.hpp"
#include "sculk/protocol/codec/packet/ClientboundCloseFormPacket.hpp"
#include "sculk/protocol/codec/packet/ClientboundControlSchemeSetPacket.hpp"
#include "sculk/protocol/codec/packet/ClientboundDataDrivenUICloseAllScreensPacket.hpp"
#include "sculk/protocol/codec/packet/ClientboundDataDrivenUIReloadPacket.hpp"
#include "sculk/protocol/codec/packet/ClientboundDataDrivenUIShowScreenPacket.hpp"
#include "sculk/protocol/codec/packet/ClientboundDataStorePacket.hpp"
#include "sculk/protocol/codec/packet/ClientboundDebugRendererPacket.hpp"
#include "sculk/protocol/codec/packet/ClientboundMapItemDataPacket.hpp"
#include "sculk/protocol/codec/packet/ClientboundTextureShiftPacket.hpp"
#include "sculk/protocol/codec/packet/ClientboundUpdateSoundDataPacket.hpp"
#include "sculk/protocol/codec/packet/CodeBuilderPacket.hpp"
#include "sculk/protocol/codec/packet/CodeBuilderSourcePacket.hpp"
#include "sculk/protocol/codec/packet/CommandBlockUpdatePacket.hpp"
#include "sculk/protocol/codec/packet/CommandOutputPacket.hpp"
#include "sculk/protocol/codec/packet/CommandRequestPacket.hpp"
#include "sculk/protocol/codec/packet/CompletedUsingItemPacket.hpp"
#include "sculk/protocol/codec/packet/ContainerClosePacket.hpp"
#include "sculk/protocol/codec/packet/ContainerOpenPacket.hpp"
#include "sculk/protocol/codec/packet/ContainerRegistryCleanupPacket.hpp"
#include "sculk/protocol/codec/packet/ContainerSetDataPacket.hpp"
#include "sculk/protocol/codec/packet/CorrectPlayerMovePredictionPacket.hpp"
#include "sculk/protocol/codec/packet/CraftingDataPacket.hpp"
#include "sculk/protocol/codec/packet/CreatePhotoPacket.hpp"
#include "sculk/protocol/codec/packet/CreativeContentPacket.hpp"
#include "sculk/protocol/codec/packet/CurrentStructureFeaturePacket.hpp"
#include "sculk/protocol/codec/packet/DeathInfoPacket.hpp"
#include "sculk/protocol/codec/packet/DebugInfoPacket.hpp"
#include "sculk/protocol/codec/packet/DimensionDataPacket.hpp"
#include "sculk/protocol/codec/packet/DisconnectPacket.hpp"
#include "sculk/protocol/codec/packet/EditorNetworkPacket.hpp"
#include "sculk/protocol/codec/packet/EduUriResourcePacket.hpp"
#include "sculk/protocol/codec/packet/EducationSettingsPacket.hpp"
#include "sculk/protocol/codec/packet/EmoteListPacket.hpp"
#include "sculk/protocol/codec/packet/EmotePacket.hpp"
#include "sculk/protocol/codec/packet/FeatureRegistryPacket.hpp"
#include "sculk/protocol/codec/packet/GameRulesChangedPacket.hpp"
#include "sculk/protocol/codec/packet/GameTestRequestPacket.hpp"
#include "sculk/protocol/codec/packet/GameTestResultsPacket.hpp"
#include "sculk/protocol/codec/packet/GraphicsOverrideParameterPacket.hpp"
#include "sculk/protocol/codec/packet/GuiDataPickItemPacket.hpp"
#include "sculk/protocol/codec/packet/HurtArmorPacket.hpp"
#include "sculk/protocol/codec/packet/InteractPacket.hpp"
#include "sculk/protocol/codec/packet/InventoryContentPacket.hpp"
#include "sculk/protocol/codec/packet/InventorySlotPacket.hpp"
#include "sculk/protocol/codec/packet/InventoryTransactionPacket.hpp"
#include "sculk/protocol/codec/packet/ItemRegistryPacket.hpp"
#include "sculk/protocol/codec/packet/ItemStackRequestPacket.hpp"
#include "sculk/protocol/codec/packet/ItemStackResponsePacket.hpp"
#include "sculk/protocol/codec/packet/JigsawStructureDataPacket.hpp"
#include "sculk/protocol/codec/packet/LabTablePacket.hpp"
#include "sculk/protocol/codec/packet/LecternUpdatePacket.hpp"
#include "sculk/protocol/codec/packet/LegacyTelemetryEventPacket.hpp"
#include "sculk/protocol/codec/packet/LessonProgressPacket.hpp"
#include "sculk/protocol/codec/packet/LevelChunkPacket.hpp"
#include "sculk/protocol/codec/packet/LevelEventGenericPacket.hpp"
#include "sculk/protocol/codec/packet/LevelEventPacket.hpp"
#include "sculk/protocol/codec/packet/LevelSoundEventPacket.hpp"
#include "sculk/protocol/codec/packet/LocatorBarPacket.hpp"
#include "sculk/protocol/codec/packet/LoginPacket.hpp"
#include "sculk/protocol/codec/packet/MapCreateLockedCopyPacket.hpp"
#include "sculk/protocol/codec/packet/MapInfoRequestPacket.hpp"
#include "sculk/protocol/codec/packet/MobArmorEquipmentPacket.hpp"
#include "sculk/protocol/codec/packet/MobEffectPacket.hpp"
#include "sculk/protocol/codec/packet/MobEquipmentPacket.hpp"
#include "sculk/protocol/codec/packet/ModalFormRequestPacket.hpp"
#include "sculk/protocol/codec/packet/ModalFormResponsePacket.hpp"
#include "sculk/protocol/codec/packet/MotionPredictionHintsPacket.hpp"
#include "sculk/protocol/codec/packet/MoveActorAbsolutePacket.hpp"
#include "sculk/protocol/codec/packet/MoveActorDeltaPacket.hpp"
#include "sculk/protocol/codec/packet/MovePlayerPacket.hpp"
#include "sculk/protocol/codec/packet/MovementEffectPacket.hpp"
#include "sculk/protocol/codec/packet/MultiplayerSettingsPacket.hpp"
#include "sculk/protocol/codec/packet/NetworkChunkPublisherUpdatePacket.hpp"
#include "sculk/protocol/codec/packet/NetworkSettingsPacket.hpp"
#include "sculk/protocol/codec/packet/NetworkStackLatencyPacket.hpp"
#include "sculk/protocol/codec/packet/NpcDialoguePacket.hpp"
#include "sculk/protocol/codec/packet/NpcRequestPacket.hpp"
#include "sculk/protocol/codec/packet/OnScreenTextureAnimationPacket.hpp"
#include "sculk/protocol/codec/packet/OpenSignPacket.hpp"
#include "sculk/protocol/codec/packet/PacketViolationWarningPacket.hpp"
#include "sculk/protocol/codec/packet/PartyChangedPacket.hpp"
#include "sculk/protocol/codec/packet/PartyDestinationCookieResponsePacket.hpp"
#include "sculk/protocol/codec/packet/PhotoTransferPacket.hpp"
#include "sculk/protocol/codec/packet/PlaySoundPacket.hpp"
#include "sculk/protocol/codec/packet/PlayStatusPacket.hpp"
#include "sculk/protocol/codec/packet/PlayerActionPacket.hpp"
#include "sculk/protocol/codec/packet/PlayerArmorDamagePacket.hpp"
#include "sculk/protocol/codec/packet/PlayerAuthInputPacket.hpp"
#include "sculk/protocol/codec/packet/PlayerEnchantOptionsPacket.hpp"
#include "sculk/protocol/codec/packet/PlayerFogPacket.hpp"
#include "sculk/protocol/codec/packet/PlayerHotbarPacket.hpp"
#include "sculk/protocol/codec/packet/PlayerInputPacket.hpp"
#include "sculk/protocol/codec/packet/PlayerListPacket.hpp"
#include "sculk/protocol/codec/packet/PlayerLocationPacket.hpp"
#include "sculk/protocol/codec/packet/PlayerSkinPacket.hpp"
#include "sculk/protocol/codec/packet/PlayerStartItemCooldownPacket.hpp"
#include "sculk/protocol/codec/packet/PlayerToggleCrafterSlotRequestPacket.hpp"
#include "sculk/protocol/codec/packet/PlayerUpdateEntityOverridesPacket.hpp"
#include "sculk/protocol/codec/packet/PlayerVideoCapturePacket.hpp"
#include "sculk/protocol/codec/packet/PositionTrackingDBClientRequestPacket.hpp"
#include "sculk/protocol/codec/packet/PositionTrackingDBServerBroadcastPacket.hpp"
#include "sculk/protocol/codec/packet/PrimitiveShapesPacket.hpp"
#include "sculk/protocol/codec/packet/PurchaseReceiptPacket.hpp"
#include "sculk/protocol/codec/packet/RefreshEntitlementsPacket.hpp"
#include "sculk/protocol/codec/packet/RemoveActorPacket.hpp"
#include "sculk/protocol/codec/packet/RemoveObjectivePacket.hpp"
#include "sculk/protocol/codec/packet/RemoveVolumeEntityPacket.hpp"
#include "sculk/protocol/codec/packet/RequestAbilityPacket.hpp"
#include "sculk/protocol/codec/packet/RequestChunkRadiusPacket.hpp"
#include "sculk/protocol/codec/packet/RequestNetworkSettingsPacket.hpp"
#include "sculk/protocol/codec/packet/RequestPermissionsPacket.hpp"
#include "sculk/protocol/codec/packet/ResourcePackChunkDataPacket.hpp"
#include "sculk/protocol/codec/packet/ResourcePackChunkRequestPacket.hpp"
#include "sculk/protocol/codec/packet/ResourcePackClientResponsePacket.hpp"
#include "sculk/protocol/codec/packet/ResourcePackDataInfoPacket.hpp"
#include "sculk/protocol/codec/packet/ResourcePackStackPacket.hpp"
#include "sculk/protocol/codec/packet/ResourcePacksInfoPacket.hpp"
#include "sculk/protocol/codec/packet/ResourcePacksReadyForValidationPacket.hpp"
#include "sculk/protocol/codec/packet/RespawnPacket.hpp"
#include "sculk/protocol/codec/packet/ScriptMessagePacket.hpp"
#include "sculk/protocol/codec/packet/SendPartyDestinationCookiePacket.hpp"
#include "sculk/protocol/codec/packet/ServerPlayerPostMovePositionPacket.hpp"
#include "sculk/protocol/codec/packet/ServerPresenceInfoPacket.hpp"
#include "sculk/protocol/codec/packet/ServerSettingsRequestPacket.hpp"
#include "sculk/protocol/codec/packet/ServerSettingsResponsePacket.hpp"
#include "sculk/protocol/codec/packet/ServerStatsPacket.hpp"
#include "sculk/protocol/codec/packet/ServerStoreInfoPacket.hpp"
#include "sculk/protocol/codec/packet/ServerToClientHandshakePacket.hpp"
#include "sculk/protocol/codec/packet/ServerboundDataDrivenScreenClosedPacket.hpp"
#include "sculk/protocol/codec/packet/ServerboundDataStorePacket.hpp"
#include "sculk/protocol/codec/packet/ServerboundDiagnosticsPacket.hpp"
#include "sculk/protocol/codec/packet/ServerboundLoadingScreenPacket.hpp"
#include "sculk/protocol/codec/packet/ServerboundPackSettingChangePacket.hpp"
#include "sculk/protocol/codec/packet/SetActorDataPacket.hpp"
#include "sculk/protocol/codec/packet/SetActorLinkPacket.hpp"
#include "sculk/protocol/codec/packet/SetActorMotionPacket.hpp"
#include "sculk/protocol/codec/packet/SetCommandsEnabledPacket.hpp"
#include "sculk/protocol/codec/packet/SetDefaultGameTypePacket.hpp"
#include "sculk/protocol/codec/packet/SetDifficultyPacket.hpp"
#include "sculk/protocol/codec/packet/SetDisplayObjectivePacket.hpp"
#include "sculk/protocol/codec/packet/SetHealthPacket.hpp"
#include "sculk/protocol/codec/packet/SetHudPacket.hpp"
#include "sculk/protocol/codec/packet/SetLastHurtByPacket.hpp"
#include "sculk/protocol/codec/packet/SetLocalPlayerAsInitializedPacket.hpp"
#include "sculk/protocol/codec/packet/SetPlayerGameTypePacket.hpp"
#include "sculk/protocol/codec/packet/SetPlayerInventoryOptionsPacket.hpp"
#include "sculk/protocol/codec/packet/SetScorePacket.hpp"
#include "sculk/protocol/codec/packet/SetScoreboardIdentityPacket.hpp"
#include "sculk/protocol/codec/packet/SetSpawnPositionPacket.hpp"
#include "sculk/protocol/codec/packet/SetTimePacket.hpp"
#include "sculk/protocol/codec/packet/SetTitlePacket.hpp"
#include "sculk/protocol/codec/packet/SettingsCommandPacket.hpp"
#include "sculk/protocol/codec/packet/ShowCreditsPacket.hpp"
#include "sculk/protocol/codec/packet/ShowProfilePacket.hpp"
#include "sculk/protocol/codec/packet/ShowStoreOfferPacket.hpp"
#include "sculk/protocol/codec/packet/SimpleEventPacket.hpp"
#include "sculk/protocol/codec/packet/SimulationTypePacket.hpp"
#include "sculk/protocol/codec/packet/SpawnExperienceOrbPacket.hpp"
#include "sculk/protocol/codec/packet/SpawnParticleEffectPacket.hpp"
#include "sculk/protocol/codec/packet/StartGamePacket.hpp"
#include "sculk/protocol/codec/packet/StopSoundPacket.hpp"
#include "sculk/protocol/codec/packet/StructureBlockUpdatePacket.hpp"
#include "sculk/protocol/codec/packet/StructureTemplateDataRequestPacket.hpp"
#include "sculk/protocol/codec/packet/StructureTemplateDataResponsePacket.hpp"
#include "sculk/protocol/codec/packet/SubChunkPacket.hpp"
#include "sculk/protocol/codec/packet/SubChunkRequestPacket.hpp"
#include "sculk/protocol/codec/packet/SubClientLoginPacket.hpp"
#include "sculk/protocol/codec/packet/SyncActorPropertyPacket.hpp"
#include "sculk/protocol/codec/packet/SyncWorldClocksPacket.hpp"
#include "sculk/protocol/codec/packet/TakeItemActorPacket.hpp"
#include "sculk/protocol/codec/packet/TextPacket.hpp"
#include "sculk/protocol/codec/packet/TickingAreasLoadStatusPacket.hpp"
#include "sculk/protocol/codec/packet/ToastRequestPacket.hpp"
#include "sculk/protocol/codec/packet/TransferPacket.hpp"
#include "sculk/protocol/codec/packet/TrimDataPacket.hpp"
#include "sculk/protocol/codec/packet/UnlockedRecipesPacket.hpp"
#include "sculk/protocol/codec/packet/UpdateAbilitiesPacket.hpp"
#include "sculk/protocol/codec/packet/UpdateAdventureSettingsPacket.hpp"
#include "sculk/protocol/codec/packet/UpdateAttributesPacket.hpp"
#include "sculk/protocol/codec/packet/UpdateBlockPacket.hpp"
#include "sculk/protocol/codec/packet/UpdateBlockSyncedPacket.hpp"
#include "sculk/protocol/codec/packet/UpdateClientInputLocksPacket.hpp"
#include "sculk/protocol/codec/packet/UpdateClientOptionsPacket.hpp"
#include "sculk/protocol/codec/packet/UpdateEquipPacket.hpp"
#include "sculk/protocol/codec/packet/UpdatePlayerGameTypePacket.hpp"
#include "sculk/protocol/codec/packet/UpdateSoftEnumPacket.hpp"
#include "sculk/protocol/codec/packet/UpdateSubChunkBlocksPacket.hpp"
#include "sculk/protocol/codec/packet/UpdateTradePacket.hpp"
#include "sculk/protocol/codec/packet/VoxelShapesPacket.hpp"

namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE {

// clang-format off
#define CREATE_PACKET_SWITCH(PACKET_ID) switch (PACKET_ID) {
#define CREATE_PACKET(PACKET_ID)                                                                                       \
    case MinecraftPacketIds::PACKET_ID: {                                                                              \
        return std::make_unique<PACKET_ID##Packet>();                                                                  \
    }
#ifdef SCULK_PROTOCOL_ENABLE_DETAIL_ERRORS
#define CREATE_PACKET_DEFAULT(PACKET_ID)                                                                               \
    default: {                                                                                                         \
        return error_utils::makeError(std::format("Invalid packet ID: {}", std::to_underlying(PACKET_ID)));            \
    }                                                                                                                  \
    }
#define MAKE_ERROR(MESSAGE, RET) error_utils::makeError(std::format(MESSAGE ": {}", RET.error().mMessage))
#else
#define CREATE_PACKET_DEFAULT(PACKET_ID)                                                                               \
    default: {                                                                                                         \
        return error_utils::makeError("Invalid packet ID");                                                            \
    }                                                                                                                  \
    }
#define MAKE_ERROR(MESSAGE, RET) error_utils::makeError(MESSAGE)
#endif
#define DEPRECATED_PACKET(PACKET_ID)
// clang-format on

Result<MinecraftPackets::PacketHeader> MinecraftPackets::readPacketHeader(ReadOnlyBinaryStream& stream) {
    std::uint32_t header{};
    if (auto status = stream.readUnsignedVarInt(header); !status) {
        return MAKE_ERROR("Failed to read packet header", status);
    }
    return PacketHeader{
        .mPacketId          = static_cast<MinecraftPacketIds>(header & 0x3FF),
        .mSenderSubClientId = static_cast<std::uint8_t>((header >> 10) & 3),
        .mTargetSubClientId = static_cast<std::uint8_t>((header >> 12) & 3)
    };
}

void MinecraftPackets::writePacketHeader(BinaryStream& stream, const PacketHeader& header) {
    stream.writeUnsignedVarInt(
        (static_cast<int>(header.mPacketId) & 0x3FF) | ((header.mSenderSubClientId & 3) << 10)
        | ((header.mTargetSubClientId & 3) << 12)
    );
}

Result<std::unique_ptr<IPacket>> MinecraftPackets::createPacket(const PacketHeader& header) {
    auto packet = createPacket(header.mPacketId);
    if (packet) {
        (*packet)->mSenderSubClientId = header.mSenderSubClientId;
        (*packet)->mTargetSubClientId = header.mTargetSubClientId;
    }
    return packet;
}

Result<std::unique_ptr<IPacket>> MinecraftPackets::readAndCreatePacketFromStream(ReadOnlyBinaryStream& stream) {
    auto header = readPacketHeader(stream);
    if (!header) {
        return MAKE_ERROR("Failed to read packet header", header);
    }
    auto packet = createPacket(*header);
    if (!packet) {
        return MAKE_ERROR("Failed to create packet", packet);
    }
    if (auto status = (*packet)->read(stream); !status) {
#ifdef SCULK_PROTOCOL_ENABLE_DETAIL_ERRORS
        return error_utils::makeError(
            std::format(
                "Failed to read {}({}) from stream: {}",
                (*packet)->getName(),
                std::to_underlying((*packet)->getId()),
                status.error().mMessage
            )
        );
#else
        return error_utils::makeError("Failed to read packet from stream");
#endif
    }
    return packet;
}

Result<std::unique_ptr<IPacket>> MinecraftPackets::readAndCreatePacketFromBuffer(std::span<const std::byte> buffer) {
    auto stream = ReadOnlyBinaryStream{buffer};
    auto packet = readAndCreatePacketFromStream(stream);
    if (!packet) {
        return MAKE_ERROR("Failed to create packet from buffer", packet);
    }
    if (stream.hasDataLeft()) {
        return error_utils::makeError("Extra data left in buffer after reading packet");
    }
    return packet;
}

Result<std::unique_ptr<IPacket>>
MinecraftPackets::readAndCreatePacketFromHeader(const PacketHeader& header, ReadOnlyBinaryStream& stream) {
    auto packet = createPacket(header);
    if (!packet) {
        return packet;
    }
    if (auto status = (*packet)->read(stream); !status) {
        return MAKE_ERROR("Failed to read packet from stream", status);
    }
    if (stream.hasDataLeft()) {
        return error_utils::makeError("Extra data left in buffer after reading packet");
    }
    return packet;
}

Result<> MinecraftPackets::readPacket(IPacket& packet, ReadOnlyBinaryStream& stream) {
    auto readResult = packet.read(stream);
    if (!readResult) {
        return readResult;
    }
    if (stream.hasDataLeft()) {
        return error_utils::makeError("Extra data left in buffer after reading packet");
    }
    return {};
}

Result<std::unique_ptr<IPacket>> MinecraftPackets::createPacket(MinecraftPacketIds packetId) {
    CREATE_PACKET_SWITCH(packetId)
    CREATE_PACKET(Login)                                  // 1
    CREATE_PACKET(PlayStatus)                             // 2
    CREATE_PACKET(ServerToClientHandshake)                // 3
    CREATE_PACKET(ClientToServerHandshake)                // 4
    CREATE_PACKET(Disconnect)                             // 5
    CREATE_PACKET(ResourcePacksInfo)                      // 6
    CREATE_PACKET(ResourcePackStack)                      // 7
    CREATE_PACKET(ResourcePackClientResponse)             // 8
    CREATE_PACKET(Text)                                   // 9
    CREATE_PACKET(SetTime)                                // 10
    CREATE_PACKET(StartGame)                              // 11
    CREATE_PACKET(AddPlayer)                              // 12
    CREATE_PACKET(AddActor)                               // 13
    CREATE_PACKET(RemoveActor)                            // 14
    CREATE_PACKET(AddItemActor)                           // 15
    CREATE_PACKET(ServerPlayerPostMovePosition)           // 16
    CREATE_PACKET(TakeItemActor)                          // 17
    CREATE_PACKET(MoveActorAbsolute)                      // 18
    CREATE_PACKET(MovePlayer)                             // 19
    DEPRECATED_PACKET(PassengerJump)                      // 20
    CREATE_PACKET(UpdateBlock)                            // 21
    CREATE_PACKET(AddPainting)                            // 22
    DEPRECATED_PACKET(TickSync)                           // 23
    DEPRECATED_PACKET(LevelSoundEventV1)                  // 24
    CREATE_PACKET(LevelEvent)                             // 25
    CREATE_PACKET(BlockEvent)                             // 26
    CREATE_PACKET(ActorEvent)                             // 27
    CREATE_PACKET(MobEffect)                              // 28
    CREATE_PACKET(UpdateAttributes)                       // 29
    CREATE_PACKET(InventoryTransaction)                   // 30
    CREATE_PACKET(MobEquipment)                           // 31
    CREATE_PACKET(MobArmorEquipment)                      // 32
    CREATE_PACKET(Interact)                               // 33
    CREATE_PACKET(BlockPickRequest)                       // 34
    CREATE_PACKET(ActorPickRequest)                       // 35
    CREATE_PACKET(PlayerAction)                           // 36
    DEPRECATED_PACKET(ActorFall)                          // 37
    CREATE_PACKET(HurtArmor)                              // 38
    CREATE_PACKET(SetActorData)                           // 39
    CREATE_PACKET(SetActorMotion)                         // 40
    CREATE_PACKET(SetActorLink)                           // 41
    CREATE_PACKET(SetHealth)                              // 42
    CREATE_PACKET(SetSpawnPosition)                       // 43
    CREATE_PACKET(Animate)                                // 44
    CREATE_PACKET(Respawn)                                // 45
    CREATE_PACKET(ContainerOpen)                          // 46
    CREATE_PACKET(ContainerClose)                         // 47
    CREATE_PACKET(PlayerHotbar)                           // 48
    CREATE_PACKET(InventoryContent)                       // 49
    CREATE_PACKET(InventorySlot)                          // 50
    CREATE_PACKET(ContainerSetData)                       // 51
    CREATE_PACKET(CraftingData)                           // 52
    DEPRECATED_PACKET(CraftingEvent)                      // 53
    CREATE_PACKET(GuiDataPickItem)                        // 54
    DEPRECATED_PACKET(AdventureSettings)                  // 55
    CREATE_PACKET(BlockActorData)                         // 56
    DEPRECATED_PACKET(PlayerInput)                        // 57
    CREATE_PACKET(LevelChunk)                             // 58
    CREATE_PACKET(SetCommandsEnabled)                     // 59
    CREATE_PACKET(SetDifficulty)                          // 60
    CREATE_PACKET(ChangeDimension)                        // 61
    CREATE_PACKET(SetPlayerGameType)                      // 62
    CREATE_PACKET(PlayerList)                             // 63
    CREATE_PACKET(SimpleEvent)                            // 64
    CREATE_PACKET(LegacyTelemetryEvent)                   // 65
    CREATE_PACKET(SpawnExperienceOrb)                     // 66
    CREATE_PACKET(ClientboundMapItemData)                 // 67
    CREATE_PACKET(MapInfoRequest)                         // 68
    CREATE_PACKET(RequestChunkRadius)                     // 69
    CREATE_PACKET(ChunkRadiusUpdated)                     // 70
    DEPRECATED_PACKET(ItemFrameDropItem)                  // 71
    CREATE_PACKET(GameRulesChanged)                       // 72
    CREATE_PACKET(Camera)                                 // 73
    CREATE_PACKET(BossEvent)                              // 74
    CREATE_PACKET(ShowCredits)                            // 75
    CREATE_PACKET(AvailableCommands)                      // 76
    CREATE_PACKET(CommandRequest)                         // 77
    CREATE_PACKET(CommandBlockUpdate)                     // 78
    CREATE_PACKET(CommandOutput)                          // 79
    CREATE_PACKET(UpdateTrade)                            // 80
    CREATE_PACKET(UpdateEquip)                            // 81
    CREATE_PACKET(ResourcePackDataInfo)                   // 82
    CREATE_PACKET(ResourcePackChunkData)                  // 83
    CREATE_PACKET(ResourcePackChunkRequest)               // 84
    CREATE_PACKET(Transfer)                               // 85
    CREATE_PACKET(PlaySound)                              // 86
    CREATE_PACKET(StopSound)                              // 87
    CREATE_PACKET(SetTitle)                               // 88
    CREATE_PACKET(AddBehaviorTree)                        // 89
    CREATE_PACKET(StructureBlockUpdate)                   // 90
    CREATE_PACKET(ShowStoreOffer)                         // 91
    CREATE_PACKET(PurchaseReceipt)                        // 92
    CREATE_PACKET(PlayerSkin)                             // 93
    CREATE_PACKET(SubClientLogin)                         // 94
    CREATE_PACKET(AutomationClientConnect)                // 95
    CREATE_PACKET(SetLastHurtBy)                          // 96
    CREATE_PACKET(BookEdit)                               // 97
    CREATE_PACKET(NpcRequest)                             // 98
    CREATE_PACKET(PhotoTransfer)                          // 99
    CREATE_PACKET(ModalFormRequest)                       // 100
    CREATE_PACKET(ModalFormResponse)                      // 101
    CREATE_PACKET(ServerSettingsRequest)                  // 102
    CREATE_PACKET(ServerSettingsResponse)                 // 103
    CREATE_PACKET(ShowProfile)                            // 104
    CREATE_PACKET(SetDefaultGameType)                     // 105
    CREATE_PACKET(RemoveObjective)                        // 106
    CREATE_PACKET(SetDisplayObjective)                    // 107
    CREATE_PACKET(SetScore)                               // 108
    CREATE_PACKET(LabTable)                               // 109
    CREATE_PACKET(UpdateBlockSynced)                      // 110
    CREATE_PACKET(MoveActorDelta)                         // 111
    CREATE_PACKET(SetScoreboardIdentity)                  // 112
    CREATE_PACKET(SetLocalPlayerAsInitialized)            // 113
    CREATE_PACKET(UpdateSoftEnum)                         // 114
    CREATE_PACKET(NetworkStackLatency)                    // 115
    DEPRECATED_PACKET(BlockPalette)                       // 116
    DEPRECATED_PACKET(ScriptCustomEvent)                  // 117
    CREATE_PACKET(SpawnParticleEffect)                    // 118
    CREATE_PACKET(AvailableActorIdentifiers)              // 119
    DEPRECATED_PACKET(LevelSoundEventV2)                  // 120
    CREATE_PACKET(NetworkChunkPublisherUpdate)            // 121
    CREATE_PACKET(BiomeDefinitionList)                    // 122
    CREATE_PACKET(LevelSoundEvent)                        // 123
    CREATE_PACKET(LevelEventGeneric)                      // 124
    CREATE_PACKET(LecternUpdate)                          // 125
    DEPRECATED_PACKET(VideoStreamConnect)                 // 126
    DEPRECATED_PACKET(AddEntity)                          // 127
    DEPRECATED_PACKET(RemoveEntity)                       // 128
    CREATE_PACKET(ClientCacheStatus)                      // 129
    CREATE_PACKET(OnScreenTextureAnimation)               // 130
    CREATE_PACKET(MapCreateLockedCopy)                    // 131
    CREATE_PACKET(StructureTemplateDataRequest)           // 132
    CREATE_PACKET(StructureTemplateDataResponse)          // 133
    DEPRECATED_PACKET(UnusedPlsUseMe)                     // 134
    CREATE_PACKET(ClientCacheBlobStatus)                  // 135
    CREATE_PACKET(ClientCacheMissResponse)                // 136
    CREATE_PACKET(EducationSettings)                      // 137
    CREATE_PACKET(Emote)                                  // 138
    CREATE_PACKET(MultiplayerSettings)                    // 139
    CREATE_PACKET(SettingsCommand)                        // 140
    CREATE_PACKET(AnvilDamage)                            // 141
    CREATE_PACKET(CompletedUsingItem)                     // 142
    CREATE_PACKET(NetworkSettings)                        // 143
    CREATE_PACKET(PlayerAuthInput)                        // 144
    CREATE_PACKET(CreativeContent)                        // 145
    CREATE_PACKET(PlayerEnchantOptions)                   // 146
    CREATE_PACKET(ItemStackRequest)                       // 147
    CREATE_PACKET(ItemStackResponse)                      // 148
    CREATE_PACKET(PlayerArmorDamage)                      // 149
    CREATE_PACKET(CodeBuilder)                            // 150
    CREATE_PACKET(UpdatePlayerGameType)                   // 151
    CREATE_PACKET(EmoteList)                              // 152
    CREATE_PACKET(PositionTrackingDBServerBroadcast)      // 153
    CREATE_PACKET(PositionTrackingDBClientRequest)        // 154
    CREATE_PACKET(DebugInfo)                              // 155
    CREATE_PACKET(PacketViolationWarning)                 // 156
    CREATE_PACKET(MotionPredictionHints)                  // 157
    CREATE_PACKET(AnimateEntity)                          // 158
    CREATE_PACKET(CameraShake)                            // 159
    CREATE_PACKET(PlayerFog)                              // 160
    CREATE_PACKET(CorrectPlayerMovePrediction)            // 161
    CREATE_PACKET(ItemRegistry)                           // 162
    DEPRECATED_PACKET(FilterText)                         // 163
    CREATE_PACKET(ClientboundDebugRenderer)               // 164
    CREATE_PACKET(SyncActorProperty)                      // 165
    CREATE_PACKET(AddVolumeEntity)                        // 166
    CREATE_PACKET(RemoveVolumeEntity)                     // 167
    CREATE_PACKET(SimulationType)                         // 168
    CREATE_PACKET(NpcDialogue)                            // 169
    CREATE_PACKET(EduUriResource)                         // 170
    CREATE_PACKET(CreatePhoto)                            // 171
    CREATE_PACKET(UpdateSubChunkBlocks)                   // 172
    DEPRECATED_PACKET(PhotoInfoRequest)                   // 173
    CREATE_PACKET(SubChunk)                               // 174
    CREATE_PACKET(SubChunkRequest)                        // 175
    CREATE_PACKET(PlayerStartItemCooldown)                // 176
    CREATE_PACKET(ScriptMessage)                          // 177
    CREATE_PACKET(CodeBuilderSource)                      // 178
    CREATE_PACKET(TickingAreasLoadStatus)                 // 179
    CREATE_PACKET(DimensionData)                          // 180
    CREATE_PACKET(AgentActionEvent)                       // 181
    CREATE_PACKET(ChangeMobProperty)                      // 182
    CREATE_PACKET(LessonProgress)                         // 183
    CREATE_PACKET(RequestAbility)                         // 184
    CREATE_PACKET(RequestPermissions)                     // 185
    CREATE_PACKET(ToastRequest)                           // 186
    CREATE_PACKET(UpdateAbilities)                        // 187
    CREATE_PACKET(UpdateAdventureSettings)                // 188
    CREATE_PACKET(DeathInfo)                              // 189
    CREATE_PACKET(EditorNetwork)                          // 190
    CREATE_PACKET(FeatureRegistry)                        // 191
    CREATE_PACKET(ServerStats)                            // 192
    CREATE_PACKET(RequestNetworkSettings)                 // 193
    CREATE_PACKET(GameTestRequest)                        // 194
    CREATE_PACKET(GameTestResults)                        // 195
    CREATE_PACKET(UpdateClientInputLocks)                 // 196
    DEPRECATED_PACKET(ClientCheatAbility)                 // 197
    CREATE_PACKET(CameraPresets)                          // 198
    CREATE_PACKET(UnlockedRecipes)                        // 199
    CREATE_PACKET(CameraInstruction)                      // 300
    DEPRECATED_PACKET(CompressedBiomeDefinitionList)      // 301
    CREATE_PACKET(TrimData)                               // 302
    CREATE_PACKET(OpenSign)                               // 303
    CREATE_PACKET(AgentAnimation)                         // 304
    CREATE_PACKET(RefreshEntitlements)                    // 305
    CREATE_PACKET(PlayerToggleCrafterSlotRequest)         // 306
    CREATE_PACKET(SetPlayerInventoryOptions)              // 307
    CREATE_PACKET(SetHud)                                 // 308
    CREATE_PACKET(AwardAchievement)                       // 309
    CREATE_PACKET(ClientboundCloseForm)                   // 310
    DEPRECATED_PACKET(ClientboundLoadingScreen)           // 311
    CREATE_PACKET(ServerboundLoadingScreen)               // 312
    CREATE_PACKET(JigsawStructureData)                    // 313
    CREATE_PACKET(CurrentStructureFeature)                // 314
    CREATE_PACKET(ServerboundDiagnostics)                 // 315
    CREATE_PACKET(CameraAimAssist)                        // 316
    CREATE_PACKET(ContainerRegistryCleanup)               // 317
    CREATE_PACKET(MovementEffect)                         // 318
    DEPRECATED_PACKET(SetMovementAuthority)               // 319
    CREATE_PACKET(CameraAimAssistPresets)                 // 320
    CREATE_PACKET(ClientCameraAimAssist)                  // 321
    CREATE_PACKET(ClientMovementPredictionSync)           // 322
    CREATE_PACKET(UpdateClientOptions)                    // 323
    CREATE_PACKET(PlayerVideoCapture)                     // 324
    CREATE_PACKET(PlayerUpdateEntityOverrides)            // 325
    CREATE_PACKET(PlayerLocation)                         // 326
    CREATE_PACKET(ClientboundControlSchemeSet)            // 327
    CREATE_PACKET(PrimitiveShapes)                        // 328
    CREATE_PACKET(ServerboundPackSettingChange)           // 329
    CREATE_PACKET(ClientboundDataStore)                   // 330
    CREATE_PACKET(GraphicsOverrideParameter)              // 331
    CREATE_PACKET(ServerboundDataStore)                   // 332
    CREATE_PACKET(ClientboundDataDrivenUIShowScreen)      // 333
    CREATE_PACKET(ClientboundDataDrivenUICloseAllScreens) // 334
    CREATE_PACKET(ClientboundDataDrivenUIReload)          // 335
    CREATE_PACKET(ClientboundTextureShift)                // 336
    CREATE_PACKET(VoxelShapes)                            // 337
    CREATE_PACKET(CameraSpline)                           // 338
    CREATE_PACKET(CameraAimAssistActorPriority)           // 339
    CREATE_PACKET(ResourcePacksReadyForValidation)        // 340
    CREATE_PACKET(LocatorBar)                             // 341
    CREATE_PACKET(PartyChanged)                           // 342
    CREATE_PACKET(ServerboundDataDrivenScreenClosed)      // 343
    CREATE_PACKET(SyncWorldClocks)                        // 344
    CREATE_PACKET(ClientboundAttributeLayerSync)          // 345
    CREATE_PACKET(ServerStoreInfo)                        // 346
    CREATE_PACKET(ServerPresenceInfo)                     // 347
    CREATE_PACKET(ClientboundUpdateSoundData)             // 348
    CREATE_PACKET(SendPartyDestinationCookie)             // 349
    CREATE_PACKET(PartyDestinationCookieResponse)         // 350
    CREATE_PACKET_DEFAULT(packetId)
}

} // namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE
