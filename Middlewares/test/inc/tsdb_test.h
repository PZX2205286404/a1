/*
 * @file    tsdb_test.h
 * @brief   FlashDB TSDB test interface
 */

#ifndef __TSDB_TEST_H__
#define __TSDB_TEST_H__

#include <stdint.h>

/* ============ 核心接口 ============ */

/**
 * @brief   init TSDB test module
 * @note    must call after fal_init()
 */
void tsdb_test_init(void);

/**
 * @brief   save a message to TSDB
 * @param   msg     message content
 * @param   len     message length (bytes)
 * @return  0=ok, negative=error
 */
int tsdb_test_save(const char *msg, uint16_t len);

/**
 * @brief   dump all messages (oldest -> newest)
 */
void tsdb_test_dump_all(void);

/**
 * @brief   format TSDB partition (erase all data)
 * @note    WARNING: will erase entire fdb_tsdb1 partition
 */
void tsdb_test_format(void);

/**
 * @brief   直接擦除指定 FAL 分区 (不依赖 FlashDB)
 * @param   part_name   分区名 (如 "fdb_tsdb1", "fdb_kvdb1", "filesystem")
 *                      传 NULL 则默认擦除 "fdb_tsdb1"
 * @return  0=ok, negative=error
 * @note    擦除后需重新调用 tsdb_test_init() 才能继续使用 TSDB
 */
int tsdb_test_erase_flash(const char *part_name);

/* ============ 预定义测试 ============ */

/**
 * @brief   run all predefined test cases
 */
void tsdb_test_run_all(void);

/**
 * @brief   test: save fault message
 */
void tsdb_test_fault_msg(void);

/**
 * @brief   test: save normal message
 */
void tsdb_test_normal_msg(void);

/**
 * @brief   start a periodic task: auto send -> save to Flash -> dump all
 * @note    must call after tsdb_test_init()
 */
void tsdb_test_start_sender(void);

/* ============ FAL 分区测试 ============ */

/**
 * @brief   测试 FAL 分区是否正常：列出所有分区 + 对 boot/app 做读写验证
 * @note    must call after fal_init()
 */
void fal_part_test(void);

#endif /* __TSDB_TEST_H__ */
