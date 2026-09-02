/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    dma.c
  * @brief   This file provides code for DMA configuration.
  ******************************************************************************
  */
/* USER CODE END Header */
#include "dma.h"

void MX_DMA_Init(void)
{
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* 中文注释：球盘PA7/TIM3_CH2的WS2812发送使用DMA1_Channel3。 */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
}
