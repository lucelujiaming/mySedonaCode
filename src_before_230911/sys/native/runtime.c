#include "sedona.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

#include "list.h"

#define NAME_LEN (32+1)
typedef struct {
    list_node_t node;
    char name[NAME_LEN];
    unsigned char en;
    unsigned int hours;
    unsigned short seconds;
    unsigned short refcnt;
} T_RUNHOUR;

typedef struct {
    list_node_t node;
    char name[NAME_LEN];
    unsigned int value;
    unsigned short refcnt;
} T_SETPOINT;

typedef struct {
    list_head_t runhours;
    list_head_t setpoints;
} T_PERSIST;

#define FILE_NAME "data.persist"
#define FILE_NAME_SAVE "data.persist.save"
// file struct
// -----------------------|
// | file len | check sum |
// | 3        | 1         |
// -----------------------------|
// | runhour len | setpoint len |
// | 2           | 2            |
// ---------------------------------|
// | runhour name | hours | seconds |
// | 33?          | 4     | 2       |
// ---------------------------------|
// | setpoint name | setpoint value |
// | 33?           | 4              |

static T_PERSIST ctx;

static unsigned int get_persist_size(void)
{
    unsigned int len = 4;
    list_node_t *_node = ctx.runhours.first;
    while (_node != NULL) {
        T_RUNHOUR *hour = list_entry_safe(_node, T_RUNHOUR);
        len += strlen(hour->name) + 1 + 4 + 2;
        _node = _node->next;
    }
    _node = ctx.setpoints.first;
    while (_node != NULL) {
        T_SETPOINT *point = list_entry_safe(_node, T_SETPOINT);
        len += strlen(point->name) + 1 + 4;
        _node = _node->next;
    }
    return len;
}

static unsigned char get_persist_checksum(void)
{
    unsigned char sum = 0;
    sum ^= ctx.runhours.len & 0xFF;
    sum ^= (ctx.runhours.len >> 8) & 0xFF;
    sum ^= ctx.setpoints.len & 0xFF;
    sum ^= (ctx.setpoints.len >> 8) & 0xFF;
    list_node_t *_node = ctx.runhours.first;
    while (_node != NULL) {
        T_RUNHOUR *hour = list_entry_safe(_node, T_RUNHOUR);
        int i;
        for (i=0; i<strlen(hour->name); i++) {
            sum ^= (unsigned char)hour->name[i];
        }
        sum ^= hour->hours & 0xFF;
        sum ^= (hour->hours >> 8) & 0xFF;
        sum ^= (hour->hours >> 16) & 0xFF;
        sum ^= (hour->hours >> 24) & 0xFF;
        sum ^= hour->seconds & 0xFF;
        sum ^= (hour->seconds >> 8) & 0xFF;
        _node = _node->next;
    }
    _node = ctx.setpoints.first;
    while (_node != NULL) {
        T_SETPOINT *point = list_entry_safe(_node, T_SETPOINT);
        int i;
        for (i=0; i<strlen(point->name); i++) {
            sum ^= (unsigned char)point->name[i];
        }
        sum ^= point->value & 0xFF;
        sum ^= (point->value >> 8) & 0xFF;
        sum ^= (point->value >> 16) & 0xFF;
        sum ^= (point->value >> 24) & 0xFF;
        _node = _node->next;
    }
    sum ^= 0x55;
    return sum;
}

static unsigned char checksum(unsigned char *data, unsigned int len, unsigned char sum)
{
    unsigned char _sum = 0;
    int i;
    for (i=0; i<len; i++) {
        _sum ^= data[i];
    }
    _sum ^= 0x55;
    if (_sum == sum)
        return 1;
    return 0;
}

Cell sys_Runtime_load(SedonaVM* vm, Cell* params)
{
    Cell ret;
    ret.ival = 0;
    int inited = 0;

    if (ctx.runhours.len == 0 && ctx.setpoints.len == 0)
    {
        FILE* file = fopen(FILE_NAME_SAVE, "rb");
        if (file != NULL) {
            if (fseek(file, 0, SEEK_END) == 0)
            {
                size_t size = ftell(file);;
                if (size > 0) {
                    unsigned char *data = malloc(size);
                    if (data != NULL) {
                        rewind(file);
                        if (size == fread(data, 1, size, file)) {
                            unsigned int persist_size = data[0] + (data[1] << 8) + (data[2] << 16);
                            unsigned char check_sum = data[3];
                            if (persist_size + 4 == size && checksum(data + 4, persist_size, check_sum) == 1) {
                                unsigned short runhour_len = data[4] + (data[5] << 8);
                                unsigned short setpoint_len = data[6] + (data[7] << 8);
                                unsigned char *p = data + 8;
                                int i;
                                for (i=0; i<runhour_len; i++) {
                                    T_RUNHOUR *hour = malloc(sizeof(T_RUNHOUR));
                                    if (hour != NULL && strlen((char *)p) < NAME_LEN - 1) {
                                        memset(hour, 0, sizeof(T_RUNHOUR));
                                        strcpy(hour->name, (char *)p);
                                        p += strlen((char *)p) + 1;
                                        hour->hours = p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);
                                        p += 4;
                                        hour->seconds = p[0] + (p[1] << 8);
                                        p += 2;
                                        list_put(&ctx.runhours, &hour->node);
                                    }
                                }
                                for (i=0; i<setpoint_len; i++) {
                                    T_SETPOINT *point = malloc(sizeof(T_SETPOINT));
                                    if (point != NULL && strlen((char *)p) < NAME_LEN - 1) {
                                        memset(point, 0, sizeof(T_SETPOINT));
                                        strcpy(point->name, (char *)p);
                                        p += strlen((char *)p) + 1;
                                        point->value = p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);
                                        p += 4;
                                        list_put(&ctx.setpoints, &point->node);
                                    }
                                }
                                if (p - data == size) {
                                    inited = 1;
                                }
                            }
                        }
                        free(data);
                    }
                }
            }
            fclose(file);
        } // (file != NULL)
        if (inited == 1) {
            remove(FILE_NAME);
            rename(FILE_NAME_SAVE, FILE_NAME);
        } else {
            file = fopen(FILE_NAME, "rb");
            if (file != NULL) {
                if (fseek(file, 0, SEEK_END) == 0)
                {
                    size_t size = ftell(file);;
                    if (size > 0) {
                        unsigned char *data = malloc(size);
                        if (data != NULL) {
                            rewind(file);
                            if (size == fread(data, 1, size, file)) {
                                unsigned int persist_size = data[0] + (data[1] << 8) + (data[2] << 16);
                                unsigned char check_sum = data[3];
                                if (persist_size + 4 == size && checksum(data + 4, persist_size, check_sum) == 1) {
                                    unsigned short runhour_len = data[4] + (data[5] << 8);
                                    unsigned short setpoint_len = data[6] + (data[7] << 8);
                                    unsigned char *p = data + 8;
                                    int i;
                                    for (i=0; i<runhour_len; i++) {
                                        T_RUNHOUR *hour = malloc(sizeof(T_RUNHOUR));
                                        if (hour != NULL && strlen((char *)p) < NAME_LEN - 1) {
                                            memset(hour, 0, sizeof(T_RUNHOUR));
                                            strcpy(hour->name, (char *)p);
                                            p += strlen((char *)p) + 1;
                                            hour->hours = p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);
                                            p += 4;
                                            hour->seconds = p[0] + (p[1] << 8);
                                            p += 2;
                                            list_put(&ctx.runhours, &hour->node);
                                        }
                                    }
                                    for (i=0; i<setpoint_len; i++) {
                                        T_SETPOINT *point = malloc(sizeof(T_SETPOINT));
                                        if (point != NULL && strlen((char *)p) < NAME_LEN - 1) {
                                            memset(point, 0, sizeof(T_SETPOINT));
                                            strcpy(point->name, (char *)p);
                                            p += strlen((char *)p) + 1;
                                            point->value = p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);
                                            p += 4;
                                            list_put(&ctx.setpoints, &point->node);
                                        }
                                    }
                                    if (p - data == size) {
                                        inited = 1;
                                    }
                                }
                            }
                            free(data);
                        }
                    }
                }
                fclose(file);
            } // (file != NULL)
        }
    } // (inited == 0)

    return ret;
}

Cell sys_Runtime_save(SedonaVM* vm, Cell* params)
{
    Cell ret;
    ret.ival = 0;

    if (ctx.runhours.len> 0 || ctx.setpoints.len > 0) {
        unsigned int persist_size = get_persist_size();
        if (persist_size > 0) {
            size_t size = persist_size + 4;
            unsigned char *data = malloc(size);
            if (data != NULL) {
                unsigned char *p = data;
                // persist size
                *p++ = persist_size & 0xFF;
                *p++ = (persist_size >> 8) & 0xFF;
                *p++ = (persist_size >> 16) & 0xFF;
                // check sum
                *p++ = get_persist_checksum();
                // runhour len
                *p++ = ctx.runhours.len & 0xFF;
                *p++ = (ctx.runhours.len >> 8) & 0xFF;
                // setpoint len
                *p++ = ctx.setpoints.len & 0xFF;
                *p++ = (ctx.setpoints.len >> 8) & 0xFF;
                // runhours
                list_node_t *_node = ctx.runhours.first;
                while (_node != NULL) {
                    T_RUNHOUR *hour = list_entry_safe(_node, T_RUNHOUR);
                    int i;
                    for (i=0; i<strlen(hour->name); i++) {
                        *p++ = (unsigned char)hour->name[i];
                    }
                    *p++ = 0;
                    *p++ = hour->hours & 0xFF;
                    *p++ = (hour->hours >> 8) & 0xFF;
                    *p++ = (hour->hours >> 16) & 0xFF;
                    *p++ = (hour->hours >> 24) & 0xFF;
                    *p++ = hour->seconds & 0xFF;
                    *p++ = (hour->seconds >> 8) & 0xFF;
                    _node = _node->next;
                }
                _node = ctx.setpoints.first;
                while (_node != NULL) {
                    T_SETPOINT *point = list_entry_safe(_node, T_SETPOINT);
                    int i;
                    for (i=0; i<strlen(point->name); i++) {
                        *p++ = (unsigned char)point->name[i];
                    }
                    *p++ = 0;
                    *p++ = point->value & 0xFF;
                    *p++ = (point->value >> 8) & 0xFF;
                    *p++ = (point->value >> 16) & 0xFF;
                    *p++ = (point->value >> 24) & 0xFF;
                    _node = _node->next;
                }
                if (p - data == size) {
                    FILE *file = fopen(FILE_NAME_SAVE, "wb");
                    if (file != NULL) {
                        fwrite(data, 1, size, file);
                        fclose(file);
                    }
                }
                free(data);
            }
        }
    } else {
        remove(FILE_NAME);
        remove(FILE_NAME_SAVE);
    }

    return ret;
}

extern unsigned long long get_tick_ms(void);
Cell sys_Runtime_timeOpen(SedonaVM* vm, Cell* params)
{
    const char *name = params[0].aval;

    Cell ret;
    ret.ival = -1;

    if (strlen(name) > 0 && strlen(name) < NAME_LEN) {
        list_node_t *_node = ctx.runhours.first;
        while (_node != NULL) {
            T_RUNHOUR *hour = list_entry_safe(_node, T_RUNHOUR);
            if (strcmp(hour->name, name) == 0) {
                hour->refcnt++;
                ret.ival = (int)hour;
                break;
            }
            _node = _node->next;
        }
        if (ret.ival == -1) {
            T_RUNHOUR *p = malloc(sizeof(T_RUNHOUR));
            memset(p, 0, sizeof(T_RUNHOUR));
            if (p != NULL) {
                strcpy(p->name, name);
                p->refcnt++;
                p->hours = 0;
                p->seconds = 0;
                list_put(&ctx.runhours, &p->node);
                ret.ival = (int)p;
            }
        }
    }

    return ret;
}

Cell sys_Runtime_timeClose(SedonaVM* vm, Cell* params)
{
    T_RUNHOUR *hour = (T_RUNHOUR*)params[0].ival;

    hour->refcnt--;
    if (hour->refcnt== 0) {
        list_delete(&ctx.runhours, &hour->node);
        free(hour);
        sys_Runtime_save(NULL, NULL);
    }

    Cell ret;
    ret.ival = 0;
    return ret;
}

static unsigned int update_count = 0;
static unsigned long long runtime_next_update = 0;
static void update_runtime(void)
{
    unsigned long long curr = get_tick_ms();
    if (runtime_next_update == 0)
        runtime_next_update = curr + 1000;
    if (runtime_next_update < curr)
    {
        list_node_t *_node = ctx.runhours.first;
        while (_node != NULL) {
            T_RUNHOUR *hour = list_entry_safe(_node, T_RUNHOUR);
            if (hour->en != 0) {
                hour->seconds ++;
                if (hour->seconds == 3600) {
                    hour->seconds = 0;
                    hour->hours ++;
                }
            }
            _node = _node->next;
        }
        runtime_next_update += 1000;
        if (++update_count == 1800) {
            update_count = 0;
            sys_Runtime_save(NULL, NULL);
        }
    }
}

Cell sys_Runtime_timeRead(SedonaVM* vm, Cell* params)
{
    T_RUNHOUR *hour = (T_RUNHOUR *)params[0].ival;
    float *buf = params[1].aval;

    update_runtime();
    buf[1] = (float)hour->hours;
    buf[2] = (float)hour->seconds;

    Cell ret;
    ret.ival = 0;
    return ret;
}

Cell sys_Runtime_timeWrite(SedonaVM* vm, Cell* params)
{
    T_RUNHOUR *hour = (T_RUNHOUR*)params[0].ival;
    float *buf = params[1].aval;

    hour->en = buf[0] == 0 ? 0 : 1;
    hour->hours = (unsigned int)buf[1];
    hour->seconds = (unsigned short)buf[2];
    sys_Runtime_save(NULL, NULL);

    Cell ret;
    ret.ival = 0;
    return ret;
}

Cell sys_Runtime_dataOpen(SedonaVM* vm, Cell* params)
{
    const char *name = params[0].aval;

    Cell ret;
    ret.ival = -1;

    if (strlen(name) > 0 && strlen(name) < NAME_LEN) {
        list_node_t *_node = ctx.setpoints.first;
        while (_node != NULL) {
            T_SETPOINT *point = list_entry_safe(_node, T_SETPOINT);
            if (strcmp(point->name, name) == 0) {
                point->refcnt++;
                ret.ival = (int)point;
                break;
            }
            _node = _node->next;
        }
        if (ret.ival == -1) {
            T_SETPOINT *p = malloc(sizeof(T_SETPOINT));
            memset(p, 0, sizeof(T_SETPOINT));
            if (p != NULL) {
                strcpy(p->name, name);
                p->refcnt++;
                p->value = 0;
                list_put(&ctx.setpoints, &p->node);
                ret.ival = (int)p;
            }
        }
    }

    return ret;
}

Cell sys_Runtime_dataClose(SedonaVM* vm, Cell* params)
{
    T_SETPOINT *point = (T_SETPOINT *)params[0].ival;

    point->refcnt--;
    if (point->refcnt== 0) {
        list_delete(&ctx.setpoints, &point->node);
        free(point);
        sys_Runtime_save(NULL, NULL);
    }

    Cell ret;
    ret.ival = 0;
    return ret;
}

Cell sys_Runtime_dataRead(SedonaVM* vm, Cell* params)
{
    T_SETPOINT *point = (T_SETPOINT *)params[0].ival;
    unsigned int *buf = params[1].aval;

    buf[0] = point->value;

    Cell ret;
    ret.ival = 0;
    return ret;
}

Cell sys_Runtime_dataWrite(SedonaVM* vm, Cell* params)
{
    T_SETPOINT *point = (T_SETPOINT *)params[0].ival;
    unsigned int *buf = params[1].aval;

    if (point->value != buf[0]) {
        point->value = buf[0];
        sys_Runtime_save(NULL, NULL);
    }

    Cell ret;
    ret.ival = 0;
    return ret;
}

#define TABLE_SIZE 50
typedef struct {
    float x[TABLE_SIZE];
    float y[TABLE_SIZE];
    int num;
} T_TABLE;

Cell sys_Runtime_tableA(SedonaVM* vm, Cell* params)
{
    Cell ret;
    ret.ival = -1;

    T_TABLE *p = malloc(sizeof(T_TABLE));
    if (p != NULL) {
        int i;
        for (i=0; i<TABLE_SIZE; i++) {
            p->x[i] = 0;
            p->y[i] = 0;
        }
        ret.ival = (int)p;
    }

    return ret;
}

Cell sys_Runtime_tableD(SedonaVM* vm, Cell* params)
{
    Cell ret;
    ret.ival = 0;

    T_TABLE *p = (T_TABLE*)params[0].ival;
    if (p != NULL && (int)p != -1) {
        free(p);
    }

    return ret;
}

Cell sys_Runtime_tableC(SedonaVM* vm, Cell* params)
{
    Cell ret;
    ret.ival = 0;

    T_TABLE *p = (T_TABLE*)params[0].ival;
    if (p != NULL && (int)p != -1) {
        float *x = params[1].aval;
        float *y = params[2].aval;
        int i;
        for (i=0; i<TABLE_SIZE; i++) {
            p->x[i] = x[i];
            p->y[i] = y[i];
        }
        for (i=TABLE_SIZE-1; i>=0; i--) {
            if (x[i] != 0 || y[i] != 0) {
                p->num = i + 1;
                break;
            }
        }
        /*
        if (p->num < TABLE_SIZE) {
            if (x[0] < x[1] && x[p->num-1] < x[p->num]) {
                p->num++;
            } else if (x[1] < x[0] && x[p->num-1] > x[p->num]) {
                p->num++;
            }
        }
        */
    }

    return ret;
}

Cell sys_Runtime_tableE(SedonaVM* vm, Cell* params)
{
    Cell ret;
    ret.fval = 0.0f;

    T_TABLE *p = (T_TABLE*)params[0].ival;
    if (p != NULL && (int)p != -1) {
        float in = params[1].fval;
        if (p->x[0] < p->x[1]) {
            if (in > p->x[p->num-1]) {
                ret.fval = p->y[p->num-1];
            } else if (in <= p->x[0]) {
                ret.fval = p->y[0];
            } else {
                int i;
                for (i=0; i<p->num-1; i++) {
                    if (p->x[i] < in && in <= p->x[i+1]) {
                        float m = (p->y[i+1] - p->y[i]) / (p->x[i+1] - p->x[i]);
                        float b = p->y[i] - (m * p->x[i]);
                        ret.fval = (m * in) + b;
                    }
                }
            }
        } else if (p->x[1] < p->x[0]) {
            if (in < p->x[p->num-1]) {
                ret.fval = p->y[p->num-1];
            } else if (in >= p->x[0]) {
                ret.fval = p->y[0];
            } else {
                int i;
                for (i=0; i<p->num-1; i++) {
                    if (p->x[i+1] <= in && in < p->x[i]) {
                        float m = (p->y[i] - p->y[i+1]) / (p->x[i] - p->x[i+1]);
                        float b = p->y[i+1] - (m * p->x[i+1]);
                        ret.fval = (m * in) + b;
                    }
                }
            }
        }
    }

    return ret;
}

Cell sys_Runtime_mathSq(SedonaVM* vm, Cell* params)
{
    float *buf = params[0].aval;
    buf[1] = powf(buf[0], 2);

    Cell ret;
    ret.ival = 0;
    return ret;
}

Cell sys_Runtime_mathSqrt(SedonaVM* vm, Cell* params)
{
    float *buf = params[0].aval;
    buf[1] = sqrtf(buf[0]);

    Cell ret;
    ret.ival = 0;
    return ret;
}

Cell sys_Runtime_mathLn(SedonaVM* vm, Cell* params)
{
    float *buf = params[0].aval;
    buf[1] = logf(buf[0]);

    Cell ret;
    ret.ival = 0;
    return ret;
}

Cell sys_Runtime_mathExp(SedonaVM* vm, Cell* params)
{
    float *buf = params[0].aval;
    buf[2] = powf(buf[0], buf[1]);

    Cell ret;
    ret.ival = 0;
    return ret;
}

Cell sys_Runtime_mathExpt(SedonaVM* vm, Cell* params)
{
    float *buf = params[0].aval;
    buf[2] = logf(buf[0])/logf(buf[1]);

    Cell ret;
    ret.ival = 0;
    return ret;
}

#include <fcntl.h>
#include <linux/rtc.h>
#include <sys/ioctl.h>

#define RTC_DRV_NAME "/dev/rtc0"

#define leapyear(year) ((year) % 4 == 0)
#define days_in_year(a) (leapyear(a) ? 366 : 365)
#define days_in_month(a) (month_days[(a) - 1])

// begin with 0
static int getyday(int y, int m, int d)
{
    int month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int yday = 0;
    int i;
    for (i=1; i<=12; i++) {
        if (i >= m) break;
        if (i == 2 && leapyear(y)) {
            yday += 29;
        } else {
            yday += days_in_month(i);
        }
    }
    yday += d;
    return yday - 1;
}

static int getwday(int y, int m, int d)
{
    int mday = 0;
    int i;
    for (i=2000; i<=2099; i++) {
        if (i >= y) break;
        mday += days_in_year(i);
    }
    mday += getyday(y, m, d);
    // 20000101 is Sat
    mday += 6;
    return (mday % 7);
}

static int currecttime(struct rtc_time * time)
// mon, mday set yday.
{
    int month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int yday = 0;
    int y = time->tm_year;
    int m = time->tm_mon + 1;
    int d = time->tm_mday;
    int i;
    for (i=1; i<=12; i++) {
        if (i >= m) break;
        if (i == 2 && leapyear(y)) {
            yday += 29;
        } else {
            yday += days_in_month(i);
        }
    }
    yday += d;
    time->tm_yday = yday - 1;
    return 0;
}

static int rtc_fd = -1;

static void set_rtc(unsigned int date, unsigned int time)
{
    struct rtc_time rtc_tm;
    rtc_tm.tm_year = (date >> 16) - 1900;
    rtc_tm.tm_mon = ((date >> 8) & 0xFF) - 1;
    rtc_tm.tm_mday = (date & 0xFF);
    rtc_tm.tm_hour = (time >> 16) & 0xFF;
    rtc_tm.tm_min = (time >> 8) & 0xFF;
    rtc_tm.tm_sec = time & 0xFF;
    rtc_tm.tm_wday = getwday(rtc_tm.tm_year + 1900, rtc_tm.tm_mon + 1, rtc_tm.tm_mday);
    rtc_tm.tm_yday = getyday(rtc_tm.tm_year + 1900, rtc_tm.tm_mon + 1, rtc_tm.tm_mday);
    if (rtc_tm.tm_year < 2019 - 1900) return;
    if (rtc_tm.tm_mon > 11) return;
    if (rtc_tm.tm_mday > 31) return;
    if (rtc_tm.tm_mday == 0) return;
    if (rtc_tm.tm_hour > 23) return;
    if (rtc_tm.tm_min > 59) return;
    if (rtc_tm.tm_sec > 59) return;
    ioctl(rtc_fd, RTC_SET_TIME, &rtc_tm);
}

int64_t sys_Runtime_now(SedonaVM* vm, Cell* params)
{
  int64_t time = 0;

  if (rtc_fd == -1) {
      rtc_fd = open(RTC_DRV_NAME, O_RDONLY);
  }
  if (rtc_fd != -1) {
      struct rtc_time rtc_tm;
      if (ioctl(rtc_fd, RTC_RD_TIME, &rtc_tm) != -1) {
          if (currecttime(&rtc_tm) == 1) {
          }
          time = ((rtc_tm.tm_year + 1900) << 8) + (rtc_tm.tm_mon + 1);
          time = time << 32;
          time = time + (rtc_tm.tm_mday << 24) +
                  (rtc_tm.tm_hour << 16) + (rtc_tm.tm_min << 8) + (rtc_tm.tm_sec);
          {
              extern unsigned int *get_mstp_datetimeptr(void);
              extern unsigned int *get_bip_datetimeptr(void);
              unsigned int *mstp_datetimeptr = get_mstp_datetimeptr();
              unsigned int *bip_datetimeptr = get_bip_datetimeptr();
              if (mstp_datetimeptr != NULL && mstp_datetimeptr[2] != 0 && mstp_datetimeptr[3] != 0) {
                  set_rtc(mstp_datetimeptr[2], mstp_datetimeptr[3]);
                  if (bip_datetimeptr != NULL) {
                      bip_datetimeptr[2] = 0;
                      bip_datetimeptr[3] = 0;
                  }
                  mstp_datetimeptr[2] = 0;
                  mstp_datetimeptr[3] = 0;
              }
              if (bip_datetimeptr != NULL && bip_datetimeptr[2] != 0 && bip_datetimeptr[3] != 0) {
                  set_rtc(bip_datetimeptr[2], bip_datetimeptr[3]);
                  bip_datetimeptr[2] = 0;
                  bip_datetimeptr[3] = 0;
              }
              if (mstp_datetimeptr != NULL) {
                  mstp_datetimeptr[0] = ((unsigned short)(rtc_tm.tm_year + 1900) << 16) | ((unsigned char)(rtc_tm.tm_mon + 1) << 8) | ((unsigned char)rtc_tm.tm_mday);
                  mstp_datetimeptr[1] = ((unsigned char)rtc_tm.tm_hour) << 16 | ((unsigned char)rtc_tm.tm_min) << 8 | ((unsigned char)rtc_tm.tm_sec);
              }
              if (bip_datetimeptr != NULL) {
                  bip_datetimeptr[0] = ((unsigned short)(rtc_tm.tm_year + 1900) << 16) | ((unsigned char)(rtc_tm.tm_mon + 1) << 8) | ((unsigned char)rtc_tm.tm_mday);
                  bip_datetimeptr[1] = ((unsigned char)rtc_tm.tm_hour) << 16 | ((unsigned char)rtc_tm.tm_min) << 8 | ((unsigned char)rtc_tm.tm_sec);
              }
          }
      }
  }

  return time;
}

Cell sys_Runtime_setTime(SedonaVM* vm, Cell* params)
{
  int64_t time = *(int64_t*)(params+0);

  if (rtc_fd == -1) {
      rtc_fd = open(RTC_DRV_NAME, O_RDONLY);
  }
  if (rtc_fd != -1) {
      struct rtc_time rtc_tm;
      int d = (int)((time >> 40) & 0xFFF);
      rtc_tm.tm_year = d - 1900;
      d = (int)((time >> 32) & 0xFF);
      rtc_tm.tm_mon = d - 1;
      d = (int)((time >> 24) & 0xFF);
      rtc_tm.tm_mday = d;
      d = (int)((time >> 16) & 0xFF);
      rtc_tm.tm_hour = d;
      d = (int)((time >> 8) & 0xFF);
      rtc_tm.tm_min = d;
      d = (int)(time & 0xFF);
      rtc_tm.tm_sec = d;
      rtc_tm.tm_wday = getwday(rtc_tm.tm_year + 1900, rtc_tm.tm_mon + 1, rtc_tm.tm_mday);
      rtc_tm.tm_yday = getyday(rtc_tm.tm_year + 1900, rtc_tm.tm_mon + 1, rtc_tm.tm_mday);
      ioctl(rtc_fd, RTC_SET_TIME, &rtc_tm);
  }

  return nullCell;
}
