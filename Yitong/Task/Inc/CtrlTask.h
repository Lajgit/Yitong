#ifndef __CTRLTASK_H__
#define __CTRLTASK_H__

#include "port_device.h"

#define HoolleMotorTimeout_time 3000
#define CardMotorTimeout_time 3000
#define HoolleMotorReverse_Time 300
#define HoolleMotorRetry_Times 3
#define ValveTimeout_time 800

/* 钢珠电机1速度设为80，按数量出珠和手动运行均使用此参数。 */
#define HoolleMotor_Speed 80
#define HoolleMotor2_Speed 80
#define HoolleMotor_Dir 0

typedef struct
{
    motor_t Motor;
    volatile uint16_t Hoolle_num;
    volatile uint8_t RetryCount;
    volatile uint8_t ClearMode;
    volatile uint8_t ManualRun;
} Motor_Hoolle;

typedef struct
{
    switch_t Switch;
    volatile uint16_t Card_num;
    volatile uint8_t RetryCount;
} Motor_Card;

typedef struct
{
    switch_t Switch;
    volatile uint8_t TriggerCount;
} Switch_Valve;

void Device_Init(void);
void CtrlTask(void);
void Hoolle_Output(Motor_Hoolle *Motor, uint16_t num);
void SteelBall_MotorSwitch(uint8_t enable);
void SteelBall_OutputEverySecond(void);
void Card_Output(Motor_Card *Switch, uint16_t num);
void Servo_SetRun(uint8_t enable);
/* 舵机有效控制后200ms释放PWM，已到限位的同向操作不刷新计时。 */
void Servo1_Decrease(void);
void Servo1_Increase(void);
void Servo1_Reset(void);

#endif
