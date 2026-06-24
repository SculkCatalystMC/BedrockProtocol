// Copyright © 2026 SculkCatalystMC. All rights reserved.
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#pragma once
#include "Traits.hpp"
#include <algorithm>
#include <bit>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE {

class BinaryStream {
    static_assert(std::endian::native == std::endian::little, "BinaryStream requires little-endian architecture");

public:
    std::vector<std::byte>& mBuffer;

private:
    constexpr void appendBytes(const std::byte* bytes, std::size_t num) {
        if (num == 0) {
            return;
        }
        mBuffer.insert(mBuffer.end(), bytes, bytes + num);
    }

    template <typename T>
    constexpr void write(T value) {
        static_assert(std::is_trivially_copyable_v<T>, "BinaryStream::write requires trivially copyable type");
        appendBytes(reinterpret_cast<const std::byte*>(&value), sizeof(T));
    }

public:
    [[nodiscard]] constexpr explicit BinaryStream(std::vector<std::byte>& buffer) : mBuffer(buffer) {}

    [[nodiscard]] constexpr const std::byte* data() const { return mBuffer.data(); }

    [[nodiscard]] constexpr std::byte* data() { return mBuffer.data(); }

    [[nodiscard]] constexpr std::size_t size() const { return mBuffer.size(); }

    constexpr void reserve(std::size_t size) { mBuffer.reserve(size); }

    constexpr void reset() { mBuffer.clear(); }

    constexpr std::string_view asStringView() const {
        return std::string_view(reinterpret_cast<const char*>(mBuffer.data()), mBuffer.size());
    }

    constexpr void writeBytes(const void* origin, std::size_t num) {
        appendBytes(reinterpret_cast<const std::byte*>(origin), num);
    }

    constexpr void writeAndMoveBuffer(std::vector<std::byte>&& buffer) {
        const std::size_t oldSize = mBuffer.size();
        mBuffer.resize(oldSize + buffer.size());
        std::memcpy(mBuffer.data() + oldSize, buffer.data(), buffer.size());
    }

    constexpr void writeBool(bool value) { write(value); }

    constexpr void writeByte(std::uint8_t value) { write(value); }

    constexpr void writeSignedChar(std::int8_t value) { write(value); }

    constexpr void writeUnsignedShort(std::uint16_t value) { write(value); }

    constexpr void writeUnsignedInt(std::uint32_t value) { write(value); }

    constexpr void writeUnsignedInt64(std::uint64_t value) { write(value); }

    constexpr void writeDouble(double value) { write(value); }

    constexpr void writeFloat(float value) { write(value); }

    constexpr void writeSignedInt(std::int32_t value) { write(value); }

    constexpr void writeSignedInt64(std::int64_t value) { write(value); }

    constexpr void writeSignedShort(std::int16_t value) { write(value); }

    constexpr void writeUnsignedVarInt(std::uint32_t uvalue) {
        do {
            std::uint8_t next_byte   = uvalue & 0x7F;
            uvalue                 >>= 7;
            if (uvalue != 0) {
                next_byte |= 0x80;
            }
            writeByte(next_byte);
        } while (uvalue != 0);
    }

    constexpr void writeUnsignedVarInt64(std::uint64_t uvalue) {
        do {
            std::uint8_t next_byte   = uvalue & 0x7F;
            uvalue                 >>= 7;
            if (uvalue != 0) {
                next_byte |= 0x80;
            }
            writeByte(next_byte);
        } while (uvalue != 0);
    }

    constexpr void writeVarInt(std::int32_t value) {
        if (value >= 0) {
            writeUnsignedVarInt(static_cast<std::uint32_t>(value) << 1);
        } else {
            writeUnsignedVarInt((static_cast<std::uint32_t>(~value) << 1) | 1);
        }
    }

    constexpr void writeVarInt64(std::int64_t value) {
        if (value >= 0) {
            writeUnsignedVarInt64(static_cast<std::uint64_t>(value) << 1);
        } else {
            writeUnsignedVarInt64((static_cast<std::uint64_t>(~value) << 1) | 1);
        }
    }

    constexpr void writeSignedBigEndianInt(std::int32_t value) {
        value = std::byteswap(value);
        write(value);
    }

    constexpr void writeString(std::string_view value) {
        writeUnsignedVarInt(static_cast<std::uint32_t>(value.size()));
        if (!value.empty()) {
            appendBytes(reinterpret_cast<const std::byte*>(value.data()), value.size());
        }
    }

    constexpr void writeLongString(std::string_view value) {
        writeSignedInt(static_cast<std::int32_t>(value.size()));
        writeRawBytes(value);
    }

    constexpr void writeRawBytes(std::string_view rawBuffer) {
        if (!rawBuffer.empty()) {
            appendBytes(reinterpret_cast<const std::byte*>(rawBuffer.data()), rawBuffer.size());
        }
    }

    template <typename T, typename P, typename W>
    constexpr void writeArray(const std::vector<T>& array, P&& prefix, W&& func) {
        using AT = std::remove_cv_t<std::remove_reference_t<traits::member_func_arg_t<P, 0>>>;
        std::invoke(std::forward<P>(prefix), *this, static_cast<AT>(array.size()));
        for (const auto& element : array) {
            if constexpr (std::is_invocable_v<W, BinaryStream&, const T&>) {
                std::invoke(std::forward<W>(func), *this, element);
            } else if constexpr (std::is_invocable_v<W, const T&, BinaryStream&>) {
                std::invoke(std::forward<W>(func), element, *this);
            } else {
                static_assert(traits::always_false_v<T>, "invalid write array function");
            }
        }
    }

    template <typename T, typename W, typename... Args>
    constexpr void writeArray(const std::vector<T>& array, W&& func) {
        writeArray(array, &BinaryStream::writeUnsignedVarInt, std::forward<W>(func));
    }

    template <typename T, typename F>
    constexpr void writeOptional(const std::optional<T>& opt, F&& func) {
        bool hasValue = opt.has_value();
        writeBool(hasValue);
        if (hasValue) {
            if constexpr (std::is_invocable_v<F, BinaryStream&, const T&>) {
                std::invoke(std::forward<F>(func), *this, *opt);
            } else if constexpr (std::is_invocable_v<F, const T&, BinaryStream&>) {
                std::invoke(std::forward<F>(func), *opt, *this);
            } else {
                static_assert(traits::always_false_v<T>, "invalid write optional function");
            }
        }
    }

    template <std::size_t N>
    constexpr void writeBitset(const std::bitset<N>& bitset) {
        if (!bitset.any()) {
            writeByte(0);
            return;
        }

        std::size_t bitIndex{};
        while (true) {
            std::uint8_t byte{};
            for (int i = 0; i < 7; i++) {
                const std::size_t currentBit = bitIndex + static_cast<std::size_t>(i);
                if (currentBit < bitset.size() && bitset.test(currentBit)) {
                    byte |= static_cast<std::uint8_t>(1u << i);
                }
            }

            bool hasHigherBits{};
            for (std::size_t i = bitIndex + 7; i < bitset.size(); i++) {
                if (bitset.test(i)) {
                    hasHigherBits = true;
                    break;
                }
            }

            if (hasHigherBits) {
                byte |= 0x80u;
            }
            writeByte(byte);

            if (!hasHigherBits) {
                return;
            }
            bitIndex += 7;
        }
    }

    template <typename T, typename F>
        requires std::is_enum_v<T>
    constexpr void writeEnum(T value, F&& func) {
        using AT = std::remove_cv_t<std::remove_reference_t<traits::member_func_arg_t<F, 0>>>;
        std::invoke(std::forward<F>(func), *this, static_cast<AT>(value));
    }

    template <typename T, typename P, typename V>
    constexpr void writeVariant(const T& var, P&& prefix, V&& visitor) {
        using AT = std::remove_cv_t<std::remove_reference_t<traits::member_func_arg_t<P, 0>>>;
        std::invoke(std::forward<P>(prefix), *this, static_cast<AT>(var.index()));
        std::visit(
            [this, &visitor](auto&& arg) { std::invoke(std::forward<V>(visitor), std::forward<decltype(arg)>(arg)); },
            var
        );
    }

    template <typename T, typename V>
    constexpr void writeVariant(const T& var, V&& visitor) {
        return writeVariant(var, &BinaryStream::writeUnsignedVarInt, std::forward<V>(visitor));
    }
};

} // namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE
