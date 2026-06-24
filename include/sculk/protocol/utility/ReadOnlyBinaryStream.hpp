// Copyright © 2026 SculkCatalystMC. All rights reserved.
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#pragma once
#include "Result.hpp"
#include "Traits.hpp"
#include "Variant.hpp"
#include <algorithm>
#include <bit>
#include <bitset>
#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE {

#ifdef SCULK_PROTOCOL_ENABLE_DETAIL_ERRORS
#define _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR(ERROR)                                                               \
    error_utils::makeError(                                                                                            \
        std::format("ReadOnlyBinaryStream::" ERROR ": mReadPointer={}, size={}", mReadPointer, mBufferView.size()),    \
        location                                                                                                       \
    )
#else
#define _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR(ERROR) error_utils::makeError("ReadOnlyBinaryStream::" ERROR)
#endif

class ReadOnlyBinaryStream {
    static_assert(
        std::endian::native == std::endian::little,
        "ReadOnlyBinaryStream requires little-endian architecture"
    );

public:
    bool                       mHasOverflowed{};
    std::span<const std::byte> mBufferView{};
    std::size_t                mReadPointer{};

private:
    template <typename T>
    constexpr Result<> read(T* target _SCULK_SL_PARAMETER_DEF) noexcept {
        static_assert(std::is_trivially_copyable_v<T>, "ReadOnlyBinaryStream::read requires trivially copyable type");
        if (mHasOverflowed) {
            return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("read overflowed");
        }
        std::size_t newReadPointer = mReadPointer + sizeof(T);

        if (newReadPointer < mReadPointer || newReadPointer > mBufferView.size()) {
            mHasOverflowed = true;
            return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("read overflowed");
        }

        std::copy_n(
            reinterpret_cast<const char*>(mBufferView.data() + mReadPointer),
            sizeof(T),
            reinterpret_cast<char*>(target)
        );
        mReadPointer = newReadPointer;
        return {};
    }

public:
    [[nodiscard]] constexpr explicit ReadOnlyBinaryStream(std::span<const std::byte> buffer) noexcept
    : mBufferView(buffer) {}

    [[nodiscard]] constexpr explicit ReadOnlyBinaryStream(std::string_view buffer) noexcept
    : mBufferView(reinterpret_cast<const std::byte*>(buffer.data()), buffer.size()) {}

    [[nodiscard]] constexpr std::size_t size() const noexcept { return mBufferView.size(); }

    [[nodiscard]] constexpr std::size_t getPosition() const noexcept { return mReadPointer; }

    constexpr void setPosition(std::size_t value) noexcept {
        if (value > mBufferView.size()) {
            mReadPointer   = mBufferView.size();
            mHasOverflowed = true;
            return;
        }
        mReadPointer = value;
    }

    constexpr void resetPosition() noexcept { setPosition(0); }

    [[nodiscard]] constexpr Result<> ignoreBytes(std::size_t length _SCULK_SL_PARAM_DEFAULT) noexcept {
        std::size_t newPointer = mReadPointer + length;
        if (newPointer < mReadPointer || newPointer > mBufferView.size()) {
            mHasOverflowed = true;
            return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("ignoreBytes overflowed");
        }
        mReadPointer = newPointer;
        return {};
    }

    [[nodiscard]] constexpr std::span<const std::byte> getLeftBufferView() const noexcept {
        return mBufferView.subspan(mReadPointer);
    }

    [[nodiscard]] constexpr bool isOverflowed() const noexcept { return mHasOverflowed; }

    [[nodiscard]] constexpr bool hasDataLeft() const noexcept { return mReadPointer < mBufferView.size(); }

    [[nodiscard]] constexpr std::span<const std::byte> view() const noexcept { return mBufferView; }

    [[nodiscard]] constexpr Result<> readBytes(void* target, std::size_t num _SCULK_SL_PARAM_DEFAULT) noexcept {
        if (mHasOverflowed) {
            return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readBytes overflowed");
        }
        if (num == 0) {
            return {};
        }

        std::size_t newPointer = mReadPointer + num;

        if (newPointer < mReadPointer || newPointer > mBufferView.size()) {
            mHasOverflowed = true;
            return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readBytes overflowed");
        }

        std::copy_n(reinterpret_cast<const char*>(mBufferView.data() + mReadPointer), num, static_cast<char*>(target));
        mReadPointer = newPointer;
        return {};
    }

    [[nodiscard]] constexpr Result<> readBool(bool& value _SCULK_SL_PARAM_DEFAULT) noexcept {
        return read(&value _SCULK_SL_PARAM_PASS);
    }

    [[nodiscard]] constexpr Result<> readByte(std::uint8_t& value _SCULK_SL_PARAM_DEFAULT) noexcept {
        return read(&value _SCULK_SL_PARAM_PASS);
    }

    [[nodiscard]] constexpr Result<> readSignedChar(std::int8_t& value _SCULK_SL_PARAM_DEFAULT) noexcept {
        return read(&value _SCULK_SL_PARAM_PASS);
    }

    [[nodiscard]] constexpr Result<> readUnsignedShort(std::uint16_t& value _SCULK_SL_PARAM_DEFAULT) noexcept {
        return read(&value _SCULK_SL_PARAM_PASS);
    }

    [[nodiscard]] constexpr Result<> readUnsignedInt(std::uint32_t& value _SCULK_SL_PARAM_DEFAULT) noexcept {
        return read(&value _SCULK_SL_PARAM_PASS);
    }

    [[nodiscard]] constexpr Result<> readUnsignedInt64(std::uint64_t& value _SCULK_SL_PARAM_DEFAULT) noexcept {
        return read(&value _SCULK_SL_PARAM_PASS);
    }

    [[nodiscard]] constexpr Result<> readDouble(double& value _SCULK_SL_PARAM_DEFAULT) noexcept {
        return read(&value _SCULK_SL_PARAM_PASS);
    }

    [[nodiscard]] constexpr Result<> readFloat(float& value _SCULK_SL_PARAM_DEFAULT) noexcept {
        return read(&value _SCULK_SL_PARAM_PASS);
    }

    [[nodiscard]] constexpr Result<> readSignedInt(std::int32_t& value _SCULK_SL_PARAM_DEFAULT) noexcept {
        return read(&value _SCULK_SL_PARAM_PASS);
    }

    [[nodiscard]] constexpr Result<> readSignedInt64(std::int64_t& value _SCULK_SL_PARAM_DEFAULT) noexcept {
        return read(&value _SCULK_SL_PARAM_PASS);
    }

    [[nodiscard]] constexpr Result<> readSignedShort(std::int16_t& value _SCULK_SL_PARAM_DEFAULT) noexcept {
        return read(&value _SCULK_SL_PARAM_PASS);
    }

    [[nodiscard]] constexpr Result<> readUnsignedVarInt(std::uint32_t& value _SCULK_SL_PARAM_DEFAULT) noexcept {
        value = 0;
        std::uint32_t shift{};
        std::uint8_t  byte{};

        do {
            if (shift >= 35) {
                mHasOverflowed = true;
                return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readUnsignedVarInt overflowed");
            }

            if (mReadPointer >= mBufferView.size()) {
                mHasOverflowed = true;
                return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readUnsignedVarInt overflowed");
            }

            byte = std::to_integer<std::uint8_t>(mBufferView[mReadPointer++]);
            if (shift == 28 && (byte & 0x80) == 0 && (byte & 0x70) != 0) {
                mHasOverflowed = true;
                return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readUnsignedVarInt overflowed");
            }
            value |= (byte & 0x7F) << shift;
            shift += 7;

        } while (byte & 0x80);

        return {};
    }

    [[nodiscard]] constexpr Result<> readUnsignedVarInt64(std::uint64_t& value _SCULK_SL_PARAM_DEFAULT) noexcept {
        value = 0;
        std::uint32_t shift{};
        std::uint8_t  byte{};

        do {
            if (shift >= 70) {
                mHasOverflowed = true;
                return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readUnsignedVarInt64 overflowed");
            }

            if (mReadPointer >= mBufferView.size()) {
                mHasOverflowed = true;
                return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readUnsignedVarInt64 overflowed");
            }

            byte = std::to_integer<std::uint8_t>(mBufferView[mReadPointer++]);
            if (shift == 63 && (byte & 0x80) == 0 && (byte & 0x7E) != 0) {
                mHasOverflowed = true;
                return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readUnsignedVarInt64 overflowed");
            }
            value |= static_cast<std::uint64_t>(byte & 0x7F) << shift;
            shift += 7;

        } while (byte & 0x80);

        return {};
    }

    [[nodiscard]] constexpr Result<> readVarInt(std::int32_t& value _SCULK_SL_PARAM_DEFAULT) noexcept {
        std::uint32_t temp{};
        if (!readUnsignedVarInt(temp _SCULK_SL_PARAM_PASS)) {
            return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readVarInt failed");
        }
        value = (temp & 1) ? ~(temp >> 1) : (temp >> 1);
        return {};
    }

    [[nodiscard]] constexpr Result<> readVarInt64(std::int64_t& value _SCULK_SL_PARAM_DEFAULT) noexcept {
        std::uint64_t temp{};
        if (!readUnsignedVarInt64(temp _SCULK_SL_PARAM_PASS)) {
            return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readVarInt64 failed");
        }
        value = (temp & 1) ? ~(temp >> 1) : (temp >> 1);
        return {};
    }

    [[nodiscard]] constexpr Result<> readSignedBigEndianInt(std::int32_t& value _SCULK_SL_PARAM_DEFAULT) noexcept {
        if (read(&value _SCULK_SL_PARAM_PASS)) {
            value = std::byteswap(value);
            return {};
        }
        return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readSignedBigEndianInt failed");
    }

    [[nodiscard]] constexpr Result<> readString(std::string& outString _SCULK_SL_PARAM_DEFAULT) noexcept {
        std::uint32_t length{};
        if (!readUnsignedVarInt(length _SCULK_SL_PARAM_PASS)) {
            outString.clear();
            return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readString failed");
        }
        return readRawBytes(outString, static_cast<std::size_t>(length) _SCULK_SL_PARAM_PASS);
    }

    [[nodiscard]] constexpr Result<> readLongString(std::string& value _SCULK_SL_PARAM_DEFAULT) noexcept {
        std::int32_t length{};
        if (!readSignedInt(length _SCULK_SL_PARAM_PASS)) {
            return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readLongString overflowed");
        }
        if (length < 0) {
            return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readLongString failed: negative length");
        }
        return readRawBytes(value, static_cast<std::size_t>(length) _SCULK_SL_PARAM_PASS);
    }

    [[nodiscard]] constexpr Result<> readRawBytes(std::string& rawBuffer, std::size_t length _SCULK_SL_PARAM_DEFAULT) {
        if (length == 0) {
            rawBuffer.clear();
            return {};
        }

        if (mReadPointer > mBufferView.size()) {
            mHasOverflowed = true;
            rawBuffer.clear();
            return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readRawBytes overflowed");
        }
        std::size_t remaining = mBufferView.size() - mReadPointer;
        if (length > remaining) {
            mHasOverflowed = true;
            rawBuffer.clear();
            return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readRawBytes overflowed");
        }

        rawBuffer.assign(reinterpret_cast<const char*>(mBufferView.data() + mReadPointer), length);
        mReadPointer += length;
        return {};
    }

    template <typename T, typename P, typename R>
    [[nodiscard]] constexpr Result<>
    readArray(std::vector<T>& outVector, P&& prefix, R&& func _SCULK_SL_PARAM_DEFAULT) {
        using AT = std::remove_cv_t<std::remove_reference_t<traits::member_func_arg_t<P, 0>>>;
        AT length{};
        if (!std::invoke(std::forward<P>(prefix), *this, length _SCULK_SL_PARAM_PASS)) {
            return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readArray overflowed");
        }

        outVector.resize(static_cast<std::size_t>(length));
        for (auto& element : outVector) {
            if constexpr (std::is_invocable_r_v<Result<>, R, ReadOnlyBinaryStream&, T&>) {
                _SCULK_READ(std::invoke(std::forward<R>(func), *this, element));
            }
#ifdef SCULK_PROTOCOL_ENABLE_DETAIL_ERRORS
            else if constexpr (std::is_invocable_r_v<Result<>, R, ReadOnlyBinaryStream&, T & _SCULK_SL_PARAMETER>) {
                _SCULK_READ(std::invoke(std::forward<R>(func), *this, element _SCULK_SL_PARAM_PASS));
            }
#endif
            else if constexpr (std::is_invocable_r_v<Result<>, R, T&, ReadOnlyBinaryStream&>) {
                _SCULK_READ(std::invoke(std::forward<R>(func), element, *this));
            } else {
                static_assert(traits::always_false_v<T>, "invalid read array function");
            }
        }
        return {};
    }

    template <typename T, typename R>
    [[nodiscard]] constexpr Result<> readArray(std::vector<T>& outVector, R&& func _SCULK_SL_PARAM_DEFAULT) {
        return readArray(
            outVector,
            &ReadOnlyBinaryStream::readUnsignedVarInt,
            std::forward<R>(func) _SCULK_SL_PARAM_PASS
        );
    }

    template <typename T, typename F>
    [[nodiscard]] constexpr Result<> readOptional(std::optional<T>& outOpt, F&& func _SCULK_SL_PARAM_DEFAULT) noexcept {
        outOpt.reset();
        bool hasValue{};
        if (!readBool(hasValue _SCULK_SL_PARAM_PASS)) {
            return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readOptional overflowed");
        }
        if (hasValue) {
            outOpt.emplace();
            if constexpr (std::is_invocable_r_v<Result<>, F, ReadOnlyBinaryStream&, T&>) {
                return std::invoke(std::forward<F>(func), *this, *outOpt);
            }
#ifdef SCULK_PROTOCOL_ENABLE_DETAIL_ERRORS
            else if constexpr (std::is_invocable_r_v<Result<>, F, ReadOnlyBinaryStream&, T & _SCULK_SL_PARAMETER>) {
                return std::invoke(std::forward<F>(func), *this, *outOpt _SCULK_SL_PARAM_PASS);
            }
#endif
            else if constexpr (std::is_invocable_r_v<Result<>, F, T&, ReadOnlyBinaryStream&>) {
                return std::invoke(std::forward<F>(func), *outOpt, *this);
            } else {
                static_assert(traits::always_false_v<T>, "invalid read optional function");
            }
        }
        return {};
    }

    template <std::size_t N>
    [[nodiscard]] constexpr Result<> readBitset(std::bitset<N>& outBitset _SCULK_SL_PARAM_DEFAULT) noexcept {
        auto bitLength8 = [](std::uint8_t value) constexpr -> std::size_t {
            std::size_t length{};
            while (value != 0) {
                value >>= 1;
                length++;
            }
            return length;
        };

        outBitset.reset();
        std::size_t chunkCount{};
        bool        seenSetBit{};
        for (std::size_t bitIndex = 0; bitIndex < outBitset.size(); bitIndex += 7) {
            std::uint8_t byte{};
            if (!readByte(byte _SCULK_SL_PARAM_PASS)) {
                mHasOverflowed = true;
                return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readBitset overflowed");
            }
            chunkCount++;

            if (bitIndex + bitLength8(byte) > outBitset.size()) {
                mHasOverflowed = true;
                return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readBitset overflowed");
            }

            const std::uint8_t payload = byte & 0x7Fu;
            if (payload != 0) {
                seenSetBit = true;
            }

            for (int i = 0; i < 7; i++) {
                const std::size_t currentBit = bitIndex + static_cast<std::size_t>(i);
                if (currentBit < outBitset.size()) {
                    outBitset.set(currentBit, (byte & static_cast<std::uint8_t>(1u << i)) != 0);
                }
            }

            if ((byte & 0x80u) == 0) {
                if (!seenSetBit && chunkCount != 1) {
                    mHasOverflowed = true;
                    return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readBitset overflowed");
                }
                if (seenSetBit && payload == 0) {
                    mHasOverflowed = true;
                    return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readBitset overflowed");
                }
                return {};
            }
        }

        mHasOverflowed = true;
        return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readBitset overflowed");
    }

    template <typename T, typename F>
        requires std::is_enum_v<T>
    [[nodiscard]] constexpr Result<> readEnum(T& outValue, F&& func _SCULK_SL_PARAM_DEFAULT) noexcept {
        using AT = std::remove_cv_t<std::remove_reference_t<traits::member_func_arg_t<F, 0>>>;
        AT value{};
        if (!std::invoke(std::forward<F>(func), *this, value _SCULK_SL_PARAM_PASS)) {
            return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readEnum overflowed");
        }
        outValue = static_cast<T>(value);
        return {};
    }

    template <typename T, typename P, typename V>
    [[nodiscard]] constexpr Result<> readVariant(T& var, P&& prefix, V&& visitor _SCULK_SL_PARAM_DEFAULT) noexcept {
        using AT = std::remove_cv_t<std::remove_reference_t<traits::member_func_arg_t<P, 0>>>;
        AT index{};
        if (!std::invoke(std::forward<P>(prefix), *this, index _SCULK_SL_PARAM_PASS)) {
            return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readVariant overflowed");
        }
        if (!emplace_variant(var, index _SCULK_SL_PARAM_PASS)) {
            return _SCULK_READ_ONLY_BINARY_STREAM_MAKE_ERROR("readVariant invalid variant index");
        }
        return std::visit(
            [&](auto&& arg) -> Result<> {
                return std::invoke(std::forward<V>(visitor), std::forward<decltype(arg)>(arg));
            },
            var
        );
    }

    template <typename T, typename V>
    [[nodiscard]] constexpr Result<> readVariant(T& var, V&& visitor _SCULK_SL_PARAM_DEFAULT) noexcept {
        return readVariant(
            var,
            &ReadOnlyBinaryStream::readUnsignedVarInt,
            std::forward<V>(visitor) _SCULK_SL_PARAM_PASS
        );
    }
};

} // namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE
