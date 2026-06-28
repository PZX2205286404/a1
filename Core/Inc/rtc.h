/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rtc.h
  * @brief   This file contains all the function prototypes for
  *          the rtc.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __RTC_H__
#define __RTC_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern RTC_HandleTypeDef hrtc;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_RTC_Init(void);

/* USER CODE BEGIN Prototypes */

/**
 * @brief   从硬件 RTC 读取当前时间，转换为 Unix 秒 (从 2000-01-01 00:00:00 起算)
 * @return  秒数 (32 位)
 * @note    HAL_RTC_GetTime 之后必须调 GetDate 以释放 shadow 寄存器
 */
uint32_t rtc_get_unix_sec(void);

/**
 * @brief   从硬件 RTC 读取当前时间并格式化为 "YYYY.M.D HH:MM:SS"
 * @param   buf   输出缓冲区，至少 20 字节
 * @param   size  缓冲区大小
 * @return  实际写入字符数 (不含 '\0')
 */
int rtc_get_datetime_str(char *buf, int size);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __RTC_H__ */

