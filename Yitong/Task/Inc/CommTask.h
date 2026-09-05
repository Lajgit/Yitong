#ifndef __COMM_TASK_H__
#define __COMM_TASK_H__

#include "stdint.h"
#include "port_communicate.h"
#include "app_list.h"

/// 版本号
#define VERSION_MAJOR  1U
#define VERSION_MINOR  0U
#define VERSION_PATCH  0U
#define VERSION_BUILD  0U

#define VERSION ((VERSION_MAJOR << 24) | \
                 (VERSION_MINOR << 16) | \
                 (VERSION_PATCH << 8)  | \
                  VERSION_BUILD)

/* 中文注释：通信协议功能码1严格按《通信协议_一统天下.xlsx》定义。 */
#define Board_to_Android 0x00U
#define Android_to_Board 0x01U
#define Board_to_Ctrl    0x02U
#define Ctrl_to_Board    0x03U
#define Board_to_Ball    0x04U
#define Ball_to_Board    0x05U

#define ResendTrigger_Time 1000U
#define MesgDeal_Time 250U
#define Max_Resend_Times 3U

/// APP写入RTC备份寄存器，请求Bootloader进入串口升级
#define OTA_REQUEST_MAGIC 0x424F5441U // ASCII "BOTA"

/* 中文注释：安卓0x01吐珠类型，0x00=瓷珠/普通珠，0x01=钢珠。 */
#define HOOLLE_TYPE_NORMAL 0x00U
#define HOOLLE_TYPE_STEEL_BALL 0x01U

/* 中文注释：安卓0x05协议只区分珠子和卡片，不再扩展第三种剩余物品类型。 */
#define REMAIN_TYPE_HOOLLE 0x00U
#define REMAIN_TYPE_CARD 0x01U

#define KEY_EVENT_SHORT 0x01U
#define KEY_EVENT_LONG 0x02U
#define KEY_EVENT_RELEASE 0x03U

/* 兼容现有调试命令，当前正式协议表未列出0x21。 */
#define MOTOR_SWITCH_OFF 0x00U
#define MOTOR_SWITCH_ON 0x01U

/// 主板发送给安卓的消息功能码
#define t_VersionRequest 0x00U
#define t_HoolleInput 0x01U
#define t_CoinInput 0x02U
#define t_Button 0x03U
#define t_SettingButton 0x04U
#define t_RemainingHoolle 0x05U
#define t_HoolleOutputTimeOut 0x07U
#define t_CardOutputTimeOut 0x08U
#define t_AlreadyUnlock 0x0DU
#define t_LightEye 0x0EU
#define t_Encoder 0x0FU
#define t_ClearRemainMesg 0x11U
#define t_SpecialButton 0x15U

/// 主板接收到安卓的消息功能码
#define r_GetVersion 0x00U
#define r_HoolleOutput 0x01U
#define r_CardOutput 0x02U
#define r_CtrlDigitalTube 0x04U
#define r_BallDigitalTube 0x05U
#define r_SceneChange 0x06U
#define r_LightBeltMode 0x07U
#define r_WinChannel 0x08U
#define r_ServoAutoRotate 0x09U
#define r_OutputAllHoolle 0x0BU
#define r_OutputRemainingItem 0x0CU
#define r_ResumeDefultSetting 0x0DU
#define r_SaveSetting 0x0EU
#define r_Unlock 0x10U
#define r_LightControl 0x15U
#define r_CtrlButtonLight 0x17U
#define r_ServoReset 0x18U
#define r_SteelBallMotorSwitch 0x21U
#define r_StopAllDevice 0xFFU
#define r_SystemReset 0xF0U

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

/// 主板→球盘功能码2
#define BALL_CMD_VERSION 0x00U
#define BALL_CMD_DIGITAL_TUBE 0x01U
#define BALL_CMD_RGB_MODE 0x02U
#define BALL_CMD_BRIGHTNESS 0x04U

/// 球盘→主板功能码2
#define BALL_REPORT_VERSION 0x00U
#define BALL_REPORT_EYE 0x01U

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
uint8_t Comm_SendMesg_FillData_withResend(Tx_HandleTypeDef *Tx, uint8_t code_1, uint8_t code_2, uint32_t data, uint8_t expandCode, ListHandle_t *List);

void Resend_Task(void);
void MesgDeal_Task(void);
void CommInit(void);
void CommTask(void);

#endif
