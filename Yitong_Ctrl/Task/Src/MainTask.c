#include "MainTask.h"
#include "CommTask.h"
#include "FlashTask.h"
#include "InterruptTask.h"
#include "KeyTask.h"
#include "LightTask.h"
#include "MesgTask.h"
#include "port_event.h"
#include "DigitalTubeTask.h"
#include "iwdg.h"
#include "tim.h"

#define SYSLIGHT_BLINK_TIME 500U
#define EncoderCheckTime 100U

Scene_t Scene = SCENE_LESSLIGHT;
Event_Handle_t Event;

extern Tx_HandleTypeDef Tx;

void System_Reset(void)
{
    __disable_irq();
    HAL_NVIC_SystemReset();
}

static void SystemLight_Task(void)
{
    static uint32_t time = 0;
    if (HAL_GetTick() - time > SYSLIGHT_BLINK_TIME)
    {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        time = HAL_GetTick();
    }
}

static void Encoder_Task(void)
{
    static uint16_t last_CNT = 0;
    static uint32_t time = 0;
    uint32_t current_time = HAL_GetTick();

    if (current_time - time > EncoderCheckTime)
    {
        uint16_t now_CNT = __HAL_TIM_GetCounter(&htim2);

        if (now_CNT > last_CNT)
        {
            Comm_SendMesg_FillData(&Tx, Ctrl_to_Board, CTRL_REPORT_ENCODER, 0U, ENCODER_LEFT);
        }
        else if (now_CNT < last_CNT)
        {
            Comm_SendMesg_FillData(&Tx, Ctrl_to_Board, CTRL_REPORT_ENCODER, 0U, ENCODER_RIGHT);
        }

        last_CNT = now_CNT;
        /* 中文注释：每次检查后更新时间戳，避免旧代码进入主循环后每次都重复检查。 */
        time = current_time;
    }
}

void Main_Init(void)
{
    CommInit();
    KeyAll_Init();
    Light_Init();
    DigitalTubeTask_Init();
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_1);
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_2);
}

void Main_Task(void)
{
    CommTask();
    HAL_IWDG_Refresh(&hiwdg);
    Key_Task();
    HAL_IWDG_Refresh(&hiwdg);
    Light_Task();
    HAL_IWDG_Refresh(&hiwdg);
    Mesg_Task();
    HAL_IWDG_Refresh(&hiwdg);
    DigitalTube_Task();
    HAL_IWDG_Refresh(&hiwdg);
    SystemLight_Task();
    Encoder_Task();
}
