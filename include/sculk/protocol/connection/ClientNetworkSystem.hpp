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
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE {

class ClientNetworkSystem final {
public:
    using ConnectionEventCallback   = std::function<void()>;
    using PacketReceiveCallback     = std::function<void(std::unique_ptr<IPacket>&& packet)>;
    using PacketParseFailedCallback = std::function<void(Session::Buffer&& buffer, std::string errorMessage)>;

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
    [[nodiscard]] NetworkStartResult start();

    void stop();

    [[nodiscard]] ConnectionResult connect(std::string_view host, std::uint16_t port);

    void disconnect();

    [[nodiscard]] bool isRunning() const noexcept;

    [[nodiscard]] bool isConnected() const noexcept;

    [[nodiscard]] bool getServerNetworkStatus(NetworkStatus& outStatus) const noexcept;

    Result<> setOnConnected(ConnectionEventCallback&& callback) noexcept;

    Result<> setOnDisconnected(ConnectionEventCallback&& callback) noexcept;

    Result<> setOnConnectionFailed(ConnectionEventCallback&& callback) noexcept;

    Result<> setOnPacketReceive(PacketReceiveCallback&& callback) noexcept;

    Result<> setOnPacketParseFailed(PacketParseFailedCallback&& callback) noexcept;

    [[nodiscard]] std::weak_ptr<Session> getSession() const noexcept;

    [[nodiscard]] bool getNetworkStatus(NetworkStatus& outStatus) const noexcept;

    [[nodiscard]] std::uint64_t getDroppedEventCallbackCount() const noexcept;

private:
    struct RakPeerDeleter {
        void operator()(RakNet::RakPeerInterface* peer) const noexcept;
    };

    struct CallbackSet final {
        ConnectionEventCallback   mOnConnected{};
        ConnectionEventCallback   mOnDisconnected{};
        ConnectionEventCallback   mOnConnectionFailed{};
        PacketReceiveCallback     mOnPacketReceive{};
        PacketParseFailedCallback mOnPacketParseFailed{};
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
    thread::TaskStrand                                        mCallbackStrand{};
    std::atomic_bool                                          mRunning{false};
    std::atomic_bool                                          mIoPumpActive{false};
    std::atomic_bool                                          mIoPumpScheduled{false};
    std::atomic_uint32_t                                      mPendingIoJobs{0};
    std::atomic_uint32_t                                      mPendingDelayedWakeups{0};
    AtomicSharedPtr<Session>                                  mSession{};
    AtomicSharedPtr<CallbackSet>                              mCallbacks{};
};

} // namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE
