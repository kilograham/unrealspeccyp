/*
 * Copyright (c) 2023 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef ROMS_H
#define ROMS_H

#include <stdint.h>
#include "../../std.h"

typedef struct {
    const char *name;
    const uint8_t *data_z;
    const uint data_z_size;
    const uint data_size;
} embedded_rom_t;

extern embedded_rom_t embedded_roms[];
extern int embedded_rom_count;
#endif
