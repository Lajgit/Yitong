#include "KeyTask.h"
#include "CommTask.h"
#include "port_key.h"

#define PLAYER_BUTTON_COUNT 5U

/* 中文注释：玩家按键1~5按原理图依次为PB3~PB7。 */
static GPIO_TypeDef *PlayerButton_Port[PLAYER_BUTTON_COUNT] = {
    PlayerButton1_GPIO_Port,
    PlayerButton2_GPIO_Port,
    PlayerButton3_GPIO_Port,
    PlayerButton4_GPIO_Port,
    PlayerButton5_GPIO_Port,
};

static uint16_t PlayerButton_Pin[PLAYER_BUTTON_COUNT] = {
    PlayerButton1_Pin,
    PlayerButton2_Pin,
    PlayerButton3_Pin,
    PlayerButton4_Pin,
    PlayerButton5_Pin,
};

static Key_HandleTypeDef PlayerButton_Key[PLAYER_BUTTON_COUNT];
static Key_HandleTypeDef *PlayerButton_List[PLAYER_BUTTON_COUNT];
static GPIO_PinState EncoderButton_LastState = GPIO_PIN_SET;
static uint32_t EncoderButton_LastTick = 0U;

extern Tx_HandleTypeDef Tx;

static void PlayerButton_ShortCallback(uint16_t id)
{
    Comm_SendMesg_FillData(&Tx, Ctrl_to_Board, CTRL_REPORT_BUTTON, id + 1U, KEY_EVENT_SHORT);
}

static void PlayerButton_LongCallback(uint16_t id)
{
    Comm_SendMesg_FillData(&Tx, Ctrl_to_Board, CTRL_REPORT_BUTTON, id + 1U, KEY_EVENT_LONG);
}

static void PlayerButton_ReleaseCallback(uint16_t id)
{
    Comm_SendMesg_FillData(&Tx, Ctrl_to_Board, CTRL_REPORT_BUTTON, id + 1U, KEY_EVENT_RELEASE);
}

static void PlayerButton_Init(void)
{
    Key_InitTypeDef init;

    for (uint16_t i = 0; i < PLAYER_BUTTON_COUNT; i++)
    {
        init.debounce_time = KEY_DEBOUNCE_TIME;
        init.longpress_time = KEY_LONG_PRESS_TIME;
        init.trigger_frequnecy = KEY_LONG_TRIGGER_FREQUENCY;
        init.short_callback = PlayerButton_ShortCallback;
        init.long_callback = PlayerButton_LongCallback;
        init.release_callback = PlayerButton_ReleaseCallback;
        init.trigger_level = GPIO_PIN_RESET;
        init.key_id = i;
        init.port = PlayerButton_Port[i];
        init.pin = PlayerButton_Pin[i];
        Key_Init(&PlayerButton_Key[i], init);
        PlayerButton_List[i] = &PlayerButton_Key[i];
    }
}

static void EncoderButton_Task(void)
{
    GPIO_PinState CurrentState = HAL_GPIO_ReadPin(Encoder_K_GPIO_Port, Encoder_K_Pin);
    uint32_t CurrentTick = HAL_GetTick();

    /*
     * 中文注释：PA15编码器按键由板上10k上拉，按下为低。
     * 协议定义的是“按下”事件，因此在稳定的高→低边沿立即上报0x03/0x03、ExpandCode=0x02，
     * 不沿用普通Key_Scan在松开后才触发短按回调的行为。
     */
    if (EncoderButton_LastState == GPIO_PIN_SET &&
        CurrentState == GPIO_PIN_RESET &&
        (uint32_t)(CurrentTick - EncoderButton_LastTick) >= KEY_DEBOUNCE_TIME)
    {
        EncoderButton_LastTick = CurrentTick;
        Comm_SendMesg_FillData(&Tx, Ctrl_to_Board, CTRL_REPORT_ENCODER, 0U, ENCODER_PRESS);
    }

    if (CurrentState != EncoderButton_LastState)
        EncoderButton_LastTick = CurrentTick;

    EncoderButton_LastState = CurrentState;
}

void KeyAll_Init(void)
{
    PlayerButton_Init();
    EncoderButton_LastState = HAL_GPIO_ReadPin(Encoder_K_GPIO_Port, Encoder_K_Pin);
    EncoderButton_LastTick = HAL_GetTick();
}

void Key_Task(void)
{
    Key_Scan(PlayerButton_List, PLAYER_BUTTON_COUNT);
    EncoderButton_Task();
}
