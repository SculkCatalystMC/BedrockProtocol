// Copyright © 2026 SculkCatalystMC. All rights reserved.
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#include "sculk/protocol/connection/compression/Zlib.hpp"
#include "sculk/protocol/utility/Result.hpp"
#include <cstdint>
#include <zlib.h>

namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE::compression::zlib {

constexpr std::size_t ZLIB_STREAM_CHUNK          = 65536;
constexpr std::size_t ZLIB_MAX_DECOMPRESSED_SIZE = 64ull * 1024ull * 1024ull;

std::vector<std::byte> compress(std::span<const std::byte> input) {
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree  = Z_NULL;
    strm.opaque = Z_NULL;
    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return std::vector<std::byte>{input.begin(), input.end()};
    }
    strm.next_in  = reinterpret_cast<Bytef*>(const_cast<std::byte*>(input.data()));
    strm.avail_in = static_cast<uInt>(input.size());
    std::vector<std::byte> output;
    Bytef                  out_buffer[ZLIB_STREAM_CHUNK];
    int                    ret;
    do {
        strm.next_out  = out_buffer;
        strm.avail_out = ZLIB_STREAM_CHUNK;
        ret            = deflate(&strm, Z_FINISH);
        if (ret == Z_STREAM_ERROR) break;
        size_t have = ZLIB_STREAM_CHUNK - strm.avail_out;
        output.insert(
            output.end(),
            reinterpret_cast<std::byte*>(out_buffer),
            reinterpret_cast<std::byte*>(out_buffer) + have
        );
    } while (strm.avail_out == 0);
    deflateEnd(&strm);
    return (ret == Z_STREAM_END) ? output : std::vector<std::byte>{input.begin(), input.end()};
}

Result<std::vector<std::byte>> decompress(std::span<const std::byte> input) {
    z_stream zstr;
    zstr.zalloc = Z_NULL;
    zstr.zfree  = Z_NULL;
    zstr.opaque = Z_NULL;
    if (inflateInit2(&zstr, -15) != Z_OK) {
        return error_utils::makeError("Failed to initialize zlib decompression");
    }
    zstr.next_in  = reinterpret_cast<Bytef*>(const_cast<std::byte*>(input.data()));
    zstr.avail_in = static_cast<uInt>(input.size());
    std::vector<std::byte> output;
    Bytef                  out_buffer[ZLIB_STREAM_CHUNK];
    int                    ret;
    do {
        zstr.next_out  = out_buffer;
        zstr.avail_out = ZLIB_STREAM_CHUNK;
        ret            = inflate(&zstr, Z_NO_FLUSH);
        if (ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) break;
        size_t have = ZLIB_STREAM_CHUNK - zstr.avail_out;
        if (output.size() + have > ZLIB_MAX_DECOMPRESSED_SIZE) {
            ret = Z_MEM_ERROR;
            break;
        }
        output.insert(
            output.end(),
            reinterpret_cast<std::byte*>(out_buffer),
            reinterpret_cast<std::byte*>(out_buffer) + have
        );
    } while (zstr.avail_out == 0);
    inflateEnd(&zstr);
    return (ret == Z_STREAM_END) ? Result<std::vector<std::byte>>(output)
                                 : error_utils::makeError("Failed to decompress data");
}

} // namespace sculk::protocol::SCULK_ABI_INLINE_NAMESPACE::compression::zlib