#ifndef __COMM_TASK_H__
#define __COMM_TASK_H__

#include "stdint.h"
#include "port_communicate.h"
#include "app_list.h"

/// 版本号
#define VERSION 20260430U

/* 中文注释：控台只保留与主板之间的两种通信方向。 */
#define Board_to_Ctrl 0x02U
#define Ctrl_to_Board 0x03U

/// 主板→控台功能码2
#define CTRL_CMD_VERSION 0x00U
#define CTRL_CMD_DIGITAL_TUBE 0x01U
#define CTRL_CMD_BELT_BRIGHTNESS 0x02U
#define CTRL_CMD_SCENE 0x03U
#define CTRL_CMD_BRIGHTNESS 0x04U

/// 控台→主板功能码2
#define CTRL_REPORT_VERSION 0x00U
#define CTRL_REPORT_BUTTON 0x01U
#define CTRL_REPORT_SPECIAL_BUTTON 0x02U
#define CTRL_REPORT_ENCODER 0x03U

#define KEY_EVENT_SHORT 0x01U
#define KEY_EVENT_LONG 0x02U
#define KEY_EVENT_RELEASE 0x03U
#define ENCODER_LEFT 0x00U
#define ENCODER_RIGHT 0x01U
#define ENCODER_PRESS 0x02U

/// 接收消息结构体
typedef struct
{
    uint8_t Head;
    uint8_t ResendID;
    uint8_t ID;
    uint8_t Code1;
    uint8_t Code2;
    uint8_t Data1;
    uint8_t Data2;
    uint8_t Data3;
    uint8_t Data4;
    uint8_t ACKbyte;
    uint8_t ExpandCode;
    uint8_t CRC16_H;
    uint8_t CRC16_L;
    uint8_t Tail;
} Mesg_TypeDef;

uint8_t Comm_SendMesg_FillData(Tx_HandleTypeDef *Tx, uint8_t code_1, uint8_t code_2, uint32_t data, uint8_t expandCode);
void CommInit(void);
void CommTask(void);

#endif
