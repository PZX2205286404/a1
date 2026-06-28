/*
 * LittleFS Porting Layer for STM32F407 + RT-Thread Nano + FAL
 */

#define LFS_NO_ASSERT
#define LFS_NO_DEBUG
#define LFS_NO_WARN
#define LFS_NO_ERROR

#include "lfs.h"
#include "lfs_port.h"
#include <fal.h>
#include <rtthread.h>
#include <string.h>

static lfs_t lfs;
static lfs_file_t file_buffer;
static const struct fal_partition *part;

#define BS 4096
#define CS 256
#define LS 128

static uint8_t rb[CS];
static uint8_t pb[CS];
static uint8_t lb[LS];

static int lfs_read(const struct lfs_config *c, lfs_block_t b, lfs_off_t o, void *buf, lfs_size_t s) {
    return fal_partition_read(part, b * BS + o, buf, s) < 0 ? LFS_ERR_IO : LFS_ERR_OK;
}

static int lfs_prog(const struct lfs_config *c, lfs_block_t b, lfs_off_t o, const void *buf, lfs_size_t s) {
    return fal_partition_write(part, b * BS + o, buf, s) < 0 ? LFS_ERR_IO : LFS_ERR_OK;
}

static int lfs_erase(const struct lfs_config *c, lfs_block_t b) {
    return fal_partition_erase(part, b * BS, BS) < 0 ? LFS_ERR_IO : LFS_ERR_OK;
}

static int lfs_sync(const struct lfs_config *c) {
    return LFS_ERR_OK;
}

static const struct lfs_config cfg = {
    .read = lfs_read,
    .prog = lfs_prog,
    .erase = lfs_erase,
    .sync = lfs_sync,
    .read_size = 256,
    .prog_size = 256,
    .block_size = BS,
    .block_count = 256,
    .cache_size = CS,
    .lookahead_size = LS,
    .read_buffer = rb,
    .prog_buffer = pb,
    .lookahead_buffer = lb,
    .block_cycles = 100,
};

int lfs_port_init(void) {
    part = fal_partition_find("fdb_kvdb1");
    if (!part) {
        rt_kprintf("[LFS] Partition not found\n");
        return -1;
    }

    int r = lfs_mount(&lfs, &cfg);
    if (r) {
        rt_kprintf("[LFS] Formatting...\n");
        fal_partition_erase(part, 0, 4*BS);
        r = lfs_format(&lfs, &cfg);
        if (r) {
            rt_kprintf("[LFS] Format failed\n");
            return -2;
        }
        r = lfs_mount(&lfs, &cfg);
        if (r) {
            rt_kprintf("[LFS] Mount failed\n");
            return -3;
        }
    }
    return 0;
}

void lfs_port_deinit(void) {
    lfs_unmount(&lfs);
}

int lfs_write_file(const char *name, const void *data, uint32_t size) {
    int r = lfs_file_open(&lfs, &file_buffer, name, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (r) return r;

    lfs_ssize_t written = lfs_file_write(&lfs, &file_buffer, data, size);
    lfs_file_close(&lfs, &file_buffer);

    return written == size ? 0 : -1;
}

int lfs_read_file(const char *name, void *data, uint32_t max_size) {
    int r = lfs_file_open(&lfs, &file_buffer, name, LFS_O_RDONLY);
    if (r) return r;

    lfs_ssize_t read = lfs_file_read(&lfs, &file_buffer, data, max_size);
    lfs_file_close(&lfs, &file_buffer);

    return read;
}

void littlefs_test(void) {
    rt_kprintf("\n=== LittleFS Test ===\n");

    if (lfs_port_init() != 0) {
        rt_kprintf("Init failed\n");
        return;
    }
    rt_kprintf("Init OK\n");

    const char *original_content = "this is a test!";
    rt_kprintf("\nOriginal: %s\n", original_content);
    rt_kprintf("Length: %d bytes\n\n", strlen(original_content));

    rt_kprintf("1. Write to Flash...\n");
    int r = lfs_write_file("test.txt", original_content, strlen(original_content));
    if (r != 0) {
        rt_kprintf("Write failed!\n");
        lfs_port_deinit();
        return;
    }
    rt_kprintf("Write OK!\n\n");

    rt_kprintf("2. Read from Flash...\n");
    char buffer[64] = {0};
    r = lfs_read_file("test.txt", buffer, sizeof(buffer));
    if (r <= 0) {
        rt_kprintf("Read failed!\n");
        lfs_port_deinit();
        return;
    }
    rt_kprintf("Read OK!\n\n");

    rt_kprintf("========================================\n");
    rt_kprintf("Content read from Flash:\n");
    rt_kprintf("%s\n", buffer);
    rt_kprintf("========================================\n\n");

    if (strcmp(original_content, buffer) == 0) {
        rt_kprintf("Verify: PASSED\n");
    } else {
        rt_kprintf("Verify: FAILED\n");
    }

    lfs_port_deinit();
    rt_kprintf("\n=== Test Done ===\n\n");
}
