#include "InterruptTask.h"
#include "CtrlTask.h"
#include "MesgTask.h"
#include "MainTask.h"
#include "CommTask.h"
#include "port_event.h"
#include "tim.h"

#define COIN_INPUT_DEBOUNCE_TIME 50U
#define HOOLLE_LOW_MIN_COUNT 20U
#define HOOLLE_INPUT_FILTER_MS 3U

volatile uint16_t HoolleInputPendingCount = 0U;

extern Event_Handle_t Mesg_event;
extern Event_Handle_t Event;
extern Motor_Hoolle Motor_Hoolle1, Motor_Hoolle2;
extern Motor_Card Card;
extern Rx_HandleTypeDef Rx1;
extern Rx_HandleTypeDef Rx2;
extern Rx_HandleTypeDef Rx3;

static uint32_t CoinInputLastTick = 0;
static uint8_t CoinInputTriggered = 0;
static uint8_t HoolleInputRawState = 1U;
static uint8_t HoolleInputStableState = 1U;
static uint8_t HoolleInputSameCount = 0U;

void HoolleInput_FilterInit(void)
{
    uint8_t CurrentState =
        HAL_GPIO_ReadPin(HoolleInput_GPIO_Port, HoolleInput_Pin) == GPIO_PIN_SET ? 1U : 0U;

    HoolleInputRawState = CurrentState;
    HoolleInputStableState = CurrentState;
    HoolleInputSameCount = 0U;
}

void HoolleInput_Scan1ms(void)
{
    uint8_t CurrentState =
        HAL_GPIO_ReadPin(HoolleInput_GPIO_Port, HoolleInput_Pin) == GPIO_PIN_SET ? 1U : 0U;

    if (CurrentState == HoolleInputRawState)
    {
        if (HoolleInputSameCount < HOOLLE_INPUT_FILTER_MS)
            HoolleInputSameCount++;
    }
    else
    {
        HoolleInputRawState = CurrentState;
        HoolleInputSameCount = 1U;
    }

    if (HoolleInputSameCount >= HOOLLE_INPUT_FILTER_MS &&
        HoolleInputStableState != HoolleInputRawState)
    {
        HoolleInputStableState = HoolleInputRawState;
        if (HoolleInputStableState == 0U && HoolleInputPendingCount < 0xFFFFU)
            HoolleInputPendingCount++;
    }
}

static void HoolleInput_IRQ(void)
{
}

static void CoinInput_IRQ(void)
{
    uint32_t CurrentTick = HAL_GetTick();

    if (CoinInputTriggered == 0U || CurrentTick - CoinInputLastTick >= COIN_INPUT_DEBOUNCE_TIME)
    {
        CoinInputLastTick = CurrentTick;
        CoinInputTriggered = 1U;
        EventGroupSetBits(&Mesg_event, MesgEvent_CoinInput);
    }
}

static void Hoolle_1_Output_IRQ(void)
{
    static uint8_t LastBallStopped = 0U;
    uint32_t LowCount;

    if (HAL_GPIO_ReadPin(HoolleOutput_1_GPIO_Port, HoolleOutput_1_Pin) == GPIO_PIN_RESET)
    {
        __HAL_TIM_SetCounter(&htim7, 0);
        Motor_Hoolle1.Motor.ResetRuntime(&Motor_Hoolle1.Motor);

        if (Motor_Hoolle1.Hoolle_num == 1U && Motor_Hoolle1.Motor.state == DEVICE_STATE_BUSY)
        {
            Motor_Hoolle1.Motor.Stop(&Motor_Hoolle1.Motor);
            LastBallStopped = 1U;
        }
        else
        {
            LastBallStopped = 0U;
        }
        return;
    }

    LowCount = __HAL_TIM_GetCounter(&htim7);
    if (LowCount > HOOLLE_LOW_MIN_COUNT)
    {
        if (Motor_Hoolle1.Hoolle_num > 0U)
        {
            Motor_Hoolle1.Hoolle_num--;
            Motor_Hoolle1.RetryCount = 0U;
            EventGroupSetBits(&Mesg_event, MesgEvent_RemainingSteelBall);
            if (Motor_Hoolle1.Hoolle_num == 0U && Motor_Hoolle1.Motor.state != DEVICE_STATE_IDLE)
                Motor_Hoolle1.Motor.state = DEVICE_STATE_STOP;
        }
        LastBallStopped = 0U;
        return;
    }

    if (LastBallStopped != 0U && Motor_Hoolle1.Hoolle_num == 1U && Motor_Hoolle1.Motor.state == DEVICE_STATE_BUSY)
        Motor_Hoolle1.Motor.state = DEVICE_STATE_START;

    LastBallStopped = 0U;
}

static void Hoolle_2_Output_IRQ(void)
{
    static uint8_t LastBallStopped = 0U;
    uint32_t LowCount;

    if (HAL_GPIO_ReadPin(HoolleOutput_2_GPIO_Port, HoolleOutput_2_Pin) == GPIO_PIN_RESET)
    {
        __HAL_TIM_SetCounter(&htim7, 0);
        Motor_Hoolle2.Motor.ResetRuntime(&Motor_Hoolle2.Motor);

        if (Motor_Hoolle2.Hoolle_num == 1U && Motor_Hoolle2.Motor.state == DEVICE_STATE_BUSY)
        {
            Motor_Hoolle2.Motor.Stop(&Motor_Hoolle2.Motor);
            LastBallStopped = 1U;
        }
        else
        {
            LastBallStopped = 0U;
        }
        return;
    }

    LowCount = __HAL_TIM_GetCounter(&htim7);
    if (LowCount > 1U)
    {
        if (Motor_Hoolle2.Hoolle_num > 0U)
        {
            Motor_Hoolle2.Hoolle_num--;
            Motor_Hoolle2.RetryCount = 0U;
            EventGroupSetBits(&Mesg_event, MesgEvent_RemainingEgg);
            if (Motor_Hoolle2.Hoolle_num == 0U && Motor_Hoolle2.Motor.state != DEVICE_STATE_IDLE)
                Motor_Hoolle2.Motor.state = DEVICE_STATE_STOP;
        }
        LastBallStopped = 0U;
        return;
    }

    if (LastBallStopped != 0U && Motor_Hoolle2.Hoolle_num == 1U && Motor_Hoolle2.Motor.state == DEVICE_STATE_BUSY)
        Motor_Hoolle2.Motor.state = DEVICE_STATE_START;

    LastBallStopped = 0U;
}

static void CardOutput_IRQ(void)
{
    Card.Switch.ResetRuntime(&Card.Switch);
    if (Card.Card_num > 0U)
    {
        Card.Card_num--;
        EventGroupSetBits(&Mesg_event, MesgEvent_CardOutputOnce);
        if (Card.Card_num == 0U && Card.Switch.state != DEVICE_STATE_IDLE)
        {
            Card.Switch.state = DEVICE_STATE_STOP;
            EventGroupSetBits(&Mesg_event, MesgEvent_CardOutputFinish);
        }
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    switch (GPIO_Pin)
    {
    case HoolleInput_Pin:
        HoolleInput_IRQ();
        break;
    case CoinInput_Pin:
        CoinInput_IRQ();
        break;
    case HoolleOutput_1_Pin:
        Hoolle_1_Output_IRQ();
        break;
    case HoolleOutput_2_Pin:
        Hoolle_2_Output_IRQ();
        break;
    case CardFeedback_Pin:
        CardOutput_IRQ();
        break;
    default:
        break;
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    Rx_HandleTypeDef *Rx = NULL;

    if (huart == Rx1.Handle.huart)
        Rx = &Rx1;
    else if (huart == Rx2.Handle.huart)
        Rx = &Rx2;
    else if (huart == Rx3.Handle.huart)
        Rx = &Rx3;

    if (Rx != NULL)
    {
        /* 中文注释：三路串口统一按单字节中断写入各自环形缓冲区。 */
        Rx->Handle.RingBuf.f_WriteByte(&Rx->Handle.RingBuf, Rx->Handle.temp_data);
        HAL_UART_Receive_IT(huart, &Rx->Handle.temp_data, 1);
    }
}
