/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    tim.c
  * @brief   This file provides code for the configuration
  *          of the TIM instances.
  ******************************************************************************
  */
/* USER CODE END Header */
#include "tim.h"

/* 球盘 PA7 对应 TIM3_CH2，但 STM32F103 的 TIM3_CH2 无可用 DMA 请求，正式版本不再初始化 TIM3 驱动 WS2812。 */
