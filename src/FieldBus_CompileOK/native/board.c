#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "board.h"

static unsigned char res_uart_table[MAX_MODBUSRTU_NUM];

int acquire_uart(int idx, char *name)
{
    if (idx < 0 || idx >= MAX_MODBUSRTU_NUM) {
        return -1;
    }
    if (res_uart_table[idx] == 0) {
        char* uart_name[MAX_MODBUSRTU_NUM] = {"6", "7"};
        strcat(name, "/");
        strcat(name, "d");
        strcat(name, "e");
        strcat(name, "v");
        strcat(name, "/");
        strcat(name, "t");
        strcat(name, "t");
        strcat(name, "y");
        strcat(name, "S");
        strcat(name, uart_name[idx]);
        res_uart_table[idx] = 1;
        return idx;
    }
    return -1;
}

void release_uart(int idx)
{
    if (idx < 0 || idx >= MAX_MODBUSRTU_NUM) {
        return;
    }
    if (res_uart_table[idx] == 1) {
        res_uart_table[idx] = 0;
    }
}
