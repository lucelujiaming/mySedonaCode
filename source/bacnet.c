#include "sedona.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

Cell BacNet_BIP_dO(SedonaVM* vm, Cell* params)
{
    int port = params[1].ival;
    int device_instance = params[2].ival;
    int retry_delay = params[3].ival;

    Cell ret;
    {
        extern int bip_open(int port, int device_instance, int retry_delay);
        ret.ival = bip_open(port, device_instance, retry_delay);
    }

    return ret;
}

Cell BacNet_BIP_dC(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;

    Cell ret;
    {
        extern int bip_close(int ctx_idx);
        ret.ival = bip_close(ctx_idx);
    }

    return ret;
}

Cell BacNet_BIP_dA(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int device_instance = params[2].ival;
    int object_type = params[3].ival;
    int object_instance = params[4].ival;
    int object_property = params[5].ival;
    int refreshms = params[6].ival;

    Cell ret;
    {
        extern int bip_add(int ctx_idx, int device_instance, int object_type, int object_instance, int object_property, int refreshms);
        ret.ival = bip_add(ctx_idx, device_instance, object_type, object_instance, object_property, refreshms);
    }

    return ret;
}

Cell BacNet_BIP_dR(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int cache = params[2].ival;
    int reg_addr = params[3].ival;
    int type = params[4].ival;
    float *buf = params[5].aval;

    Cell ret;
    ret.ival = -1;
    if (type >= 0) {
        extern int bip_read(int ctx_idx, int cache, int reg_addr, float *buf, int type);
        int result = bip_read(ctx_idx, cache, reg_addr, buf, type);
        if (result > 0) {
            ret.ival = result;
        }
    }

    return ret;
}

Cell BacNet_BIP_dW(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int cache = params[2].ival;
    int reg_addr = params[3].ival;
    int type = params[4].ival;
    float *buf = params[5].aval;

    Cell ret;
    ret.ival = -1;
    if (type >= 0) {
        extern int bip_write(int ctx_idx, int cache, int reg_addr, float *buf, int type);
        int result = bip_write(ctx_idx, cache, reg_addr, buf, type);
        if (result > 0) {
            ret.ival = result;
        }
    }

    return ret;
}

Cell BacNet_MSTP_dO(SedonaVM* vm, Cell* params)
{
    int port = params[1].ival;
    int band = params[2].ival;
    int device_instance = params[3].ival;
    int mac_address = params[4].ival;
    int retry_delay = params[5].ival;

    Cell ret;
    {
        extern int mstp_open(int port, int band, int device_instance, int mac_address, int retry_delay);
        ret.ival = mstp_open(port, band, device_instance, mac_address, retry_delay);
    }

    return ret;
}

Cell BacNet_MSTP_dC(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;

    Cell ret;
    {
        extern int mstp_close(int ctx_idx);
        ret.ival = mstp_close(ctx_idx);
    }

    return ret;
}

Cell BacNet_MSTP_dA(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int device_instance = params[2].ival;
    int object_type = params[3].ival;
    int object_instance = params[4].ival;
    int object_property = params[5].ival;
    int refreshms = params[6].ival;

    Cell ret;
    {
        extern int mstp_add(int ctx_idx, int device_instance, int object_type, int object_instance, int object_property, int refreshms);
        ret.ival = mstp_add(ctx_idx, device_instance, object_type, object_instance, object_property, refreshms);
    }

    return ret;
}

Cell BacNet_MSTP_dR(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int cache = params[2].ival;
    int reg_addr = params[3].ival;
    int type = params[4].ival;
    float *buf = params[5].aval;

    Cell ret;
    ret.ival = -1;
    if (type >= 0) {
        extern int mstp_read(int ctx_idx, int cache, int reg_addr, float *buf, int type);
        int result = mstp_read(ctx_idx, cache, reg_addr, buf, type);
        if (result > 0) {
            ret.ival = result;
        }
    }

    return ret;
}

Cell BacNet_MSTP_dW(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int cache = params[2].ival;
    int reg_addr = params[3].ival;
    int type = params[4].ival;
    float *buf = params[5].aval;

    Cell ret;
    ret.ival = -1;
    if (type >= 0) {
        extern int mstp_write(int ctx_idx, int cache, int reg_addr, float *buf, int type);
        int result = mstp_write(ctx_idx, cache, reg_addr, buf, type);
        if (result > 0) {
            ret.ival = result;
        }
    }

    return ret;
}

Cell BacNet_BIP_dS(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    char *url = params[2].aval;
    int *time = params[3].aval;
    float *value = params[4].aval;

    Cell ret;
    ret.ival = -1;

    {
        extern int bip_add_schedule(int ctx_idx, char *urlBuf, int *timeBuf, float *valueBuf);
        int result = bip_add_schedule(ctx_idx, url, time, value);
        if (result >= 0) {
            ret.ival = result;
        }
    }

    return ret;
}

Cell BacNet_BIP_dT(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int sche_idx = params[2].ival;
    int *time = params[3].aval;
    float *value = params[4].aval;

    Cell ret;
    ret.ival = -1;

    {
        extern int bip_get_schedule(int ctx_idx, int sche_idx, int *timeBuf, float *valueBuf);
        int result = bip_get_schedule(ctx_idx, sche_idx, time, value);
        if (result >= 0) {
            ret.ival = result;
        }
    }

    return ret;
}

Cell BacNet_MSTP_dS(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    char *url = params[2].aval;
    int *time = params[3].aval;
    float *value = params[4].aval;

    Cell ret;
    ret.ival = -1;

    {
        extern int mstp_add_schedule(int ctx_idx, char *urlBuf, int *timeBuf, float *valueBuf);
        int result = mstp_add_schedule(ctx_idx, url, time, value);
        if (result >= 0) {
            ret.ival = result;
        }
    }

    return ret;
}

Cell BacNet_MSTP_dT(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int sche_idx = params[2].ival;
    int *time = params[3].aval;
    float *value = params[4].aval;

    Cell ret;
    ret.ival = -1;

    {
        extern int mstp_get_schedule(int ctx_idx, int sche_idx, int *timeBuf, float *valueBuf);
        int result = mstp_get_schedule(ctx_idx, sche_idx, time, value);
        if (result >= 0) {
            ret.ival = result;
        }
    }

    return ret;
}

// #define ENDIAN_SWAP 1
Cell BacNet_BIP_dD(SedonaVM* vm, Cell* params)
{
    char * bufInput = params[1].aval;
    double * bufOutput = params[2].aval;

    Cell ret;
    ret.ival = -1;
// #ifdef ENDIAN_SWAP
	char * pDoubleValue = (char *)(&bufOutput[0]);
	pDoubleValue[3] = bufInput[0];
	pDoubleValue[2] = bufInput[1];
	pDoubleValue[1] = bufInput[2];
	pDoubleValue[0] = bufInput[3];
	pDoubleValue[7] = bufInput[4];
	pDoubleValue[6] = bufInput[5];
	pDoubleValue[5] = bufInput[6];
	pDoubleValue[4] = bufInput[7];
// #else
// 		memcpy(&(bufOutput[0]), bufInput, (size_t)8);
// #endif
	ret.ival = 0;
	
    return ret;
}

// #define ENDIAN_SWAP 1
Cell BacNet_BIP_dF(SedonaVM* vm, Cell* params)
{
    char * bufInput = params[1].aval;
    float * bufOutput = params[2].aval;
	
    Cell ret;
    ret.ival = -1;
// #ifdef ENDIAN_SWAP
	int i = 0, j = 3;
	char * pFloatValue = (char *)(&bufOutput[0]);
	for (; j >= 0; i++, j--)
	{
		pFloatValue[j] = bufInput[i];
		//	printf("[%s:%s:%d] BacNet_BIP_cF Fill %02X\n",
		//		__FILE__, __FUNCTION__, __LINE__, bufInput[i]);
	}
	// printf("[%s:%s:%d] BacNet_BIP_cF Fill %f\n",
	//		 __FILE__, __FUNCTION__, __LINE__, bufOutput[0]);
// #else
//		memcpy(&(bufOutput[0]), bufInput, (size_t)4);
// #endif
	ret.ival = 0;
	
    return ret;
}

// #define ENDIAN_SWAP 1
Cell BacNet_BIP_eD(SedonaVM* vm, Cell* params)
{
    double * bufInput = params[1].aval;
    char * bufOutput = params[2].aval;

    Cell ret;
    ret.ival = -1;
	char * pCharValue = (char *)(&bufInput[0]);
	bufOutput[0] = pCharValue[3];
	bufOutput[1] = pCharValue[2];
	bufOutput[2] = pCharValue[1];
	bufOutput[3] = pCharValue[0];
	bufOutput[4] = pCharValue[7];
	bufOutput[5] = pCharValue[6];
	bufOutput[6] = pCharValue[5];
	bufOutput[7] = pCharValue[4];
	
	ret.ival = 0;
	
    return ret;
}

// #define ENDIAN_SWAP 1
Cell BacNet_BIP_eF(SedonaVM* vm, Cell* params)
{
    float * bufInput = params[1].aval;
    char * bufOutput = params[2].aval;
	
    Cell ret;
    ret.ival = -1;
	char * pCharValue = (char *)(&bufInput[0]);
	bufOutput[0] = pCharValue[3];
	bufOutput[1] = pCharValue[2];
	bufOutput[2] = pCharValue[1];
	bufOutput[3] = pCharValue[0];
	ret.ival = 0;
	
    return ret;
}


