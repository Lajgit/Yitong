#include "DigitalTubeTask.h"

#define DigitalTube_Refresh_Time 25

static uint8_t DigitalBuffer[4] = {0x82};
DigitalTube_t DigitalTube;

void DigitalTubeTask_Init(void)
{
    DigitalTube_Init_t Init;

    Init.hspi = &hspi2;
    Init.Buffer = DigitalBuffer;
    Init.bit_num = sizeof(DigitalBuffer);
    Init.LE_GPIO = Tube_RCLK_GPIO_Port;
    Init.LE_Pin = Tube_RCLK_Pin;
    Init.CODE_CA = DIGITAL3BIT_CODE_CA;

    DigitalTube_Init(&DigitalTube, Init);
    DigitalTube.Set_Num(&DigitalTube, 0, 0, 4);
    DigitalTube.Refresh(&DigitalTube);

    /* 中文注释：首帧数据完成锁存后再拉低SEG_OE，使能数码管输出。 */
    HAL_GPIO_WritePin(Tube_OE_GPIO_Port, Tube_OE_Pin, GPIO_PIN_RESET);
}

void DigitalTube_Task(void)
{
    static uint32_t time = 0;
    if (HAL_GetTick() - time > DigitalTube_Refresh_Time)
    {
        DigitalTube.Refresh(&DigitalTube);
        time = HAL_GetTick();
    }
}
