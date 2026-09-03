#include "LightTask.h"
#include "tim.h"
#include "main.h"

#define BALL_LED_COUNT 5U
#define BALL_COLOR_COUNT 9U
#define BALL_MODE_ON 0x00U
#define BALL_MODE_OFF 0x01U
#define BALL_MODE_BLINK 0x02U
#define BALL_MODE_BREATH 0x03U
#define BALL_LIGHT_PA7_PERIOD_TEST 1U
#define BALL_LIGHT_PA7_PERIOD_TEST_INTERVAL 500U
#define BALL_LIGHT_PA7_PERIOD_TEST_COLOR 7U
#define BALL_LIGHT_PA7_PERIOD_TEST_LIGHTNESS 10U
#define BALL_LIGHT_GPIO_TIMING_TEST 1U

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

/* 中文注释：以下延时按72MHz主频粗调，用于绕开TIM3_CH2 DMA限制，直接验证PA7 GPIO能否输出WS2812时序。 */
#define BALL_WS2812_DELAY_T0H() \
    do                          \
    {                           \
        BALL_WS2812_NOP16();    \
        BALL_WS2812_NOP4();     \
    } while (0)

#define BALL_WS2812_DELAY_T0L() \
    do                          \
    {                           \
        BALL_WS2812_NOP16();    \
        BALL_WS2812_NOP16();    \
        BALL_WS2812_NOP16();    \
        BALL_WS2812_NOP4();     \
    } while (0)

#define BALL_WS2812_DELAY_T1H() \
    do                          \
    {                           \
        BALL_WS2812_NOP16();    \
        BALL_WS2812_NOP16();    \
        BALL_WS2812_NOP16();    \
    } while (0)

#define BALL_WS2812_DELAY_T1L() \
    do                          \
    {                           \
        BALL_WS2812_NOP16();    \
        BALL_WS2812_NOP4();     \
    } while (0)

#define BALL_LIGHT_GPIO_SET() (BallLight_GPIO_Port->BSRR = BallLight_Pin)
#define BALL_LIGHT_GPIO_CLR() (BallLight_GPIO_Port->BRR = BallLight_Pin)

static RGB_t Light1_RGBbuffer[Light1_RGBbuffer_SIZE];
static uint8_t Light1_CRRbuffer[Light1_CRRbuffer_SIZE];
static uint8_t BallLedColor[BALL_LED_COUNT] = {0U};
static uint8_t BallLedMode[BALL_LED_COUNT] = {BALL_MODE_OFF, BALL_MODE_OFF, BALL_MODE_OFF, BALL_MODE_OFF, BALL_MODE_OFF};
static uint8_t BallLightness = 5U;
static uint8_t BallLightDirty = 1U;

Semaphore_t Light1_Semaphore = {1U};
Light_t Light1;

static void BallLight_GpioTimingInit(void)
{
#if BALL_LIGHT_GPIO_TIMING_TEST
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = BallLight_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(BallLight_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(BallLight_GPIO_Port, BallLight_Pin, GPIO_PIN_RESET);
#endif
}

void Light_Init(void)
{
    /* 中文注释：球盘5颗WS2812原硬件连接PA7，当前测试版改为PA7普通GPIO时序输出，不依赖TIM3_CH2 DMA。 */
    RGB_Init(&Light1,
             &htim3,
             TIM_CHANNEL_2,
             Light1_RGBbuffer_SIZE,
             Light1_RGBbuffer,
             Light1_CRRbuffer,
             &Light1_Semaphore,
             RGB);
    RegisterLight(ColorLight, &Light1);
    BallLight_GpioTimingInit();
    BallLightDirty = 1U;
}

void BallLight_SetMode(uint8_t led_id, uint8_t color_id, uint8_t mode)
{
    if (led_id < 1U || led_id > BALL_LED_COUNT)
        return;
    if (color_id >= BALL_COLOR_COUNT)
        return;
    if (mode > BALL_MODE_BREATH)
        return;

    /* 中文注释：协议灯珠编号从1开始，内部数组从0开始。 */
    BallLedColor[led_id - 1U] = color_id;
    BallLedMode[led_id - 1U] = mode;
    BallLightDirty = 1U;
}

void BallLight_SetBrightness(uint8_t brightness)
{
    if (brightness > 10U)
        brightness = 10U;

    BallLightness = brightness;
    BallLightDirty = 1U;
}

static uint8_t BallLight_GetRelativeLightness(uint8_t mode, uint32_t tick)
{
    uint32_t phase;

    if (mode == BALL_MODE_OFF)
        return 0U;
    if (mode == BALL_MODE_ON)
        return 255U;
    if (mode == BALL_MODE_BLINK)
        return ((tick / 500U) & 0x01U) != 0U ? 255U : 0U;

    /* 中文注释：呼吸模式使用2秒三角波，不阻塞主循环。 */
    phase = tick % 2000U;
    if (phase < 1000U)
        return (uint8_t)((phase * 255U) / 1000U);

    return (uint8_t)(((2000U - phase) * 255U) / 1000U);
}

static uint8_t BallLight_ScaleColor(uint8_t value, uint8_t absolute, uint8_t relative)
{
    return (uint8_t)(((uint32_t)value * relative * absolute) / 2550U);
}

static void BallLight_GpioSendBit(uint8_t bit)
{
    if (bit != 0U)
    {
        BALL_LIGHT_GPIO_SET();
        BALL_WS2812_DELAY_T1H();
        BALL_LIGHT_GPIO_CLR();
        BALL_WS2812_DELAY_T1L();
    }
    else
    {
        BALL_LIGHT_GPIO_SET();
        BALL_WS2812_DELAY_T0H();
        BALL_LIGHT_GPIO_CLR();
        BALL_WS2812_DELAY_T0L();
    }
}

static void BallLight_GpioSendByte(uint8_t value)
{
    for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1U)
        BallLight_GpioSendBit((value & mask) != 0U ? 1U : 0U);
}

static void BallLight_GpioFlush(uint32_t tick)
{
#if BALL_LIGHT_GPIO_TIMING_TEST
    __disable_irq();

    for (uint8_t i = 0U; i < BALL_LED_COUNT; i++)
    {
        RGB_t color = Color_table[BallLedColor[i]];
        uint8_t relative = BallLight_GetRelativeLightness(BallLedMode[i], tick);
        uint8_t r = BallLight_ScaleColor(color.R, BallLightness, relative);
        uint8_t g = BallLight_ScaleColor(color.G, BallLightness, relative);
        uint8_t b = BallLight_ScaleColor(color.B, BallLightness, relative);

        /* 中文注释：多数WS2812采用GRB字节顺序；白色测试下RGB/GRB无颜色差异。 */
        BallLight_GpioSendByte(g);
        BallLight_GpioSendByte(r);
        BallLight_GpioSendByte(b);
    }

    BALL_LIGHT_GPIO_CLR();
    __enable_irq();
#else
    (void)tick;
#endif
}

static void BallLight_PA7PeriodTest(uint32_t tick)
{
#if BALL_LIGHT_PA7_PERIOD_TEST
    static uint32_t LastTestTick = 0U;
    static uint8_t TestLedIndex = 0U;
    static uint8_t TestStarted = 0U;

    if (TestStarted == 0U || (uint32_t)(tick - LastTestTick) >= BALL_LIGHT_PA7_PERIOD_TEST_INTERVAL)
    {
        for (uint8_t i = 0U; i < BALL_LED_COUNT; i++)
        {
            BallLedColor[i] = BALL_LIGHT_PA7_PERIOD_TEST_COLOR;
            BallLedMode[i] = BALL_MODE_OFF;
        }

        /* 中文注释：测试模式每500ms轮流点亮5颗灯，并强制PA7发送一次WS2812数据，便于示波器抓波形。 */
        BallLedMode[TestLedIndex] = BALL_MODE_ON;
        BallLightness = BALL_LIGHT_PA7_PERIOD_TEST_LIGHTNESS;
        BallLightDirty = 1U;

        TestLedIndex++;
        if (TestLedIndex >= BALL_LED_COUNT)
            TestLedIndex = 0U;

        LastTestTick = tick;
        TestStarted = 1U;
    }
#else
    (void)tick;
#endif
}

void Light_Task(void)
{
    static uint32_t LastRefreshTick = 0U;
    uint32_t CurrentTick = HAL_GetTick();
    uint8_t DynamicMode = 0U;

    BallLight_PA7PeriodTest(CurrentTick);

#if BALL_LIGHT_GPIO_TIMING_TEST
    if (BallLightDirty == 0U)
        return;

    BallLight_GpioFlush(CurrentTick);
    BallLightDirty = 0U;
    LastRefreshTick = CurrentTick;
    return;
#else
    for (uint8_t i = 0U; i < BALL_LED_COUNT; i++)
    {
        if (BallLedMode[i] == BALL_MODE_BLINK || BallLedMode[i] == BALL_MODE_BREATH)
        {
            DynamicMode = 1U;
            break;
        }
    }

    if (BallLightDirty == 0U && DynamicMode == 0U)
        return;
    if (BallLightDirty == 0U && (uint32_t)(CurrentTick - LastRefreshTick) < 20U)
        return;
    if (SemaphoreTake(&Light1_Semaphore) == false)
        return;

    for (uint8_t i = 0U; i < BALL_LED_COUNT; i++)
    {
        uint8_t relative = BallLight_GetRelativeLightness(BallLedMode[i], CurrentTick);
        RGB_SetOneColor(&Light1,
                        i,
                        Color_table[BallLedColor[i]],
                        BallLightness,
                        relative);
    }

    RGB_Flush(&Light1);
    BallLightDirty = 0U;
    LastRefreshTick = CurrentTick;
#endif
}
