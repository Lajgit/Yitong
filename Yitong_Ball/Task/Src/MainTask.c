#include "MainTask.h"
#include "CommTask.h"
#include "InterruptTask.h"
#include "KeyTask.h"
#include "LightTask.h"
#include "DigitalTubeTask.h"
#include "port_event.h"
#include "iwdg.h"
#include "tim.h"

#define SYSLIGHT_BREATH_STEP_TIME 10U
#define SYSLIGHT_BREATH_PWM_MAX 1000U
#define SYSLIGHT_BREATH_STEP 5U

Event_Handle_t Event;

void System_Reset(void)
{
    __disable_irq();
    HAL_NVIC_SystemReset();
}

static void SystemLight_Task(void)
{
    static uint32_t LastTick = 0U;
    static uint16_t Duty = 0U;
    static uint8_t Increase = 1U;
    uint32_t CurrentTick = HAL_GetTick();

    if ((uint32_t)(CurrentTick - LastTick) < SYSLIGHT_BREATH_STEP_TIME)
        return;

    LastTick = CurrentTick;

    if (Increase != 0U)
    {
        if (Duty + SYSLIGHT_BREATH_STEP >= SYSLIGHT_BREATH_PWM_MAX)
        {
            Duty = SYSLIGHT_BREATH_PWM_MAX;
            Increase = 0U;
        }
        else
        {
            Duty += SYSLIGHT_BREATH_STEP;
        }
    }
    else
    {
        if (Duty <= SYSLIGHT_BREATH_STEP)
        {
            Duty = 0U;
            Increase = 1U;
        }
        else
        {
            Duty -= SYSLIGHT_BREATH_STEP;
        }
    }

    /* 中文注释：PA15呼吸灯为低电平点亮，TIM2已配置低有效PWM，因此CCR数值可直接表示亮度。 */
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, Duty);
}

void Main_Init(void)
{
    /* 中文注释：球盘初始化主板通信、7路光眼、5颗RGB灯、两块外接数码管和PA15呼吸灯。 */
    CommInit();
    KeyAll_Init();
    Light_Init();
    DigitalTubeTask_Init();

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0U);
    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK)
        Error_Handler();
}

void Main_Task(void)
{
    CommTask();
    HAL_IWDG_Refresh(&hiwdg);
    Key_Task();
    HAL_IWDG_Refresh(&hiwdg);
    Light_Task();
    HAL_IWDG_Refresh(&hiwdg);
    DigitalTube_Task();
    HAL_IWDG_Refresh(&hiwdg);
    SystemLight_Task();
}
