#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/time.h>
#include <pthread.h>
#include "modbus.h"
#include "board.h"
#include "list.h"

extern unsigned long long get_tick_ms(void);

// 数据包最大承载的连续读取个数。
#define MODBUS_READ_BLOCK_SIZE 120
// 合并寄存器最大间隙。
#define MODBUS_READ_SKIP_SIZE 50

// 多个读连续尝试次数
#define MAX_MREAD_ERROR_COUNT 3
// 多个读最大出错次数
#define MAX_MREAD_ERROR_COUNT_PLUS 1
// 当额度最大出错次数
#define MAX_SREAD_ERROR_COUNT 1
// 读取出错多少次之后掉线。
#define MAX_ERROR_COUNT (MAX_MREAD_ERROR_COUNT + MAX_MREAD_ERROR_COUNT_PLUS + MAX_SREAD_ERROR_COUNT)

//#define DEBUG_LIST
#ifdef DEBUG_LIST
static int mem_count;
#endif

typedef struct {
    list_node_t node;
    unsigned short addr;

    unsigned char endian:1;
    unsigned char is_force_update:1;
    unsigned char is_dirty:1;

    unsigned char err_cnt;

    unsigned short val;
    unsigned short val_w;
} element_t;

typedef struct {
    list_node_t node;
    unsigned char addr;

    // 设备寄存去读写预热时间。
    unsigned int point_delay_ms;

    // 设备刷新周期。
    unsigned int refresh_ms;
    unsigned long long next_update;

    // 设备出错时间，用于出错之后重试时刻的确定。
    unsigned long long last_err;

    // 设备扫描花费时间。
    unsigned int spent_time_ms;
    // 设备发送数据包总和
    unsigned int packet_cnt_total;
    unsigned int packet_cnt_err;

    list_head_t DO; // 00001, list of element_t.
    list_head_t DI; // 10001
    list_head_t INPUT; // 30001
    list_head_t HOLD; // 40001
} device_t;

typedef struct {
    list_node_t node;
    device_t *dev;
    element_t *reg;
} req_t;

#define LOCK_WRITE_QUEUE while (c->write_queue_lock != 0) usleep(10*1000); c->write_queue_lock = 1
#define UNLOCK_WRITE_QUEUE c->write_queue_lock = 0
typedef struct
{
    modbus_t *ctx_modbus;
    pthread_t ctx_thread;
    unsigned char ctx_thread_running;
    unsigned char ctx_added;
    unsigned int retry_delay_ms;

    list_head_t devices; // list of device_t

    int write_queue_lock;
    list_head_t write_queue; // list of req_t
} context_t;
static context_t context_table[MAX_MODBUSRTU_NUM];

static device_t* get_next_read_device(context_t *c)
{
    unsigned long long curr_tick = get_tick_ms();
    unsigned long long next_tick = curr_tick;

    list_node_t *dev_node = c->devices.first;
    while (c->ctx_thread_running != 0 && dev_node != NULL) {
        device_t *device = list_entry_safe(dev_node, device_t);
        if (device->next_update < next_tick)
            next_tick = device->next_update;
        dev_node = dev_node->next;
    }

    if (c->ctx_thread_running != 0 && next_tick != curr_tick) {
        dev_node = c->devices.first;
        while (dev_node != NULL) {
            device_t *device = list_entry_safe(dev_node, device_t);
            if (device->next_update == next_tick) {
                device->next_update = get_tick_ms() + (unsigned long long)device->refresh_ms;
                return device;
            }
            dev_node = dev_node->next;
        }
    }

    return NULL;
}

static inline void exec_one_reg_write_from_list(context_t *c)
{
    if (c->ctx_thread_running != 0 && c->write_queue.len > 0) {
        LOCK_WRITE_QUEUE;
        list_node_t *node = list_get(&c->write_queue);
        req_t *req = list_entry_safe(node, req_t);
        UNLOCK_WRITE_QUEUE;
        device_t *device = req->dev;
        element_t *element = req->reg;
        if (element->err_cnt == 0) {
            int ctx_idx = c - &context_table[0];
            unsigned char devid = device->addr;
            unsigned short address = element->addr;
            if (element->is_dirty == 1) {
                if (address > 0 && address < 10000) {
                    if (device->point_delay_ms > 0) {
                        usleep(device->point_delay_ms * 1000);
                    }
                    modbus_set_slave(c->ctx_modbus, devid);
                    led_on(ctx_idx);
                    modbus_write_bit(c->ctx_modbus, address - 1, element->val_w);
                    led_off(ctx_idx);
                    element->is_dirty = 0;
                } else if (address > 40000 && address < 50000) {
                    if (device->point_delay_ms > 0) {
                        usleep(device->point_delay_ms * 1000);
                    }
                    modbus_set_slave(c->ctx_modbus, devid);
                    led_on(ctx_idx);
                    modbus_write_register(c->ctx_modbus, address - 40001, element->val_w);
                    led_off(ctx_idx);
                    element->is_dirty = 0;
                }
                device->packet_cnt_total ++;
            }
        } else {
            element->is_force_update = 1;
        }
        free(req);
#ifdef DEBUG_LIST
        mem_count--;
#endif
    }
}

static inline void exec_mutli_element_write(context_t *c, device_t *device, element_t *start_element, int len)
{
    int ctx_idx = c - &context_table[0];
    unsigned char devid = device->addr;
    unsigned short start_addr = start_element->addr;
    if (start_addr > 0 && start_addr < 10000) {
        list_node_t *reg_node = &start_element->node;
        int write_len = len;
        unsigned char write_addr = 0;
        unsigned char write_size = 0;
        unsigned char v[MODBUS_READ_BLOCK_SIZE] = {0};
        while (write_len > 0) {
            if (reg_node == NULL) {
                printf("Error: write node not enough !\n");
                break;
            }
            element_t *element = list_entry_safe(reg_node, element_t);
            if (element->addr - start_addr == len - write_len) { // 连续
                if (element->is_dirty == 1) {
                    if (write_size == 0) {
                        write_addr = element->addr - start_addr;
                    }
                    write_size ++;
                    v[element->addr - start_addr] = element->val_w;
                    element->is_dirty = 0;
                } else if (write_size > 0) {
                    if (device->point_delay_ms > 0) {
                        usleep(device->point_delay_ms * 1000);
                    }
                    modbus_set_slave(c->ctx_modbus, devid);
                    led_on(ctx_idx);
                    if (write_size == 1) {
                        modbus_write_bit(c->ctx_modbus, write_addr + start_addr - 1, v[write_addr]);
                    } else {
                        modbus_write_bits(c->ctx_modbus, write_addr + start_addr - 1, write_size, &v[write_addr]);
                    }
                    device->packet_cnt_total ++;
                    led_off(ctx_idx);
                    write_size = 0;
                }
                reg_node = reg_node->next;
            } else { // 不连续
                if (write_size > 0) {
                    if (device->point_delay_ms > 0) {
                        usleep(device->point_delay_ms * 1000);
                    }
                    modbus_set_slave(c->ctx_modbus, devid);
                    led_on(ctx_idx);
                    if (write_size == 1) {
                        modbus_write_bit(c->ctx_modbus, write_addr + start_addr - 1, v[write_addr]);
                    } else {
                        modbus_write_bits(c->ctx_modbus, write_addr + start_addr - 1, write_size, &v[write_addr]);
                    }
                    device->packet_cnt_total ++;
                    led_off(ctx_idx);
                    write_size = 0;
                }
            }
            write_len --;
        }
        if (write_size > 0) {
            if (device->point_delay_ms > 0) {
                usleep(device->point_delay_ms * 1000);
            }
            modbus_set_slave(c->ctx_modbus, devid);
            led_on(ctx_idx);
            if (write_size == 1) {
                modbus_write_bit(c->ctx_modbus, write_addr + start_addr - 1, v[write_addr]);
            } else {
                modbus_write_bits(c->ctx_modbus, write_addr + start_addr - 1, write_size, &v[write_addr]);
            }
            device->packet_cnt_total ++;
            led_off(ctx_idx);
            write_size = 0;
        }
    } else if (start_addr > 40000 && start_addr < 50000) {
        list_node_t *reg_node = &start_element->node;
        int write_len = len;
        unsigned char write_addr = 0;
        unsigned char write_size = 0;
        unsigned short v[MODBUS_READ_BLOCK_SIZE] = {0};
        while (write_len > 0) {
            if (reg_node == NULL) {
                printf("Error: write node not enough !\n");
                break;
            }
            element_t *element = list_entry_safe(reg_node, element_t);
            if (element->addr - start_addr == len - write_len) { // 连续
                if (element->is_dirty == 1) {
                    if (write_size == 0) {
                        write_addr = element->addr - start_addr;
                    }
                    write_size ++;
                    v[element->addr - start_addr] = element->val_w;
                    element->is_dirty = 0;
                } else if (write_size > 0) {
                    if (device->point_delay_ms > 0) {
                        usleep(device->point_delay_ms * 1000);
                    }
                    modbus_set_slave(c->ctx_modbus, devid);
                    led_on(ctx_idx);
                    if (write_size == 1) {
                        modbus_write_register(c->ctx_modbus, write_addr + start_addr - 40001, v[write_addr]);
                    } else {
                        modbus_write_registers(c->ctx_modbus, write_addr + start_addr - 40001, write_size, &v[write_addr]);
                    }
                    device->packet_cnt_total ++;
                    led_off(ctx_idx);
                    write_size = 0;
                }
                reg_node = reg_node->next;
            } else { // 不连续
                if (write_size > 0) {
                    if (device->point_delay_ms > 0) {
                        usleep(device->point_delay_ms * 1000);
                    }
                    modbus_set_slave(c->ctx_modbus, devid);
                    led_on(ctx_idx);
                    if (write_size == 1) {
                        modbus_write_register(c->ctx_modbus, write_addr + start_addr - 40001, v[write_addr]);
                    } else {
                        modbus_write_registers(c->ctx_modbus, write_addr + start_addr - 40001, write_size, &v[write_addr]);
                    }
                    device->packet_cnt_total ++;
                    led_off(ctx_idx);
                    write_size = 0;
                }
            }
            write_len --;
        }
        if (write_size > 0) {
            if (device->point_delay_ms > 0) {
                usleep(device->point_delay_ms * 1000);
            }
            modbus_set_slave(c->ctx_modbus, devid);
            led_on(ctx_idx);
            if (write_size == 1) {
                modbus_write_register(c->ctx_modbus, write_addr + start_addr - 40001, v[write_addr]);
            } else {
                modbus_write_registers(c->ctx_modbus, write_addr + start_addr - 40001, write_size, &v[write_addr]);
            }
            device->packet_cnt_total ++;
            led_off(ctx_idx);
            write_size = 0;
        }
    }
    // clean write req list.
    LOCK_WRITE_QUEUE;
    list_node_t *req_node = c->write_queue.first;
    while (req_node != NULL) {
        req_t *_req = list_entry_safe(req_node, req_t);
        if (_req->reg->is_dirty == 0) {
            list_delete(&c->write_queue, req_node);
            free(_req);
#ifdef DEBUG_LIST
            mem_count--;
#endif
            break;
        }
        req_node = req_node->next;
    }
    UNLOCK_WRITE_QUEUE;
}

static inline void exec_mutli_element_read(context_t *c, device_t *device, element_t *start_element, int len)
{
    int ctx_idx = c - &context_table[0];
    unsigned char devid = device->addr;
    unsigned short start_addr = start_element->addr;
    int retry_cnt = start_element->err_cnt == 0 ? MAX_MREAD_ERROR_COUNT : 1;
    if (start_addr > 0 && start_addr < 10000) {
        unsigned char v[MODBUS_READ_BLOCK_SIZE] = {0};
        while (retry_cnt > 0) {
            retry_cnt --;
            if (device->point_delay_ms > 0) {
                usleep(device->point_delay_ms * 1000);
            }
            modbus_set_slave(c->ctx_modbus, devid);
            led_on(ctx_idx);
            int read_len = modbus_read_bits(c->ctx_modbus, start_addr - 1, len, v);
            device->packet_cnt_total ++;
            led_off(ctx_idx);
            if (read_len == len) {
                list_node_t *reg_node = &start_element->node;
                while (read_len > 0) {
                    if (reg_node == NULL) {
                        printf("Error: read node not enough !\n");
                        break;
                    }
                    element_t *element = list_entry_safe(reg_node, element_t);
                    if (element->addr - start_addr == len - read_len) {
                        element->val = v[len - read_len];
                        element->err_cnt = 0;
                        reg_node = reg_node->next;
                    }
                    read_len --;
                }
                break;
            } else {
                led_blink(MAX_MODBUSRTU_NUM + ctx_idx);
                device->packet_cnt_err ++;
                read_len = len;
                list_node_t *reg_node = &start_element->node;
                while (read_len > 0) {
                    if (reg_node == NULL) {
                        printf("Error: read node not enough !\n");
                        break;
                    }
                    element_t *element = list_entry_safe(reg_node, element_t);
                    if (element->addr - start_addr == len - read_len) {
                        element->err_cnt ++;
                        reg_node = reg_node->next;
                    }
                    read_len --;
                }
                usleep(200000);
            }
        }
    } else if (start_addr > 10000 && start_addr < 20000) {
        unsigned char v[MODBUS_READ_BLOCK_SIZE] = {0};
        while (retry_cnt > 0) {
            retry_cnt --;
            if (device->point_delay_ms > 0) {
                usleep(device->point_delay_ms * 1000);
            }
            modbus_set_slave(c->ctx_modbus, devid);
            led_on(ctx_idx);
            int read_len = modbus_read_input_bits(c->ctx_modbus, start_addr - 10001, len, v);
            device->packet_cnt_total ++;
            led_off(ctx_idx);
            if (read_len == len) {
                list_node_t *reg_node = &start_element->node;
                while (read_len > 0) {
                    if (reg_node == NULL) {
                        printf("Error: read node not enough !\n");
                        break;
                    }
                    element_t *element = list_entry_safe(reg_node, element_t);
                    if (element->addr - start_addr == len - read_len) {
                        element->val = v[len - read_len];
                        element->err_cnt = 0;
                        reg_node = reg_node->next;
                    }
                    read_len --;
                }
                break;
            } else {
                led_blink(MAX_MODBUSRTU_NUM + ctx_idx);
                device->packet_cnt_err ++;
                read_len = len;
                list_node_t *reg_node = &start_element->node;
                while (read_len > 0) {
                    if (reg_node == NULL) {
                        printf("Error: read node not enough !\n");
                        break;
                    }
                    element_t *element = list_entry_safe(reg_node, element_t);
                    if (element->addr - start_addr == len - read_len) {
                        element->err_cnt ++;
                        reg_node = reg_node->next;
                    }
                    read_len --;
                }
                usleep(200000);
            }
        }
    } else if (start_addr > 30000 && start_addr < 40000) {
        unsigned short v[MODBUS_READ_BLOCK_SIZE] = {0};
        while (retry_cnt > 0) {
            retry_cnt --;
            if (device->point_delay_ms > 0) {
                usleep(device->point_delay_ms * 1000);
            }
            modbus_set_slave(c->ctx_modbus, devid);
            led_on(ctx_idx);
            int read_len = modbus_read_input_registers(c->ctx_modbus, start_addr - 30001, len, v);
            device->packet_cnt_total ++;
            led_off(ctx_idx);
            if (read_len == len) {
                list_node_t *reg_node = &start_element->node;
                while (read_len > 0) {
                    if (reg_node == NULL) {
                        printf("Error: read node not enough !\n");
                        break;
                    }
                    element_t *element = list_entry_safe(reg_node, element_t);
                    if (element->addr - start_addr == len - read_len) {
                        element->val = v[len - read_len];
                        element->err_cnt = 0;
                        reg_node = reg_node->next;
                    }
                    read_len --;
                }
                break;
            } else {
                led_blink(MAX_MODBUSRTU_NUM + ctx_idx);
                device->packet_cnt_err ++;
                read_len = len;
                list_node_t *reg_node = &start_element->node;
                while (read_len > 0) {
                    if (reg_node == NULL) {
                        printf("Error: read node not enough !\n");
                        break;
                    }
                    element_t *element = list_entry_safe(reg_node, element_t);
                    if (element->addr - start_addr == len - read_len) {
                        element->err_cnt ++;
                        reg_node = reg_node->next;
                    }
                    read_len --;
                }
                usleep(200000);
            }
        }
    } else if (start_addr > 40000 && start_addr < 50000) {
        unsigned short v[MODBUS_READ_BLOCK_SIZE] = {0};
        while (retry_cnt > 0) {
            retry_cnt --;
            if (device->point_delay_ms > 0) {
                usleep(device->point_delay_ms * 1000);
            }
            modbus_set_slave(c->ctx_modbus, devid);
            led_on(ctx_idx);
            int read_len = modbus_read_registers(c->ctx_modbus, start_addr - 40001, len, v);
            device->packet_cnt_total ++;
            led_off(ctx_idx);
            if (read_len == len) {
                list_node_t *reg_node = &start_element->node;
                while (read_len > 0) {
                    if (reg_node == NULL) {
                        printf("Error: read node not enough !\n");
                        break;
                    }
                    element_t *element = list_entry_safe(reg_node, element_t);
                    if (element->addr - start_addr == len - read_len) {
                        element->val = v[len - read_len];
                        element->err_cnt = 0;
                        reg_node = reg_node->next;
                    }
                    read_len --;
                }
                break;
            } else {
                led_blink(MAX_MODBUSRTU_NUM + ctx_idx);
                device->packet_cnt_err ++;
                read_len = len;
                list_node_t *reg_node = &start_element->node;
                while (read_len > 0) {
                    if (reg_node == NULL) {
                        printf("Error: read node not enough !\n");
                        break;
                    }
                    element_t *element = list_entry_safe(reg_node, element_t);
                    if (element->addr - start_addr == len - read_len) {
                        element->err_cnt ++;
                        reg_node = reg_node->next;
                    }
                    read_len --;
                }
                usleep(200000);
            }
        }
    }
}

static inline void exec_single_element_write(context_t *c, device_t *device, element_t *element)
{
    int ctx_idx = c - &context_table[0];
    unsigned char devid = device->addr;
    unsigned short address = element->addr;
    if (element->is_dirty == 1) {
        if (element->err_cnt == 0) {
            if (address > 0 && address < 10000) {
                if (device->point_delay_ms > 0) {
                    usleep(device->point_delay_ms * 1000);
                }
                modbus_set_slave(c->ctx_modbus, devid);
                led_on(ctx_idx);
                modbus_write_bit(c->ctx_modbus, address - 1, element->val_w);
                led_off(ctx_idx);
            } else if (address > 40000 && address < 50000) {
                if (device->point_delay_ms > 0) {
                    usleep(device->point_delay_ms * 1000);
                }
                modbus_set_slave(c->ctx_modbus, devid);
                led_on(ctx_idx);
                modbus_write_register(c->ctx_modbus, address - 40001, element->val_w);
                led_off(ctx_idx);
            }
        } else {
            element->is_force_update = 1;
        }
        device->packet_cnt_total ++;
        element->is_dirty = 0;
        // clean write req list.
        LOCK_WRITE_QUEUE;
        list_node_t *req_node = c->write_queue.first;
        while (req_node != NULL) {
            req_t *_req = list_entry_safe(req_node, req_t);
            if (_req->dev == device && _req->reg == element) {
                list_delete(&c->write_queue, req_node);
                free(_req);
#ifdef DEBUG_LIST
                mem_count--;
#endif
                break;
            }
            req_node = req_node->next;
        }
        UNLOCK_WRITE_QUEUE;
    }
}

static inline void exec_single_element_read(context_t *c, device_t *device, element_t *element)
{
    int ctx_idx = c - &context_table[0];
    unsigned char devid = device->addr;
    unsigned short address = element->addr;
    if (address > 0 && address < 10000) {
        unsigned char v;
        if (device->point_delay_ms > 0) {
            usleep(device->point_delay_ms * 1000);
        }
        modbus_set_slave(c->ctx_modbus, devid);
        led_on(ctx_idx);
        if (modbus_read_bits(c->ctx_modbus, address - 1, 1, &(v)) == 1) {
            led_off(ctx_idx);
            element->val = v;
            element->err_cnt = 0;
        } else {
            led_off(ctx_idx);
            led_blink(MAX_MODBUSRTU_NUM + ctx_idx);
            if (element->err_cnt < MAX_ERROR_COUNT) element->err_cnt ++;
        }
    } else if (address > 10000 && address < 20000) {
        unsigned char v;
        if (device->point_delay_ms > 0) {
            usleep(device->point_delay_ms * 1000);
        }
        modbus_set_slave(c->ctx_modbus, devid);
        led_on(ctx_idx);
        if (modbus_read_input_bits(c->ctx_modbus, address - 10001, 1, &(v)) == 1) {
            led_off(ctx_idx);
            element->val = v;
            element->err_cnt = 0;
        } else {
            led_off(ctx_idx);
            led_blink(MAX_MODBUSRTU_NUM + ctx_idx);
            if (element->err_cnt < MAX_ERROR_COUNT) element->err_cnt ++;
        }
    } else if (address > 30000 && address < 40000) {
        unsigned short v;
        if (device->point_delay_ms > 0) {
            usleep(device->point_delay_ms * 1000);
        }
        modbus_set_slave(c->ctx_modbus, devid);
        led_on(ctx_idx);
        if (modbus_read_input_registers(c->ctx_modbus, address - 30001, 1, &(v)) == 1) {
            led_off(ctx_idx);
            element->val = v;
            element->err_cnt = 0;
        } else {
            led_off(ctx_idx);
            led_blink(MAX_MODBUSRTU_NUM + ctx_idx);
            if (element->err_cnt < MAX_ERROR_COUNT) element->err_cnt ++;
        }
    } else if (address > 40000 && address < 50000) {
        unsigned short v;
        if (device->point_delay_ms > 0) {
            usleep(device->point_delay_ms * 1000);
        }
        modbus_set_slave(c->ctx_modbus, devid);
        led_on(ctx_idx);
        if (modbus_read_registers(c->ctx_modbus, address - 40001, 1, &(v)) == 1) {
            led_off(ctx_idx);
            element->val = v;
            element->err_cnt = 0;
        } else {
            led_off(ctx_idx);
            led_blink(MAX_MODBUSRTU_NUM + ctx_idx);
            if (element->err_cnt < MAX_ERROR_COUNT) element->err_cnt ++;
        }
    }
    device->packet_cnt_total ++;
}

static inline void exec_one_list_read(context_t *c, device_t *device, list_head_t *list)
{
    list_node_t *curr = list->first;
    list_node_t *start = curr;
    int len = 0;
    while (c->ctx_thread_running != 0 && curr != NULL) {
        element_t *curr_element = list_entry_safe(curr, element_t);
        if (curr->next != NULL) {
            element_t *next_element = list_entry_safe(curr->next, element_t);
            int want_merge_len = next_element->addr - curr_element->addr;
            if (MAX_SREAD_ERROR_COUNT > 0) {
                if (curr_element->err_cnt > MAX_ERROR_COUNT - MAX_SREAD_ERROR_COUNT) {
                    want_merge_len = MODBUS_READ_SKIP_SIZE;
                }
            }
            if (want_merge_len < MODBUS_READ_SKIP_SIZE && len + want_merge_len < MODBUS_READ_BLOCK_SIZE) {
                len += want_merge_len;
            } else {
                len ++;
                element_t *start_element = list_entry_safe(start, element_t);
                if (len > 1) {
                    //printf("Mutli Read %d[%d]\n", start_element->addr, len);
                    if (start_element->err_cnt == 0) {
                        exec_mutli_element_write(c, device, start_element, len);
                    }
                    exec_mutli_element_read(c, device, start_element, len);
                } else {
                    //printf("Single Read %d\n", start_element->addr);
                    if (start_element->err_cnt == 0) {
                        exec_single_element_write(c, device, start_element);
                    }
                    exec_single_element_read(c, device, start_element);
                }
                start = curr->next;
                len = 0;
            }
        } else {
            len ++;
            element_t *start_element = list_entry_safe(start, element_t);
            if (len > 1) {
                //printf("Mutli read %d[%d]\n", start_element->addr, len);
                if (start_element->err_cnt == 0) {
                    exec_mutli_element_write(c, device, start_element, len);
                }
                exec_mutli_element_read(c, device, start_element, len);
            } else {
                //printf("Single read %d\n", start_element->addr);
                if (start_element->err_cnt == 0) {
                    exec_single_element_write(c, device, start_element);
                }
                exec_single_element_read(c, device, start_element);
            }
        }
        curr = curr->next;
    }
}

static inline int is_device_offline(device_t *device)
{
    list_node_t *reg_node = device->DO.first;
    while (reg_node != NULL) {
        element_t *element = list_entry_safe(reg_node, element_t);
        if (element->err_cnt != MAX_ERROR_COUNT) return 0;
        reg_node = reg_node->next;
    }
    reg_node = device->DI.first;
    while (reg_node != NULL) {
        element_t *element = list_entry_safe(reg_node, element_t);
        if (element->err_cnt != MAX_ERROR_COUNT) return 0;
        reg_node = reg_node->next;
    }
    reg_node = device->INPUT.first;
    while (reg_node != NULL) {
        element_t *element = list_entry_safe(reg_node, element_t);
        if (element->err_cnt != MAX_ERROR_COUNT) return 0;
        reg_node = reg_node->next;
    }
    reg_node = device->HOLD.first;
    while (reg_node != NULL) {
        element_t *element = list_entry_safe(reg_node, element_t);
        if (element->err_cnt != MAX_ERROR_COUNT) return 0;
        reg_node = reg_node->next;
    }
    return 1;
}

static inline void exec_one_device_read(context_t *c, device_t *device)
{
    if (is_device_offline(device) == 1) {
        // 检测掉线设备。
        element_t *element = NULL;
        if (device->DO.first != NULL) {
            element = list_entry_safe(device->DO.first, element_t);
        } else if (device->DI.first != NULL) {
            element = list_entry_safe(device->DI.first, element_t);
        } else if (device->INPUT.first != NULL) {
            element = list_entry_safe(device->INPUT.first, element_t);
        } else if (device->HOLD.first != NULL) {
            element = list_entry_safe(device->HOLD.first, element_t);
        }
        if (element != NULL) {
            exec_single_element_read(c, device, element);
            if (is_device_offline(device) == 1) {
                device->next_update = device->next_update - (unsigned long long)device->refresh_ms + c->retry_delay_ms;
            } else {
                // 新上线设备。
                list_node_t *reg_node = device->DO.first;
                while (reg_node != NULL) {
                    element = list_entry_safe(reg_node, element_t);
                    reg_node = reg_node->next;
                }
                reg_node = device->DI.first;
                while (reg_node != NULL) {
                    element = list_entry_safe(reg_node, element_t);
                    reg_node = reg_node->next;
                }
                reg_node = device->INPUT.first;
                while (reg_node != NULL) {
                    element = list_entry_safe(reg_node, element_t);
                    reg_node = reg_node->next;
                }
                reg_node = device->HOLD.first;
                while (reg_node != NULL) {
                    element = list_entry_safe(reg_node, element_t);
                    reg_node = reg_node->next;
                }
                // 读取全部寄存器。
                if (c->ctx_thread_running != 0)
                    exec_one_list_read(c, device, &device->DO);
                if (c->ctx_thread_running != 0)
                    exec_one_list_read(c, device, &device->DI);
                if (c->ctx_thread_running != 0)
                    exec_one_list_read(c, device, &device->INPUT);
                if (c->ctx_thread_running != 0)
                    exec_one_list_read(c, device, &device->HOLD);
                // 更新写入数据。
                reg_node = device->DO.first;
                while (reg_node != NULL) {
                    element = list_entry_safe(reg_node, element_t);
                    if (element->err_cnt == 0 && element->is_force_update == 1) {
                        int ctx_idx = c - &context_table[0];
                        unsigned char devid = device->addr;
                        unsigned short address = element->addr;
                        if (device->point_delay_ms > 0) {
                            usleep(device->point_delay_ms * 1000);
                        }
                        modbus_set_slave(c->ctx_modbus, devid);
                        led_on(ctx_idx);
                        modbus_write_bit(c->ctx_modbus, address - 1, element->val_w);
                        led_off(ctx_idx);
                        element->is_dirty = 0;
                        element->is_force_update = 0;
                    }
                    reg_node = reg_node->next;
                }
                reg_node = device->HOLD.first;
                while (reg_node != NULL) {
                    element = list_entry_safe(reg_node, element_t);
                    if (element->err_cnt == 0 && element->is_force_update == 1) {
                        int ctx_idx = c - &context_table[0];
                        unsigned char devid = device->addr;
                        unsigned short address = element->addr;
                        if (device->point_delay_ms > 0) {
                            usleep(device->point_delay_ms * 1000);
                        }
                        modbus_set_slave(c->ctx_modbus, devid);
                        led_on(ctx_idx);
                        modbus_write_register(c->ctx_modbus, address - 40001, element->val_w);
                        led_off(ctx_idx);
                        element->is_dirty = 0;
                        element->is_force_update = 0;
                    }
                    reg_node = reg_node->next;
                }
                // 更新计数。
                device->next_update = get_tick_ms() + (unsigned long long)device->refresh_ms;
            }
        }
        device->spent_time_ms = 0;
        return;
    }

    unsigned long long delta = get_tick_ms();
    if (c->ctx_thread_running != 0)
        exec_one_list_read(c, device, &device->DO);
    if (c->ctx_thread_running != 0)
        exec_one_list_read(c, device, &device->DI);
    if (c->ctx_thread_running != 0)
        exec_one_list_read(c, device, &device->INPUT);
    if (c->ctx_thread_running != 0)
        exec_one_list_read(c, device, &device->HOLD);

    if (is_device_offline(device) == 1) {
        device->next_update = device->next_update - (unsigned long long)device->refresh_ms + c->retry_delay_ms;
        device->spent_time_ms = 0;
    } else {
        // 统计设备消费时间。
        delta = get_tick_ms() - delta;
        device->spent_time_ms = (unsigned int)delta;
    }
}

static void* thread_modbus_rtu_update(void *arg)
{
    context_t *c = (context_t *)arg;

    // wait for added event.
    while (c->ctx_thread_running != 0 && c->ctx_added == 0)
    {
        usleep(10*1000);
    }

#ifdef DEBUG_LIST
    {
        list_node_t *dev_node = c->devices.first;
        while (dev_node != NULL) {
            device_t *device = list_entry_safe(dev_node, device_t);
            printf("Device [%d]\n", device->addr);
            list_node_t *reg_node = device->DO.first;
            while (reg_node != NULL) {
                element_t *element = list_entry_safe(reg_node, element_t);
                printf("  %d\n", element->addr);
                reg_node = reg_node->next;
            }
            reg_node = device->DI.first;
            while (reg_node != NULL) {
                element_t *element = list_entry_safe(reg_node, element_t);
                printf("  %d\n", element->addr);
                reg_node = reg_node->next;
            }
            reg_node = device->INPUT.first;
            while (reg_node != NULL) {
                element_t *element = list_entry_safe(reg_node, element_t);
                printf("  %d\n", element->addr);
                reg_node = reg_node->next;
            }
            reg_node = device->HOLD.first;
            while (reg_node != NULL) {
                element_t *element = list_entry_safe(reg_node, element_t);
                printf("  %d\n", element->addr);
                reg_node = reg_node->next;
            }
            dev_node = dev_node->next;
        }
    }
#endif

    while (c->ctx_thread_running != 0)
    {
        device_t* next_read_device = get_next_read_device(c);
        if (next_read_device != NULL) {
            exec_one_device_read(c, next_read_device);
        } else if (c->write_queue.len > 0) {
            exec_one_reg_write_from_list(c);
        } else {
            usleep(10*1000);
        }
    }

    // free device regs
    {
        list_node_t *dev_node = list_get(&c->devices);
        while (dev_node) {
            device_t *device = list_entry_safe(dev_node, device_t);
            // DO
            list_node_t *reg_node = list_get(&device->DO);
            while (reg_node) {
                element_t *element = list_entry_safe(reg_node, element_t);
                free(element);
#ifdef DEBUG_LIST
                mem_count--;
#endif
                reg_node = list_get(&device->DO);
            }
            // DI
            reg_node = list_get(&device->DI);
            while (reg_node) {
                element_t *element = list_entry_safe(reg_node, element_t);
                free(element);
#ifdef DEBUG_LIST
                mem_count--;
#endif
                reg_node = list_get(&device->DI);
            }
            // INPUT
            reg_node = list_get(&device->INPUT);
            while (reg_node) {
                element_t *element = list_entry_safe(reg_node, element_t);
                free(element);
#ifdef DEBUG_LIST
                mem_count--;
#endif
                reg_node = list_get(&device->INPUT);
            }
            // HOLD
            reg_node = list_get(&device->HOLD);
            while (reg_node) {
                element_t *element = list_entry_safe(reg_node, element_t);
                free(element);
#ifdef DEBUG_LIST
                mem_count--;
#endif
                reg_node = list_get(&device->HOLD);
            }
            // final free the device.
            free(device);
#ifdef DEBUG_LIST
            mem_count--;
#endif
            dev_node = list_get(&c->devices);
        }
        memset(&c->devices, 0, sizeof(list_head_t));
    }

    if (c->write_queue.len) {
        list_node_t *node = list_get(&c->write_queue);
        while (node) {
            req_t *req = list_entry_safe(node, req_t);
            free(req);
#ifdef DEBUG_LIST
            mem_count--;
#endif
            node = list_get(&c->write_queue);
        }
        memset(&c->write_queue, 0, sizeof(list_head_t));
    }
#ifdef DEBUG_LIST
    printf("MEM COUNT %d\n", mem_count);
#endif

    c->ctx_added = 0;

    modbus_close(c->ctx_modbus);
    modbus_free(c->ctx_modbus);
    c->ctx_modbus = NULL;

    pthread_exit(NULL);

    return (void*)NULL;
}

int rtu_read(int ctx_idx, int device_addr, int addr, float *buf, int len)
{
    // read device elapsed time.
    if (addr == 0) {
        context_t *c = &context_table[ctx_idx];
        list_node_t *dev_node = c->devices.first;
        while (dev_node != NULL) {
            device_t *device = list_entry_safe(dev_node, device_t);
            if (device->addr == device_addr) {
                if (is_device_offline(device) == 1) {
                    return 0;
                }
                if (device->spent_time_ms > 65535) {
                    buf[0] = 65535;
                } else {
                    buf[0] = (float)device->spent_time_ms;
                }
                if (device->packet_cnt_total == 0xFFFFFFFF) {
                    device->packet_cnt_total = 0;
                    device->packet_cnt_err = 0;
                }
                buf[1] = (float)device->packet_cnt_total;
                buf[2] = (float)device->packet_cnt_err;
                return 3;
            }
            dev_node = dev_node->next;
        }
        return 0;
    }

    element_t *element = (element_t *)device_addr;
    if (element != NULL && element->addr == addr) {
        if (element->err_cnt != MAX_ERROR_COUNT) {
            if (len == 1) {
                // int
                short val = (short)element->val;
                buf[0] = (float)val;
                return 1;
            }
            if (len == 2) {
                // word
                buf[0] = (float)element->val;
                return 1;
            }
            if (len == 3) {
                // dint
                element_t *next_element = list_entry_safe(element->node.next, element_t);
                if (next_element != NULL && next_element->addr == addr + 1) {
                    int data = 0;
                    if (element->endian != 0) {
                        unsigned int _data = (element->val << 16) | next_element->val;
                        data = (int)_data;
                    } else {
                        unsigned int _data = (next_element->val << 16) | element->val;
                        data = (int)_data;
                    }
                    buf[0] = (float)data;
                    return 2;
                }
            }
            if (len == 4) {
                // dword
                element_t *next_element = list_entry_safe(element->node.next, element_t);
                if (next_element != NULL && next_element->addr == addr + 1) {
                    unsigned int data = 0;
                    if (element->endian != 0) {
                        data = (element->val << 16) | next_element->val;
                    } else {
                        data = (next_element->val << 16) | element->val;
                    }
                    buf[0] = (float)data;
                    return 2;
                }
            }
            if (len == 5) {
                // real
                element_t *next_element = list_entry_safe(element->node.next, element_t);
                if (next_element != NULL && next_element->addr == addr + 1) {
                    unsigned int data = 0;
                    if (element->endian != 0) {
                        data = (element->val << 16) | next_element->val;
                    } else {
                        data = (next_element->val << 16) | element->val;
                    }
                    float *p = (float *)((char *)&data);
                    buf[0] = *p;
                    return 2;
                }
            }
        }
    }

 // printf("Err while read: %d %d\n", addr, len);
    return -1;
}

static inline void write_queue_put(int ctx_idx, element_t *element)
{
    context_t *c = &context_table[ctx_idx];

    // find the device.
    list_node_t *dev_node = c->devices.first;
    device_t *device = NULL;
    while (dev_node != NULL && device == NULL) {
        device_t *curr_device = list_entry_safe(dev_node, device_t);
        list_head_t *reg_head = NULL;
        if (element->addr > 0 && element->addr < 10000) {
            reg_head = &curr_device->DO;
        } else if (element->addr > 10000 && element->addr < 20000) {
            reg_head = NULL;//&curr_device->DI;
        } else if (element->addr > 30000 && element->addr < 40000) {
            reg_head = NULL;//&curr_device->INPUT;
        } else if (element->addr > 40000 && element->addr < 50000) {
            reg_head = &curr_device->HOLD;
        }
        if (reg_head != NULL) {
            list_node_t *reg_node = reg_head->first;
            while (reg_node != NULL) {
                element_t *curr_element = list_entry_safe(reg_node, element_t);
                if (curr_element == element) {
                    device = curr_device;
                    break;
                }
                reg_node = reg_node->next;
            }
        }
        dev_node = dev_node->next;
    }

    if (device == NULL) {
        return;
    }

    LOCK_WRITE_QUEUE;
    list_node_t *req_node = c->write_queue.first;
    int is_exist = 0;
    while (req_node != NULL) {
        req_t *_req = list_entry_safe(req_node, req_t);
        if (_req->dev == device && _req->reg == element) {
            is_exist = 1;
            break;
        }
        req_node = req_node->next;
    }
    if (is_exist == 0) {
        req_t *req = malloc(sizeof(req_t));
        if (req != NULL) {
            memset(req, 0, sizeof(req_t));
            req->dev = device;
            req->reg = element;
            list_put(&c->write_queue, &req->node);
#ifdef DEBUG_LIST
            mem_count++;
#endif
        }
    }
    UNLOCK_WRITE_QUEUE;
}

int rtu_write(int ctx_idx, int device_addr, int addr, float *buf, int len)
{
    int result = -1;

    if ((addr > 10000 && addr < 20000) || (addr > 30000 && addr < 40000)) {
        return result;
    }

    element_t *element = (element_t *)device_addr;
    if (element != NULL && element->addr == addr) {
        if (addr < 20000) {
            if (buf[0] == 0) {
                element->val_w = 0;
            } else {
                element->val_w = 1;
            }
            if (element->err_cnt != MAX_ERROR_COUNT) {
                element->is_dirty = 1;
                write_queue_put(ctx_idx, element);
                result = 1;
            } else {
                element->is_force_update = 1;
            }
        } else {
            if (len == 1) {
                // int
                short data = (short)buf[0];
                element->val_w = (unsigned short)data;
                if (element->err_cnt != MAX_ERROR_COUNT) {
                    element->is_dirty = 1;
                    write_queue_put(ctx_idx, element);
                    result = 1;
                } else {
                    element->is_force_update = 1;
                }
            }
            if (len == 2) {
                // word
                element->val_w = (unsigned short)buf[0];
                if (element->err_cnt != MAX_ERROR_COUNT) {
                    element->is_dirty = 1;
                    write_queue_put(ctx_idx, element);
                    result = 1;
                } else {
                    element->is_force_update = 1;
                }
            }
            if (len == 3) {
                // dint
                element_t *next_element = list_entry_safe(element->node.next, element_t);
                if (next_element != NULL && next_element->addr == addr + 1) {
                    int data = (int)buf[0];
                    if (element->endian != 0) {
                        unsigned int _data = (unsigned int)data;
                        element->val_w = (_data >> 16) & 0xFFFF;
                        next_element->val_w = (_data & 0xFFFF);
                    } else {
                        unsigned int _data = (unsigned int)data;
                        next_element->val_w = (_data >> 16) & 0xFFFF;
                        element->val_w = (_data & 0xFFFF);
                    }
                    if (element->err_cnt != MAX_ERROR_COUNT) {
                        element->is_dirty = 1;
                        next_element->is_dirty = 1;
                        write_queue_put(ctx_idx, element);
                        write_queue_put(ctx_idx, next_element);
                        result = 2;
                    } else {
                        element->is_force_update = 1;
                        next_element->is_force_update = 1;
                    }
                }
            }
            if (len == 4) {
                // dword
                element_t *next_element = list_entry_safe(element->node.next, element_t);
                if (next_element != NULL && next_element->addr == addr + 1) {
                    unsigned int data = (unsigned int)buf[0];
                    if (element->endian != 0) {
                        element->val_w = (data >> 16) & 0xFFFF;
                        next_element->val_w = (data & 0xFFFF);
                    } else {
                        next_element->val_w = (data >> 16) & 0xFFFF;
                        element->val_w = (data & 0xFFFF);
                    }
                    if (element->err_cnt != MAX_ERROR_COUNT) {
                        element->is_dirty = 1;
                        next_element->is_dirty = 1;
                        write_queue_put(ctx_idx, element);
                        write_queue_put(ctx_idx, next_element);
                        result = 2;
                    } else {
                        element->is_force_update = 1;
                        next_element->is_force_update = 1;
                    }
                }
            }
            if (len == 5) {
                // real
                element_t *next_element = list_entry_safe(element->node.next, element_t);
                if (next_element != NULL && next_element->addr == addr + 1) {
                    unsigned int data = *((unsigned int *)buf);
                    if (element->endian != 0) {
                        element->val_w = (data >> 16) & 0xFFFF;
                        next_element->val_w = (data & 0xFFFF);
                    } else {
                        next_element->val_w = (data >> 16) & 0xFFFF;
                        element->val_w = (data & 0xFFFF);
                    }
                    if (element->err_cnt != MAX_ERROR_COUNT) {
                        element->is_dirty = 1;
                        next_element->is_dirty = 1;
                        write_queue_put(ctx_idx, element);
                        write_queue_put(ctx_idx, next_element);
                        result = 2;
                    } else {
                        element->is_force_update = 1;
                        next_element->is_force_update = 1;
                    }
                }
            }
        }
    }

    return result;
}

static inline void list_ordered_put(list_head_t *head, element_t *element)
{
    if (head->len == 0) {
        list_put(head, &element->node);
    } else {
        element_t *first_elem = list_entry_safe(head->first, element_t);
        if (first_elem->addr > element->addr) {
            list_put_begin(head, &element->node);
        } else {
            list_node_t *prev = NULL;
            list_node_t *reg_node = head->first;
            while (reg_node != NULL) {
                if (reg_node->next == NULL) {
                    prev = reg_node;
                    break;
                }
                element_t *next_elem = list_entry_safe(reg_node->next, element_t);
                if (next_elem->addr > element->addr) {
                    prev = reg_node;
                    break;
                }
                reg_node = reg_node->next;
            }
            if (prev == head->first && head->len == 1) {
                list_put(head, &element->node);
            } else {
                head->len++;
                element->node.next = prev->next;
                prev->next = &element->node;
                if (prev == head->last) {
                    head->last = &element->node;
                }
            }
        }
    }

#ifdef LIST_CHECK
    list_check(head);
#endif
}

int rtu_add(int ctx_idx, int device_addr, int addr, int len, int refreshms, int reg_delay)
{
    context_t *c = &context_table[ctx_idx];
    int endian = (device_addr >> 8) & 0xFF;
    device_addr = device_addr & 0xFF;

    if (device_addr == 0 && addr == 0 && len == 0 && refreshms == 0 && reg_delay == 0) {
        // add END.
        c->ctx_added = 1;
        return 0;
    } else if (addr == 0 && len == 0) {
        // add device node.
        // find the device by device_addr
        device_t *device = NULL;
        list_node_t *dev_node = c->devices.first;
        while (dev_node != NULL) {
            device_t *_device = list_entry_safe(dev_node, device_t);
            if (_device->addr == device_addr) {
                device = _device;
                break;
            }
            dev_node = dev_node->next;
        }
        if (device != NULL) {
            return -1;
        }
        // if not found, create it.
        device = malloc(sizeof(device_t));
        if (device == NULL) {
            return -1;
        }
#ifdef DEBUG_LIST
        mem_count++;
#endif
        memset(device, 0, sizeof(device_t));
        device->addr = device_addr;
        device->refresh_ms = refreshms;
        device->point_delay_ms = reg_delay;
        list_put(&c->devices, &device->node);
        return 0;
    }

    // add register node
    // find the device by device_addr
    device_t *device = NULL;
    list_node_t *dev_node = c->devices.first;
    while (dev_node != NULL) {
        device_t *_device = list_entry_safe(dev_node, device_t);
        if (_device->addr == device_addr) {
            device = _device;
            break;
        }
        dev_node = dev_node->next;
    }

    if (device == NULL) {
        return 0;
    }

    int register_node_addr = 0;
    int is_new_node = 1;
    // search the list to check is the node is exist.
    list_head_t *reg_head = NULL;
    if (addr > 0 && addr < 10000) {
        reg_head = &device->DO;
    } else if (addr > 10000 && addr < 20000) {
        reg_head = &device->DI;
    } else if (addr > 30000 && addr < 40000) {
        reg_head = &device->INPUT;
    } else if (addr > 40000 && addr < 50000) {
        reg_head = &device->HOLD;
    }
    if (reg_head != NULL) {
        list_node_t *reg_node = reg_head->first;
        while (reg_node != NULL) {
            element_t *element = list_entry_safe(reg_node, element_t);
            if (element->addr == addr) {
                register_node_addr = (int)element;
                is_new_node = 0;
                break;
            }
            reg_node = reg_node->next;
        }
    }
    if (is_new_node == 1 && reg_head != NULL) {
        element_t *element = malloc(sizeof(element_t));
        if (element == NULL) {
            return 0;
        }
#ifdef DEBUG_LIST
        mem_count++;
#endif
        memset(element, 0, sizeof(element_t));
        element->addr = addr;
        element->endian = (endian == 0 ? 0 : 1);
        list_ordered_put(reg_head, element);
        register_node_addr = (int)element;
    }

    return register_node_addr;
}

int rtu_open(int ctx_idx, int band, int parity, int data_bit, int stop_bit, int retry_delay, int rto)
{
    char uart_name[32] = { 0 };
    char parity_name[] = {'N', 'E', 'O'};

    if (acquire_uart(ctx_idx, uart_name) >= 0) {
        context_t *c = &context_table[ctx_idx];
        if (c->ctx_modbus != NULL || c->ctx_thread_running != 0){
            release_uart(ctx_idx);
            return -1;
        }

        modbus_t* ctx = modbus_new_rtu(uart_name, band, parity_name[parity], data_bit, stop_bit);
        if (ctx == NULL) {
            printf("new rtu err !!! \n");
            release_uart(ctx_idx);
            return -1;
        }

        c->retry_delay_ms = retry_delay * 1000;
        modbus_set_response_timeout(ctx, 0, rto * 1000);
        int bto = 4000;
        if (band <= 1200) { bto = 38000; }
        else if (band <= 2400) { bto = 20000; }
        else if (band <= 4800) { bto = 10000; }
        else if (band <= 9600) { bto = 6000; }
        modbus_set_byte_timeout(ctx, 0, bto);
        modbus_set_byte_timeout(ctx, 0, 100000);
        modbus_set_debug(ctx, FALSE);
        modbus_set_error_recovery(ctx, MODBUS_ERROR_RECOVERY_LINK | MODBUS_ERROR_RECOVERY_PROTOCOL);
        //modbus_rtu_set_rts(ctx, MODBUS_RTU_RTS_NONE);
        //modbus_rtu_set_rts(ctx, MODBUS_RTU_RTS_UP);
        //modbus_rtu_set_rts(ctx, MODBUS_RTU_RTS_DOWN);
        //modbus_rtu_set_rts_delay(ctx, 10000);

        c->ctx_modbus = ctx;
        memset(&c->devices, 0, sizeof(list_head_t));
        memset(&c->write_queue, 0, sizeof(list_head_t));

        c->ctx_thread_running = 1;
        c->ctx_added = 0;
        pthread_create(&c->ctx_thread, NULL, thread_modbus_rtu_update, c);
        led_blink(ctx_idx);

        return ctx_idx;
    }

    return -1;
}

int rtu_close(int ctx_idx)
{
    context_t *c = &context_table[ctx_idx];
    if (c->ctx_modbus == NULL || c->ctx_thread_running == 0) {
        return -1;
    }

    c->ctx_thread_running = 0;
    pthread_join(c->ctx_thread, NULL);

    while (c->ctx_modbus != NULL) {
        usleep(10*1000);
    }

    release_uart(ctx_idx);

    return 0;
}
