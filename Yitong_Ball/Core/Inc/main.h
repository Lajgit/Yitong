/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  ******************************************************************************
  */
/* USER CODE END Header */
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

void Error_Handler(void);

/* 中文注释：球盘原理图User_Led实际接PA15，通过TIM2_CH1重映射输出呼吸灯PWM。 */
#define LED_Pin GPIO_PIN_15
#define LED_GPIO_Port GPIOA

/* 中文注释：球盘5颗WS2812串联，数据输入接PA7/TIM3_CH2。 */
#define BallLight_Pin GPIO_PIN_7
#define BallLight_GPIO_Port GPIOA

/* 中文注释：球盘本板5组光眼为PB3~PB7。 */
#define BallEye1_Pin GPIO_PIN_3
#define BallEye1_GPIO_Port GPIOB
#define BallEye2_Pin GPIO_PIN_4
#define BallEye2_GPIO_Port GPIOB
#define BallEye3_Pin GPIO_PIN_5
#define BallEye3_GPIO_Port GPIOB
#define BallEye4_Pin GPIO_PIN_6
#define BallEye4_GPIO_Port GPIOB
#define BallEye5_Pin GPIO_PIN_7
#define BallEye5_GPIO_Port GPIOB

/* 中文注释：两块外接单数码管模块的光眼反馈分别进入PA4/PA5。 */
#define BallEyeFB1_Pin GPIO_PIN_4
#define BallEyeFB1_GPIO_Port GPIOA
#define BallEyeFB2_Pin GPIO_PIN_5
#define BallEyeFB2_GPIO_Port GPIOA

/* 中文注释：两块级联SM16306数码管模块使用PB12~PB15软件移位。 */
#define Tube_OE_Pin GPIO_PIN_12
#define Tube_OE_GPIO_Port GPIOB
#define Tube_RCLK_Pin GPIO_PIN_13
#define Tube_RCLK_GPIO_Port GPIOB
#define Tube_SRCLK_Pin GPIO_PIN_14
#define Tube_SRCLK_GPIO_Port GPIOB
#define Tube_SER_Pin GPIO_PIN_15
#define Tube_SER_GPIO_Port GPIOB

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
