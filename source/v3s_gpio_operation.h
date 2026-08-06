#ifndef __V3S_GPIO_H__
#define __V3S_GPIO_H__
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#define V3S_PIO_BASE_ADDRESS 0x01C20800

#define V3S_PIO_NUMBER 7

//unsigned int 4字节 一个PIO_Struct占36字节,对应十六进制就是0x24，正好是一个offset值。
typedef struct
{
    unsigned int V3S_CFG[4];
    unsigned int V3S_DAT;
    unsigned int V3S_DRV0;
    unsigned int V3S_DRV1;
    unsigned int V3S_PUL0;
    unsigned int V3S_PUL1;
} V3S_PIO_Struct;

typedef struct
{
    V3S_PIO_Struct V3S_Pn[V3S_PIO_NUMBER];
} V3S_PIO_Map;

/*
  The chip has 5 ports for multi-functional input/output pins. They are shown below: 
    Port B(PB): 10 input/output port 
    Port C(PC): 4 input/output port 
    Port E(PE) : 25 input/output port 
    Port F(PF) : 7 input/output port 
    Port G(PG) : 6 input/output port 
  For various system configurations, these ports can be easily configured by software. 
  All these ports can be configured as GPIO if multiplexed functions are not used. 
  The total 2 group external PIO interrupt sources are supported and 
  interrupt mode can be configured by software.
 */
typedef enum {
    V3S_PA = 0,   // Not used
    V3S_PB = 1,
    V3S_PC = 2,
    V3S_PD = 3,   // Not used
    V3S_PE = 4,
    V3S_PF = 5,
    V3S_PG = 6,
} V3S_PORT;

typedef enum {
    V3S_IN = 0x00,
    V3S_OUT = 0x01,
    V3S_AUX = 0x02,
    V3S_INT = 0x06,
    V3S_DISABLE = 0x07,
} V3S_PIN_MODE;


typedef enum {
    V3S_UART_NONE = 0x00,
    V3S_UART_485  = 0x01,
    V3S_UART_232  = 0x02,
} V3S_UART_MODE;

// extern V3S_PIO_Map *g_V3S_PIO;

typedef struct
{
    char magicNumber;
	V3S_UART_MODE  enumUartMode;
    pthread_mutex_t* objMutex;
    V3S_PIO_Map * objV3sPIOMap;
} V3S_GPIO_Operator;

// extern V3S_GPIO_Operator* g_V3S_GPIO_Operator = NULL ;

int V3S_GPIO_Init(V3S_PIO_Map ** objV3sPIOMap);
void V3S_GPIO_ConfigPin(V3S_GPIO_Operator * objV3sPIOOperator, V3S_PORT port, unsigned int pin, V3S_PIN_MODE mode);
void V3S_GPIO_SetPin(V3S_GPIO_Operator * objV3sPIOOperator, V3S_PORT port, unsigned int pin, unsigned int level);
unsigned int V3S_GPIO_GetPin(V3S_GPIO_Operator * objV3sPIOOperator, V3S_PORT port, unsigned int pin);
void V3S_GPIO_Free(void);
#endif
