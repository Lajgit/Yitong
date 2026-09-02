#include "MainTask.h"
#include "CommTask.h"
#include "CtrlTask.h"
#include "FlashTask.h"
#include "InterruptTask.h"
#include "KeyTask.h"
#include "MesgTask.h"
#include "port_event.h"
#include "DigitalTubeTask.h"
#include "iwdg.h"

#define SYSLIGHT_BLINK_TIME 500U

/* 兼容旧工程中仍参与编译但已退出运行路径的灯效模块。 */
Scene_t Scene = IdleScene;
Event_Handle_t Event;

void System_Reset(void)
{
    __disable_irq();
    HAL_NVIC_SystemReset();
}

static void SystemLight_Task(void)
{
    static uint32_t time = 0U;
    if (HAL_GetTick() - time > SYSLIGHT_BLINK_TIME)
    {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        time = HAL_GetTick();
    }
}

void Main_Init(void)
{
    FlashTask_Init();
    /* 中文注释：CommInit同时初始化USART1安卓、USART2球盘、USART3控台三条协议链路。 */
    CommInit();
    Device_Init();
    HoolleInput_FilterInit();
    KeyAll_Init();

    /* 中文注释：保留主板原理图现有本地数码管接口，不参与安卓0x04/0x05的分板转发。 */
    DigitalTubeTask_Init();
}

void Main_Task(void)
{
    CommTask();
    HAL_IWDG_Refresh(&hiwdg);
    FlashTask();
    HAL_IWDG_Refresh(&hiwdg);
    Key_Task();
    HAL_IWDG_Refresh(&hiwdg);
    CtrlTask();
    HAL_IWDG_Refresh(&hiwdg);
    Mesg_Task();
    HAL_IWDG_Refresh(&hiwdg);
    DigitalTube_Task();
    SystemLight_Task();
}
