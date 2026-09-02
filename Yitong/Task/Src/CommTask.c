#include "CommTask.h"
#include "MesgTask.h"
#include "CtrlTask.h"
#include "KeyTask.h"
#include "MainTask.h"
#include "FlashTask.h"
#include "app_crc.h"
#include "app_list.h"
#include "string.h"
#include "usart.h"

#define Mesg_Head 0xAA
#define Mesg_Tail 0x55

static void USART_RequestMesg(Tx_HandleTypeDef *Tx, Mesg_TypeDef *mesg);
static bool Board_WriteBootRequest(uint32_t request_magic);
static void Board_SystemRestart(bool enter_bootloader);
static bool USART_ReceiveMesg_Verify(void *self, void *mesg);
static uint32_t Mesg_GetData32(const Mesg_TypeDef *mesg);

ListHandle_t ResendList, DealList;
static ListNode_t ResendList_buffer[100];
static ListNode_t DealList_buffer[100];

static Mesg_TypeDef MesgTable[256];
static uint8_t rx1_buffer[512];
static uint8_t rx2_buffer[256];
static uint8_t rx3_buffer[256];
static Mesg_TypeDef Receive1_mesg;
static Mesg_TypeDef Receive2_mesg;
static Mesg_TypeDef Receive3_mesg;

Tx_HandleTypeDef Tx1;
Tx_HandleTypeDef Tx2;
Tx_HandleTypeDef Tx3;
Rx_HandleTypeDef Rx1;
Rx_HandleTypeDef Rx2;
Rx_HandleTypeDef Rx3;

extern Event_Handle_t Mesg_event;
extern Motor_Card Card;
extern Motor_Hoolle Motor_Hoolle1;
extern Motor_Hoolle Motor_Hoolle2;
extern Switch_Valve Lock_Valve;
extern ListHandle_t ResendList, DealList;

static uint32_t Mesg_GetData32(const Mesg_TypeDef *mesg)
{
    return ((uint32_t)mesg->Data1 << 24) |
           ((uint32_t)mesg->Data2 << 16) |
           ((uint32_t)mesg->Data3 << 8) |
           (uint32_t)mesg->Data4;
}

static bool Board_WriteBootRequest(uint32_t request_magic)
{
    uint32_t timeout;
    uint32_t retry;

    __HAL_RCC_PWR_CLK_ENABLE();
    __DSB();
    (void)RCC->APB1ENR;
    HAL_PWR_EnableBkUpAccess();

    timeout = 100000U;
    while (((PWR->CR & PWR_CR_DBP) == 0U) && (timeout > 0U))
        timeout--;

    if (timeout == 0U)
        return false;

    __HAL_RCC_RTC_ENABLE();
    __DSB();
    (void)RCC->BDCR;

    for (retry = 0U; retry < 3U; retry++)
    {
        RTC->BKP0R = request_magic;
        __DSB();
        __ISB();

        if (RTC->BKP0R == request_magic)
        {
            HAL_PWR_DisableBkUpAccess();
            return true;
        }
    }

    HAL_PWR_DisableBkUpAccess();
    return false;
}

static void Board_SystemRestart(bool enter_bootloader)
{
    uint32_t request_magic = enter_bootloader ? OTA_REQUEST_MAGIC : 0U;

    if (!Board_WriteBootRequest(request_magic))
        return;

    /* 中文注释：复位前停止本板所有可能持续动作的执行器。 */
    SteelBall_MotorSwitch(MOTOR_SWITCH_OFF);
    Motor_Hoolle2.Motor.state = DEVICE_STATE_STOP;
    Card.Switch.state = DEVICE_STATE_STOP;
    Servo_SetRun(0U);

    HAL_Delay(100U);
    NVIC_SystemReset();
}

static bool USART_ReceiveMesg_Verify(void *self, void *mesg)
{
    Rx_HandleTypeDef *rx = (Rx_HandleTypeDef *)self;
    Mesg_TypeDef *Rx_mesg = (Mesg_TypeDef *)mesg;
    uint16_t crc16 = CRC16_calculate(rx->Queue.Buf, 11);
    uint16_t mesg_crc16 = ((uint16_t)Rx_mesg->CRC16_H << 8) | Rx_mesg->CRC16_L;
    return crc16 == mesg_crc16;
}

/* 中文注释：处理安卓→主板命令，并按协议把控台/球盘命令路由到对应串口。 */
static void USART1_Deal(void *Rx_mesg)
{
    uint32_t data;
    Mesg_TypeDef *mesg = (Mesg_TypeDef *)Rx_mesg;

    if (mesg->Code1 == Android_to_Board)
    {
        if (mesg->ACKbyte != 0U)
            USART_RequestMesg(&Tx1, mesg);

        if (List_IsExistID(&DealList, mesg->ID) != false)
            return;

        data = Mesg_GetData32(mesg);

        switch (mesg->Code2)
        {
        case r_GetVersion:
            /* 中文注释：安卓0x00请求的是球盘版本，转发给球盘0x04/0x00。 */
            Comm_SendMesg_FillData(&Tx2, Board_to_Ball, BALL_CMD_VERSION, 0U, 0U);
            break;

        case r_HoolleOutput:
            if (mesg->ExpandCode == HOOLLE_TYPE_NORMAL)
                Hoolle_Output(&Motor_Hoolle2, (uint16_t)data);
            else if (mesg->ExpandCode == HOOLLE_TYPE_STEEL_BALL)
                Hoolle_Output(&Motor_Hoolle1, (uint16_t)data);
            break;

        case r_CardOutput:
            Card_Output(&Card, (uint16_t)data);
            break;

        case r_CtrlDigitalTube:
            Comm_SendMesg_FillData(&Tx3, Board_to_Ctrl, CTRL_CMD_DIGITAL_TUBE, data, 0U);
            break;

        case r_BallDigitalTube:
            Comm_SendMesg_FillData(&Tx2, Board_to_Ball, BALL_CMD_DIGITAL_TUBE, data, 0U);
            break;

        case r_SceneChange:
            /* 中文注释：安卓0x01/0x06按协议使用Data4传场景，主板转控台0x02/0x03时再放入补充位，Data4固定0。 */
            Comm_SendMesg_FillData(&Tx3, Board_to_Ctrl, CTRL_CMD_SCENE, 0U, mesg->Data4);
            break;

        case r_LightBeltMode:
            /* 中文注释：现行主板→控台/球盘协议没有灯带模式功能码，不擅自复用其他命令。 */
            break;

        case r_WinChannel:
            /* 中文注释：分板协议未定义中奖通道下发目标，保持不处理。 */
            break;

        case r_OutputAllHoolle:
            /*
             * 中文注释：协议0x0B补充位固定0x00，对应瓷珠通道；
             * 不同时清钢珠，避免改变原协议中0x00瓷珠/0x01钢珠的类型语义。
             */
            Motor_Hoolle2.ClearMode = 1U;
            Hoolle_Output(&Motor_Hoolle2, (uint16_t)(0xFFFFU - Motor_Hoolle2.Hoolle_num));
            break;

        case r_OutputRemainingItem:
            /* 中文注释：协议“吐出剩余物品”沿用原逻辑，仅继续瓷珠和卡片。 */
            Hoolle_Output(&Motor_Hoolle2, 0U);
            Card_Output(&Card, 0U);
            break;

        case r_ResumeDefultSetting:
            ResumeSetting();
            break;

        case r_SaveSetting:
            EventGroupSetBits(&Mesg_event, Event_FlashData);
            break;

        case r_Unlock:
            Lock_Valve.Switch.state = DEVICE_STATE_START;
            EventGroupSetBits(&Mesg_event, MesgEvent_Unlock);
            break;

        case r_ServoControl:
            Servo_SetRun(mesg->ExpandCode == 0x01U ? 1U : 0U);
            break;

        case r_LightControl:
            /* 中文注释：安卓0x15与主板→球盘0x02字段一致，原样转发。 */
            Comm_SendMesg_FillData(&Tx2, Board_to_Ball, BALL_CMD_RGB_MODE, data, mesg->ExpandCode);
            break;

        case r_CtrlButtonLight:
            /* 中文注释：现行主板→控台协议没有按键灯功能码，暂不创造私有命令。 */
            break;

        case r_ServoReset:
            Servo_SetRun(0U);
            break;

        case r_SteelBallMotorSwitch:
            /* 中文注释：保留既有0x21调试功能，不改变正式协议中的其他功能码。 */
            if (mesg->ExpandCode == MOTOR_SWITCH_OFF)
                SteelBall_MotorSwitch(MOTOR_SWITCH_OFF);
            else if (mesg->ExpandCode == MOTOR_SWITCH_ON)
                SteelBall_MotorSwitch(MOTOR_SWITCH_ON);
            break;

        case r_StopAllDevice:
            SteelBall_MotorSwitch(MOTOR_SWITCH_OFF);
            Motor_Hoolle2.Motor.state = DEVICE_STATE_STOP;
            Card.Switch.state = DEVICE_STATE_STOP;
            Servo_SetRun(0U);
            break;

        case r_SystemReset:
            Board_SystemRestart(data == OTA_REQUEST_MAGIC);
            break;

        default:
            break;
        }

        List_AddNode(&DealList, mesg->ID, HAL_GetTick());
    }
    else if (mesg->Code1 == Board_to_Android)
    {
        List_DeleteNode(&ResendList, mesg->ID);
    }
}

/* 中文注释：处理球盘→主板0x05，并转换为安卓协议0x00/0x0E。 */
static void USART2_Deal(void *Rx_mesg)
{
    Mesg_TypeDef *mesg = (Mesg_TypeDef *)Rx_mesg;

    if (mesg->Code1 != Ball_to_Board)
        return;

    switch (mesg->Code2)
    {
    case BALL_REPORT_VERSION:
        Comm_SendMesg_FillData_withResend(
            &Tx1,
            Board_to_Android,
            t_VersionRequest,
            Mesg_GetData32(mesg),
            0U,
            &ResendList);
        break;

    case BALL_REPORT_EYE:
        if (mesg->Data4 >= 1U && mesg->Data4 <= 5U)
        {
            Comm_SendMesg_FillData_withResend(
                &Tx1,
                Board_to_Android,
                t_LightEye,
                (uint32_t)mesg->Data4,
                0x00U,
                &ResendList);
        }
        else if (mesg->Data4 == 6U || mesg->Data4 == 7U)
        {
            /*
             * 中文注释：协议表球盘光眼写1~6，但当前原理图实际是PB3~PB7五路+FB1/FB2两路，共7路。
             * ID6/7映射为安卓黄色两组的编号1/2，ExpandCode=0x01。
             */
            Comm_SendMesg_FillData_withResend(
                &Tx1,
                Board_to_Android,
                t_LightEye,
                (uint32_t)(mesg->Data4 - 5U),
                0x01U,
                &ResendList);
        }
        break;

    default:
        break;
    }
}

/* 中文注释：处理控台→主板0x03，并转换为安卓按键/编码器协议。 */
static void USART3_Deal(void *Rx_mesg)
{
    Mesg_TypeDef *mesg = (Mesg_TypeDef *)Rx_mesg;

    if (mesg->Code1 != Ctrl_to_Board)
        return;

    switch (mesg->Code2)
    {
    case CTRL_REPORT_VERSION:
        /* 中文注释：安卓协议没有独立控台版本上报功能码，仅接收不转发。 */
        break;

    case CTRL_REPORT_BUTTON:
        Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_Button, (uint32_t)mesg->Data4, mesg->ExpandCode);
        break;

    case CTRL_REPORT_SPECIAL_BUTTON:
        Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_SpecialButton, (uint32_t)mesg->Data4, mesg->ExpandCode);
        break;

    case CTRL_REPORT_ENCODER:
        if (mesg->ExpandCode <= 0x01U)
        {
            Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_Encoder, 0U, mesg->ExpandCode);
        }
        /* 中文注释：控台定义0x02=编码器按下，但安卓0x0F仅定义左/右，因此按下暂不转发。 */
        break;

    default:
        break;
    }
}

static uint8_t USART_SendMesg(Tx_HandleTypeDef *Tx, Mesg_TypeDef *mesg)
{
    static uint8_t ID = 0;
    uint8_t data[14];
    uint16_t crc;

    ID++;
    mesg->ResendID = 0;
    mesg->ID = ID;
    MesgTable[ID] = *mesg;
    memcpy(data, mesg, 14);
    crc = CRC16_calculate(data, 11);
    data[11] = (uint8_t)(crc >> 8);
    data[12] = (uint8_t)crc;
    HAL_UART_Transmit(Tx->huart, data, 14, 100);
    return ID;
}

uint8_t Comm_SendMesg_FillData(Tx_HandleTypeDef *Tx, uint8_t code_1, uint8_t code_2, uint32_t data, uint8_t expandCode)
{
    Mesg_TypeDef mesg = {0};
    mesg.Head = Mesg_Head;
    mesg.Code1 = code_1;
    mesg.Code2 = code_2;
    mesg.Data1 = (uint8_t)(data >> 24);
    mesg.Data2 = (uint8_t)(data >> 16);
    mesg.Data3 = (uint8_t)(data >> 8);
    mesg.Data4 = (uint8_t)data;
    mesg.ACKbyte = 0x00;
    mesg.ExpandCode = expandCode;
    mesg.Tail = Mesg_Tail;
    return USART_SendMesg(Tx, &mesg);
}

uint8_t Comm_SendMesg_FillData_withResend(Tx_HandleTypeDef *Tx, uint8_t code_1, uint8_t code_2, uint32_t data, uint8_t expandCode, ListHandle_t *List)
{
    uint8_t ID;
    Mesg_TypeDef mesg = {0};
    mesg.Head = Mesg_Head;
    mesg.Code1 = code_1;
    mesg.Code2 = code_2;
    mesg.Data1 = (uint8_t)(data >> 24);
    mesg.Data2 = (uint8_t)(data >> 16);
    mesg.Data3 = (uint8_t)(data >> 8);
    mesg.Data4 = (uint8_t)data;
    mesg.ACKbyte = 0x01;
    mesg.ExpandCode = expandCode;
    mesg.Tail = Mesg_Tail;
    ID = USART_SendMesg(Tx, &mesg);
    List_AddNode(List, ID, HAL_GetTick());
    return ID;
}

static uint8_t USART_ReSendMesg(Tx_HandleTypeDef *Tx, Mesg_TypeDef *mesg)
{
    uint8_t data[14];
    uint16_t crc;

    mesg->ResendID++;
    if (mesg->ResendID > Max_Resend_Times)
        return 1;

    memcpy(data, mesg, 14);
    data[0] = Mesg_Head;
    data[1] = mesg->ResendID;
    crc = CRC16_calculate(data, 11);
    data[11] = (uint8_t)(crc >> 8);
    data[12] = (uint8_t)crc;
    data[13] = Mesg_Tail;
    HAL_UART_Transmit(Tx->huart, data, 14, 100);
    return 0;
}

static void USART_RequestMesg(Tx_HandleTypeDef *Tx, Mesg_TypeDef *mesg)
{
    uint8_t data[14];
    uint16_t crc;

    memcpy(data, mesg, 14);
    crc = CRC16_calculate(data, 11);
    data[11] = (uint8_t)(crc >> 8);
    data[12] = (uint8_t)crc;
    data[13] = Mesg_Tail;
    HAL_UART_Transmit(Tx->huart, data, 14, 100);
}

void Resend_Task(void)
{
    ListNode_t *Current = ResendList.Head;
    uint32_t CurrentTime = HAL_GetTick();

    for (uint8_t i = 0; i < ResendList.NodeCount; i++)
    {
        if (CurrentTime - Current->Value > ResendTrigger_Time)
        {
            USART_ReSendMesg(&Tx1, &(MesgTable[Current->ID]));
            Current->Value = CurrentTime;
            if (MesgTable[Current->ID].ResendID >= Max_Resend_Times)
                List_DeleteNode(&ResendList, Current->ID);
        }
        Current = Current->Next;
    }
}

void MesgDeal_Task(void)
{
    ListNode_t *Current = DealList.Head;
    uint32_t CurrentTime = HAL_GetTick();

    for (uint8_t i = 0; i < DealList.NodeCount; i++)
    {
        if (CurrentTime - Current->Value > MesgDeal_Time)
            List_DeleteNode(&DealList, Current->ID);
        Current = Current->Next;
    }
}

void CommInit(void)
{
    Rx_InitTypeDef Rxinit;
    Tx_InitTypeDef Tx_init;

    List_Create(&ResendList, ResendList_buffer, 100);
    List_Create(&DealList, DealList_buffer, 100);

    /* 中文注释：USART1=安卓，USART2=球盘，USART3=控台。 */
    Rxinit.Frame_Head = Mesg_Head;
    Rxinit.Frame_Tail = Mesg_Tail;
    Rxinit.Mesg_Len = 14U;
    Rxinit.Receive = Rx_Receive;
    Rxinit.Verify = USART_ReceiveMesg_Verify;

    Rxinit.huart = &huart1;
    Rxinit.RingBuf = rx1_buffer;
    Rxinit.RingBuf_Size = sizeof(rx1_buffer);
    Rxinit.Deal = USART1_Deal;
    Communicate_Rx_Init(&Rx1, Rxinit);

    Rxinit.huart = &huart2;
    Rxinit.RingBuf = rx2_buffer;
    Rxinit.RingBuf_Size = sizeof(rx2_buffer);
    Rxinit.Deal = USART2_Deal;
    Communicate_Rx_Init(&Rx2, Rxinit);

    Rxinit.huart = &huart3;
    Rxinit.RingBuf = rx3_buffer;
    Rxinit.RingBuf_Size = sizeof(rx3_buffer);
    Rxinit.Deal = USART3_Deal;
    Communicate_Rx_Init(&Rx3, Rxinit);

    Tx_init.hdma = NULL;
    Tx_init.TxBuf = NULL;
    Tx_init.TxBuf_Size = 0;

    Tx_init.huart = &huart1;
    Communicate_Tx_Init(&Tx1, Tx_init);
    Tx_init.huart = &huart2;
    Communicate_Tx_Init(&Tx2, Tx_init);
    Tx_init.huart = &huart3;
    Communicate_Tx_Init(&Tx3, Tx_init);
}

void CommTask(void)
{
    Rx1.Receive(&Rx1, &Receive1_mesg, 14);
    Rx2.Receive(&Rx2, &Receive2_mesg, 14);
    Rx3.Receive(&Rx3, &Receive3_mesg, 14);
}
