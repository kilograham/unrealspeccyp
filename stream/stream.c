/*
 * Copyright (c) 2023 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>
#include <sys/param.h>
#include "stream.h"
#ifdef ENABLE_COMPRESSED_STREAM
#include "miniz_tinfl.h"
#endif
//#include "mu_alloc.h"
#if PICO_ON_DEVICE
#include "hardware/dma.h"
#include "hardware/structs/xip_ctrl.h"
#endif
#include "hardware/gpio.h"

CU_REGISTER_DEBUG_PINS(xip_stream_dma)

// ---- select at most one ---
//CU_SELECT_DEBUG_PINS(xip_stream_dma)

struct memory_stream {
    struct stream stream;
    const uint8_t *data;
    size_t data_size;
    uint32_t pos;
    bool data_owned;
};

static inline struct memory_stream *to_ms(struct stream *s) {
    assert(s);
    assert(s->funcs->type == memory);
    return (struct memory_stream *)s;
}

int32_t memory_stream_read(struct stream *s, uint8_t *buffer, size_t len, bool all_data_required) {
    struct memory_stream *ms = to_ms(s);
    int32_t avail = ms->data_size - ms->pos;
    if (len > avail) {
        // todo we should hard_assert i.e. bkpt even with NDEBUG
        assert(all_data_required);
        len = avail;
    }
    memcpy(buffer, ms->data + ms->pos, len);
    ms->pos += len;
    return len;
}

const uint8_t *memory_stream_peek(struct stream *s, size_t min_len, size_t *available_len, bool *available_reaches_eos) {
    struct memory_stream *ms = to_ms(s);
    size_t available = ms->data_size - ms->pos;
    // we always reach the end of the stream as it is right there in memory!
    if (available_reaches_eos) *available_reaches_eos = true;
    if (available_len) *available_len = available;
    if (available >= min_len) {
        return ms->data + ms->pos;
    }
    return NULL;
}

void memory_stream_reset(struct stream *s) {
    to_ms(s)->pos = 0;
}

bool memory_stream_skip(struct stream *s, size_t bytes) {
    struct memory_stream *ms = to_ms(s);
    uint32_t max = ms->pos + bytes;
    if (max >= ms->data_size) {
        bool rc = max == ms->data_size;
        ms->pos = ms->data_size;
        return rc;
    } else {
        ms->pos = max;
        return true;
    }
}

void memory_stream_close(struct stream *s) {
    struct memory_stream *ms = to_ms(s);
    if (ms->data_owned) free((void*)ms->data);
    free(ms);
}

uint32_t memory_stream_pos(struct stream *s) {
    return to_ms(s)->pos;
}

bool memory_stream_is_eos(struct stream *s) {
    struct memory_stream *ms = to_ms(s);
    return ms->pos == ms->data_size;
}

const struct stream_funcs memory_stream_funcs = {
    .reset = memory_stream_reset,
    .skip = memory_stream_skip,
    .peek = memory_stream_peek,
    .read = memory_stream_read,
    .close = memory_stream_close,
    .is_eos = memory_stream_is_eos,
#ifndef NDEBUG
    .pos = memory_stream_pos,
#endif
#ifndef NDEBUG
    .type = memory
#endif
};

struct stream *memory_stream_open(const uint8_t *buffer, size_t size, bool own_buffer) {
    struct memory_stream *ms = (struct memory_stream *)calloc(1, sizeof(struct memory_stream));
    ms->stream.funcs = &memory_stream_funcs;
    ms->stream.size = size;
    ms->data = buffer;
    ms->data_size = size;
    ms->data_owned = own_buffer;
    return &ms->stream;
}

#ifdef ENABLE_COMPRESSED_STREAM
#ifdef USE_SINGLE_COMPRESSED_STREAM_INSTANCE
static tinfl_decompressor inflator;
static uint8_t inflator_buffer[MAX_COMPRESSED_STREAM_PEEK + TINFL_LZ_DICT_SIZE];
#else
#error no
#endif
struct compressed_stream {
    struct stream self;
    struct stream *underlying;
    uint8_t *logical_buffer;
    int32_t buf_base_pos;
    int32_t buf_current_index; // index into logical buffer (could be negative down to -MAX_PEEK)
    int32_t buf_end_index;
};

static void compressed_stream_fill(struct compressed_stream *cs) {
    cs->buf_base_pos += cs->buf_end_index; // old buffer complete
    cs->buf_current_index = 0;
    tinfl_status status;
    size_t in_bytes, out_bytes;
    bool eos;
    // todo this isn't ideal; we should really allow returing when there is not enough input available and we have output
    //   however, we need to make sure we preserve the buffer so far exactly in that case
    int decode_offset = 0;
    DEBUG_PINS_SET(xip_stream_dma, 1);
    DEBUG_PINS_XOR(xip_stream_dma, 2);
    DEBUG_PINS_XOR(xip_stream_dma, 2);
    do {
        out_bytes = TINFL_LZ_DICT_SIZE - decode_offset;
        const uint8_t *next_in = stream_peek_avail(cs->underlying, 1, &in_bytes, &eos);
        if (!in_bytes) break;
        assert(next_in);
        status = tinfl_decompress(&inflator, (const mz_uint8 *)next_in, &in_bytes, cs->logical_buffer,
            (mz_uint8 *)cs->logical_buffer + decode_offset, &out_bytes,
            (eos ? 0 : TINFL_FLAG_HAS_MORE_INPUT) |
            TINFL_FLAG_PARSE_ZLIB_HEADER);
        // we should always be deocompressing a whole buffer full unless we are at the end
        assert(status == TINFL_STATUS_DONE || status == TINFL_STATUS_NEEDS_MORE_INPUT || (status == TINFL_STATUS_HAS_MORE_OUTPUT && out_bytes == TINFL_LZ_DICT_SIZE - decode_offset));
        stream_skip(cs->underlying, in_bytes);
        decode_offset += out_bytes;
    } while (status == TINFL_STATUS_NEEDS_MORE_INPUT);
    DEBUG_PINS_SET(xip_stream_dma, 1);
    DEBUG_PINS_XOR(xip_stream_dma, 4);
    DEBUG_PINS_XOR(xip_stream_dma, 4);
    cs->buf_end_index = decode_offset;
}

static inline struct compressed_stream *to_cs(struct stream *s) {
    assert(s);
    assert(s->funcs->type == compressed);
    return (struct compressed_stream *)s;
}

int32_t compressed_stream_read_internal(struct compressed_stream *cs, uint8_t *buffer, size_t len, bool all_data_required) {
    int32_t read = 0;
    int32_t remaining = len;
    int32_t avail = cs->buf_end_index - cs->buf_current_index;
    while (remaining > 0 && avail > 0) {
        int to_copy = MIN(remaining, avail);
        if (buffer) memcpy(buffer + read, cs->logical_buffer + cs->buf_current_index, to_copy);
        read += to_copy;
        cs->buf_current_index += to_copy;
        remaining -= to_copy;
        avail -= to_copy;
        if (!avail) {
            compressed_stream_fill(cs);
            avail = cs->buf_end_index - cs->buf_current_index;
        }
    }
    if (all_data_required) assert(len == read);
    return read;
}

int32_t compressed_stream_read(struct stream *s, uint8_t *buffer, size_t len, bool all_data_required) {
    struct compressed_stream *cs = to_cs(s);
    return compressed_stream_read_internal(cs, buffer, len, all_data_required);
}

const uint8_t *compressed_stream_peek(struct stream *s, size_t min_len, size_t *available_len, bool *available_reaches_eos) {
    assert(min_len < MAX_COMPRESSED_STREAM_PEEK);
    struct compressed_stream *cs = to_cs(s);
    int32_t avail = cs->buf_end_index - cs->buf_current_index;
    if (avail && avail < min_len)
    {
        // todo this is wrong... we should try filling more b4 we move data once
        //  we no longer always fill an entire buffer
        // move the remaining data b4 the main buffer area
        memcpy(cs->logical_buffer - avail, cs->logical_buffer + cs->buf_current_index, avail);
        compressed_stream_fill(cs);
        // back up a bit
        cs->buf_current_index = -avail;
        avail = cs->buf_end_index - cs->buf_current_index;
    }
    if (available_len) *available_len = avail;
    if (available_reaches_eos) *available_reaches_eos = stream_is_eos(cs->underlying);
    if (min_len <= avail) {
        return cs->logical_buffer + cs->buf_current_index;
    } else {
        return NULL;
    }
}

void compressed_stream_reset(struct stream *s) {
    struct compressed_stream *cs = to_cs(s);
    stream_reset(cs->underlying);
    tinfl_init(&inflator);
    cs->buf_base_pos = 0;
    cs->buf_end_index = 0;
    compressed_stream_fill(cs);
}

bool compressed_stream_skip(struct stream *s, size_t bytes) {
    struct compressed_stream *cs = to_cs(s);
    return bytes == compressed_stream_read_internal(cs, NULL, bytes, false);
}

void compressed_stream_close(struct stream *s) {
    struct compressed_stream *cs = to_cs(s);
    stream_close(cs->underlying);
    free(cs);
}

uint32_t compressed_stream_pos(struct stream *s) {
    struct compressed_stream *cs = to_cs(s);
    return cs->buf_base_pos + cs->buf_current_index;
}

bool compressed_stream_is_eos(struct stream *s) {
    struct compressed_stream *cs = to_cs(s);
    return  cs->buf_end_index - cs->buf_current_index != 0;
}

const struct stream_funcs compressed_stream_funcs = {
    .reset = compressed_stream_reset,
    .skip = compressed_stream_skip,
    .peek = compressed_stream_peek,
    .read = compressed_stream_read,
    .close = compressed_stream_close,
    .is_eos = compressed_stream_is_eos,
#ifndef NDEBUG
    .pos = compressed_stream_pos,
#endif
#ifndef NDEBUG
    .type = compressed
#endif
};

struct stream *compressed_stream_open(struct stream *underlying, size_t uncompressed_size) {
    struct compressed_stream *cs = (struct compressed_stream *)calloc(1, sizeof(struct compressed_stream));
    cs->self.funcs = &compressed_stream_funcs;
    cs->self.size = uncompressed_size;
    cs->underlying = underlying;
    cs->logical_buffer = inflator_buffer + MAX_COMPRESSED_STREAM_PEEK;
    compressed_stream_reset(&cs->self);
    return &cs->self;
}
#endif

struct xip_stream {
    struct stream self;
    const uint8_t *src; // word aligned
    int32_t transfer_start;
    int32_t transfer_size;
    uint8_t *buffer; // word aligned
    size_t buffer_size;
    int32_t buf_read;
    int32_t buf_write;
    int32_t buf_count;
    uint32_t buf_read_absolute_offset; // offset within stream is (this + buf_read)
    uint dma_channel; // todo use ptr
    bool transfer_in_progress;
};

static inline struct xip_stream *to_xs(struct stream *s) {
    assert(s);
    assert(s->funcs->type == xip);
    return (struct xip_stream *)s;
}

static inline bool xip_stream_dma_busy(struct xip_stream *xs) {
#if PICO_ON_DEVICE
    return dma_channel_is_busy(xs->dma_channel);
#else
    return false;
#endif
}

static void xip_stream_start_dma(struct xip_stream *xs, const uint8_t *src, uint8_t *dest, uint32_t words) {
    assert(!xs->transfer_in_progress);
    xs->transfer_size = words * 4;
#if PICO_ON_DEVICE
    xip_ctrl_hw->stream_addr = (uintptr_t)src;
    xip_ctrl_hw->stream_ctr = words;
    dma_channel_transfer_to_buffer_now(xs->dma_channel, dest, words);
#else
    memcpy(dest, src, words*4);
#endif
    DEBUG_PINS_SET(xip_stream_dma, 2);
    xs->transfer_in_progress = true;
}

static void xip_stream_cancel_dma(struct xip_stream *xs) {
#if PICO_ON_DEVICE
    while (dma_channel_is_busy(xs->dma_channel));
    xip_ctrl_hw->stream_ctr = 0;
    dma_channel_abort(xs->dma_channel);
    while (!(xip_ctrl_hw->stat & XIP_STAT_FIFO_EMPTY)) {
        xip_ctrl_hw->stream_fifo;
    }
#endif
    DEBUG_PINS_CLR(xip_stream_dma, 2);
    xs->transfer_in_progress = false;
}

// return true if more data was made available
static bool xip_stream_background_fill(struct xip_stream *xs, bool block) {
    bool rc = false;
    bool polling = false;
    if (block) {
        DEBUG_PINS_SET(xip_stream_dma, 4);
    }
    do {
        if (xs->transfer_in_progress) {
            assert(xs->transfer_size);
            // we take as much as we can from the stream in progress
#if PICO_ON_DEVICE
            uint8_t *write_addr = (uint8_t *)dma_channel_hw_addr(xs->dma_channel)->write_addr;
            int write_pos = write_addr - xs->buffer;
            assert(write_pos >= xs->buf_write && write_pos <= xs->buffer_size);
            int new_available = write_pos - xs->buf_write;
#else
            int new_available = xs->transfer_size;
#endif
            if (new_available > 0) {
                if (new_available >= xs->transfer_size) {
                    assert(!xip_stream_dma_busy(xs));
                    assert(new_available == xs->transfer_size);
                    // we have reached the end
                    xs->transfer_in_progress = false;
                DEBUG_PINS_CLR(xip_stream_dma, 2);
                }
                xs->buf_write += new_available;
                if (xs->buf_write == xs->buffer_size) {
                    xs->buf_write = 0;
                }
                xs->buf_count += new_available;
                xs->transfer_start += new_available;
                xs->transfer_size -= new_available;
                rc = true;
            } else {
                // todo check for error ...
                if (!polling) {
                    DEBUG_PINS_SET(xip_stream_dma, 1);
                    polling = true;
                }
            }
        }
        if (!xs->transfer_in_progress) {
            int32_t space_words = (xs->buffer_size - xs->buf_count) >> 2;
            // todo if we used aligned buffer, we can use wrapping DMA
            int32_t linear_space_words = (xs->buffer_size - xs->buf_write) >> 2;
            // adjust for buffer space
            if (space_words < linear_space_words) {
                linear_space_words = space_words;
            }
            //adjust for end of stream
            if (linear_space_words * 4 > (xs->self.size - xs->transfer_start)) {
                linear_space_words = (xs->self.size + 3 - xs->transfer_start) >> 2;
            }
            if (block || linear_space_words > 4 || linear_space_words < space_words) {
                if (linear_space_words) {
                    xip_stream_start_dma(xs, xs->src + xs->transfer_start, xs->buffer + xs->buf_write,
                                         linear_space_words);
                }
            }
        }
    } while (!rc && block && xs->transfer_in_progress);
    if (polling) {
        DEBUG_PINS_CLR(xip_stream_dma, 1);
    }
    if (block) {
        DEBUG_PINS_CLR(xip_stream_dma, 4);
    }
    return rc;
}

int32_t xip_stream_read(struct stream *s, uint8_t *buffer, size_t len, bool all_data_required) {
    struct xip_stream *xs = to_xs(s);
    int32_t remaining = len;
    while (remaining > 0) {
        // do we have any data already
        int to_copy = MIN(remaining, xs->buf_count);
        if (to_copy) {
            if (xs->buf_read + to_copy <= xs->buffer_size) {
                memcpy(buffer, xs->buffer + xs->buf_read, to_copy);
                xs->buf_read += to_copy;
                if (xs->buf_read == xs->buffer_size) {
                    xs->buf_read = 0;
                    xs->buf_read_absolute_offset += xs->buffer_size; // we have progressed another buffer full
                }
            } else {
                int first_fragment_size = xs->buffer_size - xs->buf_read;
                memcpy(buffer, xs->buffer + xs->buf_read, first_fragment_size);
                memcpy(buffer + first_fragment_size, xs->buffer, to_copy - first_fragment_size);
                xs->buf_read = to_copy - first_fragment_size;
                xs->buf_read_absolute_offset += xs->buffer_size; // we have progressed another buffer full
            }
            buffer += to_copy;
            remaining -= to_copy;
            xs->buf_count -= to_copy;
            xip_stream_background_fill(xs, false);
            if (!all_data_required) break;
        } else {
            if (!xip_stream_background_fill(xs, true)) {
                break;
            }
        }
    }
    if (all_data_required) assert(!remaining);
    return len - remaining;
}

void xip_stream_reset(struct stream *s) {
    struct xip_stream *xs = to_xs(s);
    xip_stream_cancel_dma(xs);
    xs->buf_read = xs->buf_write = xs->buf_count = 0;
    xs->buf_read_absolute_offset = 0;
    xs->transfer_start = 0;
    xs->transfer_in_progress = false;
#ifndef NDEBUG
    xs->transfer_size = 0;
#endif
    xip_stream_background_fill(xs, false);
}

bool xip_stream_skip(struct stream *s, size_t len) {
    struct xip_stream *xs = to_xs(s);
    assert(len <= xs->buffer_size);
    while (len != 0) {
        int skip = MIN(xs->buf_count, len);
        if (skip) {
            xs->buf_read += skip;
            len -= skip;
            if (xs->buf_read >= xs->buffer_size) {
                xs->buf_read -= xs->buffer_size;
                xs->buf_read_absolute_offset += xs->buffer_size;
            }
            xs->buf_count -= skip;
        } else {
            if (xs->transfer_in_progress) {
                if (skip > xs->transfer_size) {
                    xip_stream_cancel_dma(xs);
                } else {
                    // not this may make a best effort attempt to return some data without necessarily waiting
                    // for the DMA transfer to complete
                    if (!xip_stream_background_fill(xs, true)) {
                        // there is no more data
                        break;
                    }
                    continue;
                }
            }
            // now we have no transfer in progress and an empty buffer
            assert(!xs->transfer_in_progress);
            assert(!xs->buf_count);
            xs->buf_read_absolute_offset += xs->buf_read;
            xs->buf_read = xs->buf_write = 0;
            skip = MIN(len, xs->self.size - xs->buf_read_absolute_offset);
            len -= skip;
            xs->buf_read_absolute_offset += skip;
            xip_stream_background_fill(xs, false);
        }
    }
    return len == 0;
}

void xip_stream_close(struct stream *s) {
    struct xip_stream *xs = to_xs(s);
    xip_stream_cancel_dma(xs);
    free(xs->buffer);
    free(xs);
}

uint32_t xip_stream_pos(struct stream *s) {
    struct xip_stream *xs = to_xs(s);
    return xs->buf_read + xs->buf_read_absolute_offset;
}

const uint8_t *xip_stream_peek(struct stream *s, size_t min_len, size_t *available_len, bool *eos) {
    assert(min_len == 1); // this isn't general yet because we don't handle split buffers; 1 byte is unsplittable!
    struct xip_stream *xs = to_xs(s);
    if (!xs->buf_count) {
        xip_stream_background_fill(xs, true);
    }
    int avail;
    if (xs->buf_count + xs->buf_read > xs->buffer_size) {
        avail = xs->buffer_size - xs->buf_read;
        if (eos) *eos = false;
        assert(avail > 0);
    } else {
        avail = xs->buf_count;
        if (eos) *eos = xs->self.size == xs->transfer_start;
    }
    if (available_len) *available_len = avail;
    if (min_len <= avail) {
        return xs->buffer + xs->buf_read;
    } else {
        return NULL;
    }
}

bool xip_stream_is_eos(struct stream *s) {
    struct xip_stream *xs = to_xs(s);
    return !xs->buf_count && !xs->transfer_in_progress;
}

const struct stream_funcs xip_stream_funxs = {
        .reset = xip_stream_reset,
        .skip = xip_stream_skip,
        .peek = xip_stream_peek,
        .read = xip_stream_read,
        .close = xip_stream_close,
        .is_eos = xip_stream_is_eos,
#ifndef NDEBUG
        .pos = xip_stream_pos,
#endif
#ifndef NDEBUG
        .type = xip
#endif
};

struct stream *xip_stream_open(const uint8_t *src, size_t size, size_t buffer_size, uint dma_channel) {
    struct xip_stream *xs = (struct xip_stream *)calloc(1, sizeof(struct xip_stream));
#if PICO_ON_DEVICE
    assert(0x1u == (((uintptr_t)src)>>28u));
#endif
    xs->self.funcs = &xip_stream_funxs;
    xs->self.size = size;
    xs->src = src;
    xs->buffer = calloc(1, buffer_size);
    xs->buffer_size = buffer_size;
    xs->dma_channel = dma_channel;

#if PICO_ON_DEVICE
    xip_ctrl_hw->stream_ctr = 0;
    xip_ctrl_hw->stream_addr = (uintptr_t)src;
    dma_channel_config c = dma_channel_get_default_config(dma_channel);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, DREQ_XIP_STREAM);
    dma_channel_set_read_addr(dma_channel, (void *)XIP_AUX_BASE, false);
    dma_channel_set_config(dma_channel, &c, false);
#endif
    xip_stream_reset(&xs->self);
    return &xs->self;
}
