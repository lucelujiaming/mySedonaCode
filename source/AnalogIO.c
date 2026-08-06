#include "sedona.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include "v3s_gpio_operation_wrapper.h"

// ==================== TPC116S4 SPI 配置 ==========================
#define TPC116S4_SPI_DEVICE     "/dev/spidev0.0"   // 你的 spidev 设备
#define TPC116S4_SPI_SPEED      1000000            // SPI 时钟 1MHz
#define TPC116S4_REF_VOLTAGE     (2.5)             // 参考电压
// =======================================================

static int tpc116s4_spi_fd;

// SPI 初始化
int tpc116s4_spi_init(void)
{
    uint8_t mode = SPI_MODE_0 | SPI_CS_HIGH;
    uint8_t bits = 8;
    uint32_t speed = TPC116S4_SPI_SPEED;

    tpc116s4_spi_fd = open(TPC116S4_SPI_DEVICE, O_RDWR);
    if (tpc116s4_spi_fd < 0) {
        perror("SPI 打开失败");
        return -1;
    }

    // 设置 SPI 参数
    ioctl(tpc116s4_spi_fd, SPI_IOC_WR_MODE, &mode);
    ioctl(tpc116s4_spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(tpc116s4_spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

    printf("✅ SPI 初始化完成\n");
    return 0;
}

int tpc116s4_write_channel(int iChannel, int iValue)
{
    uint8_t tx_buf[8] = {0};
    uint8_t rx_buf[8] = {0};
    
    tx_buf[0] = iChannel, tx_buf[1] = iValue / 0x100, tx_buf[2] = iValue % 0x100;

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx_buf,
        .rx_buf = (unsigned long)rx_buf,
        .len = 3,
        .speed_hz = TPC116S4_SPI_SPEED,
        .bits_per_word = 8,
    };

    printf("iChannel=%d, iValue = %d, tx_buf is   %02X:%02X:%02X\n", 
            iChannel, iValue, tx_buf[0], tx_buf[1], tx_buf[2]);
    // SPI 读取数据
    if (ioctl(tpc116s4_spi_fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        perror("tpc116s4_write_channel::tpc116s4_spi_fd SPI 传输失败");
        return -1;
    }
    return 0;
}

#define   CHANNEL_A     8
#define   CHANNEL_B     10
#define   CHANNEL_C     12
#define   CHANNEL_D     14
Cell AnalogIO_TPC116s4_WriteChannel(SedonaVM* vm, Cell* params)
{
    int   iChannel        = params[1].ival;
    float fPercentValue   = params[2].fval;
	
    int iValue = (int)(fPercentValue * 65536);
	
    // char cData[4];
    Cell ret;
    ret.ival = -1;
    
    // cData[0] = iChannel, cData[1] = iValue / 0x100, cData[2] = iValue % 0x100;
    
    printf("\n===== tpc116s4 set (%d, %d:%f) =====\n", iChannel, iValue, fPercentValue);

	pthread_mutex_lock(g_V3S_GPIO_Operator->objMutex); // 加锁
    // PF1	SPI-LOAD	DAC输出更新信号，为高时不更新，为低时更新DAC输出
    V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator, V3S_PF, 1, V3S_OUT);
    // PF6	DAC-1	第一组输出，输出高时恒压，输出低时横流	
    V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator, V3S_PF, 6, V3S_OUT);
    // PF5	DAC-2	第二组输出，输出高时恒压，输出低时横流	
    V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator, V3S_PF, 5, V3S_OUT);
    // PF4	DAC-3	第三组输出，输出高时恒压，输出低时横流	
    V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator, V3S_PF, 4, V3S_OUT);
    // PF3	DAC-4	第四组输出，输出高时恒压，输出低时横流	
    V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator, V3S_PF, 3, V3S_OUT);
    
    // LED灯控制
    V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator, V3S_PG, 4, V3S_OUT);	
    V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator, V3S_PF, 2, V3S_OUT);
    
    // V3S_GPIO_SetPin(V3S_PG, 4, 1);
    // V3S_GPIO_SetPin(V3S_PF, 2, 1);
    
    // Current Mode
    if(iChannel > 0x10) {
        iChannel = iChannel - 0x10;
        if(iChannel == CHANNEL_A)
        {
            // Not use PF6 temply
            // V3S_GPIO_SetPin(V3S_PF, 6, 0);
        }
        else if(iChannel == CHANNEL_B)
        {
            V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PF, 5, 0);
        }
        else if(iChannel == CHANNEL_C)
        {
            V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PF, 4, 0);
        }
        else if(iChannel == CHANNEL_C)
        {
            V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PF, 3, 0);
        }
    }
    // Voltage Mode
    else {
        if(iChannel == CHANNEL_A)
        {
            // Not use PF6 temply
            // V3S_GPIO_SetPin(V3S_PF, 6, 1);
        }
        else if(iChannel == CHANNEL_B)
        {
            V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PF, 5, 1);
        }
        else if(iChannel == CHANNEL_C)
        {
            V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PF, 4, 1);
        }
        else if(iChannel == CHANNEL_C)
        {
            V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PF, 3, 1);
        }
    }
	pthread_mutex_unlock(g_V3S_GPIO_Operator->objMutex); // 解锁
    
    if (tpc116s4_spi_init()  < 0) 
    {   
        close(tpc116s4_spi_fd); 
        return ret;
    }
    // printf("\n开始输出数据...\n\n");
    tpc116s4_write_channel(iChannel, iValue);
    
	pthread_mutex_lock(g_V3S_GPIO_Operator->objMutex); // 加锁
    // V3S_GPIO_SetPin(V3S_PF, 1, 0);
    // printf("V3S_GPIO_SetPin(V3S_PF, 1, 0)\n");
    V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PF, 1, 0);
    // printf("V3S_GPIO_SetPin(V3S_PF, 1, 0)\n");
    usleep(1000);
    V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PF, 1, 1);
    // printf("V3S_GPIO_SetPin(V3S_PF, 1, 1)\n");
	pthread_mutex_unlock(g_V3S_GPIO_Operator->objMutex); // 解锁
    
    close(tpc116s4_spi_fd);
    ret.ival = 1;
    return ret;
}

// ==================== SPI 配置 ==========================
#define TPAFE51760_SPI_DEVICE     "/dev/spidev0.1"   // 你的 spidev 设备
#define TPAFE51760_SPI_SPEED      1000000     // SPI 时钟 1MHz
#define TPAFE51760_CHANNELS       16          // 16 通道
#define TPAFE51760_VREF           1           // 这里使用1，也就是不使用参考电压。
                                              // 不同测试模式下的参考电压不同，需要在上层处理。

// =======================================================

static int tpafe51760_spi_fd;

// 启动 tpafe51760 转换（CONVST 脉冲）
void tpafe51760_start_convert(void)
{
	pthread_mutex_lock(g_V3S_GPIO_Operator->objMutex); // 加锁
    V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PE, 16, 1);
    usleep(1);
    V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PE, 16, 0);
	pthread_mutex_unlock(g_V3S_GPIO_Operator->objMutex); // 解锁
    usleep(20);    // 等待转换完成
}

// SPI 初始化
int tpafe51760_spi_init(void)
{
    uint8_t mode = SPI_MODE_1 | SPI_CS_HIGH;  // tpafe51760必须MODE1
    uint8_t bits = 8;
    uint32_t speed = TPAFE51760_SPI_SPEED;

    tpafe51760_spi_fd = open(TPAFE51760_SPI_DEVICE, O_RDWR);
    if (tpafe51760_spi_fd < 0) {
        perror("SPI 打开失败");
        return -1;
    }

    // 设置 SPI 参数
    ioctl(tpafe51760_spi_fd, SPI_IOC_WR_MODE, &mode);
    ioctl(tpafe51760_spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(tpafe51760_spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

    // printf("✅ SPI 初始化完成\n");
    return 0;
}

// 读取 tpafe51760 16 通道数据
int tpafe51760_read_all_channels(uint16_t *data)
{
    uint8_t tx_buf[32] = {0};
    uint8_t rx_buf[32] = {0};

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx_buf,
        .rx_buf = (unsigned long)rx_buf,
        .len = 32,
        .speed_hz = TPAFE51760_SPI_SPEED,
        .bits_per_word = 8,
    };

    // 启动转换
    tpafe51760_start_convert();

    // SPI 读取数据
    if (ioctl(tpafe51760_spi_fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        perror("tpafe51760_read_all_channels::tpafe51760_spi_fd SPI 传输失败");
        return -1;
    }

    // 解析 16 通道数据
    for (int i = 0; i < TPAFE51760_CHANNELS; i++) {
        data[i] = (rx_buf[2*i] << 8) | rx_buf[2*i + 1];
    }

    return 0;
}

// 原始值转电压
float tpafe51760_raw_to_voltage(uint16_t raw)
{
    int16_t signed_val = (int16_t)raw;
    // return (float)signed_val / 65536.0f * TPAFE51760_VREF;
    return (float)signed_val / 32768.0f * TPAFE51760_VREF;
}

// 读取 tpafe51760 16 通道数据
int tpafe51760_write_data(char *dataTx, int iTXLen, char *dataRx)
{
    char tx_buf[32] = {0};
    char rx_buf[32] = {0};

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx_buf,
        .rx_buf = (unsigned long)rx_buf,
        .len = iTXLen,
        .speed_hz = TPAFE51760_SPI_SPEED,
        .bits_per_word = 8,
    };
    
    printf("dataTx is %02X:%02X:%02X with %d\n", 
              dataTx[0], dataTx[1], dataTx[2], iTXLen);
    memcpy(tx_buf, dataTx, iTXLen);

    // SPI 读取数据
    if (ioctl(tpafe51760_spi_fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        perror("tpafe51760_write_data::tpafe51760_spi_fd SPI 传输失败");
        return -1;
    }
    
    memcpy(dataRx, rx_buf, iTXLen);
    printf("dataRx is %02X:%02X:%02X\n", 
              dataRx[0], dataRx[1], dataRx[2]);
    return iTXLen;
}

Cell AnalogIO_tpafe51760_InputInit(SedonaVM* vm, Cell* params)
{
    char config_req_data[8] = {0};
    char config_rsp_data[8] = {0};
    Cell ret;
    ret.ival = -1;
    
    // 0X80(128) + (0x02) * 2
    config_req_data[0] = 132;
    // 0x62 = 98: burst mode enable, seqencer enable, no OS, yes status, no CRC
    config_req_data[1] = 98;
    
	pthread_mutex_lock(g_V3S_GPIO_Operator->objMutex); // 加锁
    V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator, V3S_PE, 18, V3S_OUT);
    usleep(10);
    V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PE, 18, 1);
	pthread_mutex_unlock(g_V3S_GPIO_Operator->objMutex); // 解锁
    usleep(500);
    if (tpafe51760_spi_init()  < 0) 
    {    
	    close(tpafe51760_spi_fd);
        return ret;
    }
    tpafe51760_write_data(config_req_data, 2, config_rsp_data);
    close(tpafe51760_spi_fd);
    ret.ival = 1;
    return ret;
}

Cell AnalogIO_tpafe51760_SetInputChannelRange(SedonaVM* vm, Cell* params)
{
    int  INPUT_RANGE_REG_A1      = 1;
    int  INPUT_RANGE_REG_A2      = 2;
    int  INPUT_RANGE_REG_B1      = 3;
    int  INPUT_RANGE_REG_B2      = 4;
    
    char config_req_data[8] = {0};
    char config_rsp_data[8] = {0};
    
    int iInputRangeRegAddr = params[1].ival;
    Cell ret;
    ret.ival = -1;
    // V3A V2A V1A V0A
    if(iInputRangeRegAddr == INPUT_RANGE_REG_A1)
    {
        config_req_data[0] = 136;  // 0X80(128) + (iInputRangeRegAddr + 3) * 2
    }
    // V7A V6A V5A V4A
    else if(iInputRangeRegAddr == INPUT_RANGE_REG_A2)
    {
        config_req_data[0] = 138;  // 0X80(128) + (iInputRangeRegAddr + 3) * 2
    }
    // V3B V2B VB1 V0B
    else if(iInputRangeRegAddr == INPUT_RANGE_REG_B1)
    {
        config_req_data[0] = 140;  // 0X80(128) + (iInputRangeRegAddr + 3) * 2
    }
    // V7B V6B VB5 V4B
    else if(iInputRangeRegAddr == INPUT_RANGE_REG_B2)
    {
        config_req_data[0] = 142;  // 0X80(128) + (iInputRangeRegAddr + 3) * 2
    }
    else {
        return ret;
    }
    config_req_data[1] = params[2].ival;
    if (tpafe51760_spi_init()  < 0) 
    {    
        return ret;
    }
    tpafe51760_write_data(config_req_data, 2, config_rsp_data);
    close(tpafe51760_spi_fd);
    ret.ival = 1;
    return ret;
}

Cell AnalogIO_tpafe51760_SetInputChannelMode(SedonaVM* vm, Cell* params)
{
    int CHANNEL_INDEX_FIRST   = 0;
    int CHANNEL_INDEX_SECOND  = 1;
    int CHANNEL_INDEX_THIRD   = 2;
    int CHANNEL_INDEX_FOURTH  = 3;
    int CHANNEL_INDEX_FIFTH   = 4;
    int CHANNEL_INDEX_SIXTH   = 5;
    int CHANNEL_INDEX_SEVENTH = 6;
    int CHANNEL_INDEX_EIGHTH  = 7;
    // int CHANNEL_INDEX_END     = 8;
    
    int iChannelGroup     = params[1].ival;
    int iChannelMode      = params[2].ival;
    
    Cell ret;
    ret.ival = -1;
    // 初始化
	pthread_mutex_lock(g_V3S_GPIO_Operator->objMutex); // 加锁
    printf("iChannelGroup %d is set to %d!\r\n", iChannelGroup, iChannelMode);
    if(iChannelGroup == CHANNEL_INDEX_FIRST)
    {
    // PE10 is used
    //    // PE10 C1-2	GPIO输出控制	C1-1和C1-2控制第一组输入
    //    // PE9  C1-1	GPIO输出控制	00：电压输入  01：电流输入  10：电阻测量  11：禁止使用
    //    V3S_GPIO_ConfigPin(V3S_PE, 10, V3S_OUT);
    //    V3S_GPIO_ConfigPin(V3S_PE,  9, V3S_OUT);
    //    V3S_GPIO_SetPin(V3S_PE,  9, iChannelMode / 2);
    //    V3S_GPIO_SetPin(V3S_PE, 10, iChannelMode % 2);
    }
    else if(iChannelGroup == CHANNEL_INDEX_SECOND)
    {
    // PE8 is used
    //    // PE8	C2-2	GPIO输出控制
    //    // PE7	C2-1	GPIO输出控制
    //    V3S_GPIO_ConfigPin(V3S_PE,  8, V3S_OUT);
    //    V3S_GPIO_ConfigPin(V3S_PE,  7, V3S_OUT);
    //    V3S_GPIO_SetPin(V3S_PE,  7, iChannelMode / 2);
    //    V3S_GPIO_SetPin(V3S_PE,  8, iChannelMode % 2);
    }
    else if(iChannelGroup == CHANNEL_INDEX_THIRD)
    {
    // PE6 is used
    //    // PE6	C3-2	GPIO输出控制
    //    // PE5	C3-1	GPIO输出控制
    //    V3S_GPIO_ConfigPin(V3S_PE, 6, V3S_OUT);
    //    V3S_GPIO_ConfigPin(V3S_PE, 5, V3S_OUT);
    //    V3S_GPIO_SetPin(V3S_PE, 5, iChannelMode / 2);
    //    V3S_GPIO_SetPin(V3S_PE, 6, iChannelMode % 2);
    }
    else if(iChannelGroup == CHANNEL_INDEX_FOURTH)
    {
        // PE4	C4-2	GPIO输出控制
        // PE3	C4-1	GPIO输出控制
        V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator, V3S_PE, 4, V3S_OUT);
        V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator, V3S_PE, 3, V3S_OUT);
        V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PE, 3, iChannelMode / 2);
        V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PE, 4, iChannelMode % 2);
    }
    else if(iChannelGroup == CHANNEL_INDEX_FIFTH)
    {
        // PE2	C5-2	GPIO输出控制
        // PE1	C5-1	GPIO输出控制
        V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator, V3S_PE, 2, V3S_OUT);
        V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator, V3S_PE, 1, V3S_OUT);
        V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PE, 1, iChannelMode / 2);
        V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PE, 2, iChannelMode % 2);
    }
    else if(iChannelGroup == CHANNEL_INDEX_SIXTH)
    {
        // PE0	C6-2	GPIO输出控制
        // PB3	C6-1	GPIO输出控制
        V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator, V3S_PE, 0, V3S_OUT);
        V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator, V3S_PB, 3, V3S_OUT);
        V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PB, 3, iChannelMode / 2);
        V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PE, 0, iChannelMode % 2);
    }
    else if(iChannelGroup == CHANNEL_INDEX_SEVENTH)
    {
        // PB4	C7-2	GPIO输出控制
        // PB5	C7-1	GPIO输出控制
        V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator, V3S_PB, 4, V3S_OUT);
        V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator, V3S_PB, 5, V3S_OUT);
        V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PB, 5, iChannelMode / 2);
        V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PB, 4, iChannelMode % 2);
    }
    else if(iChannelGroup == CHANNEL_INDEX_EIGHTH)
    {
        // PB6	C8-2	GPIO输出控制
        // PB7	C8-1	GPIO输出控制
        V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator, V3S_PB, 6, V3S_OUT);
        V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator, V3S_PB, 7, V3S_OUT);
        V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PB, 7, iChannelMode / 2);
        V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PB, 6, iChannelMode % 2);
    }
    else 
    {
	    printf("NOTICE::iChannelGroup %d is error", iChannelGroup);
    }
	pthread_mutex_unlock(g_V3S_GPIO_Operator->objMutex); // 解锁
    ret.ival = 1;
    return ret;
}

Cell AnalogIO_TPAfe51760_ReadChannel(SedonaVM* vm, Cell* params)
{
    uint16_t adc_data[TPAFE51760_CHANNELS] = {0};
    
    int iChannel = params[1].ival;
    float * bufOutput = (float *)params[2].aval;
    
    Cell ret;
    ret.ival = -1;
    if((iChannel >= 8) || (iChannel < 0))
    {    
	    printf("iChannel %d is error", iChannel);
        return ret;
    }
    
    // printf("\n===== tpafe51760 读取程序 (V3S PE16) with iChannel = %d=====\n", iChannel);

    // 初始化
	pthread_mutex_lock(g_V3S_GPIO_Operator->objMutex); // 加锁
    V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator, V3S_PE, 16, V3S_OUT);
    // PE16初始化为低电平。初始化为不启动数据转换。
    V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PE, 16, 0);
	pthread_mutex_unlock(g_V3S_GPIO_Operator->objMutex); // 解锁
    if (tpafe51760_spi_init()  < 0) 
    {    
        close(tpafe51760_spi_fd);
        return ret;
    }
    tpafe51760_read_all_channels(adc_data);
    
    // // 打印 16 通道
    // for (int i = 0; i < 16; i++) {
    //     printf("CH%02d: 0x%04X | %+.4f V ((float)signed_val / 65536.0f * TPAFE51760_VREF) with %d and %f\n",
    //            i, adc_data[i], tpafe51760_raw_to_voltage(adc_data[i]), adc_data[i], adc_data[i]/65536.0f);
    // }
    // 打印 16 通道
    bufOutput[0] = tpafe51760_raw_to_voltage(adc_data[iChannel * 2]);
    bufOutput[1] = tpafe51760_raw_to_voltage(adc_data[iChannel * 2 + 1]);

    // printf("-----------bufOutput[0] = %+.4f and bufOutput[1] = %+.4f-----------\n", bufOutput[0], bufOutput[1]);
    usleep(1000);
    close(tpafe51760_spi_fd);
    ret.ival = 1;
    return ret;
}

Cell AnalogIO_TPAfe51760_ReadAllChannels(SedonaVM* vm, Cell* params)
{
    uint16_t adc_data[TPAFE51760_CHANNELS] = {0};
    
    float * bufOutput = (float *)params[1].aval;
    Cell ret;
    ret.ival = -1;
    
    // printf("\n===== tpafe51760 读取程序 (V3S PE16) with iChannel = %d=====\n", iChannel);

    // 初始化
	pthread_mutex_lock(g_V3S_GPIO_Operator->objMutex); // 加锁
    V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator, V3S_PE, 16, V3S_OUT);
    // PE16初始化为低电平。初始化为不启动数据转换。
    V3S_GPIO_SetPin(g_V3S_GPIO_Operator, V3S_PE, 16, 0);
	pthread_mutex_unlock(g_V3S_GPIO_Operator->objMutex); // 解锁
    if (tpafe51760_spi_init()  < 0) 
    {    
        close(tpafe51760_spi_fd);
        return ret;
    }
    tpafe51760_read_all_channels(adc_data);
    
    // // 打印 16 通道
    // for (int i = 0; i < 16; i++) {
    //     printf("CH%02d: 0x%04X | %+.4f V ((float)signed_val / 65536.0f * TPAFE51760_VREF) with %d and %f\n",
    //            i, adc_data[i], tpafe51760_raw_to_voltage(adc_data[i]), adc_data[i], adc_data[i]/65536.0f);
    // }
    // 打印 16 通道
    for (int i = 0; i < 8; i++) {
        bufOutput[i] = tpafe51760_raw_to_voltage(adc_data[i * 2]);
    }
    
    // printf("-----------bufOutput[0] = %+.4f and bufOutput[1] = %+.4f-----------\n", bufOutput[0], bufOutput[1]);
    usleep(1000);
    close(tpafe51760_spi_fd);
    ret.ival = 1;
    return ret;
}

Cell DigitalIO_SetDigitalValue(SedonaVM* vm, Cell* params)
{
    int CHANNEL_INDEX_FIRST   = 0;
    int CHANNEL_INDEX_SECOND  = 1;
    int CHANNEL_INDEX_THIRD   = 2;
    int CHANNEL_INDEX_FOURTH  = 3;
    
    int   iChannelGroup   = params[1].ival;
    float fValue          = params[2].fval;
    
    Cell ret;
    ret.ival = -1;
	pthread_mutex_lock(g_V3S_GPIO_Operator->objMutex); // 加锁
    // PG0	DO1	数字输出通道1
    if(iChannelGroup == CHANNEL_INDEX_FIRST)
    {
        V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator,V3S_PG, 0, V3S_OUT);
        if(fValue != 0.0)
            V3S_GPIO_SetPin(g_V3S_GPIO_Operator,V3S_PG, 0, 1);
        else
            V3S_GPIO_SetPin(g_V3S_GPIO_Operator,V3S_PG, 0, 0);
    }
    // PG1	DO2	数字输出通道2
    else if(iChannelGroup == CHANNEL_INDEX_SECOND)
    {
        V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator,V3S_PG, 1, V3S_OUT);
        if(fValue != 0.0)
            V3S_GPIO_SetPin(g_V3S_GPIO_Operator,V3S_PG, 1, 1);
        else
            V3S_GPIO_SetPin(g_V3S_GPIO_Operator,V3S_PG, 1, 0);
    }
    // PG2	DO3	数字输出通道3
    else if(iChannelGroup == CHANNEL_INDEX_THIRD)
    {
        V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator,V3S_PG, 2, V3S_OUT);
        if(fValue != 0.0)
            V3S_GPIO_SetPin(g_V3S_GPIO_Operator,V3S_PG, 2, 1);
        else
            V3S_GPIO_SetPin(g_V3S_GPIO_Operator,V3S_PG, 2, 0);
    }
    // PG3	DO4	数字输出通道4
    else if(iChannelGroup == CHANNEL_INDEX_FOURTH)
    {
        V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator,V3S_PG, 3, V3S_OUT);
        if(fValue != 0.0)
            V3S_GPIO_SetPin(g_V3S_GPIO_Operator,V3S_PG, 3, 1);
        else
            V3S_GPIO_SetPin(g_V3S_GPIO_Operator,V3S_PG, 3, 0);
    }
    else 
    {
	    printf("NOTICE::iChannelGroup %d is error", iChannelGroup);
    }
	pthread_mutex_unlock(g_V3S_GPIO_Operator->objMutex); // 解锁
    ret.ival = 1;
    return ret;
}


Cell DigitalIO_GetDigitalValueByChannel(SedonaVM* vm, Cell* params)
{
    int CHANNEL_INDEX_FIRST   = 0;
    int CHANNEL_INDEX_SECOND  = 1;
    int CHANNEL_INDEX_THIRD   = 2;
    int CHANNEL_INDEX_FOURTH  = 3;
    
    int  iGetPinValue = 0;
    
    int   iChannelGroup   = params[1].ival;
    float * bufOutput = (float *)params[2].aval;
    
    Cell ret;
    ret.ival = -1;
	pthread_mutex_lock(g_V3S_GPIO_Operator->objMutex); // 加锁
    // PE11	DI1	数字输入通道1
    if(iChannelGroup == CHANNEL_INDEX_FIRST)
    {
        V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator,V3S_PE, 11, V3S_IN);
        iGetPinValue = V3S_GPIO_GetPin(g_V3S_GPIO_Operator,V3S_PE, 11);
        if(iGetPinValue)
            bufOutput[0] = 1.0;
        else 
            bufOutput[0] = 0.0;
    }
    // PE12	DI2	数字输入通道2
    else if(iChannelGroup == CHANNEL_INDEX_SECOND)
    {
        V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator,V3S_PE, 12, V3S_IN);
        iGetPinValue = V3S_GPIO_GetPin(g_V3S_GPIO_Operator,V3S_PE, 12);
        if(iGetPinValue)
            bufOutput[0] = 1.0;
        else 
            bufOutput[0] = 0.0;
    }
    // PE13	DI3	数字输入通道3
    else if(iChannelGroup == CHANNEL_INDEX_THIRD)
    {
        V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator,V3S_PE, 13, V3S_IN);
        iGetPinValue = V3S_GPIO_GetPin(g_V3S_GPIO_Operator,V3S_PE, 13);
        if(iGetPinValue)
            bufOutput[0] = 1.0;
        else 
            bufOutput[0] = 0.0;
    }
    // PE14	DI4	数字输入通道4
    else if(iChannelGroup == CHANNEL_INDEX_FOURTH)
    {
        V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator,V3S_PE, 14, V3S_IN);
        iGetPinValue = V3S_GPIO_GetPin(g_V3S_GPIO_Operator,V3S_PE, 14);
        if(iGetPinValue)
            bufOutput[0] = 1.0;
        else 
            bufOutput[0] = 0.0;
    }
    else 
    {
	    printf("NOTICE::iChannelGroup %d is error", iChannelGroup);
    }
	pthread_mutex_unlock(g_V3S_GPIO_Operator->objMutex); // 解锁
    ret.ival = 1;
    return ret;
}


Cell DigitalIO_GetDigitalValues(SedonaVM* vm, Cell* params)
{
    int  iGetPinValue = 0;
    float * bufOutput = (float *)params[1].aval;
    Cell ret;
    ret.ival = -1;
    
    // 初始化
	pthread_mutex_lock(g_V3S_GPIO_Operator->objMutex); // 加锁
    V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator,V3S_PE, 11, V3S_IN);
    V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator,V3S_PE, 12, V3S_IN);
    V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator,V3S_PE, 13, V3S_IN);
    V3S_GPIO_ConfigPin(g_V3S_GPIO_Operator,V3S_PE, 14, V3S_IN);
    // PE11	DI1	数字输入通道1
    iGetPinValue = V3S_GPIO_GetPin(g_V3S_GPIO_Operator,V3S_PE, 11);
    if(iGetPinValue)
        bufOutput[0] = 1.0;
    else 
        bufOutput[0] = 0.0;
    // PE12	DI2	数字输入通道2
    iGetPinValue = V3S_GPIO_GetPin(g_V3S_GPIO_Operator,V3S_PE, 12);
    if(iGetPinValue)
        bufOutput[1] = 1.0;
    else 
        bufOutput[1] = 0.0;
    // PE13	DI3	数字输入通道3
    iGetPinValue = V3S_GPIO_GetPin(g_V3S_GPIO_Operator,V3S_PE, 13);
    if(iGetPinValue)
        bufOutput[2] = 1.0;
    else 
        bufOutput[2] = 0.0;
    // PE14	DI4	数字输入通道4
    iGetPinValue = V3S_GPIO_GetPin(g_V3S_GPIO_Operator,V3S_PE, 14);
    if(iGetPinValue)
        bufOutput[3] = 1.0;
    else 
        bufOutput[3] = 0.0;
    // 
	pthread_mutex_unlock(g_V3S_GPIO_Operator->objMutex); // 解锁
    ret.ival = 1;
    return ret;
}