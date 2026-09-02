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

#define LED_Pin GPIO_PIN_13
#define LED_GPIO_Port GPIOC

#define Encoder_A_Pin GPIO_PIN_0
#define Encoder_A_GPIO_Port GPIOA
#define Encoder_B_Pin GPIO_PIN_1
#define Encoder_B_GPIO_Port GPIOA

#define WS2812_1_Pin GPIO_PIN_6
#define WS2812_1_GPIO_Port GPIOA
/* 中文注释：控台原理图第二路WS2812实际接PB0，对应TIM3_CH3。 */
#define WS2812_2_Pin GPIO_PIN_0
#define WS2812_2_GPIO_Port GPIOB

/*
 * 中文注释：控台数码管按原理图继续使用硬件SPI2：
 * PB13=SPI2_SCK/SRCLK1，PB15=SPI2_MOSI/ISER1，PB14=RCLK1锁存，PB12=SEG_OE。
 */
#define SPI2_OE_Pin GPIO_PIN_12
#define SPI2_OE_GPIO_Port GPIOB
#define SPI2_LE_Pin GPIO_PIN_14
#define SPI2_LE_GPIO_Port GPIOB

#define PWM1_Pin GPIO_PIN_8
#define PWM1_GPIO_Port GPIOA
#define PWM2_Pin GPIO_PIN_9
#define PWM2_GPIO_Port GPIOA

#define Encoder_K_Pin GPIO_PIN_15
#define Encoder_K_GPIO_Port GPIOA

/* 中文注释：玩家按键1~5按控台原理图依次接PB3~PB7。 */
#define PlayerButton1_Pin GPIO_PIN_3
#define PlayerButton1_GPIO_Port GPIOB
#define PlayerButton2_Pin GPIO_PIN_4
#define PlayerButton2_GPIO_Port GPIOB
#define PlayerButton3_Pin GPIO_PIN_5
#define PlayerButton3_GPIO_Port GPIOB
#define PlayerButton4_Pin GPIO_PIN_6
#define PlayerButton4_GPIO_Port GPIOB
#define PlayerButton5_Pin GPIO_PIN_7
#define PlayerButton5_GPIO_Port GPIOB

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
