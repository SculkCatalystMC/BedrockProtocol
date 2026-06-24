// Copyright © 2026 SculkCatalystMC. All rights reserved.
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "sculk/protocol/Version.hpp"
#include "sculk/protocol/connection/thread/AtomicSharedPtr.hpp"
#include "sculk/protocol/connection/thread/ThreadPool.hpp"

#include <atomic>
#include <chrono>
#include <concepts>
#include <concurrentqueue.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>

namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE::thread {

class TaskStrand final {
public:
    using BackpressurePolicy =
        std::function<std::chrono::steady_clock::duration(std::uint32_t pendingTasks, std::uint32_t maxPendingTasks)>;

#ifndef _LIBCPP_VERSION
    using Task = std::move_only_function<void()>;
#else
    class Task final {
    private:
        struct Concept {
            virtual ~Concept()           = default;
            virtual void call() noexcept = 0;
        };

        template <typename F>
        struct Model final : Concept {
            F mFunction;

            explicit Model(F&& function) noexcept(std::is_nothrow_move_constructible_v<F>)
            : mFunction(std::forward<F>(function)) {}

            void call() noexcept override { std::invoke(mFunction); }
        };

    public:
        Task() = default;

        template <typename F>
            requires std::invocable<F&>
        Task(F&& function) : mModel(std::make_unique<Model<std::decay_t<F>>>(std::forward<F>(function))) {}

        Task(const Task&)            = delete;
        Task& operator=(const Task&) = delete;

        Task(Task&&) noexcept            = default;
        Task& operator=(Task&&) noexcept = default;

        explicit operator bool() const noexcept { return static_cast<bool>(mModel); }

        void operator()() noexcept { mModel->call(); }

    private:
        std::unique_ptr<Concept> mModel{};
    };
#endif

    static constexpr std::uint32_t DEFAULT_MAX_PENDING_TASKS = 2048;

    explicit TaskStrand(ThreadPool& threadPool, std::uint32_t maxPendingTasks = DEFAULT_MAX_PENDING_TASKS) noexcept
    : mState(std::make_shared<State>(threadPool, maxPendingTasks)) {}

    void setMaxPendingTasks(std::uint32_t maxPendingTasks) noexcept {
        mState->mMaxPendingTasks.store(maxPendingTasks, std::memory_order_release);
    }

    // Sets a policy that decides how long to wait when the strand is overloaded.
    void setBackpressurePolicy(BackpressurePolicy policy) noexcept {
        if (policy) {
            mState->mBackpressurePolicy.store(std::make_shared<BackpressurePolicy>(std::move(policy)));
        } else {
            mState->mBackpressurePolicy.store(nullptr);
        }
    }

    [[nodiscard]] std::uint32_t maxPendingTasks() const noexcept {
        return mState->mMaxPendingTasks.load(std::memory_order_acquire);
    }

    template <typename F>
        requires std::invocable<F&>
    [[nodiscard]] bool enqueue(F&& task) noexcept {
        auto state = mState;
        Task wrapped{std::forward<F>(task)};
        if (!wrapped) {
            return false;
        }

        const auto pending    = state->mPendingTasks.fetch_add(1, std::memory_order_acq_rel) + 1;
        const auto maxPending = state->mMaxPendingTasks.load(std::memory_order_acquire);
        if (pending > maxPending) {
            auto backpressurePolicy = state->mBackpressurePolicy.load(std::memory_order_acquire);
            if (backpressurePolicy) {
                const auto delay = (*backpressurePolicy)(pending, maxPending);
                if (delay > std::chrono::steady_clock::duration::zero()) {
                    std::this_thread::sleep_for(delay);
                }
            } else {
                state->mPendingTasks.fetch_sub(1, std::memory_order_acq_rel);
                return false;
            }
        }

        if (!state->mTasks.enqueue(std::move(wrapped))) {
            state->mPendingTasks.fetch_sub(1, std::memory_order_acq_rel);
            return false;
        }

        scheduleDrain(std::move(state), state->mThreadPool);
        return true;
    }

    [[nodiscard]] std::uint32_t pendingTasks() const noexcept {
        return mState->mPendingTasks.load(std::memory_order_acquire);
    }

private:
    struct State final {
        explicit State(ThreadPool& threadPool, std::uint32_t maxPendingTasks) noexcept
        : mThreadPool(threadPool),
          mMaxPendingTasks(maxPendingTasks) {}

        ThreadPool&                         mThreadPool;
        moodycamel::ConcurrentQueue<Task>   mTasks{};
        std::atomic_uint32_t                mMaxPendingTasks{DEFAULT_MAX_PENDING_TASKS};
        std::atomic_uint32_t                mPendingTasks{0};
        std::atomic_bool                    mDrainScheduled{false};
        AtomicSharedPtr<BackpressurePolicy> mBackpressurePolicy{};
    };

    void scheduleDrain(std::shared_ptr<State> state, ThreadPool& pool) noexcept {
        if (state->mDrainScheduled.exchange(true, std::memory_order_acq_rel)) {
            return;
        }

        const bool submitted = pool.submit([state]() noexcept { drain(std::move(state)); });
        if (!submitted) {
            state->mDrainScheduled.store(false, std::memory_order_release);
            drain(std::move(state));
        }
    }

    static void drain(std::shared_ptr<State> state) noexcept {
        Task task;

        for (;;) {
            while (state->mTasks.try_dequeue(task)) {
                task();
                task = Task{};
                state->mPendingTasks.fetch_sub(1, std::memory_order_acq_rel);
            }

            state->mDrainScheduled.store(false, std::memory_order_release);

            if (state->mTasks.size_approx() == 0) {
                break;
            }

            if (state->mDrainScheduled.exchange(true, std::memory_order_acq_rel)) {
                break;
            }
        }
    }

private:
    std::shared_ptr<State> mState{};
};

} // namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE::thread
