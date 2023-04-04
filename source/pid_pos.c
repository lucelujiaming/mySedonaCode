#include "sedona.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

// PID
typedef struct {
    float output;
    float err0;
    float err1;
    float err2;
}T_PID;

// PID Share Begin
#include <sys/ipc.h>
#include <sys/shm.h>

typedef struct {
    float pv;
    float setpt;
    float prop_band;
    float dead_band;
    float integ_const;
    float diff_const;
    float bias;
    float integ_limit;
    float opp_ctrl;
    float clear;
    float result;
    float out;
    float sigma_err;
    float last_err;
    // pid pos
    float pos_v;
    float pos_setpt;
    float pos_output;
    float pos_err0;
    float pos_err1;
    float pos_err2;
} T_PID_SHARE;
static T_PID_SHARE * pid_share_data = NULL;
static unsigned int pid_shart_ctx = 0;
#define SHM_SIZE 128
enum { KEY_ID = 0x58474150 };
static int shmid;
static T_PID_SHARE *pid_share_get(void)
{
    if (pid_share_data == NULL) {
        shmid = shmget(KEY_ID, SHM_SIZE, IPC_CREAT | IPC_EXCL | 0644);
        if (shmid == -1) {
            if (errno == EEXIST) {
                shmid = shmget(KEY_ID, 0, 0);
            }
        }
        if (shmid != -1) {
            char *shmbuf = shmat(shmid, NULL, 0);
            if (shmbuf == (void*) -1L) {
                shmbuf = NULL;
            }
            if (shmbuf != NULL) {
                pid_share_data = (T_PID_SHARE *)shmbuf;
            }
        }
    }
    return pid_share_data;
}
// PID Share End

Cell PID_Pid_a(SedonaVM* vm, Cell* params)
{
    Cell ret;

    char *p = malloc(sizeof(T_PID));

    if (p != NULL) {
        memset(p, 0, sizeof(T_PID));
        ret.ival = (int)p;
    } else {
        ret.ival = 0;
    }

// PID Share Begin
    if (pid_shart_ctx == 0) {
        pid_shart_ctx = (unsigned int)p;
    }
// PID Share End
    return ret;
}

Cell PID_Pid_f(SedonaVM* vm, Cell* params)
{
    T_PID *ctx = (T_PID *)params[1].ival;
    float v = params[2].fval;
    float setpt = params[3].fval;
    float kp = params[4].fval / 1000;
    float ki = params[5].fval / 1000;
    float kd = params[6].fval / 1000;
    float dead_band = params[7].fval;
    float opp_ctrl = params[8].fval;
    float clear = params[9].fval;
    float out_plus = 0.0f;

    Cell ret;
    ret.fval = 0.0f;

    if (ctx != NULL) {
        if (clear != 0.0f) {
            ctx->output = 0.0f;
            ctx->err0 = 0.0f;
            ctx->err1 = 0.0f;
            ctx->err2 = 0.0f;
        }
        if (opp_ctrl != 0.0f) {
            ctx->err0 = setpt - v;
        } else {
            ctx->err0 = v - setpt;
        }
        if (fabs(ctx->err0) < (dead_band/2.0f)) {
        } else {
            out_plus = kp * (ctx->err0 - ctx->err1) + kd * (ctx->err0 - 2 * ctx->err1 + ctx->err2);
            if (!((ctx->output >= 1.0f && ctx->err0 > 0) || (ctx->output <= 0.0f && ctx->err0 < 0))) {
                out_plus += ki * ctx->err0;
            }
            ctx->output += out_plus;
            if (ctx->output < 0.0f) {
                ctx->output = 0.0f;
            }
            if (ctx->output > 1.0f) {
                ctx->output = 1.0f;
            }
        }
        ret.fval = ctx->output * 100.0f;
        ctx->err2 = ctx->err1;
        ctx->err1 = ctx->err0;
    }

// PID Share Begin
    if (pid_shart_ctx == (unsigned int)ctx) {
        T_PID_SHARE * pid_s = pid_share_get();
        if (pid_s != NULL) {
            pid_s->pos_v = v;
            pid_s->pos_setpt = setpt;
            pid_s->pos_output = ctx->output;
            pid_s->pos_err0 = ctx->err0;
            pid_s->pos_err1 = ctx->err1;
            pid_s->pos_err2 = ctx->err2;
        }
    }
// PID Share End
    return ret;
}

Cell PID_Pid_d(SedonaVM* vm, Cell* params)
{
    char *p = (char *)params[1].ival;

    if (p != NULL) {
        free(p);
    }
// PID Share Begin
    if (pid_shart_ctx == (unsigned int)p) {
        pid_shart_ctx = 0;
    }
// PID Share Begin

    Cell ret;
    ret.ival = 0;
    return ret;
}
