#include "CommTask.h"
#include "MesgTask.h"
#include "CtrlTask.h"
#include "KeyTask.h"
#include "MainTask.h"
#include "FlashTask.h"
#include "DigitalTubeTask.h"
#include "app_crc.h"
#include "app_list.h"
#include "string.h"
#include "usart.h"

#define Mesg_Head 0xAA
#define Mesg_Tail 0x55

static void USART_RequestMesg(Tx_HandleTypeDef *Tx, Mesg_TypeDef *mesg);
static bool Board_WriteBootRequest(uint32_t request_magic);
static void Board_SystemRestart(bool enter_bootloader);

ListHandle_t ResendList, DealList;
static ListNode_t ResendList_buffer[100];
static ListNode_t DealList_buffer[100];

static Mesg_TypeDef MesgTable[256];
static uint8_t rx1_buffer[512];
static Mesg_TypeDef Receive1_mesg;

Tx_HandleTypeDef Tx1;
Rx_HandleTypeDef Rx1;

extern Event_Handle_t Mesg_event;
extern Motor_Card Card;
extern Motor_Hoolle Motor_Hoolle1;
extern Motor_Hoolle Motor_Hoolle2;
extern servo_t Servo1;
extern Switch_Valve Lock_Valve;
extern ListHandle_t ResendList, DealList;

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

    /* 中文注释：复位前强制退出0x21手动常转模式并立即停止吐珠电机。 */
    SteelBall_MotorSwitch(MOTOR_SWITCH_OFF);
    Motor_Hoolle2.Motor.state = DEVICE_STATE_STOP;
    Card.Switch.state = DEVICE_STATE_STOP;

    /*
     * USART_RequestMesg()已经在命令处理前完成应答，
     * 延时后再复位，给安卓端留出接收应答的时间。
     */
    HAL_Delay(100U);
    NVIC_SystemReset();
}

/// 串口1消息验证
static bool USART1_ReceiveMesg_Verify(void *self, void *mesg)
{
    Rx_HandleTypeDef *rx = (Rx_HandleTypeDef *)self;
    Mesg_TypeDef *Rx_mesg = (Mesg_TypeDef *)mesg;
    uint16_t crc16, mesg_crc16;
    crc16 = CRC16_calculate(rx->Queue.Buf, 11);
    mesg_crc16 = Rx_mesg->CRC16_H << 8 | Rx_mesg->CRC16_L;
    if (crc16 == mesg_crc16)
        return true;
    return false;
}

/// 串口1消息处理
static void USART1_Deal(void *Rx_mesg)
{
    uint32_t data;
    Mesg_TypeDef *mesg = (Mesg_TypeDef *)Rx_mesg;
    if (mesg->Code1 == Android_to_Board)
    {
        USART_RequestMesg(&Tx1, mesg);
        if (List_IsExistID(&DealList, mesg->ID) == false)
        {
            switch (mesg->Code2)
            {
            /// 版本请求
            case r_GetVersion:
                EventGroupSetBits(&Mesg_event, MesgEvent_VersionRequest);
                break;

            /// 出钢珠/扭蛋
            case r_HoolleOutput:
                data = ((mesg->Data3 << 8) | mesg->Data4);
                /* 中文注释：正式协议固定0x00=扭蛋，0x01=钢珠；其他值不执行。 */
                if (mesg->ExpandCode == HOOLLE_TYPE_EGG)
                    Hoolle_Output(&Motor_Hoolle2, data);
                else if (mesg->ExpandCode == HOOLLE_TYPE_STEEL_BALL)
                    Hoolle_Output(&Motor_Hoolle1, data);
                break;

            /// 出卡
            case r_CardOutput:
                data = ((mesg->Data3 << 8) | mesg->Data4);
                Card_Output(&Card, data);
                break;

            /// 清钢珠/清扭蛋
            case r_OutputAllHoolle:
                if (mesg->ExpandCode == HOOLLE_TYPE_EGG)
                {
                    Motor_Hoolle2.ClearMode = 1;
                    Hoolle_Output(&Motor_Hoolle2, 0xFFFF - Motor_Hoolle2.Hoolle_num);
                }
                else if (mesg->ExpandCode == HOOLLE_TYPE_STEEL_BALL)
                {
                    Motor_Hoolle1.ClearMode = 1;
                    Hoolle_Output(&Motor_Hoolle1, 0xFFFF - Motor_Hoolle1.Hoolle_num);
                }
                break;

            /// 继续扭蛋和卡片剩余输出
            case r_OutputRemainingItem:
                Hoolle_Output(&Motor_Hoolle2, 0);
                Card_Output(&Card, 0);
                break;

            /// 恢复默认设置
            case r_ResumeDefultSetting:
                ResumeSetting();
                break;

            /// 保存设置
            case r_SaveSetting:
                EventGroupSetBits(&Mesg_event, Event_FlashData);
                break;

            /// 开锁
            case r_Unlock:
                Lock_Valve.Switch.state = DEVICE_STATE_START;
                EventGroupSetBits(&Mesg_event, MesgEvent_Unlock);
                break;

            /// 舵机1归零
            case r_ServoReset:
                Servo1.SetAngle(&Servo1, 90);
                break;

            /// 吐珠/钢珠电机手动开关
            case r_SteelBallMotorSwitch:
                /*
                 * 中文注释：0x21使用ExpandCode控制手动常转。
                 * 0x00立即关闭，0x01开启后持续运行，不受数量和超时状态机影响。
                 */
                if (mesg->ExpandCode == MOTOR_SWITCH_OFF)
                    SteelBall_MotorSwitch(MOTOR_SWITCH_OFF);
                else if (mesg->ExpandCode == MOTOR_SWITCH_ON)
                    SteelBall_MotorSwitch(MOTOR_SWITCH_ON);
                break;

            /// 停止所有设备
            case r_StopAllDevice:
                /* 中文注释：0xFF必须同时退出0x21手动常转模式，防止下一轮CtrlTask重新启动。 */
                SteelBall_MotorSwitch(MOTOR_SWITCH_OFF);
                Motor_Hoolle2.Motor.state = DEVICE_STATE_STOP;
                Card.Switch.state = DEVICE_STATE_STOP;
                break;

            /// 系统复位/进入Bootloader
            case r_SystemReset:
                data = (uint32_t)mesg->Data1 << 24 |
                       (uint32_t)mesg->Data2 << 16 |
                       (uint32_t)mesg->Data3 << 8 |
                       (uint32_t)mesg->Data4;
                Board_SystemRestart(data == OTA_REQUEST_MAGIC);
                break;

            /// 数码管显示
            case r_DigitalTubeData:
                data = (uint32_t)mesg->Data1 << 24 |
                       (uint32_t)mesg->Data2 << 16 |
                       (uint32_t)mesg->Data3 << 8 |
                       (uint32_t)mesg->Data4;
                /* 中文注释：当前没有独立控台，四位数码管由主板SPI2直接刷新。 */
                DigitalTube.Set_Num(&DigitalTube, 0, data, 4);
                DigitalTube.Refresh(&DigitalTube);
                break;
            }

            /// 将该消息包加入已处理列表，防止短时间内重复处理同样ID的消息包
            List_AddNode(&DealList, mesg->ID, HAL_GetTick());
        }
    }
    /// 收到的是应答
    else if (mesg->Code1 == Board_to_Android)
    {
        /// 重发列表中去除该消息包
        List_DeleteNode(&ResendList, mesg->ID);
    }
}

/// 发送消息，无重传
static uint8_t USART_SendMesg(Tx_HandleTypeDef *Tx, Mesg_TypeDef *mesg)
{
    static uint8_t ID = 0;
    uint8_t data[14];
    uint16_t crc;

    ID++;                  // 每次发送新消息都会自增
    mesg->ResendID = 0;    // 重发次数清零
    mesg->ID = ID;         // 赋予新ID号
    MesgTable[ID] = *mesg; // 保存消息包
    memcpy(data, mesg, 14);
    crc = CRC16_calculate(data, 11);
    data[11] = crc >> 8;
    data[12] = crc;
    HAL_UART_Transmit(Tx->huart, data, 14, 100);
    return ID;
}

/// 填入参数发送消息，无重传
uint8_t Comm_SendMesg_FillData(Tx_HandleTypeDef *Tx, uint8_t code_1, uint8_t code_2, uint32_t data, uint8_t expandCode)
{
    Mesg_TypeDef mesg = {0};
    mesg.Head = Mesg_Head;
    mesg.ResendID = 0;
    mesg.ID = 0;
    mesg.Code1 = code_1;
    mesg.Code2 = code_2;
    mesg.Data1 = (uint8_t)(data >> 24);
    mesg.Data2 = (uint8_t)(data >> 16);
    mesg.Data3 = (uint8_t)(data >> 8);
    mesg.Data4 = (uint8_t)(data);
    mesg.ACKbyte = 0x00;
    mesg.ExpandCode = expandCode;
    mesg.Tail = Mesg_Tail;
    return USART_SendMesg(Tx, &mesg);
}

/// 填充数据发送消息并加入重发列表
uint8_t Comm_SendMesg_FillData_withResend(Tx_HandleTypeDef *Tx, uint8_t code_1, uint8_t code_2, uint32_t data, uint8_t expandCode, ListHandle_t *List)
{
    uint8_t ID;
    Mesg_TypeDef mesg = {0};
    mesg.Head = Mesg_Head;
    mesg.ResendID = 0;
    mesg.ID = 0;
    mesg.Code1 = code_1;
    mesg.Code2 = code_2;
    mesg.Data1 = (uint8_t)(data >> 24);
    mesg.Data2 = (uint8_t)(data >> 16);
    mesg.Data3 = (uint8_t)(data >> 8);
    mesg.Data4 = (uint8_t)(data);
    mesg.ACKbyte = 0x01;
    mesg.ExpandCode = expandCode;
    mesg.Tail = Mesg_Tail;
    ID = USART_SendMesg(Tx, &mesg);
    List_AddNode(List, ID, HAL_GetTick());
    return ID;
}

/// 发送重发消息
static uint8_t USART_ReSendMesg(Tx_HandleTypeDef *Tx, Mesg_TypeDef *mesg)
{
    uint8_t data[14];
    uint16_t crc;
    mesg->ResendID++;
    if (mesg->ResendID > Max_Resend_Times)
        return 1;
    data[0] = Mesg_Head;
    data[1] = mesg->ResendID;
    data[2] = mesg->ID;
    data[3] = mesg->Code1;
    data[4] = mesg->Code2;
    data[5] = mesg->Data1;
    data[6] = mesg->Data2;
    data[7] = mesg->Data3;
    data[8] = mesg->Data4;
    data[9] = mesg->ACKbyte;
    data[10] = mesg->ExpandCode;
    crc = CRC16_calculate(data, 11);
    data[11] = crc >> 8;
    data[12] = crc;
    data[13] = Mesg_Tail;
    HAL_UART_Transmit(Tx->huart, data, 14, 100);
    return 0;
}

/// 发送应答消息
static void USART_RequestMesg(Tx_HandleTypeDef *Tx, Mesg_TypeDef *mesg)
{
    uint8_t data[14];
    uint16_t crc;
    data[0] = Mesg_Head;
    data[1] = mesg->ResendID;
    data[2] = mesg->ID;
    data[3] = mesg->Code1;
    data[4] = mesg->Code2;
    data[5] = mesg->Data1;
    data[6] = mesg->Data2;
    data[7] = mesg->Data3;
    data[8] = mesg->Data4;
    data[9] = mesg->ACKbyte;
    data[10] = mesg->ExpandCode;
    crc = CRC16_calculate(data, 11);
    data[11] = crc >> 8;
    data[12] = crc;
    data[13] = Mesg_Tail;
    HAL_UART_Transmit(Tx->huart, data, 14, 100);
}

/* ----------检测重发消息---------- */
void Resend_Task(void)
{
    ListNode_t *Current = ResendList.Head;
    uint32_t CurrentTime = HAL_GetTick();
    for (uint8_t i = 0; i < ResendList.NodeCount; i++)
    {
        // 超时时间内未收到应答，立即重发
        if (CurrentTime - Current->Value > ResendTrigger_Time)
        {
            USART_ReSendMesg(&Tx1, &(MesgTable[Current->ID]));
            Current->Value = CurrentTime;
            // 如果重发次数达到最大次数，则从重发列表中删除
            if (MesgTable[Current->ID].ResendID >= Max_Resend_Times)
                List_DeleteNode(&ResendList, Current->ID);
        }
        Current = Current->Next;
    }
}

/* ----------清除已执行消息任务---------- */
void MesgDeal_Task(void)
{
    ListNode_t *Current = DealList.Head;
    uint32_t CurrentTime = HAL_GetTick();
    for (uint8_t i = 0; i < DealList.NodeCount; i++)
    {
        // 达到超时时间则从列表中删除，表示可接收同样ID的新消息
        if (CurrentTime - Current->Value > MesgDeal_Time)
            List_DeleteNode(&DealList, Current->ID);
        Current = Current->Next;
    }
}

/* ----------通信初始化---------- */
void CommInit(void)
{
    List_Create(&ResendList, ResendList_buffer, 100);
    List_Create(&DealList, DealList_buffer, 100);

    /* 中文注释：当前无独立控台，正式通信只初始化主板与安卓之间的USART1。 */
    Rx_InitTypeDef Rxinit;
    Rxinit.huart = &huart1;
    Rxinit.RingBuf = rx1_buffer;
    Rxinit.RingBuf_Size = sizeof(rx1_buffer);
    Rxinit.Frame_Head = Mesg_Head;
    Rxinit.Frame_Tail = Mesg_Tail;
    Rxinit.Receive = Rx_Receive;
    Rxinit.Verify = USART1_ReceiveMesg_Verify;
    Rxinit.Deal = USART1_Deal;
    Communicate_Rx_Init(&Rx1, Rxinit);

    Tx_InitTypeDef Tx_init;
    Tx_init.huart = &huart1;
    Tx_init.hdma = NULL;
    Tx_init.TxBuf = NULL;
    Tx_init.TxBuf_Size = 0;
    Communicate_Tx_Init(&Tx1, Tx_init);
}

void CommTask(void)
{
    Rx1.Receive(&Rx1, &Receive1_mesg, 14);
}
