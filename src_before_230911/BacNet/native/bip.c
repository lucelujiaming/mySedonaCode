#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/ipc.h>
#include <sys/shm.h>

typedef struct {
    unsigned int instance;
    unsigned int refresh_ms;
    unsigned int elapsed_ms;
} T_DEVICE;

typedef struct {
    unsigned int instance;
    unsigned int property;
    unsigned int type;
    unsigned int device_instance;
    float value;
    float value_w;
} T_OBJECT;

#define SCHEDULE_DAY_VALUE_SIZE 8
typedef struct {
    unsigned int instance;
    float default_value;
    float present_value;
    float value[7*SCHEDULE_DAY_VALUE_SIZE];
    unsigned short year[2];
    unsigned char month[2];
    unsigned char day[2];
    unsigned char hour[7*SCHEDULE_DAY_VALUE_SIZE];
    unsigned char min[7*SCHEDULE_DAY_VALUE_SIZE];
    unsigned char sec[7*SCHEDULE_DAY_VALUE_SIZE];
    unsigned char is_dirty;
    char name[32];
} T_SCHEDULE;

typedef struct _context {
    volatile unsigned int opend; // master use
    volatile unsigned int used;  // slave use
    unsigned int device_size;
    unsigned int object_size;
    unsigned int schedule_size;
    unsigned int port;
    unsigned int device_instance;
    unsigned int retry_delay;
    unsigned int curr_date;
    unsigned int curr_time;
    unsigned int set_date;
    unsigned int set_time;
} T_CONTEXT;

#define SHM_SIZE 4096
enum { KEY_ID = 0x5847414F };

static unsigned int shmsize;
static int shmid;
static char *shmbuf;
static T_CONTEXT *context = NULL;
#define DEVICE_PTR ((char *)(((char *)context) + sizeof(T_CONTEXT)))
#define OBJECT_PTR ((char *)(DEVICE_PTR + sizeof(T_DEVICE) * context->device_size))
#define SCHEDULE_PTR ((char *)(OBJECT_PTR + sizeof(T_OBJECT) * context->object_size))

int bip_open(int port, int device_instance, int retry_delay)
{
    if (context == NULL) {
        shmsize = SHM_SIZE;
        shmbuf = NULL;
        shmid = shmget(KEY_ID, shmsize, IPC_CREAT | IPC_EXCL | 0644);
        if (shmid == -1) {
            if (errno == EEXIST) {
                shmid = shmget(KEY_ID, 0, 0);
            }
        }
        if (shmid != -1) {
            shmbuf = shmat(shmid, NULL, 0);
            if (shmbuf == (void*) -1L) {
                shmbuf = NULL;
            }
        }
        if (shmbuf != NULL) {
            context = (T_CONTEXT *)shmbuf;
        }
    }
    if (context == NULL) {
        return -1;
    }
    context->opend = 0;
    if (context->used == 1) {
        while (context->used == 1) {
            usleep(10*1000);
        }
    }
    context->device_size = 0;
    context->object_size = 0;
    context->schedule_size = 0;
    context->port = port;
    context->device_instance = device_instance;
    context->retry_delay = retry_delay;
    return 0;
}

int bip_close(int ctx_idx)
{
    if (context == NULL) {
        return -1;
    }
    context->opend = 0;
    if (context->used == 1) {
        while (context->used == 1) {
            usleep(10*1000);
        }
    }
    return 0;
}

int bip_add(int ctx_idx, int device_instance, int object_type, int object_instance, int object_property, int refreshms)
{
    if (refreshms == 0) {
        // add node
        int i;
        int is_exist = 0;
        for (i=0; i<context->object_size; i++) {
            T_OBJECT *obj = (T_OBJECT *)(OBJECT_PTR + sizeof(T_OBJECT) * i);
            if (obj->instance == object_instance && obj->device_instance == device_instance && obj->type == (unsigned char)(object_type & 0xFF) && obj->property == object_property) {
                is_exist = 1;
                break;
            }
        }
        if (is_exist == 0) {
            T_OBJECT *obj = (T_OBJECT *)(OBJECT_PTR + sizeof(T_OBJECT) * context->object_size);
            memset(obj, 0, sizeof(T_OBJECT));
            obj->instance = object_instance;
            obj->property = object_property;
            obj->type = (unsigned char)(object_type & 0xFF);
            obj->device_instance = device_instance;
            context->object_size ++;
        }
        return context->object_size;
    } else if (refreshms == 1) {
        context->opend = 1;
        /*
        {
            int i;
            for (i=0; i<context->device_size; i++) {
                T_DEVICE *dev = (T_DEVICE *)(DEVICE_PTR + sizeof(T_DEVICE) * i);
                printf("Dev: @%p, %d, %d\n", dev, dev->instance, dev->refresh_ms);
            }
            for (i=0; i<context->object_size; i++) {
                T_OBJECT *obj = (T_OBJECT *)(OBJECT_PTR + sizeof(T_OBJECT) * i);
                printf("Obj: @%p, %d, %d, %d, %d\n", obj, obj->instance, obj->property, obj->type, obj->device_instance);
            }
            printf("-------------------------------------\n\n");
        }
        */
    } else {
        // add device
        int i;
        int is_exist = 0;
        for (i=0; i<context->device_size; i++) {
            T_DEVICE *dev = (T_DEVICE *)(DEVICE_PTR + sizeof(T_DEVICE) * i);
            if (dev->instance == device_instance) {
                is_exist = 1;
                break;
            }
        }
        if (is_exist == 0) {
            T_DEVICE *dev = (T_DEVICE *)(DEVICE_PTR + sizeof(T_DEVICE) * context->device_size);
            dev->instance = device_instance;
            dev->refresh_ms = refreshms;
            dev->elapsed_ms = 0;
            context->device_size ++;
            return 0;
        }
    }
    return -1;
}

int bip_read(int ctx_idx, int cache, int reg_addr, float *buf, int type)
{
    if (type == 10000) {
        if (cache == context->device_instance) {
            buf[0] = 0;
            return 1;
        } else {
            int i;
            T_DEVICE *dev = NULL;
            for (i=0; i<context->device_size; i++) {
                T_DEVICE *dev_t = (T_DEVICE *)(DEVICE_PTR + sizeof(T_DEVICE) * i);
                if (dev_t->instance == cache) {
                    dev = dev_t;
                    break;
                }
            }
            if (dev != NULL && dev->elapsed_ms != 0) {
                buf[0] = (float)dev->elapsed_ms;
                return 1;
            }
        }
    } else {
        int obj_idx = cache - 1;
        if (obj_idx >= 0 && obj_idx < context->object_size) {
            T_OBJECT *obj = (T_OBJECT *)(OBJECT_PTR + sizeof(T_OBJECT) * obj_idx);
            //printf("Read: %d %d %d %d\n", obj_idx, cache, reg_addr, type);
            if (reg_addr == obj->instance && type == obj->type) {
                buf[0] = (float)obj->value;
                return 1;
            }
        }
    }
    return -1;
}

int bip_write(int ctx_idx, int cache, int reg_addr, float *buf, int type)
{
    int obj_idx = cache - 1;
    if (obj_idx >= 0 && obj_idx < context->object_size) {
        T_OBJECT *obj = (T_OBJECT *)(OBJECT_PTR + sizeof(T_OBJECT) * obj_idx);
        if (reg_addr == obj->instance && type == obj->type) {
            if (obj->device_instance == context->device_instance) {
                // self
                obj->value = buf[0];
                return 1;
            } else if (!(type == 0 || type == 3)) {
                // other, not ai and bi
                obj->value_w = buf[0];
                return 1;
            }
        }
    }
    return -1;
}

unsigned int *get_bip_datetimeptr(void)
{
    if (context != NULL && context->opend == 1 && context->used == 1) {
        return &(context->curr_date);
    }
    return NULL;
}

int bip_add_schedule(int ctx_idx, char *urlBuf, int *timeBuf, float *valueBuf)
{
    extern int date_check(unsigned int *date);
    extern int time_check(int *time);

    int is_dirty = 0;
    char name[32] = { 0 };
    int i, j, k;

    T_SCHEDULE schedule = { 0 };

    //printf("Add Schedule %d\n", ctx_idx);

    i = urlBuf[0];
    memcpy(name, urlBuf + 1, i);
    strcpy(schedule.name, name);
    //printf("  Schedule Name: %s\n", name);

    schedule.instance = (unsigned int)timeBuf[0];
    //printf("  Schedule Instance: %d\n", schedule.instance);

    // startYear
    i = timeBuf[1];
    if (date_check((unsigned int *)&i) == 1) {
        is_dirty = 1;
    }
    schedule.year[0] = (i / 10000) % 10000;
    schedule.month[0] = (i / 100) % 100;
    schedule.day[0] = i % 100;
    //printf("  Start Date: %d-%d-%d\n", schedule.year[0], schedule.month[0], schedule.day[0]);

    // endYear
    i = timeBuf[2];
    if (date_check((unsigned int *)&i) == 1) {
        is_dirty = 1;
    }
    schedule.year[1] = (i / 10000) % 10000;
    schedule.month[1] = (i / 100) % 100;
    schedule.day[1] = i % 100;
    //printf("  End Date: %d-%d-%d\n", schedule.year[1], schedule.month[1], schedule.day[1]);

    schedule.present_value = valueBuf[0];
    schedule.default_value = valueBuf[1];
    //printf("  Schedule Default Value: %f\n", data[1]);

    //week time
    for (i=0; i<7; i++) {
        unsigned int day_time[SCHEDULE_DAY_VALUE_SIZE] = { 0 };
        float day_value[SCHEDULE_DAY_VALUE_SIZE] = { 0.0f };
        int day_index = 0;

        for (j=0; j<SCHEDULE_DAY_VALUE_SIZE; j++) {
            k = timeBuf[3 + i * SCHEDULE_DAY_VALUE_SIZE + j];
            day_time[day_index] = timeBuf[3 + i * SCHEDULE_DAY_VALUE_SIZE + j];
            if (time_check(&k) == 1) {
                is_dirty = 1;
            }
            day_time[day_index] = k;
            day_value[day_index] = valueBuf[2 + i * SCHEDULE_DAY_VALUE_SIZE + j];
            if (day_time[day_index] == 0 && day_value[day_index] == 0.0f) {
                continue;
            }
            day_index ++;
        }
        // sort
        k = 1;
        while (k) {
            k = 0;
            for (j=0; j<day_index-1; j++) {
                if (day_time[j] > day_time[j+1]) {
                    unsigned int day_time_swp = day_time[j];
                    float day_value_swp = day_value[j];
                    day_time[j] = day_time[j + 1];
                    day_value[j] = day_value[j + 1];
                    day_time[j + 1] = day_time_swp;
                    day_value[j + 1] = day_value_swp;
                    k = 1;
                }
            }
        }
        // save
        for (j=0; j<SCHEDULE_DAY_VALUE_SIZE; j++) {
            int valueIndex = i * SCHEDULE_DAY_VALUE_SIZE + j;
            if (j < day_index) {
                schedule.hour[valueIndex] = (day_time[j] / 10000);
                schedule.min[valueIndex] = (day_time[j] / 100) % 100;
                schedule.sec[valueIndex] = day_time[j] % 100;
                schedule.value[valueIndex] = day_value[j];
            } else {
                schedule.hour[valueIndex] = 0;
                schedule.min[valueIndex] = 0;
                schedule.sec[valueIndex] = 0;
                schedule.value[valueIndex] = 0.0f;
            }
        }
    }

    //for (i=0; i<56; i++) {
    //    printf("  %d-%d %02d:%02d:%02d %f\n", ((i/8) + 1), ((i%8)) + 1, schedule.hour[i], schedule.min[i], schedule.sec[i], schedule.value[i]);
    //}

    if (context != NULL) {
        if (context->schedule_size >= 32) {
            return -1;
        }
        for (i=0; i<context->schedule_size; i++) {
            T_SCHEDULE *sche = (T_SCHEDULE *)(SCHEDULE_PTR + sizeof(T_SCHEDULE) * i);
            if (sche->instance == schedule.instance) {
                return -1;
            }
        }
        if (is_dirty == 1) {
            schedule.is_dirty = 1;
        }
        memcpy((SCHEDULE_PTR + sizeof(T_SCHEDULE) * context->schedule_size), &schedule, sizeof(T_SCHEDULE));
        context->schedule_size ++;
        return context->schedule_size - 1;
    }

    return -1;
}

int bip_get_schedule(int ctx_idx, int sche_idx, int *timeBuf, float *valueBuf)
{
    if (sche_idx < 0) {
        return -1;
    }

    if (context != NULL && sche_idx < context->schedule_size) {
        T_SCHEDULE *sche = (T_SCHEDULE *)(SCHEDULE_PTR + sizeof(T_SCHEDULE) * sche_idx);
        int i, j;

        timeBuf[1] = sche->year[0] * 10000 + sche->month[0] * 100 + sche->day[0];
        timeBuf[2] = sche->year[1] * 10000 + sche->month[1] * 100 + sche->day[1];

        valueBuf[0] = sche->present_value;

        for (i=0; i<7; i++) {
            for (j=0; j<SCHEDULE_DAY_VALUE_SIZE; j++) {
                int valueIndex = i * SCHEDULE_DAY_VALUE_SIZE + j;
                timeBuf[valueIndex + 3] = sche->hour[valueIndex] * 10000 + sche->min[valueIndex] * 100 + sche->sec[valueIndex];
                valueBuf[valueIndex + 2] = sche->value[valueIndex];
            }
        }

        if (sche->is_dirty > 1) {
            sche->is_dirty --;
        }
        if (sche->is_dirty == 1) {
            sche->is_dirty = 0;
            return 1;
        } else {
            return 0;
        }
    }

    return -1;
}
