#include "sedona.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

// PID
typedef struct {
    //struct timeval tv;
    float output;
    float sigma_err;
    float last_err;
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

Cell PID_PidBand_a(SedonaVM* vm, Cell* params)
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

Cell PID_PidBand_f(SedonaVM* vm, Cell* params)
{
    T_PID *ctx = (T_PID *)params[1].ival;
    float pv = params[2].fval;
    float setpt = params[3].fval;
    float prop_band = params[4].fval;
    float dead_band = params[5].fval;
    float integ_const = params[6].fval;
    float diff_const = params[7].fval;
    float bias = params[8].fval;
    float integ_limit = params[9].fval;
    float opp_ctrl = params[10].fval;
    float clear = params[11].fval;
    float result = 0.0f;
    Cell ret;
    ret.fval = -1.0f;

    if (ctx != NULL) {
        /*
        if (ctx->tv.tv_sec == 0) {
            gettimeofday(&ctx->tv, NULL);
            ctx->tv.tv_sec += 1;
        } else {
            struct timeval _tv;
            gettimeofday(&_tv, NULL);
            if (_tv.tv_sec > ctx->tv.tv_sec && _tv.tv_usec >= ctx->tv.tv_usec) {
                ctx->tv.tv_sec += 1;
            } else {
                result = ctx->output;
                if (result > 1.0f)
                    ret.fval = 100.0f;
                else if (result < 0)
                    ret.fval = 0.0f;
                else
                    ret.fval = result * 100.0f;
                return ret;
            }
        }
        */

        // pid
        float delta_err, err, hi_limit, pid_value;
        if (clear != 0.0f) {
            ctx->sigma_err = 0.0f;
            ctx->last_err = 0.0f;
        }
        err = pv - setpt;
        if (opp_ctrl != 0.0f) {
            err = setpt - pv;
        }
        if (fabs(err) < (dead_band/2.0f)) {
            result = ctx->output;
        } else {
            if (prop_band == 0.0f) {
                prop_band = 1.0f;
            }
            pid_value = err;
            if (integ_const > 0) {
                ctx->sigma_err += err;
                if (integ_limit > 0) {
                    hi_limit = integ_limit*prop_band*integ_const/100.0f;
                    if (ctx->sigma_err > hi_limit) {
                        ctx->sigma_err = hi_limit;
                    } else {
                        if (ctx->sigma_err < -hi_limit) {
                            ctx->sigma_err = -hi_limit;
                        }
                    }
                }
                pid_value += ctx->sigma_err/integ_const;
            }
            if (diff_const > 0) {
                delta_err= err - ctx->last_err;
                pid_value += delta_err * diff_const;
                ctx->last_err = err;
            }
            pid_value /= prop_band;
            pid_value = bias/100.0f + pid_value;
            ctx->output = pid_value;

            result = ctx->output;
        }

        // normalization
        if (result > 1.0f)
            ret.fval = 100.0f;
        else if (result < 0)
            ret.fval = 0.0f;
        else
            ret.fval = result * 100.0f;
    }

// PID Share Begin
    if (pid_shart_ctx == (unsigned int)ctx) {
        T_PID_SHARE * pid_s = pid_share_get();
        if (pid_s != NULL) {
            pid_s->pv = pv;
            pid_s->setpt = setpt;
            pid_s->prop_band = prop_band;
            pid_s->dead_band = dead_band;
            pid_s->integ_const = integ_const;
            pid_s->diff_const = diff_const;
            pid_s->bias = bias;
            pid_s->integ_limit = integ_limit;
            pid_s->opp_ctrl = opp_ctrl;
            pid_s->clear = clear;
            pid_s->result = result;
            pid_s->out = ret.fval;
            pid_s->sigma_err = ctx->sigma_err;
            pid_s->last_err = ctx->last_err;
        }
    }
// PID Share End
    return ret;
}

Cell PID_PidBand_d(SedonaVM* vm, Cell* params)
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
