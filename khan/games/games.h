/*
 * Copyright (c) 2023 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef GAMES_H
#define GAMES_H

#define GF_SLOW_TAPE 1
#define GF_NO_WAIT_VBLANK 2

typedef struct {
    const char *name;
    const char *ext;
    const uint8_t *data_z;
    const uint data_z_size;
    const uint data_size;
    uint8_t flags;
} embedded_game_t;

extern embedded_game_t embedded_games[];
extern int embedded_game_count;
extern int embedded_game_default; // can default to -1
#endif
