#include "sedona.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

Cell ModBus_rtuMaster_dO(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int band = params[2].ival;
    int parity = params[3].ival;
    int data_bit = params[4].ival;
    int stop_bit = params[5].ival;
    int retry_delay = params[6].ival;
    int rto = params[7].ival;

    Cell ret;
    {
        extern int rtu_open(int ctx_idx, int band, int parity, int data_bit, int stop_bit, int retry_delay, int rto);
        ret.ival = rtu_open(ctx_idx, band, parity, data_bit, stop_bit, retry_delay, rto);
    }

    return ret;
}

Cell ModBus_rtuMaster_dC(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;

    Cell ret;
    {
        extern int rtu_close(int ctx_idx);
        ret.ival = rtu_close(ctx_idx);
    }

    return ret;
}

Cell ModBus_rtuMaster_dA(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int device_addr = params[2].ival;
    int addr = params[3].ival;
    int len = params[4].ival;
    int refreshms = params[5].ival;
    int pointdelay = params[6].ival;

    Cell ret;
    {
        extern int rtu_add(int ctx_idx, int device_addr, int addr, int len, int refreshms, int reg_delay);
        ret.ival = rtu_add(ctx_idx, device_addr, addr, len, refreshms, pointdelay);
    }

    return ret;
}

Cell ModBus_rtuMaster_dR(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int device_addr = params[2].ival;
    int addr = params[3].ival;
    int len = params[4].ival;
    float *buf = params[5].aval;

    Cell ret;
    ret.ival = -1;
    if (len > 0) {
        extern int rtu_read(int ctx_idx, int device_addr, int addr, float *buf, int len);
        int result = rtu_read(ctx_idx, device_addr, addr, buf, len);
        if (result > 0) {
            ret.ival = result;
        }
    }

    return ret;
}

Cell ModBus_rtuMaster_dW(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int device_addr = params[2].ival;
    int addr = params[3].ival;
    int len = params[4].ival;
    float *buf = params[5].aval;

    Cell ret;
    ret.ival = -1;
    if (len > 0) {
        extern int rtu_write(int ctx_idx, int device_addr, int addr, float *buf, int len);
        int result = rtu_write(ctx_idx, device_addr, addr, buf, len);
        if (result > 0) {
            ret.ival = result;
        }
    }

    return ret;
}

Cell ModBus_rtuSlave_dO(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int band = params[2].ival;
    int parity = params[3].ival;
    int data_bit = params[4].ival;
    int stop_bit = params[5].ival;

    Cell ret;
    {
        extern int rtu_master_open(int ctx_idx, int band, int parity, int data_bit, int stop_bit);
        ret.ival = rtu_master_open(ctx_idx, band, parity, data_bit, stop_bit);
    }

    return ret;
}

Cell ModBus_rtuSlave_dC(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;

    Cell ret;
    {
        extern int rtu_master_close(int ctx_idx);
        ret.ival = rtu_master_close(ctx_idx);
    }

    return ret;
}

Cell ModBus_rtuSlave_dA(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int device_addr = params[2].ival;
    int addr = params[3].ival;
    int len = params[4].ival;

    Cell ret;
    {
        extern int rtu_master_add(int ctx_idx, int device_addr, int addr, int len);
        ret.ival = rtu_master_add(ctx_idx, device_addr, addr, len);
    }

    return ret;
}

Cell ModBus_rtuSlave_dR(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int device_addr = params[2].ival;
    int addr = params[3].ival;
    int len = params[4].ival;
    float *buf = params[5].aval;

    Cell ret;
    ret.ival = -1;
    if (len > 0) {
        extern int rtu_master_read(int ctx_idx, int device_addr, int addr, float *buf, int len);
        int result = rtu_master_read(ctx_idx, device_addr, addr, buf, len);
        if (result > 0) {
            ret.ival = result;
        }
    }

    return ret;
}

Cell ModBus_rtuSlave_dW(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int device_addr = params[2].ival;
    int addr = params[3].ival;
    int len = params[4].ival;
    float *buf = params[5].aval;

    Cell ret;
    ret.ival = -1;
    if (len > 0) {
        extern int rtu_master_write(int ctx_idx, int device_addr, int addr, float *buf, int len);
        int result = rtu_master_write(ctx_idx, device_addr, addr, buf, len);
        if (result > 0) {
            ret.ival = result;
        }
    }

    return ret;
}

Cell ModBus_tcpClient_dO(SedonaVM* vm, Cell* params)
{
    char *ip = params[1].aval;
    int port = params[2].ival;

    Cell ret;
    {
        extern int tcp_open(char *ip, int port);
        ret.ival = tcp_open(ip, port);
    }

    return ret;
}

Cell ModBus_tcpClient_dC(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;

    Cell ret;
    {
        extern int tcp_close(int ctx_idx);
        ret.ival = tcp_close(ctx_idx);
    }

    return ret;
}

Cell ModBus_tcpClient_dA(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int device_addr = params[2].ival;
    int addr = params[3].ival;
    int len = params[4].ival;
    int refreshms = params[5].ival;

    Cell ret;
    {
        extern int tcp_add(int ctx_idx, int device_addr, int addr, int len, int refreshms);
        ret.ival = tcp_add(ctx_idx, device_addr, addr, len, refreshms);
    }

    return ret;
}

Cell ModBus_tcpClient_dR(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int device_addr = params[2].ival;
    int addr = params[3].ival;
    int len = params[4].ival;
    float *buf = params[5].aval;

    Cell ret;
    ret.ival = -1;
    if (len > 0) {
        extern int tcp_read(int ctx_idx, int device_addr, int addr, float *buf, int len);
        int result = tcp_read(ctx_idx, device_addr, addr, buf, len);
        if (result > 0) {
            ret.ival = result;
        }
    }

    return ret;
}

Cell ModBus_tcpClient_dW(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int device_addr = params[2].ival;
    int addr = params[3].ival;
    int len = params[4].ival;
    float *buf = params[5].aval;

    Cell ret;
    ret.ival = -1;
    if (len > 0) {
        extern int tcp_write(int ctx_idx, int device_addr, int addr, float *buf, int len);
        int result = tcp_write(ctx_idx, device_addr, addr, buf, len);
        if (result > 0) {
            ret.ival = result;
        }
    }

    return ret;
}

Cell ModBus_tcpServer_dO(SedonaVM* vm, Cell* params)
{
    int port = params[1].ival;

    Cell ret;
    {
        extern int tcp_master_open(int port);
        ret.ival = tcp_master_open(port);
    }

    return ret;
}

Cell ModBus_tcpServer_dC(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;

    Cell ret;
    {
        extern int tcp_master_close(int ctx_idx);
        ret.ival = tcp_master_close(ctx_idx);
    }

    return ret;
}

Cell ModBus_tcpServer_dA(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int device_addr = params[2].ival;
    int addr = params[3].ival;
    int len = params[4].ival;

    Cell ret;
    {
        extern int tcp_master_add(int ctx_idx, int device_addr, int addr, int len);
        ret.ival = tcp_master_add(ctx_idx, device_addr, addr, len);
    }

    return ret;
}

Cell ModBus_tcpServer_dR(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int device_addr = params[2].ival;
    int addr = params[3].ival;
    int len = params[4].ival;
    float *buf = params[5].aval;

    Cell ret;
    ret.ival = -1;
    if (len > 0) {
        extern int tcp_master_read(int ctx_idx, int device_addr, int addr, float *buf, int len);
        int result = tcp_master_read(ctx_idx, device_addr, addr, buf, len);
        if (result > 0) {
            ret.ival = result;
        }
    }

    return ret;
}

Cell ModBus_tcpServer_dW(SedonaVM* vm, Cell* params)
{
    int ctx_idx = params[1].ival;
    int device_addr = params[2].ival;
    int addr = params[3].ival;
    int len = params[4].ival;
    float *buf = params[5].aval;

    Cell ret;
    ret.ival = -1;
    if (len > 0) {
        extern int tcp_master_write(int ctx_idx, int device_addr, int addr, float *buf, int len);
        int result = tcp_master_write(ctx_idx, device_addr, addr, buf, len);
        if (result > 0) {
            ret.ival = result;
        }
    }

    return ret;
}
