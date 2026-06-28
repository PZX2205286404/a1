/*
 * LittleFS Porting Layer for STM32F407 + RT-Thread Nano + FAL
 * Based on official littlefs v2.5.1
 */
#ifndef LFS_PORT_H
#define LFS_PORT_H

#include "lfs.h"
#include "fal.h"

#ifdef __cplusplus
extern "C" {
#endif

int lfs_port_init(void);
void littlefs_test(void);

#ifdef __cplusplus
}
#endif

#endif
