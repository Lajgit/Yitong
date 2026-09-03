#include "LightTask.h"
#include "main.h"

#define BALL_LED_COUNT 5U
#define BALL_COLOR_COUNT 9U

#define BALL_MODE_ON 0U
#define BALL_MODE_OFF 1U
#define BALL_MODE_BLINK 2U
#define BALL_MODE_BREATH 3U

#define BALL_LIGHT_REFRESH_INTERVAL 20U
#define BALL_LIGHT_BLINK_INTERVAL 500U
#define BALL_LIGHT_BREATH_PERIOD 2000U
#define BALL_WS2812_RESET_NOP_COUNT 5000U

/* 球盘 PA7 对应 TIM3_CH2，但 STM32F103 的 TIM3_CH2 无可用 DMA 请求，正式版本采用 PA7 普通 GPIO 时序输出 WS2812。 */
#define BALL_WS2812_NOP4()  \
    do                      \
    {                       \
        __NOP();            \
        __NOP();            \
        __NOP();            \
        __NOP();            \
    } while (0)

#define BALL_WS2812_NOP8()  \
    do                      \
    {                       \
        BALL_WS2812_NOP4(); \
        BALL_WS2812_NOP4(); \
    } while (0)

#define BALL_WS2812_NOP16() \
    do                      \
    {                       \
        BALL_WS2812_NOP8(); \
        BALL_WS2812_NOP8(); \
    } while (0)

/* 中文注释：PA7 GPIO bit-bang 时序采用实测已能点亮 WS2812 的延时参数，避免正式版延时过短导致灯珠不识别。 */
#define BALL_LIGHT_DELAY_T0H() \
    do                         \
    {                          \
        BALL_WS2812_NOP16();   \
        BALL_WS2812_NOP4();    \
    } while (0)

#define BALL_LIGHT_DELAY_T0L() \
    do                         \
    {                          \
        BALL_WS2812_NOP16();   \
        BALL_WS2812_NOP16();   \
        BALL_WS2812_NOP16();   \
        BALL_WS2812_NOP4();    \
    } while (0)

#define BALL_LIGHT_DELAY_T1H() \
    do                         \
    {                          \
        BALL_WS2812_NOP16();   \
        BALL_WS2812_NOP16();   \
        BALL_WS2812_NOP16();   \
    } while (0)

#define BALL_LIGHT_DELAY_T1L() \
    do                         \
    {                          \
        BALL_WS2812_NOP16();   \
        BALL_WS2812_NOP4();    \
    } while (0)

#define BALL_LIGHT_GPIO_SET() (BallLight_GPIO_Port->BSRR = BallLight_Pin)
#define BALL_LIGHT_GPIO_CLR() (BallLight_GPIO_Port->BRR = BallLight_Pin)

static uint8_t BallLedColor[BALL_LED_COUNT] = {0U};
static uint8_t BallLedMode[BALL_LED_COUNT] = {
    BALL_MODE_OFF,
    BALL_MODE_OFF,
    BALL_MODE_OFF,
    BALL_MODE_OFF,
    BALL_MODE_OFF,
};
static uint8_t BallLightness = 5U;
static uint8_t BallLightDirty = 1U;

static void BallLight_GpioTimingInit(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    HAL_GPIO_WritePin(BallLight_GPIO_Port, BallLight_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = BallLight_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(BallLight_GPIO_Port, &GPIO_InitStruct);
}

void Light_Init(void)
{
    BallLight_GpioTimingInit();
    BallLightDirty = 1U;
}

void BallLight_SetMode(uint8_t led_id, uint8_t color, uint8_t mode)
{
    if (led_id == 0U || led_id > BALL_LED_COUNT)
    {
        return;
    }

    if (color >= BALL_COLOR_COUNT)
    {
        return;
    }

    if (mode > BALL_MODE_BREATH)
    {
        return;
    }

    BallLedColor[led_id - 1U] = color;
    BallLedMode[led_id - 1U] = mode;
    BallLightDirty = 1U;
}

void BallLight_SetBrightness(uint8_t brightness)
{
    if (brightness > 10U)
    {
        brightness = 10U;
    }

    BallLightness = brightness;
    BallLightDirty = 1U;
}

static uint8_t BallLight_GetRelativeLightness(uint8_t mode, uint32_t tick)
{
    uint32_t phase;

    switch (mode)
    {
        case BALL_MODE_ON:
            return 255U;

        case BALL_MODE_OFF:
            return 0U;

        case BALL_MODE_BLINK:
            return ((tick / BALL_LIGHT_BLINK_INTERVAL) % 2U) == 0U ? 255U : 0U;

        case BALL_MODE_BREATH:
            phase = tick % BALL_LIGHT_BREATH_PERIOD;
            if (phase < (BALL_LIGHT_BREATH_PERIOD / 2U))
            {
                return (uint8_t)((phase * 255U) / (BALL_LIGHT_BREATH_PERIOD / 2U));
            }
            return (uint8_t)(((BALL_LIGHT_BREATH_PERIOD - phase) * 255U) / (BALL_LIGHT_BREATH_PERIOD / 2U));

        default:
            return 0U;
    }
}

static uint8_t BallLight_ScaleColor(uint8_t value, uint8_t relative)
{
    uint32_t scaled;

    scaled = (uint32_t)value * relative * BallLightness;
    scaled /= (255U * 10U);
    if (scaled > 255U)
    {
        scaled = 255U;
    }

    return (uint8_t)scaled;
}

static void BallLight_GpioSendBit(uint8_t bit)
{
    if (bit != 0U)
    {
        BALL_LIGHT_GPIO_SET();
        BALL_LIGHT_DELAY_T1H();
        BALL_LIGHT_GPIO_CLR();
        BALL_LIGHT_DELAY_T1L();
    }
    else
    {
        BALL_LIGHT_GPIO_SET();
        BALL_LIGHT_DELAY_T0H();
        BALL_LIGHT_GPIO_CLR();
        BALL_LIGHT_DELAY_T0L();
    }
}

static void BallLight_GpioSendByte(uint8_t data)
{
    uint8_t mask;

    for (mask = 0x80U; mask != 0U; mask >>= 1U)
    {
        BallLight_GpioSendBit((data & mask) != 0U);
    }
}

static void BallLight_GpioResetDelay(void)
{
    volatile uint32_t delay = BALL_WS2812_RESET_NOP_COUNT;

    BALL_LIGHT_GPIO_CLR();
    while (delay-- != 0U)
    {
        __NOP();
    }
}

static void BallLight_GpioFlush(uint32_t tick)
{
    uint8_t i;
    uint8_t relative;
    RGB_t color;
    uint32_t primask;

    /* 发送 WS2812 数据期间关闭中断，避免位宽被中断打断；发送完成后恢复进入函数前的中断状态。 */
    primask = __get_PRIMASK();
    __disable_irq();

    for (i = 0U; i < BALL_LED_COUNT; i++)
    {
        relative = BallLight_GetRelativeLightness(BallLedMode[i], tick);
        color = Color_table[BallLedColor[i]];

        BallLight_GpioSendByte(BallLight_ScaleColor(color.G, relative));
        BallLight_GpioSendByte(BallLight_ScaleColor(color.R, relative));
        BallLight_GpioSendByte(BallLight_ScaleColor(color.B, relative));
    }

    BALL_LIGHT_GPIO_CLR();

    if (primask == 0U)
    {
        __enable_irq();
    }

    /* WS2812 复位/锁存要求低电平至少 50us，低电平保持不需要继续关中断。 */
    BallLight_GpioResetDelay();
}

void Light_Task(void)
{
    static uint32_t LastRefreshTick = 0U;
    uint32_t CurrentTick;
    uint8_t DynamicMode = 0U;
    uint8_t i;

    CurrentTick = HAL_GetTick();

    for (i = 0U; i < BALL_LED_COUNT; i++)
    {
        if (BallLedMode[i] == BALL_MODE_BLINK || BallLedMode[i] == BALL_MODE_BREATH)
        {
            DynamicMode = 1U;
            break;
        }
    }

    if (BallLightDirty == 0U && DynamicMode == 0U)
    {
        return;
    }

    if (BallLightDirty == 0U && (CurrentTick - LastRefreshTick) < BALL_LIGHT_REFRESH_INTERVAL)
    {
        return;
    }

    BallLight_GpioFlush(CurrentTick);
    BallLightDirty = 0U;
    LastRefreshTick = CurrentTick;
}
