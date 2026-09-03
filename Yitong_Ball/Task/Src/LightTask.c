#include "LightTask.h"
#include "tim.h"

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

static RGB_t Light1_RGBbuffer[Light1_RGBbuffer_SIZE];
static uint8_t Light1_CRRbuffer[Light1_CRRbuffer_SIZE];
static uint8_t BallLedColor[BALL_LED_COUNT] = {0U};
static uint8_t BallLedMode[BALL_LED_COUNT] = {BALL_MODE_OFF, BALL_MODE_OFF, BALL_MODE_OFF, BALL_MODE_OFF, BALL_MODE_OFF};
static uint8_t BallLightness = 5U;
static uint8_t BallLightDirty = 1U;

Semaphore_t Light1_Semaphore = {1U};
Light_t Light1;

void Light_Init(void)
{
    /* 中文注释：球盘5颗WS2812连接PA7/TIM3_CH2。 */
    RGB_Init(&Light1,
             &htim3,
             TIM_CHANNEL_2,
             Light1_RGBbuffer_SIZE,
             Light1_RGBbuffer,
             Light1_CRRbuffer,
             &Light1_Semaphore,
             RGB);
    RegisterLight(ColorLight, &Light1);
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

        /* 中文注释：PA7测试模式每500ms轮流点亮5颗灯，并强制发送一次WS2812数据，便于示波器抓波形。 */
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
}
