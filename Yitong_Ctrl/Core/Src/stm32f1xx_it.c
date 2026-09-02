/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    stm32f1xx_it.c
 * @brief   Interrupt Service Routines.
 ******************************************************************************
 */
/* USER CODE END Header */
#include "main.h"
#include "stm32f1xx_it.h"
/* USER CODE BEGIN Includes */
#include "port_lighteffect.h"
#include "LightTask.h"
/* USER CODE END Includes */

extern DMA_HandleTypeDef hdma_tim3_ch1_trig;
extern DMA_HandleTypeDef hdma_tim3_ch3;
extern UART_HandleTypeDef huart3;
extern Light_t Light1, Light2;

void NMI_Handler(void)
{
  while (1)
  {
  }
}

void HardFault_Handler(void)
{
  while (1)
  {
  }
}

void MemManage_Handler(void)
{
  while (1)
  {
  }
}

void BusFault_Handler(void)
{
  while (1)
  {
  }
}

void UsageFault_Handler(void)
{
  while (1)
  {
  }
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
  HAL_IncTick();
  LightEffectTimer_ISR();
}

void DMA1_Channel2_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_tim3_ch3);
  /* 中文注释：PB0/TIM3_CH3为控台第二路WS2812。 */
  RGB_FinishCallback(&Light2, &hdma_tim3_ch3);
}

void DMA1_Channel6_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_tim3_ch1_trig);
  RGB_FinishCallback(&Light1, &hdma_tim3_ch1_trig);
}

void USART3_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart3);
}
