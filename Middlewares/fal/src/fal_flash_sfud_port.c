/*
 * Copyright (c) 2020, Armink, <armink.ztl@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fal.h>
#include <sfud.h>

#ifdef FAL_USING_SFUD_PORT

static sfud_flash_t sfud_dev = NULL;

static int init(void);
static int read(long offset, uint8_t *buf, size_t size);
static int write(long offset, const uint8_t *buf, size_t size);
static int erase(long offset, size_t size);

struct fal_flash_dev nor_flash0 =
{
    .name       = "norflash0",
    .addr       = 0,
    .len        = 16 * 1024 * 1024,    /* 16MB W25Q128 */
    .blk_size   = 4096,
    .ops        = {init, read, write, erase},
    .write_gran = 1
};

static int init(void)
{
    /* SFUD ??? */
    if (sfud_init() != SFUD_SUCCESS) {
        return -1;
    }
    
    /* ??? SFUD ????? */
    sfud_dev = sfud_get_device(0);
    
    if (sfud_dev == NULL || !sfud_dev->init_ok)
    {
        return -1;
    }

    /* ?? Flash ??(? SFDP ??????) */
    nor_flash0.blk_size = sfud_dev->chip.erase_gran;
    nor_flash0.len = sfud_dev->chip.capacity;

    return 0;
}

static int read(long offset, uint8_t *buf, size_t size)
{
    if (sfud_dev == NULL || !sfud_dev->init_ok) return -1;
    
    sfud_read(sfud_dev, nor_flash0.addr + offset, size, buf);
    return size;
}

static int write(long offset, const uint8_t *buf, size_t size)
{
    if (sfud_dev == NULL || !sfud_dev->init_ok) return -1;
    
    if (sfud_write(sfud_dev, nor_flash0.addr + offset, size, buf) != SFUD_SUCCESS)
    {
        return -1;
    }
    return size;
}

static int erase(long offset, size_t size)
{
    if (sfud_dev == NULL || !sfud_dev->init_ok) return -1;
    
    if (sfud_erase(sfud_dev, nor_flash0.addr + offset, size) != SFUD_SUCCESS)
    {
        return -1;
    }
    return 0;
}

#endif /* FAL_USING_SFUD_PORT */