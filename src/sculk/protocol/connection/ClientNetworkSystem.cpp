// Copyright © 2026 SculkCatalystMC. All rights reserved.
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#include "sculk/protocol/connection/ClientNetworkSystem.hpp"
#include "sculk/protocol/codec/MinecraftPackets.hpp"
#include "sculk/protocol/connection/detail/RakPacketOwner.hpp"
#include <MessageIdentifiers.h>
#include <chrono>
#include <functional>

namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE {

namespace {

constexpr std::uint8_t MINECRAFT_BATCH_PACKET_ID                  = 0xFE;
constexpr auto         IO_PUMP_IDLE_WAIT                          = std::chrono::milliseconds(20);
constexpr auto         RAW_INGRESS_WINDOW                         = std::chrono::seconds(1);
constexpr std::size_t  DEFAULT_RAW_INGRESS_MAX_PACKET_BYTES       = 8ULL * 1024ULL * 1024ULL;
constexpr std::size_t  DEFAULT_RAW_INGRESS_MAX_PACKETS_PER_SECOND = 2048;

} // namespace

ClientNetworkSystem::ClientNetworkSystem(std::size_t workerThreadCount)
: mPeer(RakNet::RakPeerInterface::GetInstance()),
  mOwnedThreadPool(std::make_unique<thread::ThreadPool>(workerThreadCount)),
  mThreadPool(mOwnedThreadPool.get()),
  mCallbackStrand(*mThreadPool),
  mRawIngressMaxPacketBytes(DEFAULT_RAW_INGRESS_MAX_PACKET_BYTES),
  mRawIngressMaxPacketsPerSecond(DEFAULT_RAW_INGRESS_MAX_PACKETS_PER_SECOND),
  mSession(std::shared_ptr<Session>{}),
  mCallbacks(std::make_shared<CallbackSet>()) {}

ClientNetworkSystem::ClientNetworkSystem(thread::ThreadPool& threadPool)
: mPeer(RakNet::RakPeerInterface::GetInstance()),
  mOwnedThreadPool(nullptr),
  mThreadPool(&threadPool),
  mCallbackStrand(*mThreadPool),
  mRawIngressMaxPacketBytes(DEFAULT_RAW_INGRESS_MAX_PACKET_BYTES),
  mRawIngressMaxPacketsPerSecond(DEFAULT_RAW_INGRESS_MAX_PACKETS_PER_SECOND),
  mSession(std::shared_ptr<Session>{}),
  mCallbacks(std::make_shared<CallbackSet>()) {}

ClientNetworkSystem::~ClientNetworkSystem() { stop(); }

NetworkStartResult ClientNetworkSystem::start() {
    if (mRunning.exchange(true, std::memory_order_acq_rel)) {
        return NetworkStartResult::AlreadyStarted;
    }

    std::array<RakNet::SocketDescriptor, 2> descriptor{};
    descriptor[0].socketFamily = AF_INET;
    descriptor[1].socketFamily = AF_INET6;

    const auto startupResult = mPeer->Startup(1, descriptor.data(), 2);
    if (startupResult != RakNet::RAKNET_STARTED) {
        mRunning.store(false, std::memory_order_release);
        return static_cast<NetworkStartResult>(startupResult);
    }

    mRawIngressWindowStart   = {};
    mRawIngressWindowPackets = 0;

    return NetworkStartResult::Success;
}

void ClientNetworkSystem::stop() {
    if (!mRunning.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    mIoPumpActive.store(false, std::memory_order_release);
    waitForPendingDelayedWakeups();
    waitForPendingIoJobs();

    auto session = mSession.load(std::memory_order_acquire);
    if (session) {
        session->disconnect();
        mSession.store(nullptr, std::memory_order_release);
    }

    if (mPeer) {
        mPeer->Shutdown(20);
    }
}

ClientNetworkSystem::ConnectionResult ClientNetworkSystem::connect(std::string_view host, std::uint16_t port) {
    if (!mRunning.load(std::memory_order_acquire)) {
        return ConnectionResult::NetworkNotStarted;
    }

    if (mSession.load(std::memory_order_acquire)) {
        return ConnectionResult::AlreadyConnectedToEndpoint;
    }

    const auto connectResult = mPeer->Connect(host.data(), port, nullptr, 0);
    if (connectResult != RakNet::ConnectionAttemptResult::CONNECTION_ATTEMPT_STARTED) {
        return static_cast<ConnectionResult>(connectResult);
    }

    mIoPumpActive.store(true, std::memory_order_release);
    scheduleIoPump();

    return ConnectionResult::ConnectionAttemptStarted;
}

void ClientNetworkSystem::disconnect() {
    mIoPumpActive.store(false, std::memory_order_release);
    waitForPendingDelayedWakeups();
    waitForPendingIoJobs();

    auto session = mSession.load(std::memory_order_acquire);
    if (session) {
        session->disconnect();
        mSession.store(nullptr, std::memory_order_release);
    }

    mRawIngressWindowStart   = {};
    mRawIngressWindowPackets = 0;
}

bool ClientNetworkSystem::isRunning() const noexcept { return mRunning.load(std::memory_order_acquire); }

bool ClientNetworkSystem::isConnected() const noexcept {
    auto session = mSession.load(std::memory_order_acquire);
    return session && session->isConnected();
}

bool ClientNetworkSystem::getServerNetworkStatus(NetworkStatus& outStatus) const noexcept {
    auto session = mSession.load(std::memory_order_acquire);
    if (!session) {
        return false;
    }

    outStatus = session->getNetworkStatus();
    return true;
}

Result<> ClientNetworkSystem::setOnConnected(ConnectionEventCallback&& callback) noexcept {
    if (mRunning.load(std::memory_order_acquire)) {
        return error_utils::makeError("Cannot set on connected callback while running");
    }

    auto callbacks                 = mCallbacks.load(std::memory_order_acquire);
    auto updatedCallbacks          = std::make_shared<CallbackSet>(*callbacks);
    updatedCallbacks->mOnConnected = std::move(callback);
    mCallbacks.store(std::move(updatedCallbacks), std::memory_order_release);
    return {};
}

Result<> ClientNetworkSystem::setOnDisconnected(ConnectionEventCallback&& callback) noexcept {
    if (mRunning.load(std::memory_order_acquire)) {
        return error_utils::makeError("Cannot set on disconnected callback while running");
    }

    auto callbacks                    = mCallbacks.load(std::memory_order_acquire);
    auto updatedCallbacks             = std::make_shared<CallbackSet>(*callbacks);
    updatedCallbacks->mOnDisconnected = std::move(callback);
    mCallbacks.store(std::move(updatedCallbacks), std::memory_order_release);
    return {};
}

Result<> ClientNetworkSystem::setOnConnectionFailed(ConnectionEventCallback&& callback) noexcept {
    if (mRunning.load(std::memory_order_acquire)) {
        return error_utils::makeError("Cannot set on connection failed callback while running");
    }

    auto callbacks                        = mCallbacks.load(std::memory_order_acquire);
    auto updatedCallbacks                 = std::make_shared<CallbackSet>(*callbacks);
    updatedCallbacks->mOnConnectionFailed = std::move(callback);
    mCallbacks.store(std::move(updatedCallbacks), std::memory_order_release);
    return {};
}

Result<> ClientNetworkSystem::setOnPacketReceive(PacketReceiveCallback&& callback) noexcept {
    if (mRunning.load(std::memory_order_acquire)) {
        return error_utils::makeError("Cannot set on packet receive callback while running");
    }

    auto callbacks                     = mCallbacks.load(std::memory_order_acquire);
    auto updatedCallbacks              = std::make_shared<CallbackSet>(*callbacks);
    updatedCallbacks->mOnPacketReceive = std::move(callback);
    mCallbacks.store(std::move(updatedCallbacks), std::memory_order_release);
    return {};
}

Result<> ClientNetworkSystem::setOnRawPacketReceive(RawPacketReceiveCallback&& callback) noexcept {
    if (mRunning.load(std::memory_order_acquire)) {
        return error_utils::makeError("Cannot set on raw packet receive callback while running");
    }

    auto callbacks = mCallbacks.load(std::memory_order_acquire);
    if (!callbacks->mOnPacketReceive) {
        return error_utils::makeError(
            "Cannot set on raw packet receive callback without setting on packet receive callback"
        );
    }

    auto updatedCallbacks                 = std::make_shared<CallbackSet>(*callbacks);
    updatedCallbacks->mOnRawPacketReceive = std::move(callback);
    mCallbacks.store(std::move(updatedCallbacks), std::memory_order_release);
    return {};
}

Result<> ClientNetworkSystem::setOnRawIngressLimitExceeded(RawIngressLimitExceededCallback&& callback) noexcept {
    if (mRunning.load(std::memory_order_acquire)) {
        return error_utils::makeError("Cannot set raw ingress limit callback while running");
    }

    auto callbacks                               = mCallbacks.load(std::memory_order_acquire);
    auto updatedCallbacks                        = std::make_shared<CallbackSet>(*callbacks);
    updatedCallbacks->mOnRawIngressLimitExceeded = std::move(callback);
    mCallbacks.store(std::move(updatedCallbacks), std::memory_order_release);
    return {};
}

Result<>
ClientNetworkSystem::setRawIngressLimits(std::size_t maxPacketBytes, std::size_t maxPacketsPerSecond) noexcept {
    if (mRunning.load(std::memory_order_acquire)) {
        return error_utils::makeError("Cannot set raw ingress limits while running");
    }

    if (maxPacketBytes == 0 || maxPacketsPerSecond == 0) {
        return error_utils::makeError("Raw ingress limits must be greater than zero");
    }

    mRawIngressMaxPacketBytes      = maxPacketBytes;
    mRawIngressMaxPacketsPerSecond = maxPacketsPerSecond;
    mRawIngressWindowStart         = {};
    mRawIngressWindowPackets       = 0;
    return {};
}

std::size_t ClientNetworkSystem::getRawIngressMaxPacketBytes() const noexcept { return mRawIngressMaxPacketBytes; }

std::size_t ClientNetworkSystem::getRawIngressMaxPacketsPerSecond() const noexcept {
    return mRawIngressMaxPacketsPerSecond;
}

Result<> ClientNetworkSystem::setOnPacketParseFailed(PacketParseFailedCallback&& callback) noexcept {
    if (mRunning.load(std::memory_order_acquire)) {
        return error_utils::makeError("Cannot set on packet parse failed callback while running");
    }
    auto callbacks = mCallbacks.load(std::memory_order_acquire);
    if (!callbacks->mOnPacketReceive) {
        return error_utils::makeError(
            "Cannot set on packet parse failed callback without setting on packet receive callback"
        );
    }

    auto updatedCallbacks                  = std::make_shared<CallbackSet>(*callbacks);
    updatedCallbacks->mOnPacketParseFailed = std::move(callback);
    mCallbacks.store(std::move(updatedCallbacks), std::memory_order_release);
    return {};
}

Result<> ClientNetworkSystem::setOnTaskPressure(TaskStrandBackpressurePolicy&& callback) noexcept {
    if (mRunning.load(std::memory_order_acquire)) {
        return error_utils::makeError("Cannot set backpressure policy while running");
    }

    mCallbackStrand.setBackpressurePolicy(std::move(callback));
    return {};
}

std::weak_ptr<Session> ClientNetworkSystem::getSession() const noexcept {
    return mSession.load(std::memory_order_acquire);
}

bool ClientNetworkSystem::getNetworkStatus(NetworkStatus& outStatus) const noexcept {
    return getServerNetworkStatus(outStatus);
}

bool ClientNetworkSystem::ioTickOnce() noexcept {
    bool progressed = false;

    while (detail::RakPacketOwner packet{mPeer.get(), mPeer->Receive()}) {
        processIncomingPacket(*packet.get());
        progressed = true;
    }

    auto session = mSession.load(std::memory_order_acquire);
    if (session) {
        progressed = session->flushIfDue(std::chrono::steady_clock::now()) || progressed;
    }

    return progressed;
}

void ClientNetworkSystem::scheduleIoPump() noexcept {
    if (!mRunning.load(std::memory_order_acquire) || !mIoPumpActive.load(std::memory_order_acquire)) {
        return;
    }

    bool expected = false;
    if (!mIoPumpScheduled.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    if (!submitIoJob([this]() noexcept { ioPumpTask(); })) {
        mIoPumpScheduled.store(false, std::memory_order_release);
    }
}

void ClientNetworkSystem::ioPumpTask() noexcept {
    if (!mRunning.load(std::memory_order_acquire) || !mIoPumpActive.load(std::memory_order_acquire)) {
        mIoPumpScheduled.store(false, std::memory_order_release);
        return;
    }

    const bool progressed = ioTickOnce();

    mIoPumpScheduled.store(false, std::memory_order_release);

    if (!mRunning.load(std::memory_order_acquire) || !mIoPumpActive.load(std::memory_order_acquire)) {
        return;
    }

    if (!progressed) {
        scheduleIoPumpAfter(IO_PUMP_IDLE_WAIT);
        return;
    }

    scheduleIoPump();
}

void ClientNetworkSystem::scheduleIoPumpAfter(std::chrono::milliseconds delay) noexcept {
    mPendingDelayedWakeups.fetch_add(1, std::memory_order_acq_rel);
    const bool submitted = mThreadPool->submitAfter(delay, [this]() noexcept {
        scheduleIoPump();
        if (mPendingDelayedWakeups.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            mPendingDelayedWakeups.notify_all();
        }
    });

    if (!submitted) {
        if (mPendingDelayedWakeups.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            mPendingDelayedWakeups.notify_all();
        }
    }
}

void ClientNetworkSystem::waitForPendingIoJobs() noexcept {
    auto pending = mPendingIoJobs.load(std::memory_order_acquire);
    while (pending != 0) {
        mPendingIoJobs.wait(pending, std::memory_order_acquire);
        pending = mPendingIoJobs.load(std::memory_order_acquire);
    }
}

void ClientNetworkSystem::waitForPendingDelayedWakeups() noexcept {
    auto pending = mPendingDelayedWakeups.load(std::memory_order_acquire);
    while (pending != 0) {
        mPendingDelayedWakeups.wait(pending, std::memory_order_acquire);
        pending = mPendingDelayedWakeups.load(std::memory_order_acquire);
    }
}

void ClientNetworkSystem::processIncomingPacket(RakNet::Packet& packet) {
    auto callbacks = mCallbacks.load(std::memory_order_acquire);

    auto resetIngressWindow = [this]() noexcept {
        mRawIngressWindowStart   = {};
        mRawIngressWindowPackets = 0;
    };

    auto disconnectSession = [this, &resetIngressWindow]() noexcept {
        auto session = mSession.load(std::memory_order_acquire);
        if (session) {
            session->disconnect();
            mSession.store(nullptr, std::memory_order_release);
        }
        resetIngressWindow();
    };

    if (!packet.data || packet.length == 0) {
        bool shouldDisconnect = true;
        if (callbacks->mOnRawIngressLimitExceeded) {
            shouldDisconnect = callbacks->mOnRawIngressLimitExceeded(0, 0);
        }

        if (shouldDisconnect) {
            disconnectSession();
        }
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (packet.length > mRawIngressMaxPacketBytes) {
        bool shouldDisconnect = true;
        if (callbacks->mOnRawIngressLimitExceeded) {
            shouldDisconnect = callbacks->mOnRawIngressLimitExceeded(
                static_cast<std::size_t>(packet.length),
                mRawIngressWindowPackets
            );
        }

        if (shouldDisconnect) {
            disconnectSession();
        }
        return;
    }

    if (mRawIngressWindowStart == std::chrono::steady_clock::time_point{}
        || now - mRawIngressWindowStart >= RAW_INGRESS_WINDOW) {
        mRawIngressWindowStart   = now;
        mRawIngressWindowPackets = 0;
    }

    if (mRawIngressWindowPackets >= mRawIngressMaxPacketsPerSecond) {
        bool shouldDisconnect = true;
        if (callbacks->mOnRawIngressLimitExceeded) {
            shouldDisconnect = callbacks->mOnRawIngressLimitExceeded(
                static_cast<std::size_t>(packet.length),
                mRawIngressWindowPackets
            );
        }

        if (shouldDisconnect) {
            disconnectSession();
        }
        return;
    }

    ++mRawIngressWindowPackets;

    const auto messageId = packet.data[0];

    if (messageId == DefaultMessageIDTypes::ID_CONNECTION_REQUEST_ACCEPTED) {
        auto session = std::make_shared<Session>(mPeer.get(), RakNet::AddressOrGUID{&packet});
        mSession.store(session, std::memory_order_release);
        resetIngressWindow();
        if (callbacks->mOnConnected) {
            const bool enqueued = mCallbackStrand.enqueue([callbacks]() { callbacks->mOnConnected(); });
            if (!enqueued) {
                callbacks->mOnConnected();
            }
        }
        return;
    }

    if (messageId == DefaultMessageIDTypes::ID_DISCONNECTION_NOTIFICATION
        || messageId == DefaultMessageIDTypes::ID_CONNECTION_LOST) {

        disconnectSession();

        if (callbacks->mOnDisconnected) {
            const bool enqueued = mCallbackStrand.enqueue([callbacks]() { callbacks->mOnDisconnected(); });
            if (!enqueued) {
                callbacks->mOnDisconnected();
            }
        }
        return;
    }

    if (messageId == DefaultMessageIDTypes::ID_CONNECTION_ATTEMPT_FAILED
        || messageId == DefaultMessageIDTypes::ID_NO_FREE_INCOMING_CONNECTIONS) {

        disconnectSession();

        if (callbacks->mOnConnectionFailed) {
            const bool enqueued = mCallbackStrand.enqueue([callbacks]() { callbacks->mOnConnectionFailed(); });
            if (!enqueued) {
                callbacks->mOnConnectionFailed();
            }
        }
        return;
    }

    if (messageId != MINECRAFT_BATCH_PACKET_ID || packet.length <= 1) {
        return;
    }

    auto session = mSession.load(std::memory_order_acquire);
    if (!session) {
        return;
    }

    const auto* payloadBegin = reinterpret_cast<const std::byte*>(packet.data + 1);
    const auto  payloadSize  = static_cast<std::size_t>(packet.length - 1);

    auto packets = session->deserializeBatchPackets(std::span<const std::byte>{payloadBegin, payloadSize});
    if (!packets) {
        return;
    }

    for (auto& payload : *packets) {
        if (callbacks->mOnPacketReceive) {
            if (callbacks->mOnRawPacketReceive && !callbacks->mOnRawPacketReceive(payload)) {
                continue;
            }

            ReadOnlyBinaryStream stream{payload};

            auto header = MinecraftPackets::readPacketHeader(stream);
            if (!header) {
                if (callbacks->mOnPacketParseFailed) {
                    callbacks->mOnPacketParseFailed(
                        static_cast<MinecraftPacketIds>(-1), // -1 as invalid packet ID
                        "Failed to read packet header"
                    );
                }
                continue;
            }

            auto packetExpected = MinecraftPackets::readAndCreatePacketFromHeader(*header, stream);
            if (packetExpected) {
                callbacks->mOnPacketReceive(std::move(*packetExpected));
            } else if (callbacks->mOnPacketParseFailed) {
                callbacks->mOnPacketParseFailed(header->mPacketId, packetExpected.error().mMessage);
            }
        } else {
            (void)session->enqueueInboundPacket(std::move(payload));
        }
    }
}

void ClientNetworkSystem::RakPeerDeleter::operator()(RakNet::RakPeerInterface* peer) const noexcept {
    if (peer) {
        RakNet::RakPeerInterface::DestroyInstance(peer);
    }
}

} // namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE
