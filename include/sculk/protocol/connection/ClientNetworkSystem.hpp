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
#include "sculk/protocol/connection/thread/TaskStrand.hpp"
#include "sculk/protocol/connection/thread/ThreadPool.hpp"
#include "thread/AtomicSharedPtr.hpp"
#include <RakPeerInterface.h>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE {

class ClientNetworkSystem final {
public:
    using ConnectionEventCallback         = std::function<void()>;
    using RawPacketReceiveCallback        = std::function<bool(Session::Buffer& payload)>;
    using PacketReceiveCallback           = std::function<void(std::unique_ptr<IPacket>&& packet)>;
    using PacketParseFailedCallback       = std::function<void(MinecraftPacketIds id, std::string_view message)>;
    using TaskStrandBackpressurePolicy    = thread::TaskStrand::BackpressurePolicy;
    using RawIngressLimitExceededCallback = std::function<bool(std::size_t packetBytes, std::size_t packetsInWindow)>;

public:
    explicit ClientNetworkSystem(std::size_t workerThreadCount = 0);
    explicit ClientNetworkSystem(thread::ThreadPool& threadPool);

    ClientNetworkSystem(const ClientNetworkSystem&)            = delete;
    ClientNetworkSystem& operator=(const ClientNetworkSystem&) = delete;
    ClientNetworkSystem(ClientNetworkSystem&&)                 = delete;
    ClientNetworkSystem& operator=(ClientNetworkSystem&&)      = delete;

    ~ClientNetworkSystem();

public:
    enum class ConnectionResult : std::uint8_t {
        ConnectionAttemptStarted           = 0,
        InvalidParameter                   = 1,
        CannotResolveDomainName            = 2,
        AlreadyConnectedToEndpoint         = 3,
        ConnectionAttemptAlreadyInProgress = 4,
        SecurityInitializationFailed       = 5,
        NetworkNotStarted                  = 6,
        UnknownError                       = 7,
    };

public:
    // Starts the client network system and initializes the RakNet peer.
    [[nodiscard]] NetworkStartResult start();

    // Stops the client network system and releases all pending work.
    void stop();

    // Initiates a connection attempt to the specified host and port.
    [[nodiscard]] ConnectionResult connect(std::string_view host, std::uint16_t port);

    // Disconnects the current session if one is active.
    void disconnect();

    // Returns whether the client network system is currently running.
    [[nodiscard]] bool isRunning() const noexcept;

    // Returns whether the client currently has an active session.
    [[nodiscard]] bool isConnected() const noexcept;

    // Copies the current server network status into the output parameter.
    [[nodiscard]] bool getServerNetworkStatus(NetworkStatus& outStatus) const noexcept;

    // Sets the callback invoked when the connection is established.
    Result<> setOnConnected(ConnectionEventCallback&& callback) noexcept;

    // Sets the callback invoked when the connection is closed.
    Result<> setOnDisconnected(ConnectionEventCallback&& callback) noexcept;

    // Sets the callback invoked when a connection attempt fails.
    Result<> setOnConnectionFailed(ConnectionEventCallback&& callback) noexcept;

    // Sets the callback invoked when a packet is received.
    Result<> setOnPacketReceive(PacketReceiveCallback&& callback) noexcept;

    // Sets the callback invoked when packet parsing fails.
    Result<> setOnPacketParseFailed(PacketParseFailedCallback&& callback) noexcept;

    // Sets the callback invoked when a raw packet is received. If this callback returns false, the packet will not be
    // processed further.
    Result<> setOnRawPacketReceive(RawPacketReceiveCallback&& callback) noexcept;

    // Sets the callback invoked when raw ingress limits are exceeded.
    // The callback return value decides whether the connection is disconnected.
    Result<> setOnRawIngressLimitExceeded(RawIngressLimitExceededCallback&& callback) noexcept;

    // Sets the raw ingress limits used before packet parsing.
    // This option can only be configured before start().
    Result<> setRawIngressLimits(std::size_t maxPacketBytes, std::size_t maxPacketsPerSecond) noexcept;

    // Returns the maximum raw packet size accepted before parsing.
    [[nodiscard]] std::size_t getRawIngressMaxPacketBytes() const noexcept;

    // Returns the maximum number of raw packets accepted per second.
    [[nodiscard]] std::size_t getRawIngressMaxPacketsPerSecond() const noexcept;

    // Sets the backpressure policy used by the callback strand.
    Result<> setOnTaskPressure(TaskStrandBackpressurePolicy&& callback) noexcept;

    // Returns a weak reference to the current session, if any.
    [[nodiscard]] std::weak_ptr<Session> getSession() const noexcept;

    // Copies the current network status into the output parameter.
    [[nodiscard]] bool getNetworkStatus(NetworkStatus& outStatus) const noexcept;

private:
    struct RakPeerDeleter {
        void operator()(RakNet::RakPeerInterface* peer) const noexcept;
    };

    struct CallbackSet final {
        ConnectionEventCallback         mOnConnected{};
        ConnectionEventCallback         mOnDisconnected{};
        ConnectionEventCallback         mOnConnectionFailed{};
        PacketReceiveCallback           mOnPacketReceive{};
        PacketParseFailedCallback       mOnPacketParseFailed{};
        RawPacketReceiveCallback        mOnRawPacketReceive{};
        RawIngressLimitExceededCallback mOnRawIngressLimitExceeded{};
    };

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

private:
    std::unique_ptr<RakNet::RakPeerInterface, RakPeerDeleter> mPeer{};
    std::unique_ptr<thread::ThreadPool>                       mOwnedThreadPool{};
    thread::ThreadPool*                                       mThreadPool{};
    thread::TaskStrand                                        mCallbackStrand;
    std::atomic_bool                                          mRunning{};
    std::atomic_bool                                          mIoPumpActive{};
    std::atomic_bool                                          mIoPumpScheduled{};
    std::atomic_uint32_t                                      mPendingIoJobs{};
    std::atomic_uint32_t                                      mPendingDelayedWakeups{};
    std::size_t                                               mRawIngressMaxPacketBytes{};
    std::size_t                                               mRawIngressMaxPacketsPerSecond{};
    std::chrono::steady_clock::time_point                     mRawIngressWindowStart{};
    std::size_t                                               mRawIngressWindowPackets{};
    AtomicSharedPtr<Session>                                  mSession{};
    AtomicSharedPtr<CallbackSet>                              mCallbacks{};
};

} // namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE
