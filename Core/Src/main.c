/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"
#include "rtc.h"
#include <rtthread.h>
#include <rthw.h>
#include "sfud.h"
#include "fal.h"
#include "lfs_port.h"
#include "flashdb.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "tsdb_test.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* ========== FlashDB Related Code ========== */

/* FlashDB KV database object */
static struct fdb_kvdb my_kvdb = {0};

/* Default KV table (auto initialized on first use) */
static struct fdb_default_kv_node default_kv_table[] = {
    {"boot_count", NULL, 0},                          /* Boot counter (integer) */
    {"wifi_ssid", "MyWiFi", sizeof("MyWiFi") - 1},    /* Default WiFi SSID (string) */
    {"wifi_password", "12345678", 8},                 /* Default WiFi password (string) */
    {"volume", NULL, 0},                              /* Volume setting (integer) */
};

/* FlashDB initialization function */
static void flashdb_init_demo(void)
{
    fdb_err_t result;
    struct fdb_default_kv default_kv;

    rt_kprintf("\n========== FlashDB Initialization ==========\n");

    /* Set default KV table */
    default_kv.kvs = default_kv_table;
    default_kv.num = sizeof(default_kv_table) / sizeof(default_kv_table[0]);

    /* Initialize KVDB using FAL partition "fdb_kvdb1" */
    result = fdb_kvdb_init(&my_kvdb, "my_database", "fdb_kvdb1", &default_kv, NULL);

    if (result != FDB_NO_ERR) {
        rt_kprintf("[FlashDB] Init failed, error code: %d\n", result);
        return;
    }

    rt_kprintf("[FlashDB] Init success!\n");
}

/* FlashDB usage demonstration function */
static void flashdb_demo(void)
{
    struct fdb_blob blob;
    int boot_count = 0;
    int volume = 0;

    rt_kprintf("\n========== FlashDB Demo ==========\n");

    /* ========== 1. Read and update boot count ========== */
    rt_kprintf("\n[Step 1] Read and update boot count...\n");

    /* Read boot count */
    fdb_kv_get_blob(&my_kvdb, "boot_count",
                    fdb_blob_make(&blob, &boot_count, sizeof(boot_count)));

    if (blob.saved.len > 0) {
        rt_kprintf("[OK] Current boot count: %d\n", boot_count);
    } else {
        rt_kprintf("[Info] First time use, boot count is 0\n");
        boot_count = 0;
    }

    /* Increase and save boot count */
    boot_count++;
    fdb_kv_set_blob(&my_kvdb, "boot_count",
                    fdb_blob_make(&blob, &boot_count, sizeof(boot_count)));
    rt_kprintf("[OK] Saved new boot count: %d\n", boot_count);

    /* ========== 2. Set and read a test value ========== */
    rt_kprintf("\n[Step 2] Set and read test value...\n");

    /* Write test value */
    volume = 75;
    fdb_kv_set_blob(&my_kvdb, "volume",
                    fdb_blob_make(&blob, &volume, sizeof(volume)));
    rt_kprintf("[OK] Saved test value: %d\n", volume);

    /* Read back and verify */
    volume = 0;
    fdb_kv_get_blob(&my_kvdb, "volume",
                    fdb_blob_make(&blob, &volume, sizeof(volume)));
    if (volume == 75) {
        rt_kprintf("[OK] Read back value: %d\n", volume);
        rt_kprintf("[OK] Verification passed! FlashDB works correctly\n");
    } else {
        rt_kprintf("[Error] Verification failed! Expected 75, got %d\n", volume);
    }

    rt_kprintf("\n========== FlashDB Demo Complete ==========\n");
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
	MX_RTC_Init();              /* 硬件 RTC 初始化，使 RTC 开始走时 */
	fal_init();
	fal_part_test();           /* FAL 分区测试：列出分区 + boot/app 读写验证 */
	boot_kvdb_test();           /* FlashDB KVDB 测试：往 boot 分区写数据 */

  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop - should never reach here */
  /* USER CODE BEGIN WHILE */


  while (1)
  {
    /* USER CODE END WHILE */
		rt_thread_mdelay(1000);
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
