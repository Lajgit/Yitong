#include "DigitalTubeTask.h"
#include "spi.h"

#define DigitalTube_Refresh_Time 100U

static uint8_t DigitalBuffer[4] = {0x82};
DigitalTube_t DigitalTube;

static void DigitalTube_SPItransmit(uint8_t *pData, uint16_t Size)
{
    HAL_SPI_Transmit(&hspi2, pData, Size, 100);
}

void DigitalTubeTask_Init(void)
{
    DigitalTube_Init_t Init;

    /*
     * 中文注释：控台原理图为PB13=SPI2_SCK/SRCLK1、PB15=SPI2_MOSI/ISER1，
     * PB14=RCLK1锁存、PB12=SEG_OE，因此继续使用原硬件SPI2驱动四位数码管。
     */
    Init.Buffer = DigitalBuffer;
    Init.bit_num = sizeof(DigitalBuffer);
    Init.LE_GPIO = SPI2_LE_GPIO_Port;
    Init.LE_Pin = SPI2_LE_Pin;
    Init.CODE_CA = DIGITAL3BIT_CODE_CA;
    Init.spi_transmit = DigitalTube_SPItransmit;
    DigitalTube_Init(&DigitalTube, Init);
    DigitalTube.Set_Num(&DigitalTube, 0, 0, 4);
    DigitalTube.Refresh(&DigitalTube);
}

void DigitalTube_Task(void)
{
    static uint32_t time = 0U;
    if ((uint32_t)(HAL_GetTick() - time) > DigitalTube_Refresh_Time)
    {
        DigitalTube.Refresh(&DigitalTube);
        time = HAL_GetTick();
    }
}
