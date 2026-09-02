/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
  ******************************************************************************
  */
/* USER CODE END Header */
#include "gpio.h"

void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_AFIO_CLK_ENABLE();

  /* 中文注释：PB3/PB4作为球盘光眼输入，PA15作为呼吸灯PWM，关闭JTAG并保留SWD。 */
  __HAL_AFIO_REMAP_SWJ_NOJTAG();

  HAL_GPIO_WritePin(Tube_OE_GPIO_Port, Tube_OE_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(Tube_RCLK_GPIO_Port, Tube_RCLK_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(Tube_SRCLK_GPIO_Port, Tube_SRCLK_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(Tube_SER_GPIO_Port, Tube_SER_Pin, GPIO_PIN_RESET);

  /* 中文注释：PA15呼吸灯由TIM2_CH1重映射为复用推挽输出，在tim.c中初始化。 */

  /* 中文注释：PB3~PB7和PA4/PA5均由原理图外围电阻提供电平，MCU不额外上下拉。 */
  GPIO_InitStruct.Pin = BallEye1_Pin | BallEye2_Pin | BallEye3_Pin | BallEye4_Pin | BallEye5_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = BallEyeFB1_Pin | BallEyeFB2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = Tube_OE_Pin | Tube_RCLK_Pin | Tube_SRCLK_Pin | Tube_SER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}
