#include "sedona.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

// PID
typedef struct {
    struct timeval tv;
    float output;
    float sigma_err;
    float last_err;
}T_PID;

Cell BasicFunc_Pid_a(SedonaVM* vm, Cell* params)
{
    Cell ret;

    char *p = malloc(sizeof(T_PID));

    if (p != NULL) {
        memset(p, 0, sizeof(T_PID));
        ret.ival = (int)p;
    } else {
        ret.ival = 0;
    }

    return ret;
}

Cell BasicFunc_Pid_f(SedonaVM* vm, Cell* params)
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

    return ret;
}

Cell BasicFunc_Pid_d(SedonaVM* vm, Cell* params)
{
    char *p = (char *)params[1].ival;

    if (p != NULL) {
        free(p);
    }

    Cell ret;
    ret.ival = 0;
    return ret;
}

// Delay Switch
typedef struct {
    unsigned int size;
    char *p_data;
    char *p_tv;
    unsigned int f_idx;
    unsigned int c_idx;
    float last;
}T_DSWI;

#define GROW_SIZE 100
#define SDWI_SIZE ((ctx->f_idx >= ctx->c_idx) ? (ctx->f_idx - ctx->c_idx) : (ctx->size + ctx->f_idx - ctx->c_idx))

Cell BasicFunc_DelaySwitch_a(SedonaVM* vm, Cell* params)
{
    Cell ret;
    float in = params[1].fval;

    char *p = malloc(sizeof(T_DSWI));

    if (p != NULL) {
        memset(p, 0, sizeof(T_DSWI));
        T_DSWI *ctx = (T_DSWI *)p;
        ctx->p_data = malloc((sizeof(float) + sizeof(struct timeval)) * GROW_SIZE);
        if (ctx->p_data)
        {
            ctx->p_tv = ctx->p_data + sizeof(float) * GROW_SIZE;
            ctx->size = GROW_SIZE;
            ctx->last = in;
            ret.ival = (int)p;
        }
        else
        {
            free(p);
            ret.ival = 0;
        }
    } else {
        ret.ival = 0;
    }

    return ret;
}

Cell BasicFunc_DelaySwitch_c(SedonaVM* vm, Cell* params)
{
    T_DSWI *ctx = (T_DSWI *)params[1].ival;
    float in = params[2].fval;
    float delay_ms = params[3].fval;

    Cell ret;
    ret.fval = -1.0f;

    if (ctx)
    {
        if ((ctx->size - SDWI_SIZE) <= 1)
        {
            char *new_data = malloc((sizeof(float) + sizeof(struct timeval)) * (ctx->size + GROW_SIZE));
            if (new_data)
            {
                char *new_tv = new_data + sizeof(float) * (ctx->size + GROW_SIZE);
                if (ctx->f_idx > ctx->c_idx)
                {
                    memcpy(new_data, ctx->p_data + sizeof(float) * ctx->c_idx, sizeof(float) * (ctx->f_idx - ctx->c_idx));
                    memcpy(new_tv, ctx->p_tv + sizeof(struct timeval) * ctx->c_idx, sizeof(struct timeval) * (ctx->f_idx - ctx->c_idx));
                    ctx->f_idx = ctx->f_idx - ctx->c_idx;
                    ctx->c_idx = 0;
                }
                else
                {
                    memcpy(new_data, ctx->p_data + sizeof(float) * ctx->c_idx, sizeof(float) * (ctx->size - ctx->c_idx));
                    memcpy(new_data + sizeof(float) * (ctx->size - ctx->c_idx), ctx->p_data, sizeof(float) * (ctx->f_idx));
                    memcpy(new_tv, ctx->p_tv + sizeof(struct timeval) * ctx->c_idx, sizeof(struct timeval) * (ctx->size - ctx->c_idx));
                    memcpy(new_tv + sizeof(struct timeval) * (ctx->size - ctx->c_idx), ctx->p_tv, sizeof(struct timeval) * (ctx->f_idx));
                    ctx->f_idx = ctx->size + ctx->f_idx - ctx->c_idx;
                    ctx->c_idx = 0;
                }
                free(ctx->p_data);
                ctx->p_data = new_data;
                ctx->p_tv = new_tv;
                ctx->size = ctx->size + GROW_SIZE;
            }
        }
        {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            long a = tv.tv_usec + ((long)delay_ms * 1000);
            long b = a / 1000000;
            tv.tv_usec = a - b * 1000000;
            tv.tv_sec = tv.tv_sec + b;
            memcpy(ctx->p_data + sizeof(float) * ctx->f_idx, &in, sizeof(float));
            memcpy(ctx->p_tv + sizeof(struct timeval) * ctx->f_idx, &tv, sizeof(struct timeval));
            ctx->f_idx ++;
            if (ctx->f_idx >= ctx->size)
            {
                ctx->f_idx = 0;
            }
        }
    }

    return ret;
}

Cell BasicFunc_DelaySwitch_e(SedonaVM* vm, Cell* params)
{
    T_DSWI *ctx = (T_DSWI *)params[1].ival;

    Cell ret;
    ret.fval = -1.0f;

    if (ctx)
    {
        if (SDWI_SIZE > 0)
        {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            struct timeval *p_tv = (struct timeval *)(ctx->p_tv + sizeof(struct timeval) * ctx->c_idx);
            if (tv.tv_sec > p_tv->tv_sec || (tv.tv_sec == p_tv->tv_sec && tv.tv_usec > p_tv->tv_usec))
            {
                float *v = (float *)(ctx->p_data + sizeof(float) * ctx->c_idx);
                ctx->last = *v;
                ctx->c_idx ++;
                if (ctx->c_idx >= ctx->size)
                {
                    ctx->c_idx = 0;
                }
            }
        }
        ret.fval = ctx->last;
    }

    return ret;
}

Cell BasicFunc_DelaySwitch_d(SedonaVM* vm, Cell* params)
{
    char *p = (char *)params[1].ival;

    if (p != NULL) {
        T_DSWI *ctx = (T_DSWI *)p;
        free(ctx->p_data);
        free(p);
    }

    Cell ret;
    ret.ival = 0;
    return ret;
}
