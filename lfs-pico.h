// https://www.raspberrypi.org/forums/viewtopic.php?t=313009

/* Copyright (C) 1883 Thomas Edison - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the BSD 3 clause license, which unfortunately
 * won't be written for another century.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A little flash file system for the Raspberry Pico
 *
 */

#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "hardware/sync.h" 
#include "pico/stdlib.h"

#include "littlefs/lfs.h"

// 1M of space for file system at top of pico flash
#define FS_SIZE (1024 * 1024)

// hal function prototypes
static int pico_read(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void* buffer,
                     lfs_size_t size);
static int pico_prog(const struct lfs_config* c, lfs_block_t block, lfs_off_t off,
                     const void* buffer, lfs_size_t size);
static int pico_erase(const struct lfs_config* c, lfs_block_t block);
static int pico_sync(const struct lfs_config* c);

// configuration of the filesystem is provided by this struct
// for Pico: prog size = 256, block size = 4096, so cache is 8K
// minimum cache = block size, must be multiple
static const struct lfs_config cfg = {
    // block device operations
    .read = pico_read,
    .prog = pico_prog,
    .erase = pico_erase,
    .sync = pico_sync,
    // block device configuration
    .read_size = 1,
    .prog_size = FLASH_PAGE_SIZE,
    .block_size = FLASH_SECTOR_SIZE,
    .block_count = FS_SIZE / FLASH_SECTOR_SIZE,
    .block_cycles = 500,
    .cache_size = FLASH_SECTOR_SIZE,
    .lookahead_size = 16,
};

// Pico specific hardware abstraction functions

// file system offset in flash
static const uint32_t fs_base = PICO_FLASH_SIZE_BYTES - FS_SIZE;

static int pico_read(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void* buffer,
                     lfs_size_t size) {
    // read flash via XIP mapped space
    uint8_t* p = (uint8_t*)(XIP_NOCACHE_NOALLOC_BASE + fs_base + (block * FLASH_SECTOR_SIZE) + off);
    memcpy(buffer, p, size);
    return 0;
}

static int pico_prog(const struct lfs_config* c, lfs_block_t block, lfs_off_t off,
                     const void* buffer, lfs_size_t size) {
    uint32_t p = (block * FLASH_SECTOR_SIZE) + off;
    // program with SDK
    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(fs_base + p, (uint8_t*) buffer, size);
    restore_interrupts(ints);
    return 0;
}

static int pico_erase(const struct lfs_config* c, lfs_block_t block) {
    uint32_t off = block * FLASH_SECTOR_SIZE;
    // erase with SDK
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(fs_base + off, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
    return 0;
}

static int pico_sync(const struct lfs_config* c) {
    // nothing to do!
    return 0;
}
