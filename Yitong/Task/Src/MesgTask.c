#include "MainTask.h"
#include "MesgTask.h"
#include "CommTask.h"
#include "CtrlTask.h"
#include "port_communicate.h"
#include "port_event.h"

extern Tx_HandleTypeDef Tx1;
extern Motor_Hoolle Motor_Hoolle1;
extern Motor_Hoolle Motor_Hoolle2;
extern Motor_Card Card;
extern ListHandle_t ResendList, DealList;

Event_Handle_t Mesg_event;
extern volatile uint16_t HoolleInputPendingCount;

void Mesg_Task(void)
{
    uint8_t HoolleInputPending = 0U;
    uint32_t Primask;

    if (EventGroupCheckBits(&Mesg_event, MesgEvent_ButtonEnterSetting))
    {
        Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_SettingButton, 0x03U, KEY_EVENT_SHORT);
        EventGroupClearBits(&Mesg_event, MesgEvent_ButtonEnterSetting);
    }

    if (EventGroupCheckBits(&Mesg_event, MesgEvent_Unlock) == true)
    {
        Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_AlreadyUnlock, 0U, 0U);
        EventGroupClearBits(&Mesg_event, MesgEvent_Unlock);
    }

    /* 中文注释：从1ms滤波累计值中原子取出一个进珠事件，串口发送放在临界区外。 */
    Primask = __get_PRIMASK();
    __disable_irq();
    if (HoolleInputPendingCount > 0U)
    {
        HoolleInputPendingCount--;
        HoolleInputPending = 1U;
    }
    __set_PRIMASK(Primask);

    if (HoolleInputPending != 0U)
    {
        Comm_SendMesg_FillData_withResend(
            &Tx1, Board_to_Android, t_HoolleInput, 0U, 0U, &ResendList);
    }

    if (EventGroupCheckBits(&Mesg_event, MesgEvent_CoinInput) == true)
    {
        Comm_SendMesg_FillData_withResend(
            &Tx1, Board_to_Android, t_CoinInput, 0U, 0U, &ResendList);
        EventGroupClearBits(&Mesg_event, MesgEvent_CoinInput);
    }

    /* 中文注释：现行安卓协议0x07的补充位固定0x00，两路珠子超时均按同一协议上报。 */
    if (EventGroupCheckBits(&Mesg_event, MesgEvent_EggOutputTimeout))
    {
        Comm_SendMesg_FillData_withResend(
            &Tx1,
            Board_to_Android,
            t_HoolleOutputTimeOut,
            (uint32_t)Motor_Hoolle2.Hoolle_num,
            REMAIN_TYPE_HOOLLE,
            &ResendList);
        EventGroupClearBits(&Mesg_event, MesgEvent_EggOutputTimeout);
    }

    if (EventGroupCheckBits(&Mesg_event, MesgEvent_SteelBallOutputTimeout))
    {
        Comm_SendMesg_FillData_withResend(
            &Tx1,
            Board_to_Android,
            t_HoolleOutputTimeOut,
            (uint32_t)Motor_Hoolle1.Hoolle_num,
            REMAIN_TYPE_HOOLLE,
            &ResendList);
        EventGroupClearBits(&Mesg_event, MesgEvent_SteelBallOutputTimeout);
    }

    if (EventGroupCheckBits(&Mesg_event, MesgEvent_CardOutputTimeout))
    {
        Comm_SendMesg_FillData_withResend(
            &Tx1,
            Board_to_Android,
            t_CardOutputTimeOut,
            (uint32_t)Card.Card_num,
            REMAIN_TYPE_CARD,
            &ResendList);
        EventGroupClearBits(&Mesg_event, MesgEvent_CardOutputTimeout);
    }

    /* 中文注释：安卓0x05只区分珠子(0x00)和卡片(0x01)，两路珠子统一使用0x00。 */
    if (EventGroupCheckBits(&Mesg_event, MesgEvent_CardOutputOnce))
    {
        Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_RemainingHoolle, (uint32_t)Card.Card_num, REMAIN_TYPE_CARD);
        EventGroupClearBits(&Mesg_event, MesgEvent_CardOutputOnce);
    }

    if (EventGroupCheckBits(&Mesg_event, MesgEvent_RemainingEgg))
    {
        Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_RemainingHoolle, (uint32_t)Motor_Hoolle2.Hoolle_num, REMAIN_TYPE_HOOLLE);
        EventGroupClearBits(&Mesg_event, MesgEvent_RemainingEgg);
    }

    if (EventGroupCheckBits(&Mesg_event, MesgEvent_RemainingSteelBall))
    {
        Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_RemainingHoolle, (uint32_t)Motor_Hoolle1.Hoolle_num, REMAIN_TYPE_HOOLLE);
        EventGroupClearBits(&Mesg_event, MesgEvent_RemainingSteelBall);
    }

    if (EventGroupCheckBits(&Mesg_event, MesgEvent_CardOutputFinish))
    {
        Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_ClearRemainMesg, 0U, 0U);
        EventGroupClearBits(&Mesg_event, MesgEvent_CardOutputFinish);
    }

    /* 兼容旧内部事件：正式安卓0x00版本请求已在CommTask中转发至球盘。 */
    if (EventGroupCheckBits(&Mesg_event, MesgEvent_VersionRequest) == true)
    {
        Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_VersionRequest, (uint32_t)VERSION, 0U);
        EventGroupClearBits(&Mesg_event, MesgEvent_VersionRequest);
    }

    Resend_Task();
    MesgDeal_Task();
}
