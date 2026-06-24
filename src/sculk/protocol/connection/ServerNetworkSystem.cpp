// Copyright © 2026 SculkCatalystMC. All rights reserved.
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#include "sculk/protocol/connection/ServerNetworkSystem.hpp"
#include "sculk/protocol/codec/MinecraftPackets.hpp"
#include "sculk/protocol/connection/detail/RakPacketOwner.hpp"
#include <MessageIdentifiers.h>
#include <RakNetTypes.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <format>
#include <functional>
#include <string_view>
#include <thread>

namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE {

namespace {

constexpr std::uint8_t       MINECRAFT_BATCH_PACKET_ID                  = 0xFE;
constexpr auto               IO_PUMP_IDLE_WAIT                          = std::chrono::milliseconds(1);
constexpr auto               FLUSH_PUMP_IDLE_WAIT                       = std::chrono::milliseconds(1);
constexpr auto               FLUSH_INTERVAL                             = std::chrono::milliseconds(20);
constexpr auto               RAW_INGRESS_WINDOW                         = std::chrono::seconds(1);
static constexpr std::size_t DEFAULT_FLUSH_READY_PER_TICK               = 512;
constexpr std::size_t        DEFAULT_RAW_INGRESS_MAX_PACKET_BYTES       = 8ULL * 1024ULL * 1024ULL;
constexpr std::size_t        DEFAULT_RAW_INGRESS_MAX_PACKETS_PER_SECOND = 128;

[[nodiscard]] auto flushPhaseOffsetForGuid(const RakNet::RakNetGUID& guid) noexcept {
    const auto intervalMs = std::chrono::duration_cast<std::chrono::milliseconds>(FLUSH_INTERVAL).count();
    if (intervalMs <= 1) {
        return std::chrono::milliseconds::zero();
    }

    const auto guidString = guid.ToString();
    const auto hash       = std::hash<std::string_view>{}(guidString);
    return std::chrono::milliseconds(hash % static_cast<std::size_t>(intervalMs));
}

[[nodiscard]] auto nextScheduledFlushDue(
    std::chrono::steady_clock::time_point previousDue,
    std::chrono::steady_clock::time_point now
) noexcept {
    auto nextDue = previousDue + FLUSH_INTERVAL;
    if (nextDue > now) {
        return nextDue;
    }

    const auto lag     = now - nextDue;
    const auto skipped = (lag / FLUSH_INTERVAL) + 1;
    return nextDue + (FLUSH_INTERVAL * skipped);
}

} // namespace

ServerNetworkSystem::ServerNetworkSystem(std::size_t workerThreadCount)
: mMotd("Sculk Protocol Library"),
  mPeer(RakNet::RakPeerInterface::GetInstance()),
  mOwnedThreadPool(std::make_unique<thread::ThreadPool>(workerThreadCount)),
  mThreadPool(mOwnedThreadPool.get()),
  mCallbacks(std::make_shared<CallbackSet>()),
  mFlushReadyPerTick(DEFAULT_FLUSH_READY_PER_TICK),
  mRawIngressMaxPacketBytes(DEFAULT_RAW_INGRESS_MAX_PACKET_BYTES),
  mRawIngressMaxPacketsPerSecond(DEFAULT_RAW_INGRESS_MAX_PACKETS_PER_SECOND) {}

ServerNetworkSystem::ServerNetworkSystem(thread::ThreadPool& threadPool)
: mMotd("Sculk Protocol Library"),
  mPeer(RakNet::RakPeerInterface::GetInstance()),
  mOwnedThreadPool(nullptr),
  mThreadPool(&threadPool),
  mCallbacks(std::make_shared<CallbackSet>()),
  mFlushReadyPerTick(DEFAULT_FLUSH_READY_PER_TICK),
  mRawIngressMaxPacketBytes(DEFAULT_RAW_INGRESS_MAX_PACKET_BYTES),
  mRawIngressMaxPacketsPerSecond(DEFAULT_RAW_INGRESS_MAX_PACKETS_PER_SECOND) {}

ServerNetworkSystem::~ServerNetworkSystem() { stop(); }

NetworkStartResult
ServerNetworkSystem::start(std::uint16_t ipv4Port, std::uint16_t ipv6Port, std::uint32_t maxConnections) {
    if (mRunning.exchange(true, std::memory_order_acq_rel)) {
        return NetworkStartResult::AlreadyStarted;
    }

    mIpv4Port       = ipv4Port;
    mIpv6Port       = ipv6Port;
    mMaxConnections = maxConnections;

    std::array<RakNet::SocketDescriptor, 2> descriptors{
        RakNet::SocketDescriptor{mIpv4Port,  nullptr},
        RakNet::SocketDescriptor{*mIpv6Port, nullptr}
    };
    descriptors[0].socketFamily = AF_INET;
    descriptors[1].socketFamily = AF_INET6;

    auto status = mPeer->Startup(mMaxConnections, descriptors.data(), 2);
    if (status != RakNet::StartupResult::RAKNET_STARTED) {
        mRunning.store(false, std::memory_order_release);
        return static_cast<NetworkStartResult>(status);
    }

    mPeer->SetMaximumIncomingConnections(mMaxConnections);
    updateAnnouncement();

    clearFlushSchedule();
    scheduleIoPump();
    scheduleFlushPump();
    return NetworkStartResult::Success;
}

NetworkStartResult ServerNetworkSystem::start(
    const std::string& ipv4Host,
    std::uint16_t      ipv4Port,
    const std::string& ipv6Host,
    std::uint16_t      ipv6Port,
    std::uint32_t      maxConnections
) {
    if (mRunning.exchange(true, std::memory_order_acq_rel)) {
        return NetworkStartResult::AlreadyStarted;
    }

    mIpv4Port       = ipv4Port;
    mIpv6Port       = ipv6Port;
    mMaxConnections = maxConnections;

    std::array<RakNet::SocketDescriptor, 2> descriptors{
        RakNet::SocketDescriptor{mIpv4Port,  ipv4Host.c_str()},
        RakNet::SocketDescriptor{*mIpv6Port, ipv6Host.c_str()}
    };
    descriptors[0].socketFamily = AF_INET;
    descriptors[1].socketFamily = AF_INET6;

    auto status = mPeer->Startup(mMaxConnections, descriptors.data(), 2);
    if (status != RakNet::StartupResult::RAKNET_STARTED) {
        mRunning.store(false, std::memory_order_release);
        return static_cast<NetworkStartResult>(status);
    }

    mPeer->SetMaximumIncomingConnections(mMaxConnections);
    updateAnnouncement();

    clearFlushSchedule();
    scheduleIoPump();
    scheduleFlushPump();
    return NetworkStartResult::Success;
}

NetworkStartResult ServerNetworkSystem::start(std::uint16_t ipv4Port, std::uint32_t maxConnections) {
    if (mRunning.exchange(true, std::memory_order_acq_rel)) {
        return NetworkStartResult::AlreadyStarted;
    }

    mIpv4Port       = ipv4Port;
    mIpv6Port       = std::nullopt;
    mMaxConnections = maxConnections;

    RakNet::SocketDescriptor descriptor{mIpv4Port, nullptr};
    descriptor.socketFamily = AF_INET;

    auto status = mPeer->Startup(mMaxConnections, &descriptor, 1);
    if (status != RakNet::StartupResult::RAKNET_STARTED) {
        mRunning.store(false, std::memory_order_release);
        return static_cast<NetworkStartResult>(status);
    }

    mPeer->SetMaximumIncomingConnections(mMaxConnections);
    updateAnnouncement();

    clearFlushSchedule();
    scheduleIoPump();
    scheduleFlushPump();
    return NetworkStartResult::Success;
}

NetworkStartResult
ServerNetworkSystem::start(const std::string& ipv4Host, std::uint16_t ipv4Port, std::uint32_t maxConnections) {
    if (mRunning.exchange(true, std::memory_order_acq_rel)) {
        return NetworkStartResult::AlreadyStarted;
    }

    mIpv4Port       = ipv4Port;
    mIpv6Port       = std::nullopt;
    mMaxConnections = maxConnections;

    RakNet::SocketDescriptor descriptor{mIpv4Port, ipv4Host.c_str()};
    descriptor.socketFamily = AF_INET;

    auto status = mPeer->Startup(mMaxConnections, &descriptor, 1);
    if (status != RakNet::StartupResult::RAKNET_STARTED) {
        mRunning.store(false, std::memory_order_release);
        return static_cast<NetworkStartResult>(status);
    }

    mPeer->SetMaximumIncomingConnections(mMaxConnections);
    updateAnnouncement();

    clearFlushSchedule();
    scheduleIoPump();
    scheduleFlushPump();
    return NetworkStartResult::Success;
}

void ServerNetworkSystem::setMotd(std::string_view motd) {
    mMotd = motd;
    updateAnnouncement();
}

void ServerNetworkSystem::updateAnnouncement() noexcept {
    auto message = std::format(
        "MCPE;{0};{1};{2};{3};{4};{5};Sculk Sensor Proxy Server;Survival;1;{6};{7};0;",
        mMotd,
        getProtocolVersion(),
        getMinecraftVersion(),
        getSessionsCount(),
        mMaxConnections,
        mPeer->GetMyGUID().ToString(),
        mIpv4Port,
        mIpv6Port.value_or(0)
    );
    std::uint16_t length = static_cast<std::uint16_t>(message.size());
    message.insert(0, 2, '\0');
    message[0] = static_cast<char>((length >> 8) & 0xFF);
    message[1] = static_cast<char>(length & 0xFF);

    mPeer->SetOfflinePingResponse(message.data(), static_cast<unsigned int>(message.size()));
}

void ServerNetworkSystem::setMaxConnections(std::uint32_t maxConnections) {
    mMaxConnections = maxConnections;
    mPeer->SetMaximumIncomingConnections(mMaxConnections);
    updateAnnouncement();
}

Result<> ServerNetworkSystem::setFlushReadyPerTick(std::size_t maxReadyPerTick) noexcept {
    if (mRunning.load(std::memory_order_acquire)) {
        return error_utils::makeError("Cannot set flush ready-per-tick while running");
    }

    mFlushReadyPerTick = std::max<std::size_t>(1, maxReadyPerTick);
    return {};
}

std::size_t ServerNetworkSystem::getFlushReadyPerTick() const noexcept { return mFlushReadyPerTick; }

void ServerNetworkSystem::stop() {
    if (!mRunning.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    if (mIoPumpThread.joinable()) {
        mIoPumpThread.request_stop();
        mIoPumpThread.join();
    }

    waitForPendingFlushWakeups();
    waitForPendingFlushJobs();
    waitForPendingSessionPacketTasks();
    clearFlushSchedule();

    std::vector<std::shared_ptr<SessionContext>> contexts;
    contexts.reserve(mSessionContexts.size());
    mSessionContexts.for_each([&contexts](const SessionContextsMap::value_type& entry) {
        contexts.push_back(entry.second);
    });

    for (const auto& context : contexts) {
        context->mSession->disconnect();
    }

    std::vector<RakNet::RakNetGUID> contextKeys;
    contextKeys.reserve(mSessionContexts.size());
    mSessionContexts.for_each([&contextKeys](const SessionContextsMap::value_type& entry) {
        contextKeys.push_back(entry.first);
    });

    for (const auto& key : contextKeys) {
        (void)removeSessionContext(key);
    }

    if (mPeer) {
        mPeer->Shutdown(20);
    }
}

bool ServerNetworkSystem::isRunning() const noexcept { return mRunning.load(std::memory_order_acquire); }

Result<NetworkStatus> ServerNetworkSystem::getClientNetworkStatus(const RakNet::RakNetGUID& guid) const noexcept {
    auto session = getSessionShared(guid);
    if (!session) {
        return error_utils::makeError("Session not found");
    }

    return session->getNetworkStatus();
}

std::size_t ServerNetworkSystem::getSessionsCount() const { return mSessionContexts.size(); }

std::shared_ptr<ServerNetworkSystem::SessionContext>
ServerNetworkSystem::getSessionContext(const RakNet::RakNetGUID& guid) const noexcept {
    std::shared_ptr<SessionContext> context{};
    (void)mSessionContexts.if_contains(guid, [&context](const SessionContextsMap::value_type& entry) {
        context = entry.second;
    });
    return context;
}

std::shared_ptr<Session> ServerNetworkSystem::getSessionShared(const RakNet::RakNetGUID& guid) const noexcept {
    auto context = getSessionContext(guid);
    if (!context) {
        return nullptr;
    }

    return context->mSession;
}

void ServerNetworkSystem::scheduleSessionFlushAt(
    const RakNet::RakNetGUID&             guid,
    std::chrono::steady_clock::time_point due
) noexcept {
    std::lock_guard lock{mFlushScheduleMutex};
    const auto      token = mNextFlushToken++;
    mFlushDueTokens[guid] = token;
    mFlushDueHeap.push(
        FlushDueEntry{
            .mDue   = due,
            .mGuid  = guid,
            .mToken = token,
        }
    );
}

void ServerNetworkSystem::cancelSessionFlush(const RakNet::RakNetGUID& guid) noexcept {
    std::lock_guard lock{mFlushScheduleMutex};
    (void)mFlushDueTokens.erase(guid);
}

void ServerNetworkSystem::clearFlushSchedule() noexcept {
    std::lock_guard lock{mFlushScheduleMutex};
    while (!mFlushDueHeap.empty()) {
        mFlushDueHeap.pop();
    }
    mFlushDueTokens.clear();
    mNextFlushToken = 1;
}

Result<> ServerNetworkSystem::setOnConnected(ConnectionEventCallback&& callback) noexcept {
    if (mRunning.load(std::memory_order_acquire)) {
        return error_utils::makeError("Cannot set on connected callback while running");
    }

    auto callbacks                 = mCallbacks.load(std::memory_order_acquire);
    auto updatedCallbacks          = std::make_shared<CallbackSet>(*callbacks);
    updatedCallbacks->mOnConnected = std::move(callback);
    mCallbacks.store(std::move(updatedCallbacks), std::memory_order_release);
    return {};
}

Result<> ServerNetworkSystem::setOnDisconnected(ConnectionEventCallback&& callback) noexcept {
    if (mRunning.load(std::memory_order_acquire)) {
        return error_utils::makeError("Cannot set on disconnected callback while running");
    }

    auto callbacks                    = mCallbacks.load(std::memory_order_acquire);
    auto updatedCallbacks             = std::make_shared<CallbackSet>(*callbacks);
    updatedCallbacks->mOnDisconnected = std::move(callback);
    mCallbacks.store(std::move(updatedCallbacks), std::memory_order_release);
    return {};
}

Result<> ServerNetworkSystem::setOnPacketReceive(PacketReceiveCallback&& callback) noexcept {
    if (mRunning.load(std::memory_order_acquire)) {
        return error_utils::makeError("Cannot set on packet receive callback while running");
    }

    auto callbacks                     = mCallbacks.load(std::memory_order_acquire);
    auto updatedCallbacks              = std::make_shared<CallbackSet>(*callbacks);
    updatedCallbacks->mOnPacketReceive = std::move(callback);
    mCallbacks.store(std::move(updatedCallbacks), std::memory_order_release);
    return {};
}

Result<> ServerNetworkSystem::setOnRawPacketReceive(RawPacketReceiveCallback&& callback) noexcept {
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

Result<> ServerNetworkSystem::setOnRawIngressLimitExceeded(RawIngressLimitExceededCallback&& callback) noexcept {
    if (mRunning.load(std::memory_order_acquire)) {
        return error_utils::makeError("Cannot set raw ingress limit callback while running");
    }

    auto callbacks                               = mCallbacks.load(std::memory_order_acquire);
    auto updatedCallbacks                        = std::make_shared<CallbackSet>(*callbacks);
    updatedCallbacks->mOnRawIngressLimitExceeded = std::move(callback);
    mCallbacks.store(std::move(updatedCallbacks), std::memory_order_release);
    return {};
}

Result<> ServerNetworkSystem::setOnPacketParseFailed(PacketParseFailedCallback&& callback) noexcept {
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

Result<> ServerNetworkSystem::setOnTaskPressure(TaskStrandBackpressurePolicy&& callback) noexcept {
    if (mRunning.load(std::memory_order_acquire)) {
        return error_utils::makeError("Cannot set backpressure policy while running");
    }

    if (callback) {
        mTaskStrandBackpressurePolicy.store(
            std::make_shared<TaskStrandBackpressurePolicy>(std::move(callback)),
            std::memory_order_release
        );
    } else {
        mTaskStrandBackpressurePolicy.store(nullptr, std::memory_order_release);
    }

    return {};
}

Result<>
ServerNetworkSystem::setRawIngressLimits(std::size_t maxPacketBytes, std::size_t maxPacketsPerSecond) noexcept {
    if (mRunning.load(std::memory_order_acquire)) {
        return error_utils::makeError("Cannot set raw ingress limits while running");
    }

    if (maxPacketBytes == 0 || maxPacketsPerSecond == 0) {
        return error_utils::makeError("Raw ingress limits must be greater than zero");
    }

    mRawIngressMaxPacketBytes      = maxPacketBytes;
    mRawIngressMaxPacketsPerSecond = maxPacketsPerSecond;
    return {};
}

std::size_t ServerNetworkSystem::getRawIngressMaxPacketBytes() const noexcept { return mRawIngressMaxPacketBytes; }

std::size_t ServerNetworkSystem::getRawIngressMaxPacketsPerSecond() const noexcept {
    return mRawIngressMaxPacketsPerSecond;
}

std::weak_ptr<Session> ServerNetworkSystem::getSession(const RakNet::RakNetGUID& guid) const noexcept {
    return getSessionShared(guid);
}

void ServerNetworkSystem::disconnectClient(const RakNet::RakNetGUID& guid) noexcept {
    auto context = removeSessionContext(guid);
    if (!context) {
        return;
    }

    context->mSession->disconnect();

    auto callbacks = mCallbacks.load(std::memory_order_acquire);
    if (callbacks->mOnDisconnected) {
        const bool enqueued = context->mStrand->enqueue([callbacks, guid]() mutable {
            callbacks->mOnDisconnected(guid, RakNet::UNASSIGNED_SYSTEM_ADDRESS);
        });
        if (!enqueued) {
            callbacks->mOnDisconnected(guid, RakNet::UNASSIGNED_SYSTEM_ADDRESS);
        }
    }
}

bool ServerNetworkSystem::ioTickOnce() noexcept {
    bool progressed = false;

    while (true) {
        detail::RakPacketOwner packet{mPeer.get(), mPeer->Receive()};
        if (!packet) {
            break;
        }

        processIncomingPacket(std::move(packet));
        progressed = true;
    }

    return progressed;
}

bool ServerNetworkSystem::flushTickOnce() noexcept {
    bool        progressed        = false;
    std::size_t processedReady    = 0;
    const auto  flushReadyPerTick = mFlushReadyPerTick;
    const auto  now               = std::chrono::steady_clock::now();

    while (processedReady < flushReadyPerTick) {
        FlushDueEntry entry{};
        {
            std::lock_guard lock{mFlushScheduleMutex};
            if (mFlushDueHeap.empty() || mFlushDueHeap.top().mDue > now) {
                break;
            }

            entry = mFlushDueHeap.top();
            mFlushDueHeap.pop();

            auto it = mFlushDueTokens.find(entry.mGuid);
            if (it == mFlushDueTokens.end() || it->second != entry.mToken) {
                continue;
            }
        }

        ++processedReady;

        auto context = getSessionContext(entry.mGuid);
        if (!context) {
            cancelSessionFlush(entry.mGuid);
            continue;
        }

        auto session = context->mSession;
        auto strand  = context->mStrand;
        if (!session->isConnected()) {
            cancelSessionFlush(entry.mGuid);
            continue;
        }

        mPendingSessionPacketTasks.fetch_add(1, std::memory_order_acq_rel);

        const bool enqueued =
            strand->enqueue([this, session, guid = entry.mGuid, previousDue = entry.mDue, now]() mutable {
                (void)session->flushIfDue(now);

                if (session->isConnected()) {
                    scheduleSessionFlushAt(guid, nextScheduledFlushDue(previousDue, now));
                } else {
                    cancelSessionFlush(guid);
                }

                if (mPendingSessionPacketTasks.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    mPendingSessionPacketTasks.notify_all();
                }
            });

        if (!enqueued) {
            if (mPendingSessionPacketTasks.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                mPendingSessionPacketTasks.notify_all();
            }

            if (session->isConnected()) {
                scheduleSessionFlushAt(entry.mGuid, nextScheduledFlushDue(entry.mDue, now));
            } else {
                cancelSessionFlush(entry.mGuid);
            }
            continue;
        }

        progressed = true;
    }

    return progressed;
}

void ServerNetworkSystem::scheduleIoPump() noexcept {
    if (!mRunning.load(std::memory_order_acquire) || mIoPumpThread.joinable()) {
        return;
    }

    mIoPumpThread = std::jthread([this](std::stop_token stopToken) noexcept { ioPumpThreadMain(stopToken); });
}

void ServerNetworkSystem::ioPumpThreadMain(std::stop_token stopToken) noexcept {
    while (!stopToken.stop_requested() && mRunning.load(std::memory_order_acquire)) {
        const bool progressed = ioTickOnce();
        if (!progressed) {
            std::this_thread::sleep_for(IO_PUMP_IDLE_WAIT);
        }
    }
}

void ServerNetworkSystem::scheduleFlushPump() noexcept {
    if (!mRunning.load(std::memory_order_acquire)) {
        return;
    }

    bool expected = false;
    if (!mFlushPumpScheduled.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    if (!submitFlushJob([this]() noexcept { flushPumpTask(); })) {
        mFlushPumpScheduled.store(false, std::memory_order_release);
    }
}

void ServerNetworkSystem::flushPumpTask() noexcept {
    if (!mRunning.load(std::memory_order_acquire)) {
        mFlushPumpScheduled.store(false, std::memory_order_release);
        return;
    }

    const bool progressed = flushTickOnce();

    mFlushPumpScheduled.store(false, std::memory_order_release);

    if (!mRunning.load(std::memory_order_acquire)) {
        return;
    }

    if (!progressed) {
        scheduleFlushPumpAfter(FLUSH_PUMP_IDLE_WAIT);
        return;
    }

    scheduleFlushPump();
}

void ServerNetworkSystem::scheduleFlushPumpAfter(std::chrono::milliseconds delay) noexcept {
    mPendingFlushWakeups.fetch_add(1, std::memory_order_acq_rel);
    const bool submitted = mThreadPool->submitAfter(delay, [this]() noexcept {
        scheduleFlushPump();
        if (mPendingFlushWakeups.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            mPendingFlushWakeups.notify_all();
        }
    });

    if (!submitted) {
        if (mPendingFlushWakeups.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            mPendingFlushWakeups.notify_all();
        }
    }
}

void ServerNetworkSystem::waitForPendingFlushJobs() noexcept {
    auto pending = mPendingFlushJobs.load(std::memory_order_acquire);
    while (pending != 0) {
        mPendingFlushJobs.wait(pending, std::memory_order_acquire);
        pending = mPendingFlushJobs.load(std::memory_order_acquire);
    }
}

void ServerNetworkSystem::waitForPendingFlushWakeups() noexcept {
    auto pending = mPendingFlushWakeups.load(std::memory_order_acquire);
    while (pending != 0) {
        mPendingFlushWakeups.wait(pending, std::memory_order_acquire);
        pending = mPendingFlushWakeups.load(std::memory_order_acquire);
    }
}

void ServerNetworkSystem::waitForPendingSessionPacketTasks() noexcept {
    auto pending = mPendingSessionPacketTasks.load(std::memory_order_acquire);
    while (pending != 0) {
        mPendingSessionPacketTasks.wait(pending, std::memory_order_acquire);
        pending = mPendingSessionPacketTasks.load(std::memory_order_acquire);
    }
}

void ServerNetworkSystem::processIncomingPacket(detail::RakPacketOwner packetOwner) {
    auto&      packetRef      = *packetOwner.get();
    auto       callbacks      = mCallbacks.load(std::memory_order_acquire);
    auto       remote         = RakNet::AddressOrGUID{&packetRef};
    const auto key            = packetRef.guid;
    auto       sessionContext = getSessionContext(key);

    if (!packetRef.data || packetRef.length == 0) {
        bool shouldDisconnect = true;
        if (callbacks->mOnRawIngressLimitExceeded) {
            shouldDisconnect = callbacks->mOnRawIngressLimitExceeded(key, packetRef.systemAddress, 0, 0);
        }

        if (shouldDisconnect) {
            if (sessionContext) {
                sessionContext->mSession->disconnect();
                (void)removeSessionContext(key);
            } else {
                mPeer->CloseConnection(remote, true, static_cast<std::uint8_t>(0), LOW_PRIORITY);
            }
        }

        return;
    }

    const auto messageId = static_cast<std::uint8_t>(packetRef.data[0]);

    if (messageId == DefaultMessageIDTypes::ID_UNCONNECTED_PING) {
        return;
    }

    if (messageId == DefaultMessageIDTypes::ID_NEW_INCOMING_CONNECTION) {
        auto newSessionContext                      = std::make_shared<SessionContext>();
        newSessionContext->mSession                 = std::make_shared<Session>(mPeer.get(), remote);
        newSessionContext->mStrand                  = std::make_shared<thread::TaskStrand>(*mThreadPool);
        newSessionContext->mRawIngressWindowStart   = std::chrono::steady_clock::time_point{};
        newSessionContext->mRawIngressWindowPackets = 0;
        auto backpressurePolicy                     = mTaskStrandBackpressurePolicy.load(std::memory_order_acquire);
        newSessionContext->mStrand->setBackpressurePolicy(backpressurePolicy ? *backpressurePolicy : nullptr);
        upsertSessionContext(key, newSessionContext);
        const auto scheduleNow = std::chrono::steady_clock::now();
        scheduleSessionFlushAt(key, scheduleNow + flushPhaseOffsetForGuid(key));

        if (callbacks->mOnConnected) {
            const bool enqueued = newSessionContext->mStrand->enqueue(
                [callbacks, guid = key, address = packetRef.systemAddress]() mutable {
                    callbacks->mOnConnected(guid, address);
                }
            );
            if (!enqueued) {
                callbacks->mOnConnected(key, packetRef.systemAddress);
            }
        }
        return;
    }

    if (!sessionContext) {
        return;
    }

    mPendingSessionPacketTasks.fetch_add(1, std::memory_order_acq_rel);

    const bool enqueued = sessionContext->mStrand->enqueue([this,
                                                            key,
                                                            address = packetRef.systemAddress,
                                                            messageId,
                                                            callbacks,
                                                            sessionContext,
                                                            packetOwner = std::move(packetOwner)]() mutable noexcept {
        processSessionPacket(
            key,
            address,
            messageId,
            std::move(sessionContext),
            std::move(callbacks),
            std::move(packetOwner)
        );
    });

    if (!enqueued) {
        if (mPendingSessionPacketTasks.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            mPendingSessionPacketTasks.notify_all();
        }
    }
}

void ServerNetworkSystem::processSessionPacket(
    const RakNet::RakNetGUID&                            key,
    const RakNet::SystemAddress&                         address,
    std::uint8_t                                         messageId,
    std::shared_ptr<ServerNetworkSystem::SessionContext> sessionContext,
    std::shared_ptr<ServerNetworkSystem::CallbackSet>    callbacks,
    detail::RakPacketOwner                               packetOwner
) noexcept {
    auto& taskPacket = *packetOwner.get();
    auto  sessionPtr = sessionContext->mSession;

    const auto packetBytes = static_cast<std::size_t>(taskPacket.length);

    if (packetBytes > mRawIngressMaxPacketBytes) {
        const bool shouldDisconnect =
            callbacks->mOnRawIngressLimitExceeded
                ? callbacks
                      ->mOnRawIngressLimitExceeded(key, address, packetBytes, sessionContext->mRawIngressWindowPackets)
                : true;
        if (shouldDisconnect) {
            auto disconnectedContext = removeSessionContext(key);
            if (disconnectedContext) {
                disconnectedContext->mSession->disconnect();
            } else {
                sessionPtr->disconnect();
            }
        }

        if (mPendingSessionPacketTasks.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            mPendingSessionPacketTasks.notify_all();
        }
        return;
    }

    const auto ingressNow = std::chrono::steady_clock::now();
    if (sessionContext->mRawIngressWindowStart == std::chrono::steady_clock::time_point{}
        || ingressNow - sessionContext->mRawIngressWindowStart >= RAW_INGRESS_WINDOW) {
        sessionContext->mRawIngressWindowStart   = ingressNow;
        sessionContext->mRawIngressWindowPackets = 0;
    }

    if (sessionContext->mRawIngressWindowPackets >= mRawIngressMaxPacketsPerSecond) {
        bool shouldDisconnect = true;
        if (callbacks->mOnRawIngressLimitExceeded) {
            shouldDisconnect =
                callbacks
                    ->mOnRawIngressLimitExceeded(key, address, packetBytes, sessionContext->mRawIngressWindowPackets);
        }
        if (shouldDisconnect) {
            auto disconnectedContext = removeSessionContext(key);
            if (disconnectedContext) {
                disconnectedContext->mSession->disconnect();
            } else {
                sessionPtr->disconnect();
            }
        }

        if (mPendingSessionPacketTasks.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            mPendingSessionPacketTasks.notify_all();
        }
        return;
    }

    ++sessionContext->mRawIngressWindowPackets;

    if (messageId == DefaultMessageIDTypes::ID_DISCONNECTION_NOTIFICATION
        || messageId == DefaultMessageIDTypes::ID_CONNECTION_LOST) {
        auto disconnectedContext = removeSessionContext(key);
        if (disconnectedContext) {
            disconnectedContext->mSession->disconnect();
        } else {
            sessionPtr->disconnect();
        }

        if (callbacks->mOnDisconnected && disconnectedContext) {
            const bool enqueued = disconnectedContext->mStrand->enqueue([callbacks, guid = key, address]() mutable {
                callbacks->mOnDisconnected(guid, address);
            });
            if (!enqueued) {
                callbacks->mOnDisconnected(key, address);
            }
        }

        if (mPendingSessionPacketTasks.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            mPendingSessionPacketTasks.notify_all();
        }
        return;
    }

    if (messageId != MINECRAFT_BATCH_PACKET_ID || packetBytes <= 1) {
        if (mPendingSessionPacketTasks.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            mPendingSessionPacketTasks.notify_all();
        }
        return;
    }

    const auto* payloadBegin = reinterpret_cast<const std::byte*>(taskPacket.data + 1);
    const auto  payloadSize  = static_cast<std::size_t>(taskPacket.length - 1);

    auto packets = sessionPtr->deserializeBatchPackets(std::span<const std::byte>{payloadBegin, payloadSize});
    if (!packets) {
        if (mPendingSessionPacketTasks.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            mPendingSessionPacketTasks.notify_all();
        }
        return;
    }

    for (auto& payload : *packets) {
        if (callbacks->mOnPacketReceive) {
            if (callbacks->mOnRawPacketReceive && !callbacks->mOnRawPacketReceive(key, address, payload)) {
                continue;
            }

            ReadOnlyBinaryStream stream{payload};

            auto header = MinecraftPackets::readPacketHeader(stream);
            if (!header) {
                if (callbacks->mOnPacketParseFailed) {
                    callbacks->mOnPacketParseFailed(
                        key,
                        address,
                        static_cast<MinecraftPacketIds>(-1), // -1 as invalid packet ID
                        "Failed to read packet header"
                    );
                }
                continue;
            }

            auto packetExpected = MinecraftPackets::readAndCreatePacketFromHeader(*header, stream);
            if (packetExpected) {
                callbacks->mOnPacketReceive(key, address, std::move(*packetExpected));
            } else if (callbacks->mOnPacketParseFailed) {
                callbacks->mOnPacketParseFailed(key, address, header->mPacketId, packetExpected.error().mMessage);
            }
        } else {
            (void)sessionPtr->enqueueInboundPacket(std::move(payload));
        }
    }

    if (mPendingSessionPacketTasks.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        mPendingSessionPacketTasks.notify_all();
    }
}

void ServerNetworkSystem::upsertSessionContext(const RakNet::RakNetGUID& key, std::shared_ptr<SessionContext> context) {
    mSessionContexts.lazy_emplace_l(
        key,
        [&context](SessionContextsMap::value_type& existing) { existing.second = context; },
        [key, &context](const auto& ctor) { ctor(key, context); }
    );
}

std::shared_ptr<ServerNetworkSystem::SessionContext>
ServerNetworkSystem::removeSessionContext(const RakNet::RakNetGUID& key) {
    cancelSessionFlush(key);
    std::shared_ptr<SessionContext> removed;
    const bool erased = mSessionContexts.erase_if(key, [&removed](SessionContextsMap::value_type& entry) {
        removed = std::move(entry.second);
        return true;
    });
    if (!erased) {
        return nullptr;
    }
    return removed;
}

void ServerNetworkSystem::RakPeerDeleter::operator()(RakNet::RakPeerInterface* peer) const noexcept {
    if (peer) {
        RakNet::RakPeerInterface::DestroyInstance(peer);
    }
}

} // namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE
