/*
 * @file    tsdb_test.c
 * @brief   FlashDB TSDB test module
 */

#include "tsdb_test.h"
#include "fal.h"
#include "flashdb.h"
#include "rtc.h"
#include "sfud.h"
#include <rtthread.h>
#include <string.h>

/* ============ config ============ */

#define TSDB_PART_NAME      "fdb_tsdb1"
#define TSDB_NAME           "test_tsdb"
#define TSDB_MAX_LOG_LEN    256
#define DT_BUF_SIZE         32

/* ============ FlashDB KVDB for boot partition ============ */

#define BOOT_KVDB_NAME      "boot_kvdb"
#define BOOT_PART_NAME      "boot"

static struct fdb_kvdb  s_boot_kvdb;
static bool             s_boot_kvdb_ok = false;

static struct fdb_default_kv_node s_boot_default_kv[] = {
    {"boot_count", NULL, 0},
    {"version",   "v1.0.0", sizeof("v1.0.0") - 1},
};

/* 前向声明 */
static fdb_time_t tsdb_test_get_time(void);
static void unix_sec_to_datetime(uint32_t sec, char *buf, int size);

static void boot_kvdb_test(void)
{
    struct fdb_blob blob;
    char msg_buf[128] = {0};
    char verify_buf[128] = {0};
    char dt_buf[DT_BUF_SIZE];
    int32_t msg_count = 0;
    fdb_err_t err;
    size_t read_len;
    uint32_t now;

    rt_kprintf("\n========== KVDB Test (boot partition) ==========\n");

    /* ========== KVDB 初始化 ========== */
    rt_kprintf("\n[boot/KVDB] Initializing on boot partition...\n");

    struct fdb_default_kv default_kv;
    default_kv.kvs = s_boot_default_kv;
    default_kv.num = sizeof(s_boot_default_kv) / sizeof(s_boot_default_kv[0]);

    err = fdb_kvdb_init(&s_boot_kvdb, BOOT_KVDB_NAME, BOOT_PART_NAME, &default_kv, NULL);
    if (err != FDB_NO_ERR) {
        rt_kprintf("[boot/KVDB] Init failed (err=%d), try erasing and retry...\n", err);
        const struct fal_partition *part = fal_partition_find(BOOT_PART_NAME);
        if (part != NULL) {
            fal_partition_erase_all(part);
        }
        err = fdb_kvdb_init(&s_boot_kvdb, BOOT_KVDB_NAME, BOOT_PART_NAME, &default_kv, NULL);
        if (err != FDB_NO_ERR) {
            rt_kprintf("[boot/KVDB] Init still failed, err=%d\n", err);
            return;
        }
    }
    s_boot_kvdb_ok = true;
    rt_kprintf("[boot/KVDB] Init OK\n");

    /* 读取已存的消息 */
    read_len = fdb_kv_get_blob(&s_boot_kvdb, "msg", fdb_blob_make(&blob, msg_buf, sizeof(msg_buf) - 1));
    if (read_len > 0) {
        msg_buf[read_len] = '\0';
        rt_kprintf("[boot/KVDB] Current msg: %s\n", msg_buf);
        if (sscanf(msg_buf, "this is No.%ld message", &msg_count) == 1) {
            rt_kprintf("[boot/KVDB] Parsed count: %ld\n", (long)msg_count);
        } else {
            msg_count = 0;
        }
    } else {
        rt_kprintf("[boot/KVDB] No message found, starting from 0\n");
        msg_count = 0;
    }

    /* 生成新消息（带时间戳）并写入 KVDB */
    msg_count++;
    now = (uint32_t)rtc_get_unix_sec();
    unix_sec_to_datetime(now, dt_buf, sizeof(dt_buf));
    rt_snprintf(msg_buf, sizeof(msg_buf), "this is No.%d message | %s", msg_count, dt_buf);
    fdb_kv_set_blob(&s_boot_kvdb, "msg", fdb_blob_make(&blob, msg_buf, strlen(msg_buf)));
    rt_kprintf("[boot/KVDB] Saved new msg: %s\n", msg_buf);

    /* 读出来验证 */
    memset(verify_buf, 0, sizeof(verify_buf));
    read_len = fdb_kv_get_blob(&s_boot_kvdb, "msg", fdb_blob_make(&blob, verify_buf, sizeof(verify_buf) - 1));
    if (read_len > 0) {
        verify_buf[read_len] = '\0';
        rt_kprintf("[boot/KVDB] Read back: %s\n", verify_buf);
        if (strcmp(msg_buf, verify_buf) == 0) {
            rt_kprintf("[boot/KVDB] Verification OK!\n");
        } else {
            rt_kprintf("[boot/KVDB] Verification FAILED! mismatch\n");
        }
    }

    rt_kprintf("\n========== Test Complete ==========\n\n");
}

/* ============ global variables ============ */

static struct fdb_tsdb  s_tsdb;
static bool             s_init_ok = false;

/**
 * @brief  基于硬件 RTC 的精确延时（等待 RTC 秒数达到目标）
 * @param  sec   要等待的秒数
 * @note   不依赖 RT-Thread tick，与硬件 RTC 时间严格同步
 */
static void rtc_delay_sec(uint32_t sec)
{
    uint32_t start = rtc_get_unix_sec();
    while ((rtc_get_unix_sec() - start) < sec) {
        /* 等待 RTC 秒数递增 */
    }
}

/**
 * @brief  判断是否为闰年 (4 位完整年份)
 */
static int is_leap_year_loc(uint16_t year)
{
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
        return 1;
    }
    return 0;
}

/**
 * @brief  FlashDB 要求的时间获取回调 → 改为从硬件 RTC 读取 Unix 秒
 */
static fdb_time_t tsdb_test_get_time(void)
{
    return (fdb_time_t)rtc_get_unix_sec();
}

/**
 * @brief  把 FlashDB 存的 Unix 秒 (以 2000-01-01 为基准) 转换为可读时间
 *         输出格式: "YYYY.M.D HH:MM:SS"
 */
static void unix_sec_to_datetime(uint32_t sec, char *buf, int size)
{
    if (buf == NULL || size <= 0) {
        return;
    }

    /* 累计每年天数表 (非闰年 + 闰年表)
     * month_day[m] = 到 m 月底 (m 从 0~12) 的累计天数
     * month_day[0] = 0, month_day[1] = 31, month_day[2] = 59 ...
     */
    static const uint16_t month_day[] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365
    };

    uint32_t rem = sec;
    uint16_t year = 2000;
    uint8_t  month = 1;
    uint8_t  day;
    uint8_t  hour, min, s;
    uint32_t days;

    /* 先算出 时:分:秒 */
    s    = (uint8_t)(rem % 60);
    rem /= 60;
    min  = (uint8_t)(rem % 60);
    rem /= 60;
    hour = (uint8_t)(rem % 24);
    days = rem / 24;

    /* 再算 年-月-日 */
    while (1) {
        uint32_t year_days = is_leap_year_loc(year) ? 366 : 365;
        if (days < year_days) {
            break;
        }
        days -= year_days;
        year++;
    }

    /* days 现在是当前年份内的 "第几天" (从 0 开始) */
    {
        uint8_t m;
        for (m = 1; m <= 12; m++) {
            /* month_day[m] = 到 m 月底的累计天数 (非闰年) */
            uint32_t md = month_day[m];
            if (m > 2 && is_leap_year_loc(year)) {
                md += 1;
            }
            if (days < md) {
                break;
            }
        }
        /* 退出循环后 m 为目标月份 (1-based) */
        month = m;
    }

    {
        /* 计算当月已过天数：days - 上月末累计天数 */
        uint32_t prev = month_day[month - 1];
        if (month > 2 && is_leap_year_loc(year)) {
            prev += 1;
        }
        day = (uint8_t)(days - prev + 1);  /* 1-based */
    }

    rt_snprintf(buf, (size_t)size, "%u.%u.%u %02u:%02u:%02u",
                (unsigned)year, (unsigned)month, (unsigned)day,
                (unsigned)hour, (unsigned)min,  (unsigned)s);
}

static bool iter_print_cb(fdb_tsl_t tsl, void *arg)
{
    uint8_t buf[TSDB_MAX_LOG_LEN + 1];
    char    dt_buf[DT_BUF_SIZE];
    struct fdb_blob blob;

    memset(buf, 0, sizeof(buf));

    fdb_tsl_to_blob(tsl, fdb_blob_make(&blob, buf, TSDB_MAX_LOG_LEN));
    fdb_blob_read((fdb_db_t)&s_tsdb, &blob);

    unix_sec_to_datetime((uint32_t)tsl->time, dt_buf, sizeof(dt_buf));

    rt_kprintf("[READ] time=%s  len=%u  data: ",
               dt_buf,
               (unsigned)tsl->log_len);
    for (uint32_t i = 0; i  < tsl->log_len; i++) {
        rt_kprintf("%c", buf[i]);
    }
    rt_kprintf("\n");

    (void)arg;
    return false;
}

/* ============ public interface ============ */

void tsdb_test_init(void)
{
    fdb_err_t err;

    const struct fal_partition *part = fal_partition_find(TSDB_PART_NAME);
    if (part == NULL) {
        rt_kprintf("[TSDB] ERROR: partition '%s' not found!\n", TSDB_PART_NAME);
        return;
    }

    err = fdb_tsdb_init(&s_tsdb, TSDB_NAME, TSDB_PART_NAME,
                        tsdb_test_get_time, TSDB_MAX_LOG_LEN, NULL);

    if (err != FDB_NO_ERR) {
        rt_kprintf("[TSDB] init failed, err=%d\n", err);
        return;
    }

    s_init_ok = true;
    rt_kprintf("[TSDB] init OK (time source: hardware RTC)\n");
}

int tsdb_test_save(const char *msg, uint16_t len)
{
    struct fdb_blob blob;
    fdb_err_t err;
    char dt_buf[DT_BUF_SIZE];

    if (!s_init_ok || msg == NULL || len == 0 || len > TSDB_MAX_LOG_LEN) {
        return -1;
    }

    err = fdb_tsl_append(&s_tsdb, fdb_blob_make(&blob, (void *)msg, len));
    if (err != FDB_NO_ERR) {
        return -2;
    }

    unix_sec_to_datetime((uint32_t)s_tsdb.last_time, dt_buf, sizeof(dt_buf));
    rt_kprintf("[TSDB] SAVE OK  time=%s  len=%u\n", dt_buf, len);
    return 0;
}

void tsdb_test_dump_all(void)
{
    if (!s_init_ok) {
        return;
    }
    rt_kprintf("\n[TSDB] ===== dump all =====\n");
    fdb_tsl_iter(&s_tsdb, iter_print_cb, NULL);
    rt_kprintf("[TSDB] ===== dump end =====\n\n");
}

void tsdb_test_format(void)
{
    if (!s_init_ok) {
        return;
    }
    rt_kprintf("[TSDB] clearing...\n");
    s_init_ok = false;
    tsdb_test_init();
    rt_kprintf("[TSDB] clear done!\n");
}

/* ============ Flash 擦除 ============ */

int tsdb_test_erase_flash(const char *part_name)
{
    const struct fal_partition *part;
    int rc;

    if (part_name == NULL) {
        part_name = TSDB_PART_NAME;
    }

    /* 1) 查找分区 */
    part = fal_partition_find(part_name);
    if (part == NULL) {
        rt_kprintf("[Flash Erase] ERROR: partition '%s' not found!\n", part_name);
        return -1;
    }

    rt_kprintf("[Flash Erase] target: '%s'  addr=0x%08X  size=%u bytes\n",
               part_name,
               (unsigned)part->offset,
               (unsigned)part->len);

    /* 2) 如果当前 TSDB 正在使用这个分区，先把它置为未初始化 */
    if (s_init_ok && strcmp(part_name, TSDB_PART_NAME) == 0) {
        s_init_ok = false;
        fdb_tsdb_deinit(&s_tsdb);
    }

    /* 3) 擦除整个分区 */
    rc = fal_partition_erase_all(part);
    if (rc < 0) {
        rt_kprintf("[Flash Erase] ERROR: erase failed, rc=%d\n", rc);
        return -2;
    }

    rt_kprintf("[Flash Erase] '%s' erased OK!\n", part_name);
    return 0;
}

/* ============ RT-Thread Shell 命令: 直接在串口输入 erase_tsdb 即可调用 ============ */

static void cmd_erase_flash(int argc, char **argv)
{
    const char *name = NULL;

    if (argc >= 2) {
        name = argv[1];
    }

    if (tsdb_test_erase_flash(name) == 0) {
        rt_kprintf("[cmd] OK. Call tsdb_test_init() again before saving.\n");
    }
}
MSH_CMD_EXPORT_ALIAS(cmd_erase_flash, erase_flash, erase a FAL partition);

static void cmd_erase_tsdb(void)
{
    tsdb_test_erase_flash(TSDB_PART_NAME);
}
MSH_CMD_EXPORT_ALIAS(cmd_erase_tsdb, erase_tsdb, erase the TSDB partition);

/* ============ 预定义测试用例 ============ */

void tsdb_test_run_all(void)
{
    if (!s_init_ok) {
        rt_kprintf("[TSDB] not init!\n");
        return;
    }

    rt_kprintf("\n[TSDB] ===== TSDB TEST =====\n");

    tsdb_test_fault_msg();
    rtc_delay_sec(1);                   /* 等 1 秒，使 normal msg 的时间戳不同 */
    tsdb_test_normal_msg();
    tsdb_test_dump_all();

    rt_kprintf("[TSDB] ===== TEST END =====\n\n");
}

void tsdb_test_fault_msg(void)
{
    const char *msg = "[FAULT] v2.0.3 Zxkj Temp HIGH";
    tsdb_test_save(msg, (uint16_t)strlen(msg));
}

void tsdb_test_normal_msg(void)
{
    const char *msg = "[OK] v2.0.3 Zxkj Temp=23C Curr=0.3A";
    tsdb_test_save(msg, (uint16_t)strlen(msg));
}

/* ============ 持续发消息的任务线程 ============ */

/* 周期性地:发一条消息 → 存到 Flash → 把所有消息读出来打印 */
static void tsdb_sender_thread(void *arg)
{
    uint32_t msg_cnt = 0;
    char     msg_buf[64];
    int      len;

    (void)arg;

    rt_kprintf("[TSDB] sender task started. send a message every 2 seconds.\n");

    while (1) {
        /* 1) 组装要发送的消息,每次带不同序号 */
        msg_cnt++;
        len = rt_snprintf(msg_buf, sizeof(msg_buf),
                          "this is a test message, id=%lu", (unsigned long)msg_cnt);

        /* 2) 模拟"串口发来的消息",直接存到 Flash */
        rt_kprintf("\n[TSDB] ---- save msg #%lu ----\n",
                   (unsigned long)msg_cnt);
        tsdb_test_save(msg_buf, (uint16_t)len);

        /* 3) 把 Flash 里目前存的所有消息读出来打印 */
        tsdb_test_dump_all();

        /* 4) 延时 2 秒 (基于硬件 RTC，确保与 RTC 时间同步) */
        rtc_delay_sec(2);
    }
}

/* 在 tsdb_test_init() 成功后调这个函数启动任务线程 */
void tsdb_test_start_sender(void)
{
    rt_thread_t tid;

    if (!s_init_ok) {
        rt_kprintf("[TSDB] tsdb_test_init() must call first!\n");
        return;
    }

    /* 动态创建一个线程
       名称:    "tsdb_sender"
       入口函数: tsdb_sender_thread
       栈大小:  1024 字节
       优先级:  12 (数字越大优先级越低)
       时间片:  20 tick
    */
    tid = rt_thread_create("tsdb_sender",
                           tsdb_sender_thread,
                           NULL,
                           1024,
                           12,
                           20);

    if (tid != NULL) {
        rt_thread_startup(tid);
        rt_kprintf("[TSDB] sender task started OK\n");
    } else {
        rt_kprintf("[TSDB] sender task create failed!\n");
    }
}

/* ============ FAL 分区测试 ============ */

void fal_part_test(void)
{
    const struct fal_partition *table;
    const struct fal_partition *part;
    size_t len, i;
    const char *names[] = {"boot", "app", "fdb_tsdb1", "fdb_kvdb1"};
    int all_ok = 1;

    rt_kprintf("\n========== FAL Partition Test ==========\n");

    table = fal_get_partition_table(&len);
    if (table == NULL || len == 0) {
        rt_kprintf("[Part Test] ERROR: partition table is empty!\n");
        return;
    }

    rt_kprintf("\n[Part List] total %u partitions:\n", (unsigned)len);
    for (i = 0; i < len; i++) {
        rt_kprintf("  [%u] %-12s  flash=%-12s  offset=0x%08X  size=%8u (%u KB)\n",
                   (unsigned)i,
                   table[i].name,
                   table[i].flash_name,
                   (unsigned)table[i].offset,
                   (unsigned)table[i].len,
                   (unsigned)(table[i].len / 1024));
    }

    rt_kprintf("\n[Partition Exist Check]\n");
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        part = fal_partition_find(names[i]);
        if (part == NULL) {
            rt_kprintf("  [FAIL] %-12s NOT FOUND!\n", names[i]);
            all_ok = 0;
        } else {
            rt_kprintf("  [OK]   %-12s  offset=0x%08X  size=%u KB\n",
                       names[i],
                       (unsigned)part->offset,
                       (unsigned)(part->len / 1024));
        }
    }

    if (all_ok) {
        rt_kprintf("\n[Result] All partitions exist and accessible!\n");
    } else {
        rt_kprintf("\n[Result] Some partitions are missing!\n");
    }

    rt_kprintf("\n========== FAL Partition Test End ==========\n\n");
}

MSH_CMD_EXPORT_ALIAS(fal_part_test, fal_test, test all FAL partitions);
