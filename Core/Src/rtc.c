/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rtc.c
  * @brief   This file provides code for the configuration
  *          of the RTC instances.
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
#include "rtc.h"
#include <rtthread.h>

/* USER CODE BEGIN 0 */

/* 判断闰年 (year 为完整 4 位年份) */
static int is_leap_year(uint16_t year)
{
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
        return 1;
    }
    return 0;
}

/* 计算自 2000-01-01 00:00:00 起经过的秒数 */
static uint32_t datetime_to_sec(uint16_t year, uint8_t month, uint8_t day,
                                uint8_t hour, uint8_t min,   uint8_t sec)
{
    /* 每年累计天数 (非闰年) */
    static const uint16_t day_table[] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    uint32_t total_days = 0;
    uint32_t y;

    if (year < 2000) {
        year = 2000;
    }

    /* 累计整年的天数 */
    for (y = 2000; y < year; y++) {
        total_days += 365;
        if (is_leap_year(y)) {
            total_days += 1;
        }
    }

    /* 本年度累计到前一个月的天数 */
    total_days += day_table[month - 1];

    /* 如果当前月份 > 2 且是闰年，多 +1 天 */
    if ((month > 2) && is_leap_year(year)) {
        total_days += 1;
    }

    /* 加上本月已过天数 (注意 1 号 = 已过 0 天, 所以 day - 1) */
    total_days += (day - 1);

    return (uint32_t)(total_days * 86400UL + (uint32_t)hour * 3600UL +
                      (uint32_t)min * 60UL + sec);
}

uint32_t rtc_get_unix_sec(void)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    uint16_t year;
    uint8_t month, day, hour, min, sec;

    /* 必须先 GetTime 再 GetDate, 否则 shadow 寄存器不会释放 */
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    year  = (uint16_t)sDate.Year + 2000; /* STM32 RTC 的 Year 为 0~99 */
    month = (uint8_t)sDate.Month;
    day   = (uint8_t)sDate.Date;
    hour  = (uint8_t)sTime.Hours;
    min   = (uint8_t)sTime.Minutes;
    sec   = (uint8_t)sTime.Seconds;

    return datetime_to_sec(year, month, day, hour, min, sec);
}

int rtc_get_datetime_str(char *buf, int size)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    uint16_t year;
    uint8_t month, day, hour, min, sec;

    if (buf == NULL || size <= 0) {
        return 0;
    }

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    year  = (uint16_t)sDate.Year + 2000;
    month = (uint8_t)sDate.Month;
    day   = (uint8_t)sDate.Date;
    hour  = (uint8_t)sTime.Hours;
    min   = (uint8_t)sTime.Minutes;
    sec   = (uint8_t)sTime.Seconds;

    /* 格式: "YYYY.M.D HH:MM:SS" */
    return rt_snprintf(buf, (size_t)size, "%u.%u.%u %02u:%02u:%02u",
                       (unsigned)year, (unsigned)month, (unsigned)day,
                       (unsigned)hour, (unsigned)min,   (unsigned)sec);
}

/* USER CODE END 0 */

RTC_HandleTypeDef hrtc;

/* RTC init function */
void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */
  /* Check if RTC is already initialized (backup register magic value) */
  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) != 0x5050)
  {
    /* First time initialization - set date/time */
    sTime.Hours = 0x21;      /* 21:05:00 */
    sTime.Minutes = 0x05;
    sTime.Seconds = 0x00;
    sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sTime.StoreOperation = RTC_STOREOPERATION_RESET;
    if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
    {
      Error_Handler();
    }
    sDate.WeekDay = RTC_WEEKDAY_SUNDAY;  /* 2026-06-15 is Sunday */
    sDate.Month = RTC_MONTH_JUNE;
    sDate.Date = 0x15;
    sDate.Year = 0x26;  /* 26 = 2026 - 2000 */

    if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
    {
      Error_Handler();
    }

    /* Mark RTC as initialized */
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 0x5050);
  }
  /* USER CODE END Check_RTC_BKUP */

  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

void HAL_RTC_MspInit(RTC_HandleTypeDef* rtcHandle)
{

  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(rtcHandle->Instance==RTC)
  {
  /* USER CODE BEGIN RTC_MspInit 0 */

  /* USER CODE END RTC_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* Enable PWR clock for backup domain access */
    __HAL_RCC_PWR_CLK_ENABLE();

    /* Unlock backup domain */
    HAL_PWR_EnableBkUpAccess();

    /* RTC clock enable */
    __HAL_RCC_RTC_ENABLE();
  /* USER CODE BEGIN RTC_MspInit 1 */

  /* USER CODE END RTC_MspInit 1 */
  }
}

void HAL_RTC_MspDeInit(RTC_HandleTypeDef* rtcHandle)
{

  if(rtcHandle->Instance==RTC)
  {
  /* USER CODE BEGIN RTC_MspDeInit 0 */

  /* USER CODE END RTC_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_RTC_DISABLE();
  /* USER CODE BEGIN RTC_MspDeInit 1 */

  /* USER CODE END RTC_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
