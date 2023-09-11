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

#define MODBUS_MAX_CLIENT 6
#define MODBUS_TCP_TIMOUT 5000
#define MODBUS_READ_BLOCK_SIZE 120

//#define DEBUG_LIST
#ifdef DEBUG_LIST
static int mem_count;
#endif

typedef struct {
    list_node_t node;
    unsigned short addr;
    unsigned short val;
    unsigned char endian:1;
} element_t;

typedef struct {
    list_node_t node;
    unsigned char addr;

    list_head_t DO; // 00001, list of element_t.
    list_head_t DI; // 10001
    list_head_t INPUT; // 30001
    list_head_t HOLD; // 40001
} device_t;

typedef struct
{
    modbus_t *ctx_modbus;
    pthread_t ctx_thread;
    unsigned char ctx_thread_running;
    unsigned char ctx_added;

    pthread_t ctx_sub_thread[MODBUS_MAX_CLIENT];
    unsigned char ctx_sub_thread_running[MODBUS_MAX_CLIENT];
    unsigned long long ctx_sub_thread_last_update[MODBUS_MAX_CLIENT];

    int curr_fd;
    int curr_idx;

    list_head_t devices; // list of device_t
} context_t;
static context_t context_table[MAX_MODBUSTCPMASTER_NUM];

static inline element_t* get_element(context_t *c, unsigned char devid, unsigned short addr, int len)
{
    device_t *device = NULL;
    list_node_t *dev_node = c->devices.first;
    while (dev_node != NULL) {
        device_t *_device = list_entry_safe(dev_node, device_t);
        if (_device->addr == devid) {
            device = _device;
            break;
        }
        dev_node = dev_node->next;
    }

    if (device != NULL) {
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
            element_t *start_element = NULL;
            int element_len = 0;
            list_node_t *reg_node = reg_head->first;
            while (reg_node != NULL) {
                element_t *element = list_entry_safe(reg_node, element_t);
                if (element->addr == addr) {
                    start_element = element;
                    element_len = 1;
                } else if (element_len != 0) {
                    if (element->addr == addr + element_len) {
                        element_len ++;
                    } else {
                        break;
                    }
                }
                reg_node = reg_node->next;
            }
            if(element_len >= len) {
                return start_element;
            }
        }
    }

    return NULL;
}

static void* thread_modbus_tcp(void *arg)
{
    context_t *c = (context_t *)arg;
    int idx = c->curr_idx;
    int fd = c->curr_fd;
    unsigned char query[MODBUS_TCP_MAX_ADU_LENGTH];
    int header_length = modbus_get_header_length(c->ctx_modbus);
    c->ctx_sub_thread_last_update[idx] = get_tick_ms();

    if (fd > 0) {
        while (c->ctx_sub_thread_running[idx] != 0 && get_tick_ms() - c->ctx_sub_thread_last_update[idx] < MODBUS_TCP_TIMOUT) {
            int s_rc;
            struct timeval tv;
            fd_set rset;
            int rc = 0;
            do {
                tv.tv_sec = 1;
                tv.tv_usec = 0;
                FD_ZERO(&rset);
                FD_SET(fd, &rset);
                s_rc = select(fd+1, &rset, NULL, NULL, &tv);
                if (s_rc > 0) {
                    modbus_set_socket(c->ctx_modbus, fd);
                    rc = modbus_receive(c->ctx_modbus, query);
                }
            } while (rc == 0 && c->ctx_sub_thread_running[idx] != 0 && get_tick_ms() - c->ctx_sub_thread_last_update[idx] < MODBUS_TCP_TIMOUT);

            if (get_tick_ms() - c->ctx_sub_thread_last_update[idx] >= MODBUS_TCP_TIMOUT) {
                break;
            }

            if (rc < 0 || c->ctx_sub_thread_running[idx] == 0) {
                break;
            }

            c->ctx_sub_thread_last_update[idx] = get_tick_ms();

            // get a new query.
            unsigned char devid = query[6];
            unsigned char function = query[header_length];
            unsigned short addr = (query[header_length+1]<<8) + query[header_length+2];
            unsigned short nb = (query[header_length + 3] << 8) + query[header_length + 4];
            if (function == MODBUS_FC_WRITE_SINGLE_COIL || function == MODBUS_FC_WRITE_SINGLE_REGISTER) {
                nb = 1;
            }

            modbus_mapping_t modbus_mapping;
            memset(&modbus_mapping, 0, sizeof(modbus_mapping_t));

            unsigned short modbus_data[MODBUS_READ_BLOCK_SIZE];

            if (addr < 10000 && nb <= 10000 && (addr + nb) <= 10000 &&
                    nb <= MODBUS_READ_BLOCK_SIZE) {
                switch (function) {
                    case MODBUS_FC_READ_COILS:
                    case MODBUS_FC_WRITE_MULTIPLE_COILS:
                        {
                            element_t *element = get_element(c, devid, addr + 1, nb);
                            if (element != NULL) {
                                int i;
                                unsigned char *_modbus_data = (unsigned char*)modbus_data;
                                list_node_t *reg_node = &element->node;
                                for (i=0; i<nb; i++) {
                                    element_t *_element = list_entry_safe(reg_node, element_t);
                                    _modbus_data[i] = (unsigned char)_element->val;
                                    reg_node = reg_node->next;
                                }
                                modbus_mapping.nb_bits = nb;
                                modbus_mapping.start_bits = addr;
                                modbus_mapping.tab_bits = _modbus_data;
                            }
                            break;
                        }
                    case MODBUS_FC_WRITE_SINGLE_COIL:
                        {
                            element_t *element = get_element(c, devid, addr + 1, 1);
                            if (element != NULL) {
                                unsigned char *_modbus_data = (unsigned char*)modbus_data;
                                _modbus_data[0] = (unsigned char)element->val;
                                modbus_mapping.nb_bits = 1;
                                modbus_mapping.start_bits = addr;
                                modbus_mapping.tab_bits = _modbus_data;
                            }
                            break;
                        }
                    case MODBUS_FC_READ_DISCRETE_INPUTS:
                        {
                            element_t *element = get_element(c, devid, addr + 10001, nb);
                            if (element != NULL) {
                                int i;
                                unsigned char *_modbus_data = (unsigned char*)modbus_data;
                                list_node_t *reg_node = &element->node;
                                for (i=0; i<nb; i++) {
                                    element_t *_element = list_entry_safe(reg_node, element_t);
                                    _modbus_data[i] = (unsigned char)_element->val;
                                    reg_node = reg_node->next;
                                }
                                modbus_mapping.nb_input_bits = nb;
                                modbus_mapping.start_input_bits = addr;
                                modbus_mapping.tab_input_bits = _modbus_data;
                            }
                            break;
                        }
                    case MODBUS_FC_READ_INPUT_REGISTERS:
                        {
                            element_t *element = get_element(c, devid, addr + 30001, nb);
                            if (element != NULL) {
                                int i;
                                list_node_t *reg_node = &element->node;
                                for (i=0; i<nb; i++) {
                                    element_t *_element = list_entry_safe(reg_node, element_t);
                                    modbus_data[i] = _element->val;
                                    reg_node = reg_node->next;
                                }
                                modbus_mapping.nb_input_registers = nb;
                                modbus_mapping.start_input_registers = addr;
                                modbus_mapping.tab_input_registers = modbus_data;
                            }
                            break;
                        }
                    case MODBUS_FC_READ_HOLDING_REGISTERS:
                    case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
                        {
                            element_t *element = get_element(c, devid, addr + 40001, nb);
                            if (element != NULL) {
                                int i;
                                list_node_t *reg_node = &element->node;
                                for (i=0; i<nb; i++) {
                                    element_t *_element = list_entry_safe(reg_node, element_t);
                                    modbus_data[i] = _element->val;
                                    reg_node = reg_node->next;
                                }
                                modbus_mapping.nb_registers = nb;
                                modbus_mapping.start_registers = addr;
                                modbus_mapping.tab_registers = modbus_data;
                            }
                            break;
                        }
                    case MODBUS_FC_WRITE_SINGLE_REGISTER:
                        {
                            element_t *element = get_element(c, devid, addr + 40001, 1);
                            if (element != NULL) {
                                modbus_data[0] = element->val;
                                modbus_mapping.nb_registers = 1;
                                modbus_mapping.start_registers = addr;
                                modbus_mapping.tab_registers = modbus_data;
                            }
                            break;
                        }
                    default:
                        {
                            break;
                        }
                }
            }

            modbus_set_socket(c->ctx_modbus, fd);
            rc = modbus_reply(c->ctx_modbus, query, rc, &modbus_mapping);

            if (rc == -1) {
                break;
            }

            // write feadback.
            if (addr < 10000 && nb <= 10000 && (addr + nb) <= 10000 &&
                    nb <= MODBUS_READ_BLOCK_SIZE) {
                switch (function) {
                    case MODBUS_FC_WRITE_MULTIPLE_COILS:
                        {
                            element_t *element = get_element(c, devid, addr + 1, nb);
                            if (element != NULL) {
                                int i;
                                unsigned char *_modbus_data = (unsigned char*)modbus_data;
                                list_node_t *reg_node = &element->node;
                                for (i=0; i<nb; i++) {
                                    element_t *_element = list_entry_safe(reg_node, element_t);
                                    _element->val = (unsigned short)_modbus_data[i];
                                    reg_node = reg_node->next;
                                }
                            }
                            break;
                        }
                    case MODBUS_FC_WRITE_SINGLE_COIL:
                        {
                            element_t *element = get_element(c, devid, addr + 1, 1);
                            if (element != NULL) {
                                unsigned char *_modbus_data = (unsigned char*)modbus_data;
                                element->val = (unsigned short)_modbus_data[0];
                            }
                            break;
                        }
                    case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
                        {
                            element_t *element = get_element(c, devid, addr + 40001, nb);
                            if (element != NULL) {
                                int i;
                                list_node_t *reg_node = &element->node;
                                for (i=0; i<nb; i++) {
                                    element_t *_element = list_entry_safe(reg_node, element_t);
                                    _element->val = modbus_data[i];
                                    reg_node = reg_node->next;
                                }
                            }
                            break;
                        }
                    case MODBUS_FC_WRITE_SINGLE_REGISTER:
                        {
                            element_t *element = get_element(c, devid, addr + 40001, 1);
                            if (element != NULL) {
                                element->val = modbus_data[0];
                            }
                            break;
                        }
                    default:
                        {
                            break;
                        }
                }
            } // end write feadback.
        }
    }

    close(fd);
    c->ctx_sub_thread_running[idx] = 0;

    pthread_exit(NULL);

    return (void*)NULL;
}

static void* thread_modbus_tcp_master(void *arg)
{
    context_t *c = (context_t *)arg;
    int socket_fd = modbus_tcp_listen(c->ctx_modbus, 1);

    // wait for added event.
    while (c->ctx_thread_running != 0 && c->ctx_added == 0)
    {
        usleep(10*1000);
    }

    // main loop.
    while (c->ctx_thread_running != 0 && socket_fd != -1)
    {
        int s_rc;
        struct timeval tv;
        fd_set rset;
        do {
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            FD_ZERO(&rset);
            FD_SET(socket_fd, &rset);
            s_rc = select(socket_fd+1, &rset, NULL, NULL, &tv);
            if (s_rc > 0) {
                s_rc = modbus_tcp_accept(c->ctx_modbus, &socket_fd);
                if (s_rc == -1) {
                    continue;
                }
            }
        } while (s_rc <= 0 && c->ctx_thread_running != 0);

        if (s_rc > 0) {
            int i;
            int has_port = 0;
            for (i=0; i<MODBUS_MAX_CLIENT; i++) {
                if (c->ctx_sub_thread_running[i] == 0) {
                    c->ctx_sub_thread_running[i] = 1;
                    c->curr_idx = i;
                    c->curr_fd = s_rc;
                    pthread_create(&c->ctx_sub_thread[i], NULL, thread_modbus_tcp, c);
                    has_port = 1;
                    break;
                }
            }
            if (has_port == 0) {
                close(s_rc);
            }
        }

        int i;
        for (i=0; i<MODBUS_MAX_CLIENT; i++) {
            if (get_tick_ms() - c->ctx_sub_thread_last_update[i] >= MODBUS_TCP_TIMOUT && c->ctx_sub_thread_running[i] == 1) {
                c->ctx_sub_thread_running[i] = 0;
                pthread_join(c->ctx_sub_thread[i], NULL);
            }
        }
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
#ifdef DEBUG_LIST
    printf("MEM COUNT %d\n", mem_count);
#endif

    c->ctx_added = 0;

    {
        int i;
        for (i=0; i<MODBUS_MAX_CLIENT; i++) {
            if (c->ctx_sub_thread_running[i] == 1) {
                c->ctx_sub_thread_running[i] = 0;
                pthread_join(c->ctx_sub_thread[i], NULL);
            }
        }
    }

    if (socket_fd != -1) {
        close(socket_fd);
    }

    modbus_close(c->ctx_modbus);
    modbus_free(c->ctx_modbus);
    c->ctx_modbus = NULL;

    pthread_exit(NULL);

    return (void*)NULL;
}

int tcp_master_read(int ctx_idx, int device_addr, int addr, float *buf, int len)
{
    // read device elapsed time.
    if (addr == 0) {
        buf[0] = 0;
        return 1;
    }

    element_t *element = (element_t *)device_addr;
    if (element != NULL && element->addr == addr) {
        if (len == 1) {
            // int
            short data = (short)element->val;
            buf[0] = (float)data;
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
                float *p = (float *)(&data);
                buf[0] = *p;
                return 2;
            }
        }
    }

    return -1;
}

int tcp_master_write(int ctx_idx, int device_addr, int addr, float *buf, int len)
{
    int result = -1;

    element_t *element = (element_t *)device_addr;
    if (element != NULL && element->addr == addr) {
        if (addr < 20000) {
            if (buf[0] == 0) {
                element->val = 0;
            } else {
                element->val = 1;
            }
            result = 1;
        } else {
            if (len == 1) {
                // int
                short data = (short)buf[0];
                element->val = (unsigned short)data;
                result = 1;
            }
            if (len == 2) {
                // word
                element->val = (unsigned short)buf[0];
                result = 1;
            }
            if (len == 3) {
                // dint
                element_t *next_element = list_entry_safe(element->node.next, element_t);
                if (next_element != NULL && next_element->addr == addr + 1) {
                    int data = (int)buf[0];
                    if (element->endian != 0) {
                        unsigned int _data = (unsigned int)data;
                        element->val = (_data >> 16) & 0xFFFF;
                        next_element->val = (_data & 0xFFFF);
                    } else {
                        unsigned int _data = (unsigned int)data;
                        next_element->val = (_data >> 16) & 0xFFFF;
                        element->val = (_data & 0xFFFF);
                    }
                    result = 2;
                }
            }
            if (len == 4) {
                // dword
                element_t *next_element = list_entry_safe(element->node.next, element_t);
                if (next_element != NULL && next_element->addr == addr + 1) {
                    unsigned int data = (unsigned int)buf[0];
                    if (element->endian != 0) {
                        element->val = (data >> 16) & 0xFFFF;
                        next_element->val = (data & 0xFFFF);
                    } else {
                        next_element->val = (data >> 16) & 0xFFFF;
                        element->val = (data & 0xFFFF);
                    }
                    result = 2;
                }
            }
            if (len == 5) {
                // real
                element_t *next_element = list_entry_safe(element->node.next, element_t);
                if (next_element != NULL && next_element->addr == addr + 1) {
                    unsigned int data = *((unsigned int *)buf);
                    if (element->endian != 0) {
                        element->val = (data >> 16) & 0xFFFF;
                        next_element->val = (data & 0xFFFF);
                    } else {
                        next_element->val = (data >> 16) & 0xFFFF;
                        element->val = (data & 0xFFFF);
                    }
                    result = 2;
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
            if (prev == head->first) {
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

int tcp_master_add(int ctx_idx, int device_addr, int addr, int len)
{
    context_t *c = &context_table[ctx_idx];
    int endian = (device_addr >> 8) & 0xFF;
    device_addr = device_addr & 0xFF;

    if (device_addr == 0 && addr == 0 && len == 0) {
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

int tcp_master_open(int port)
{
    int i;
    int ctx_idx = -1;
    context_t *c;

    for (i=0; i<MAX_MODBUSTCPMASTER_NUM; i++) {
        c = &context_table[i];
        if (c->ctx_modbus == NULL && c->ctx_thread_running == 0) {
            ctx_idx = i;
            break;
        }
    }

    if (ctx_idx < 0 || ctx_idx >= MAX_MODBUSTCPMASTER_NUM) {
        return -1;
    }

    c = &context_table[ctx_idx];
    if (c->ctx_modbus != NULL || c->ctx_thread_running != 0) {
        return -1;
    }

    modbus_t* ctx = modbus_new_tcp(NULL, port);
    if (ctx == NULL) {
        printf("new tcp err !!! \n");
        return -1;
    }

    modbus_set_debug(ctx, FALSE);
    modbus_set_error_recovery(ctx, MODBUS_ERROR_RECOVERY_LINK | MODBUS_ERROR_RECOVERY_PROTOCOL);

    c->ctx_modbus = ctx;
    memset(&c->devices, 0, sizeof(list_head_t));

    c->ctx_thread_running = 1;
    pthread_create(&c->ctx_thread, NULL, thread_modbus_tcp_master, c);

    return ctx_idx;
}

int tcp_master_close(int ctx_idx)
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

    return 0;
}
