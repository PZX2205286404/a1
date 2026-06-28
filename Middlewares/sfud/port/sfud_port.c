/*
 * SFUD Portable Interface for STM32F407 + HAL + RT-Thread
 */

#include <sfud.h>
#include <stdarg.h>
#include "spi.h"
#include "gpio.h"
#include <rtthread.h>

static char log_buf[256];

/* CS ??: PB14 */
#define SFUD_CS_PORT    GPIOB
#define SFUD_CS_PIN     GPIO_PIN_14

static void cs_take(void)
{
    HAL_GPIO_WritePin(SFUD_CS_PORT, SFUD_CS_PIN, GPIO_PIN_RESET);
}

static void cs_release(void)
{
    HAL_GPIO_WritePin(SFUD_CS_PORT, SFUD_CS_PIN, GPIO_PIN_SET);
}

/**
 * SPI write then read
 */
static sfud_err spi_write_read(const sfud_spi *spi, const uint8_t *write_buf, size_t write_size, uint8_t *read_buf,
        size_t read_size) {
    sfud_err result = SFUD_SUCCESS;
    size_t i;

    if (write_size == 0 && read_size == 0) {
        return SFUD_SUCCESS;
    }

    cs_take();

    /* ??? */
    for (i = 0; i < write_size; i++) {
        SPI1_ReadWriteByte(write_buf[i]);
    }

    /* ??? */
    for (i = 0; i < read_size; i++) {
        read_buf[i] = SPI1_ReadWriteByte(0xFF);
    }

    cs_release();

    return result;
}

#ifdef SFUD_USING_QSPI
static sfud_err qspi_read(const struct __sfud_spi *spi, uint32_t addr, sfud_qspi_read_cmd_format *qspi_read_cmd_format,
        uint8_t *read_buf, size_t read_size) {
    return SFUD_ERR_NOT_FOUND;
}
#endif

/* ????: 1 ?? */
static void retry_delay_1us(void)
{
    volatile uint32_t delay = 20;  // HCLK=168MHz ?? 1us
    while(delay--);
}

/* ???(?????????) */
static void spi_lock(const sfud_spi *spi) {
    __disable_irq();    // ??????!
}

static void spi_unlock(const sfud_spi *spi) {
    __enable_irq();     // ??????
}

sfud_err sfud_spi_port_init(sfud_flash *flash) {
    sfud_err result = SFUD_SUCCESS;

    /* ?? SPI ???? */
    flash->spi.wr = spi_write_read;
    flash->spi.lock = spi_lock;      // ???
    flash->spi.unlock = spi_unlock;
    flash->spi.user_data = NULL;
    
    /* ?????? */
    flash->retry.delay = retry_delay_1us;
    flash->retry.times = 100000;     // ????

#ifdef SFUD_USING_QSPI
    flash->spi.qspi_read = qspi_read;
#endif

    return result;
}

void sfud_log_debug(const char *file, const long line, const char *format, ...) {
    va_list args;
    va_start(args, format);
    rt_kprintf("[SFUD](%s:%ld) ", file, line);
    vsnprintf(log_buf, sizeof(log_buf), format, args);
    rt_kprintf("%s\n", log_buf);
    va_end(args);
}

void sfud_log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    rt_kprintf("[SFUD]");
    vsnprintf(log_buf, sizeof(log_buf), format, args);
    rt_kprintf("%s\n", log_buf);
    va_end(args);
}
