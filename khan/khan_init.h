/*
 * Copyright (c) 2023 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef KHAN_INIT_H
#define KHAN_INIT_H

#include "pico/scanvideo.h"

#ifdef __cplusplus
extern "C" {
#endif

//extern struct khan_image {
//    const char *name;
//    void (*worker)();
//} images[];
//extern const uint image_count;

extern bool no_wait_for_vblank;
extern const struct scanvideo_mode video_mode_khan;

#define VIDEO_KHAN_PROGRAM_NAME "video_khan"

#if PICO_ON_DEVICE
pio_sm_config video_khan_configure_pio(PIO pio, uint sm, uint offset);
bool video_khan_adapt_for_mode(const struct scanvideo_pio_program *program, const struct scanvideo_mode *mode,
                               struct scanvideo_scanline_buffer *missing_scanvideo_scanline_buffer,
                               uint16_t *modifiable_instructions);
bool video_khan_adapt_for_mode54(const struct scanvideo_pio_program *program, const struct scanvideo_mode *mode,
                                 struct scanvideo_scanline_buffer *missing_scanvideo_scanline_buffer,
                                 uint16_t *modifiable_instructions);
#endif

#ifdef __cplusplus
}
#endif

#endif