#include "CtrlTask.h"
#include "MesgTask.h"
#include "MainTask.h"
#include "KeyTask.h"
#include "CommTask.h"
#include "tim.h"
#include "port_device.h"
#include "port_event.h"
#include "string.h"

#define STEEL_BALL_OUTPUT_INTERVAL_MS 500U
#define SERVO1_RELEASE_TIME_MS 200U
#define SERVO1_CENTER_ANGLE 90U

Motor_Hoolle Motor_Hoolle1, Motor_Hoolle2;
Motor_Card Card;
servo_t Servo1;
Switch_Valve Lock_Valve;
static volatile uint8_t Servo1Run = 0U;
/* 与DANJIE一致：最后一次有效控制后200ms停止PWM，避免到位或堵转后持续带力。 */
static uint32_t Servo1LastControlTick = 0U;
static uint8_t Servo1PwmRunning = 0U;

extern Tx_HandleTypeDef Tx1;
extern Scene_t Scene;
extern Event_Handle_t Mesg_event;
extern Event_Handle_t Event;

static inline uint32_t Get_SysTime(void)
{
    return HAL_GetTick();
}

/* 超时释放后，新动作需要重新开启舵机PWM。 */
static void Servo1_StartPwm(void)
{
    if (Servo1PwmRunning == 0U)
    {
        HAL_TIM_PWM_Start(Servo1.htim, Servo1.channel);
        Servo1PwmRunning = 1U;
    }
}

/* 无实际堵转反馈，按最后一次有效角度控制计时释放PWM。 */
static void Ctrl_Servo1(void)
{
    if (Servo1PwmRunning != 0U &&
        (uint32_t)(Get_SysTime() - Servo1LastControlTick) >= SERVO1_RELEASE_TIME_MS)
    {
        HAL_TIM_PWM_Stop(Servo1.htim, Servo1.channel);
        Servo1PwmRunning = 0U;
    }
}

/* 已到最小限位时不刷新计时，持续同向旋转也会在200ms后释放PWM。 */
void Servo1_Decrease(void)
{
    if (Servo1.angle <= Servo1.min_angle)
        return;

    Servo1_StartPwm();
    Servo1.DecreaseAngle(&Servo1, 1U);
    Servo1LastControlTick = Get_SysTime();
}

/* 已到最大限位时不刷新计时，反向有效控制仍可重新启动PWM。 */
void Servo1_Increase(void)
{
    if (Servo1.angle >= Servo1.max_angle)
        return;

    Servo1_StartPwm();
    Servo1.IncreaseAngle(&Servo1, 1U);
    Servo1LastControlTick = Get_SysTime();
}

/* 安卓归中和编码器按下均重新驱动90°，200ms后释放PWM。 */
void Servo1_Reset(void)
{
    Servo1_StartPwm();
    Servo1.SetAngle(&Servo1, SERVO1_CENTER_ANGLE);
    Servo1LastControlTick = Get_SysTime();
}

static void Ctrl_HoolleMotor(Motor_Hoolle *Motor, uint16_t speed, uint8_t dir, uint32_t timeout, uint32_t reverse_time, uint8_t retry_times, void (*Timeout_callbcak)(void))
{
    if (Motor->ManualRun != 0U)
    {
        if (Motor->Motor.state != DEVICE_STATE_BUSY)
        {
            Motor->Motor.SetSpeed(&Motor->Motor, speed, dir);
            Motor->Motor.state = DEVICE_STATE_BUSY;
        }
        return;
    }

    if (Motor->Motor.state == DEVICE_STATE_START)
    {
        Motor->Motor.SetSpeed(&Motor->Motor, speed, dir);
        Motor->Motor.state = DEVICE_STATE_BUSY;
    }

    if (Motor->Motor.state == DEVICE_STATE_STOP)
    {
        Motor->Motor.Stop(&Motor->Motor);
        Motor->Motor.state = DEVICE_STATE_IDLE;
        Motor->Hoolle_num = 0;
        Motor->ClearMode = 0;
    }

    if (Motor->Motor.state == DEVICE_STATE_TIMEOUT)
    {
        if (Motor->Motor.GetRuntime(&Motor->Motor) > HoolleMotorReverse_Time)
        {
            if (Motor->RetryCount < retry_times)
            {
                Motor->Motor.state = DEVICE_STATE_START;
                Motor->RetryCount++;
            }
            else
            {
                Motor->Motor.state = DEVICE_STATE_IDLE;
                Motor->Motor.Stop(&Motor->Motor);

                if (Motor->ClearMode != 0U)
                {
                    Motor->Hoolle_num = 0;
                    Motor->ClearMode = 0;
                }

                if (Timeout_callbcak != NULL)
                    Timeout_callbcak();
            }
        }
    }

    if (Motor->Motor.state == DEVICE_STATE_PAUSE)
    {
        Motor->Motor.LosePower(&Motor->Motor);
        Motor->Motor.ResetRuntime(&Motor->Motor);
    }

    if (Motor->Motor.GetRuntime(&Motor->Motor) > timeout && Motor->Motor.state != DEVICE_STATE_IDLE)
    {
        Motor->Motor.state = DEVICE_STATE_TIMEOUT;
        Motor->Motor.LosePower(&Motor->Motor);
        HAL_Delay(1);
        Motor->Motor.SetSpeed(&Motor->Motor, speed, !dir);
    }
}

static void Ctrl_CardMotor(Motor_Card *Card, uint32_t timeout, void (*Timeout_callbcak)(void))
{
    if (Card->Switch.state == DEVICE_STATE_START)
    {
        Card->Switch.on(&Card->Switch);
        Card->Switch.state = DEVICE_STATE_BUSY;
    }

    if (Card->Switch.state == DEVICE_STATE_STOP)
    {
        Card->Switch.off(&Card->Switch);
        Card->Switch.state = DEVICE_STATE_IDLE;
        Card->Card_num = 0;
    }

    if (Card->Switch.state == DEVICE_STATE_TIMEOUT)
    {
        Card->Switch.off(&Card->Switch);
        Card->Switch.state = DEVICE_STATE_IDLE;
        if (Timeout_callbcak != NULL)
            Timeout_callbcak();
    }

    if (Card->Switch.GetRuntime(&Card->Switch) > CardMotorTimeout_time && Card->Switch.state != DEVICE_STATE_IDLE)
        Card->Switch.state = DEVICE_STATE_TIMEOUT;
}

static void Ctrl_Valve(Switch_Valve *Valve, uint32_t timeout, void (*Timeout_callbcak)(void))
{
    if (Valve->Switch.state == DEVICE_STATE_START)
    {
        Valve->Switch.on(&Valve->Switch);
        Valve->Switch.state = DEVICE_STATE_BUSY;
    }

    if (Valve->Switch.state == DEVICE_STATE_STOP)
    {
        Valve->Switch.off(&Valve->Switch);
        Valve->Switch.state = DEVICE_STATE_IDLE;
    }

    if (Valve->Switch.state == DEVICE_STATE_TIMEOUT)
    {
        Valve->Switch.state = DEVICE_STATE_IDLE;
        Valve->Switch.off(&Valve->Switch);
        if (Timeout_callbcak != NULL)
            Timeout_callbcak();
    }

    if (Valve->Switch.GetRuntime(&Valve->Switch) > timeout && Valve->Switch.state != DEVICE_STATE_IDLE)
        Valve->Switch.state = DEVICE_STATE_TIMEOUT;
}

static void SteelBallMotorTimeout_callback(void)
{
    EventGroupSetBits(&Mesg_event, MesgEvent_SteelBallOutputTimeout);
}

static void EggMotorTimeout_callback(void)
{
    EventGroupSetBits(&Mesg_event, MesgEvent_EggOutputTimeout);
}

static void CardMotorTimeout_callback(void)
{
    EventGroupSetBits(&Mesg_event, MesgEvent_CardOutputTimeout);
}

void Hoolle_Output(Motor_Hoolle *Motor, uint16_t num)
{
    if (Motor->ManualRun != 0U)
    {
        Motor->Hoolle_num = 0U;
        Motor->RetryCount = 0U;
        Motor->ClearMode = 0U;
        return;
    }

    Motor->Hoolle_num += num;
    if (Motor->Hoolle_num != 0U)
    {
        Motor->Motor.state = DEVICE_STATE_START;
        Motor->Motor.runtick = Get_SysTime();
        Motor->RetryCount = 0U;
    }
}

void SteelBall_MotorSwitch(uint8_t enable)
{
    if (enable != 0U)
    {
        Motor_Hoolle1.Hoolle_num = 0U;
        Motor_Hoolle1.RetryCount = 0U;
        Motor_Hoolle1.ClearMode = 0U;
        Motor_Hoolle1.ManualRun = 1U;
        EventGroupClearBits(&Mesg_event, MesgEvent_RemainingSteelBall | MesgEvent_SteelBallOutputTimeout);
        Motor_Hoolle1.Motor.SetSpeed(&Motor_Hoolle1.Motor, HoolleMotor_Speed, HoolleMotor_Dir);
        Motor_Hoolle1.Motor.state = DEVICE_STATE_BUSY;
        return;
    }

    Motor_Hoolle1.ManualRun = 0U;
    Motor_Hoolle1.Hoolle_num = 0U;
    Motor_Hoolle1.RetryCount = 0U;
    Motor_Hoolle1.ClearMode = 0U;
    Motor_Hoolle1.Motor.Stop(&Motor_Hoolle1.Motor);
    Motor_Hoolle1.Motor.state = DEVICE_STATE_IDLE;
}

void Card_Output(Motor_Card *Switch, uint16_t num)
{
    Switch->Card_num += num;
    if (Switch->Card_num != 0U)
    {
        Switch->Switch.state = DEVICE_STATE_START;
        Switch->Switch.runtick = Get_SysTime();
    }
}

void SteelBall_OutputEverySecond(void)
{
    static uint32_t LastOutputTick = 0U;
    uint32_t CurrentTick = Get_SysTime();

    if ((uint32_t)(CurrentTick - LastOutputTick) < STEEL_BALL_OUTPUT_INTERVAL_MS)
        return;

    if (Motor_Hoolle1.ManualRun != 0U ||
        Motor_Hoolle1.Motor.state != DEVICE_STATE_IDLE ||
        Motor_Hoolle1.Hoolle_num != 0U)
        return;

    LastOutputTick = CurrentTick;
    Hoolle_Output(&Motor_Hoolle1, 1U);
}

void Device_Init(void)
{
    Device_Motor_Init(&Motor_Hoolle1.Motor, &htim1, TIM_CHANNEL_1, &htim1, TIM_CHANNEL_2);
    Device_Motor_Init(&Motor_Hoolle2.Motor, &htim1, TIM_CHANNEL_3, &htim1, TIM_CHANNEL_4);
    Device_Switch_Init(&Card.Switch, CardOutput_GPIO_Port, CardOutput_Pin, GPIO_PIN_SET);
    Device_Switch_Init(&Lock_Valve.Switch, GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
    /* 与DANJIE舵机1保持相同的53°~127°限位，上电归中后也执行200ms释放。 */
    Device_Servo_Init(&Servo1, &htim2, TIM_CHANNEL_3, 53, 127, SERVO1_CENTER_ANGLE);
    Servo1PwmRunning = 1U;
    Servo1LastControlTick = Get_SysTime();
    HAL_TIM_Base_Start(&htim7);

    Motor_Hoolle1.Hoolle_num = 0;
    Motor_Hoolle2.Hoolle_num = 0;
    Motor_Hoolle1.RetryCount = 0;
    Motor_Hoolle2.RetryCount = 0;
    Motor_Hoolle1.ClearMode = 0;
    Motor_Hoolle2.ClearMode = 0;
    Motor_Hoolle1.ManualRun = 0;
    Motor_Hoolle2.ManualRun = 0;
    Card.Card_num = 0;
    Servo1Run = 0U;
}

void Servo_SetRun(uint8_t enable)
{
    /*
     * 中文注释：舵机自动旋转只控制自动运行状态。
     * 停止时保留当前目标角度，最后一次有效控制200ms后释放PWM；归中由安卓0x18处理。
     */
    Servo1Run = enable != 0U ? 1U : 0U;
}

static void Servo_AutoRun(servo_t *Servo, uint32_t time)
{
    static uint32_t Time = 0;
    static uint8_t dir = 0;
    uint32_t time_now = HAL_GetTick();

    if (time_now - Time < time)
        return;

    Time = time_now;
    if (dir == 0U)
    {
        if (Servo->angle + 2U >= Servo->max_angle)
        {
            Servo->angle = Servo->max_angle;
            dir = 1U;
        }
        else
        {
            Servo->angle += 2U;
        }
    }
    else
    {
        if (Servo->angle <= Servo->min_angle + 2U)
        {
            Servo->angle = Servo->min_angle;
            dir = 0U;
        }
        else
        {
            Servo->angle -= 2U;
        }
    }

    /* 自动摆动的有效角度变化同样恢复PWM并刷新200ms释放计时。 */
    Servo1_StartPwm();
    Servo->SetAngle(Servo, Servo->angle);
    Servo1LastControlTick = time_now;
}

void CtrlTask(void)
{
    Ctrl_HoolleMotor(&Motor_Hoolle1, HoolleMotor_Speed, HoolleMotor_Dir, HoolleMotorTimeout_time, HoolleMotorReverse_Time, HoolleMotorRetry_Times, SteelBallMotorTimeout_callback);
    Ctrl_HoolleMotor(&Motor_Hoolle2, HoolleMotor2_Speed, HoolleMotor_Dir, HoolleMotorTimeout_time, HoolleMotorReverse_Time, HoolleMotorRetry_Times, EggMotorTimeout_callback);
    Ctrl_CardMotor(&Card, CardMotorTimeout_time, CardMotorTimeout_callback);
    Ctrl_Valve(&Lock_Valve, ValveTimeout_time, NULL);

    Ctrl_Servo1();
    if (Servo1Run != 0U)
        Servo_AutoRun(&Servo1, 75U);
}
