/*
 * Copyright (c) 2023 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef STREAM_H
#define STREAM_H

#include "pico/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_COMPRESSED_STREAM
// statically allocate all state (this is the only option right now!)
#define USE_SINGLE_COMPRESSED_STREAM_INSTANCE
#define MAX_COMPRESSED_STREAM_PEEK 128
#endif

struct stream;

typedef int32_t (*stream_read_func)(struct stream *stream, uint8_t *buffer, size_t len, bool all_data_required);

typedef const uint8_t *(*stream_peek_func)(struct stream *stream, size_t min_len, size_t *available_len, bool *eof);

typedef void (*stream_close_func)(struct stream *stream);

typedef void (*stream_reset_func)(struct stream *stream);

typedef bool (*stream_skip_func)(struct stream *stream, size_t bytes);

typedef bool (*stream_is_eos_func)(struct stream *stream);

#ifndef NDEBUG
// only used by debug atm
typedef uint32_t (*stream_pos_func)(struct stream *stream);
#endif

enum stream_type {
    memory,
    compressed,
    xip
};

struct stream_funcs {
    stream_read_func read;
    stream_peek_func peek;
    stream_close_func close;
    stream_reset_func reset;
    stream_skip_func skip;
    stream_is_eos_func is_eos;
#ifndef NDEBUG
    stream_pos_func pos;
#endif
#ifndef NDEBUG
    enum stream_type type;
#endif
};

struct stream {
    const struct stream_funcs *funcs;
    size_t size;
};

struct memory_stream;
struct compressed_stream;

struct stream *memory_stream_open(const uint8_t *buffer, size_t size, bool free_buffer);

struct stream *xip_stream_open(const uint8_t *buffer, size_t size, size_t buffer_size, uint dma_channel);

#ifdef ENABLE_COMPRESSED_STREAM

struct stream *compressed_stream_open(struct stream *underlying, size_t uncompressed_size);

#endif

// return number of bytes read, -1 for end of file
inline static int32_t stream_read(struct stream *stream, uint8_t *buffer, size_t len, bool all_data_required)
{
    return stream->funcs->read(stream, buffer, len, all_data_required);
}

inline static const uint8_t *stream_peek(struct stream *stream, size_t min_len)
{
    return stream->funcs->peek(stream, min_len, NULL, NULL);
}

inline static const uint8_t *stream_peek_avail(struct stream *stream, size_t min_len, size_t *available_len, bool *eof)
{
    return stream->funcs->peek(stream, min_len, available_len, eof);
}

inline static void stream_reset(struct stream *stream)
{
    stream->funcs->reset(stream);
}

#ifndef NDEBUG
inline static uint32_t stream_pos(struct stream *stream) {
    return stream->funcs->pos(stream);
}
#endif

inline static void stream_close(struct stream *stream)
{
    stream->funcs->close(stream);
}

inline static bool stream_skip(struct stream *stream, size_t size)
{
    return stream->funcs->skip(stream, size);
}

inline static bool stream_is_eos(struct stream *stream)
{
    return stream->funcs->is_eos(stream);
}

#ifdef __cplusplus
}
#endif

#endif //SOFTWARE_STREAM_H