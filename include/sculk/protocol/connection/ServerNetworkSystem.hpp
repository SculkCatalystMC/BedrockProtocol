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
#include <optional>
#include <parallel_hashmap/phmap.h>
#include <shared_mutex>
#include <string>
#include <string_view>
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
}

class ServerNetworkSystem final {
public:
    using ConnectionEventCallback =
        std::function<void(const RakNet::RakNetGUID& guid, const RakNet::SystemAddress& address)>;
    using PacketReceiveCallback = std::function<
        void(const RakNet::RakNetGUID& guid, const RakNet::SystemAddress& address, std::unique_ptr<IPacket>&& packet)>;
    using PacketParseFailedCallback = std::function<void(
        const RakNet::RakNetGUID&    guid,
        const RakNet::SystemAddress& address,
        Session::Buffer&&            packet,
        std::string                  errorMessage
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
    [[nodiscard]] NetworkStartResult
    start(std::uint16_t ipv4Port, std::uint16_t ipv6Port, std::uint32_t maxConnections);

    [[nodiscard]] NetworkStartResult start(
        const std::string& ipv4Host,
        std::uint16_t      ipv4Port,
        const std::string& ipv6Host,
        std::uint16_t      ipv6Port,
        std::uint32_t      maxConnections
    );

    [[nodiscard]] NetworkStartResult start(std::uint16_t ipv4Port, std::uint32_t maxConnections);

    [[nodiscard]] NetworkStartResult
    start(const std::string& ipv4Host, std::uint16_t ipv4Port, std::uint32_t maxConnections);

    void setMotd(std::string_view motd);

    void updateAnnouncement() noexcept;

    void setMaxConnections(std::uint32_t maxConnections);

    void stop();

    [[nodiscard]] bool isRunning() const noexcept;

    [[nodiscard]] Result<NetworkStatus> getClientNetworkStatus(const RakNet::RakNetGUID& guid) const noexcept;

    [[nodiscard]] std::size_t getSessionsCount() const;

    [[nodiscard]] std::uint64_t getDroppedEventCallbackCount() const noexcept;

    Result<> setOnConnected(ConnectionEventCallback&& callback) noexcept;

    Result<> setOnDisconnected(ConnectionEventCallback&& callback) noexcept;

    Result<> setOnPacketReceive(PacketReceiveCallback&& callback) noexcept;

    Result<> setOnPacketParseFailed(PacketParseFailedCallback&& callback) noexcept;

    [[nodiscard]] std::weak_ptr<Session> getSession(const RakNet::RakNetGUID& guid) const noexcept;

    void disconnectClient(const RakNet::RakNetGUID& guid) noexcept;

private:
    struct RakPeerDeleter {
        void operator()(RakNet::RakPeerInterface* peer) const noexcept;
    };

    struct CallbackSet final {
        ConnectionEventCallback   mOnConnected{};
        ConnectionEventCallback   mOnDisconnected{};
        PacketReceiveCallback     mOnPacketReceive{};
        PacketParseFailedCallback mOnPacketParseFailed{};
    };

    struct SessionContext final {
        std::shared_ptr<Session>            mSession{};
        std::shared_ptr<thread::TaskStrand> mStrand{};
    };

    using SessionContextsMap =
        detail::ThreadSafeParallelFlatHashMap<RakNet::RakNetGUID, std::shared_ptr<SessionContext>>;

private:
    [[nodiscard]] bool ioTickOnce() noexcept;

    void scheduleIoPump() noexcept;

    void ioPumpTask() noexcept;

    void scheduleIoPumpAfter(std::chrono::milliseconds delay) noexcept;

    void waitForPendingIoJobs() noexcept;

    void waitForPendingDelayedWakeups() noexcept;

    template <typename F>
        requires std::invocable<F&> && std::is_nothrow_invocable_v<F&>
    bool submitIoJob(F&& job) noexcept {
        mPendingIoJobs.fetch_add(1, std::memory_order_acq_rel);

        const bool submitted = mThreadPool->submit([this, job = std::forward<F>(job)]() mutable noexcept {
            job();
            if (mPendingIoJobs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                mPendingIoJobs.notify_all();
            }
        });

        if (!submitted) {
            if (mPendingIoJobs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                mPendingIoJobs.notify_all();
            }
        }

        return submitted;
    }

    void processIncomingPacket(RakNet::Packet& packet);

    [[nodiscard]] std::shared_ptr<SessionContext> getSessionContext(const RakNet::RakNetGUID& guid) const noexcept;

    [[nodiscard]] std::shared_ptr<Session> getSessionShared(const RakNet::RakNetGUID& guid) const noexcept;

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
    std::atomic_bool                                          mRunning{false};
    std::atomic_bool                                          mIoPumpScheduled{false};
    std::atomic_uint32_t                                      mPendingIoJobs{0};
    std::atomic_uint32_t                                      mPendingDelayedWakeups{0};
    std::size_t                                               mFlushCursor{0};
    SessionContextsMap                                        mSessionContexts{};
    AtomicSharedPtr<CallbackSet>                              mCallbacks{};
};

} // namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE
