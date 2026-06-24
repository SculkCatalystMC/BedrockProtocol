// Copyright © 2026 SculkCatalystMC. All rights reserved.
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#pragma once
#include "NetworkStartResult.hpp"
#include "Session.hpp"
#include "sculk/protocol/codec/packet/IPacket.hpp"
#include "sculk/protocol/connection/thread/AtomicSharedPtr.hpp"
#include "sculk/protocol/connection/thread/TaskStrand.hpp"
#include "sculk/protocol/connection/thread/ThreadPool.hpp"
#include <RakPeerInterface.h>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <parallel_hashmap/phmap.h>
#include <queue>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE {

namespace detail {
template <
    class K,
    class V,
    class Hash  = phmap::priv::hash_default_hash<K>,
    class Eq    = phmap::priv::hash_default_eq<K>,
    class Alloc = phmap::priv::Allocator<phmap::priv::Pair<const K, V>>,
    size_t N    = 6>
using ThreadSafeParallelFlatHashMap = phmap::parallel_flat_hash_map<K, V, Hash, Eq, Alloc, N, std::shared_mutex>;

class RakPacketOwner;
} // namespace detail

class ServerNetworkSystem final {
public:
    using ConnectionEventCallback =
        std::function<void(const RakNet::RakNetGUID& guid, const RakNet::SystemAddress& address)>;
    using RawPacketReceiveCallback = std::function<
        bool(const RakNet::RakNetGUID& guid, const RakNet::SystemAddress& address, Session::Buffer& payload)>;
    using PacketReceiveCallback = std::function<
        void(const RakNet::RakNetGUID& guid, const RakNet::SystemAddress& address, std::unique_ptr<IPacket>&& packet)>;
    using PacketParseFailedCallback       = std::function<void(
        const RakNet::RakNetGUID&    guid,
        const RakNet::SystemAddress& address,
        MinecraftPacketIds           id,
        std::string_view             message
    )>;
    using TaskStrandBackpressurePolicy    = thread::TaskStrand::BackpressurePolicy;
    using RawIngressLimitExceededCallback = std::function<bool(
        const RakNet::RakNetGUID&    guid,
        const RakNet::SystemAddress& address,
        std::size_t                  packetBytes,
        std::size_t                  packetsInWindow
    )>;

public:
    explicit ServerNetworkSystem(std::size_t workerThreadCount = 0);
    explicit ServerNetworkSystem(thread::ThreadPool& threadPool);

    ServerNetworkSystem(const ServerNetworkSystem&)            = delete;
    ServerNetworkSystem& operator=(const ServerNetworkSystem&) = delete;
    ServerNetworkSystem(ServerNetworkSystem&&)                 = delete;
    ServerNetworkSystem& operator=(ServerNetworkSystem&&)      = delete;

    ~ServerNetworkSystem();

public:
    // Starts the server network system on the specified IPv4/IPv6 ports.
    [[nodiscard]] NetworkStartResult
    start(std::uint16_t ipv4Port, std::uint16_t ipv6Port, std::uint32_t maxConnections);

    // Starts the server network system with explicit bind addresses.
    [[nodiscard]] NetworkStartResult start(
        const std::string& ipv4Host,
        std::uint16_t      ipv4Port,
        const std::string& ipv6Host,
        std::uint16_t      ipv6Port,
        std::uint32_t      maxConnections
    );

    // Starts the server network system on IPv4 only.
    [[nodiscard]] NetworkStartResult start(std::uint16_t ipv4Port, std::uint32_t maxConnections);

    // Starts the server network system with an explicit IPv4 bind address.
    [[nodiscard]] NetworkStartResult
    start(const std::string& ipv4Host, std::uint16_t ipv4Port, std::uint32_t maxConnections);

    // Updates the server message of the day.
    void setMotd(std::string_view motd);

    // Pushes the latest announcement data to connected clients.
    void updateAnnouncement() noexcept;

    // Sets the maximum number of concurrent connections.
    void setMaxConnections(std::uint32_t maxConnections);

    // Sets the maximum number of ready sessions handled per flush tick.
    // This option can only be configured before start(), the default value is 512.
    Result<> setFlushReadyPerTick(std::size_t maxReadyPerTick) noexcept;

    // Returns the current maximum number of ready sessions handled per flush tick.
    [[nodiscard]] std::size_t getFlushReadyPerTick() const noexcept;

    // Stops the server network system and waits for in-flight work to finish.
    void stop();

    // Returns whether the server network system is currently running.
    [[nodiscard]] bool isRunning() const noexcept;

    // Returns the current status of the client identified by the given GUID.
    [[nodiscard]] Result<NetworkStatus> getClientNetworkStatus(const RakNet::RakNetGUID& guid) const noexcept;

    // Returns the number of active sessions.
    [[nodiscard]] std::size_t getSessionsCount() const;

    // Sets the callback invoked when a client connects.
    Result<> setOnConnected(ConnectionEventCallback&& callback) noexcept;

    // Sets the callback invoked when a client disconnects.
    Result<> setOnDisconnected(ConnectionEventCallback&& callback) noexcept;

    // Sets the callback invoked when a packet is received.
    Result<> setOnPacketReceive(PacketReceiveCallback&& callback) noexcept;

    // Sets the callback invoked when a raw packet is received. If this callback returns false, the packet will not be
    // processed further.
    Result<> setOnRawPacketReceive(RawPacketReceiveCallback&& callback) noexcept;

    // Sets the callback invoked when raw ingress limits are exceeded.
    // The callback return value decides whether the connection is disconnected.
    Result<> setOnRawIngressLimitExceeded(RawIngressLimitExceededCallback&& callback) noexcept;

    // Sets the callback invoked when packet parsing fails.
    Result<> setOnPacketParseFailed(PacketParseFailedCallback&& callback) noexcept;

    // Sets the backpressure policy used by newly created session strands.
    Result<> setOnTaskPressure(TaskStrandBackpressurePolicy&& callback) noexcept;

    // Sets the raw ingress limits used before packet parsing.
    // This option can only be configured before start().
    Result<> setRawIngressLimits(std::size_t maxPacketBytes, std::size_t maxPacketsPerSecond) noexcept;

    // Returns the maximum raw packet size accepted before parsing.
    [[nodiscard]] std::size_t getRawIngressMaxPacketBytes() const noexcept;

    // Returns the maximum number of raw packets accepted per second.
    [[nodiscard]] std::size_t getRawIngressMaxPacketsPerSecond() const noexcept;

    // Returns a weak reference to the session for the given client GUID.
    [[nodiscard]] std::weak_ptr<Session> getSession(const RakNet::RakNetGUID& guid) const noexcept;

    // Disconnects the specified client if it is connected.
    void disconnectClient(const RakNet::RakNetGUID& guid) noexcept;

private:
    struct RakPeerDeleter {
        void operator()(RakNet::RakPeerInterface* peer) const noexcept;
    };

    struct CallbackSet final {
        ConnectionEventCallback         mOnConnected{};
        ConnectionEventCallback         mOnDisconnected{};
        PacketReceiveCallback           mOnPacketReceive{};
        PacketParseFailedCallback       mOnPacketParseFailed{};
        RawPacketReceiveCallback        mOnRawPacketReceive{};
        RawIngressLimitExceededCallback mOnRawIngressLimitExceeded{};
    };

    struct SessionContext final {
        std::shared_ptr<Session>              mSession{};
        std::shared_ptr<thread::TaskStrand>   mStrand{};
        std::chrono::steady_clock::time_point mRawIngressWindowStart{};
        std::size_t                           mRawIngressWindowPackets{0};
    };

    struct FlushDueEntry final {
        std::chrono::steady_clock::time_point mDue{};
        RakNet::RakNetGUID                    mGuid{};
        std::uint64_t                         mToken{};
    };

    struct FlushDueEntryCompare final {
        bool operator()(const FlushDueEntry& lhs, const FlushDueEntry& rhs) const noexcept {
            if (lhs.mDue == rhs.mDue) {
                return lhs.mToken > rhs.mToken;
            }
            return lhs.mDue > rhs.mDue;
        }
    };

    using SessionContextsMap =
        detail::ThreadSafeParallelFlatHashMap<RakNet::RakNetGUID, std::shared_ptr<SessionContext>>;
    using FlushDueTokenMap = phmap::flat_hash_map<RakNet::RakNetGUID, std::uint64_t>;

private:
    [[nodiscard]] bool ioTickOnce() noexcept;

    [[nodiscard]] bool flushTickOnce() noexcept;

    void scheduleIoPump() noexcept;

    void ioPumpThreadMain(std::stop_token stopToken) noexcept;

    void scheduleFlushPump() noexcept;

    void flushPumpTask() noexcept;

    void scheduleFlushPumpAfter(std::chrono::milliseconds delay) noexcept;

    void waitForPendingFlushJobs() noexcept;

    void waitForPendingFlushWakeups() noexcept;

    void waitForPendingSessionPacketTasks() noexcept;

    template <typename F>
        requires std::invocable<F&> && std::is_nothrow_invocable_v<F&>
    bool submitFlushJob(F&& job) noexcept {
        mPendingFlushJobs.fetch_add(1, std::memory_order_acq_rel);

        const bool submitted = mThreadPool->submit([this, job = std::forward<F>(job)]() mutable noexcept {
            job();
            if (mPendingFlushJobs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                mPendingFlushJobs.notify_all();
            }
        });

        if (!submitted) {
            if (mPendingFlushJobs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                mPendingFlushJobs.notify_all();
            }
        }

        return submitted;
    }

    void processIncomingPacket(detail::RakPacketOwner packet);

    void processSessionPacket(
        const RakNet::RakNetGUID&                            key,
        const RakNet::SystemAddress&                         address,
        std::uint8_t                                         messageId,
        std::shared_ptr<ServerNetworkSystem::SessionContext> sessionContext,
        std::shared_ptr<ServerNetworkSystem::CallbackSet>    callbacks,
        detail::RakPacketOwner                               packetOwner
    ) noexcept;

    [[nodiscard]] std::shared_ptr<SessionContext> getSessionContext(const RakNet::RakNetGUID& guid) const noexcept;

    [[nodiscard]] std::shared_ptr<Session> getSessionShared(const RakNet::RakNetGUID& guid) const noexcept;

    void scheduleSessionFlushAt(const RakNet::RakNetGUID& guid, std::chrono::steady_clock::time_point due) noexcept;

    void cancelSessionFlush(const RakNet::RakNetGUID& guid) noexcept;

    void clearFlushSchedule() noexcept;

    void upsertSessionContext(const RakNet::RakNetGUID& key, std::shared_ptr<SessionContext> context);

    std::shared_ptr<SessionContext> removeSessionContext(const RakNet::RakNetGUID& key);

private:
    std::uint32_t                                             mMaxConnections{};
    std::uint16_t                                             mIpv4Port{};
    std::optional<std::uint16_t>                              mIpv6Port{};
    std::string                                               mMotd{};
    std::unique_ptr<RakNet::RakPeerInterface, RakPeerDeleter> mPeer{};
    std::unique_ptr<thread::ThreadPool>                       mOwnedThreadPool{};
    thread::ThreadPool*                                       mThreadPool{};
    std::jthread                                              mIoPumpThread{};
    std::atomic_bool                                          mRunning{};
    std::atomic_bool                                          mFlushPumpScheduled{};
    std::atomic_uint32_t                                      mPendingFlushJobs{};
    std::atomic_uint32_t                                      mPendingFlushWakeups{};
    std::atomic_uint32_t                                      mPendingSessionPacketTasks{};
    std::size_t                                               mFlushReadyPerTick{};
    std::size_t                                               mRawIngressMaxPacketBytes{};
    std::size_t                                               mRawIngressMaxPacketsPerSecond{};
    AtomicSharedPtr<TaskStrandBackpressurePolicy>             mTaskStrandBackpressurePolicy{};
    SessionContextsMap                                        mSessionContexts{};
    mutable std::mutex                                        mFlushScheduleMutex{};
    std::priority_queue<FlushDueEntry, std::vector<FlushDueEntry>, FlushDueEntryCompare> mFlushDueHeap{};
    FlushDueTokenMap                                                                     mFlushDueTokens{};
    std::uint64_t                                                                        mNextFlushToken{1};
    AtomicSharedPtr<CallbackSet>                                                         mCallbacks{};
};

} // namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE
