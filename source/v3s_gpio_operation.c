#include <pthread.h>
#include "v3s_gpio_operation.h"

// V3S_PIO_Map * g_V3S_PIO = NULL;
unsigned int *v3s_gpio_map;
unsigned int PageSize;

// 1.PB0-PB9
// 2.PC0-PC3
// 4.PE0-PE24
// 5.PF0-PF6
// 6.PG0-PG5
static unsigned int sub_gpio_number[V3S_PIO_NUMBER] = {
    0, 9, 3, 0, 24, 6, 5
};

int V3S_GPIO_Init(V3S_PIO_Map ** objV3sPIOMap)
{
    unsigned int fd;
    unsigned int addr_start, addr_offset;
    unsigned int PageMask;
    if ((fd = open("/dev/mem", O_RDWR)) == -1)
    {
        printf("open error\r\n");
        return -1;
    }

    PageSize = sysconf(_SC_PAGESIZE); // 使用sysconf查询系统页面大小
    PageMask = (~(PageSize - 1));     // 计算页掩码
    // printf("PageSize:%d,PageMask:0x%.8X\r\n", PageSize, PageMask);

    // 计算用于mmap的起始地址和偏移量。
    // 使用mmap映射物理内存的话，必须页对齐!
    // 所以这个起始地址应该是0x1000的整数倍，也就是PageMask这个掩码的来源。
    // 那么明显0x01C20800需要减去0x800才是整数倍！
    addr_start = V3S_PIO_BASE_ADDRESS & PageMask;   //0x01C20800 & 0xfffff000 =  0x1C20000
    addr_offset = V3S_PIO_BASE_ADDRESS & ~PageMask; //0x01C20800 & 0x00000100 = 0x800
    // printf("addr_start:%.8X,addr_offset:0x%.8X\r\n", addr_start, addr_offset);
    // mmap(系统自动分配内存地址，映射区长度“内存页的整数倍”，
    // 选择可读可写，MAP_SHARED=与其他所有映射到这个对象的
    // 进程共享空间，文件句柄，被映射内容的起点)
    //////////////////////////////////// 
    //offest 映射物理内存的话，必须页对齐! 
    // 所以这个起始地址应该是0x1000的整数倍，
    // 那么明显0x01C20800需要减去0x800才是整数倍！
    // g_V3S_PIO = NULL;
    if ((v3s_gpio_map = (unsigned int*) mmap(NULL, PageSize * 2, 
                PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr_start)) == NULL)
    {
        printf("mmap error\r\n");
        close(fd);
        return -1;
    }
    // printf("gpio_map:%.8X\r\n", (unsigned int)v3s_gpio_map);
    // 这里已经将0x1c20000的地址映射到了内存中，
    // 但是我们需要的地址是0x01C20800，所以要再加上地址偏移量～
    *objV3sPIOMap = (V3S_PIO_Map *)((unsigned int)v3s_gpio_map + addr_offset);
    printf("PIO:0x%.8X\r\n", (unsigned int)objV3sPIOMap);

    close(fd); //映射好之后就可以关闭文件？
    return 0;
}

/*
 * CFG[0] used by Px07 ~ Px00
 * CFG[1] used by Px15 ~ Px08
 ******************************************************************
 * 每一个CFG[x]包含8个引脚，一个引脚使用4个比特位控制。
 * 其中只有低3位可用，第4个比特位保留。
 * 这就是代码中7的由来。
 * 首先计算pin % 8 * 4，也就是说如果设定的是PB2。
 * 则port = PB，pin = 2，因此上，pin % 8 * 4 = 8。
 * 则 (unsigned int)0x07 << pin % 8 * 4 = 1792 = 0x700。
 * 之后对1792求反，得到了4294965503 = 0xFFFF F8FF
 * 之后求并，就清除了对应的配置位。
 * 
 * 使用同样的方法，就可以在后面使用mode设置对应的比特位。
 */
void V3S_GPIO_ConfigPin(V3S_GPIO_Operator * objV3sPIOOperator, V3S_PORT port, unsigned int pin, V3S_PIN_MODE mode)
{
	if(objV3sPIOOperator == NULL)
	{
        printf("V3S_GPIO_ConfigPin::objV3sPIOOperator == NULL!\r\n");
		return;
	}
	else if(objV3sPIOOperator->objV3sPIOMap == NULL)
	{
		printf("V3S_GPIO_ConfigPin::objV3sPIOOperator->magicNumber == %X!\r\n", 
			objV3sPIOOperator->magicNumber);
        printf("V3S_GPIO_ConfigPin::objV3sPIOOperator->objV3sPIOMap == NULL!\r\n");
		return;
	}
	
    if((port == V3S_PA) || (port == V3S_PD))
    {
        printf("The %d is wrong port!\r\n", port);
        return;
    }
    if(sub_gpio_number[port] < pin)
    {
        printf("Pin %d is wrong to port %d!\r\n", pin, port);
        return;
    }
    // if (v3s_gpio_map == NULL)
    // {
    //     printf("v3s_gpio_map == NULL!\r\n");
    //     return;
    // }
    
    // printf("V3S_GPIO_ConfigPin::Pin %d is config at port %d!\r\n", pin, port);
    // 把pin指定的对应配置位清零。
    objV3sPIOOperator->objV3sPIOMap->V3S_Pn[port].V3S_CFG[pin / 8] &= ~((unsigned int)0x07 << pin % 8 * 4);
    // 设置pin指定的对应配置位。
    objV3sPIOOperator->objV3sPIOMap->V3S_Pn[port].V3S_CFG[pin / 8] |= ((unsigned int)mode << pin % 8 * 4);
    // printf("struct PIO_Struct size : %d\r\n",sizeof(g_PIO->Pn[port]));
}

void V3S_GPIO_SetPin(V3S_GPIO_Operator * objV3sPIOOperator, V3S_PORT port, unsigned int pin, unsigned int level)
{
	if((objV3sPIOOperator == NULL) || (objV3sPIOOperator->objV3sPIOMap == NULL))
	{
        printf("V3S_GPIO_SetPin::objV3sPIOOperator == NULL!\r\n");
		return;
	}
    int iTestValue = 0;
    if((port == V3S_PA) || (port == V3S_PD))
    {
        printf("The %d is wrong port!\r\n", port);
        return;
    }
    if(sub_gpio_number[port] < pin)
    {
        printf("Pin %d is wrong to port %d!\r\n", pin, port);
        return;
    }
    // if (v3s_gpio_map == NULL)
    // {
    //     printf("v3s_gpio_map == NULL!\r\n");
    //     return;
    // }
	
    if (level)
    {
        objV3sPIOOperator->objV3sPIOMap->V3S_Pn[port].V3S_DAT |= (1 << pin);
    }
    else
    {
        objV3sPIOOperator->objV3sPIOMap->V3S_Pn[port].V3S_DAT &= ~(1 << pin);
    }
    
    iTestValue = V3S_GPIO_GetPin(objV3sPIOOperator, port, pin);
    printf("Pin %d is %d to port %d!\r\n", pin, iTestValue, port);
    
}

unsigned int V3S_GPIO_GetPin(V3S_GPIO_Operator * objV3sPIOOperator, V3S_PORT port, unsigned int pin)
{
	if((objV3sPIOOperator == NULL) || (objV3sPIOOperator->objV3sPIOMap == NULL))
	{
        printf("V3S_GPIO_GetPin::objV3sPIOOperator == NULL!\r\n");
		return 2;
	}
    if((port == V3S_PA) || (port == V3S_PD))
    {
        printf("The %d is wrong port!\r\n", port);
        return 2;
    }
    if(sub_gpio_number[port] < pin)
    {
        printf("Pin %d is wrong to port %d!\r\n", pin, port);
        return 2;
    }
    // if (v3s_gpio_map == NULL)
    // {
    //     printf("v3s_gpio_map == NULL!\r\n");
    //     return -1;
    // }

    if (objV3sPIOOperator->objV3sPIOMap->V3S_Pn[port].V3S_DAT & (1 << pin))
    {
        // printf("g_V3S_PIO->V3S_Pn[%d].V3S_DAT[%d] is set\r\n", (int)port, pin);
        return 1;
    }
    else
    {
        // printf("g_V3S_PIO->V3S_Pn[%d].V3S_DAT[%d] is unset\r\n", (int)port, pin);
        return 0;
    }
}

void V3S_GPIO_Free(void)
{
    if (v3s_gpio_map == NULL)
    {
        return ;
    }
    if ((munmap(v3s_gpio_map, PageSize * 2)) == 0)//取消映射
    {
        ;// printf("unmap success!\r\n");
    }
    else
    {
        printf("unmap failed!\r\n");
    }
}

