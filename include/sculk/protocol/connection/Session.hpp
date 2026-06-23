// Copyright © 2026 SculkCatalystMC. All rights reserved.
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#pragma once
#include "sculk/protocol/codec/level/CompressionAlgorithm.hpp"
#include "sculk/protocol/connection/NetworkStatus.hpp"
#include "sculk/protocol/connection/encryption/CryptoManager.hpp"
#include "sculk/protocol/utility/Enum.hpp"
#include "sculk/protocol/utility/Result.hpp"
#include <RakPeerInterface.h>
#include <atomic>
#include <chrono>
#include <concurrentqueue.h>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE {

class Session {
public:
    using Buffer          = std::vector<std::byte>;
    using BufferView      = std::span<const std::byte>;
    using BatchedBuffer   = std::vector<Buffer>;
    using OutboundBuffers = std::vector<Buffer>;

protected:
    RakNet::RakPeerInterface*             mPeer{};
    RakNet::AddressOrGUID                 mRemote{};
    moodycamel::ConcurrentQueue<Buffer>   mInboundPackets{};
    std::deque<Buffer>                    mOutboundPackets{};
    std::atomic_uint32_t                  mInboundQueuedPackets{0};
    std::atomic_uint32_t                  mOutboundQueuedPackets{0};
    std::atomic_uint64_t                  mInboundQueuedBytes{0};
    std::atomic_uint64_t                  mOutboundQueuedBytes{0};
    std::atomic_uint64_t                  mDroppedInboundPackets{0};
    std::atomic_uint64_t                  mDroppedOutboundPackets{0};
    std::atomic_bool                      mConnected{};
    std::optional<CompressionAlgorithm>   mCompressionType{};
    std::int32_t                          mCompressionThreshold{};
    std::optional<CryptoManager>          mEncryption{};
    std::chrono::steady_clock::time_point mNextFlushAt{};
    mutable std::mutex                    mMutex{};

public:
    static constexpr std::uint32_t MAX_INBOUND_QUEUED_PACKETS  = 4096;
    static constexpr std::uint32_t MAX_OUTBOUND_QUEUED_PACKETS = 4096;
    static constexpr std::uint64_t MAX_INBOUND_QUEUED_BYTES    = 8ULL * 1024ULL * 1024ULL;
    static constexpr std::uint64_t MAX_OUTBOUND_QUEUED_BYTES   = 8ULL * 1024ULL * 1024ULL;

public:
    explicit Session(RakNet::RakPeerInterface* peer, const RakNet::AddressOrGUID& remote) noexcept;

    Session(const Session&)            = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&)                 = delete;
    Session& operator=(Session&&)      = delete;

    ~Session();

public:
    [[nodiscard]] bool isCompressed() const noexcept;

    void setCompressed(CompressionAlgorithm type, std::int32_t threshold) noexcept;

    [[nodiscard]] bool isEncrypted() const noexcept;

    void setEncrypted(std::vector<std::byte>&& key) noexcept;

    bool sendPacket(Buffer&& buffer);

    bool sendPacket(BufferView buffer);

    bool sendPacketImmediately(Buffer&& buffer);

    bool sendPacketImmediately(BufferView buffer);

    [[nodiscard]] bool flush();

    [[nodiscard]] bool flushIfDue(std::chrono::steady_clock::time_point now) noexcept;

    [[nodiscard]] bool receivePacket(Buffer& outBuffer) noexcept;

    void disconnect() noexcept;

    [[nodiscard]] bool isConnected() const noexcept;

    [[nodiscard]] RakNet::RakNetGUID getGuid() const noexcept;

    [[nodiscard]] RakNet::SystemAddress getSystemAddress() const noexcept;

    [[nodiscard]] RakNet::AddressOrGUID getEndpoint() const noexcept;

    [[nodiscard]] NetworkStatus getNetworkStatus() const noexcept;

public:
    [[nodiscard]] bool sendBatchedBufferImmediately(Buffer&& packetsBuffer) noexcept;

    [[nodiscard]] bool hasPendingInboundPackets() const noexcept;

    [[nodiscard]] bool hasPendingOutboundPackets() const noexcept;

    [[nodiscard]] std::uint64_t droppedInboundPackets() const noexcept;

    [[nodiscard]] std::uint64_t droppedOutboundPackets() const noexcept;

    [[nodiscard]] bool enqueueInboundPacket(Buffer&& buffer) noexcept;

    [[nodiscard]] Result<Buffer> serializeBatchedPackets(const BatchedBuffer& packets);

    [[nodiscard]] Result<BatchedBuffer> deserializeBatchPackets(std::span<const std::byte> batchedBuffer);

private:
    [[nodiscard]] bool flushPendingBeforeStateChangeUnlocked() noexcept;

    [[nodiscard]] bool dequeueOutboundUnlocked(OutboundBuffers& outPackets) noexcept;
};

} // namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE
