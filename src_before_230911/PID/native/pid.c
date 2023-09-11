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

    return ret;
}

Cell PID_PidBand_d(SedonaVM* vm, Cell* params)
{
    char *p = (char *)params[1].ival;

    if (p != NULL) {
        free(p);
    }

    Cell ret;
    ret.ival = 0;
    return ret;
}
