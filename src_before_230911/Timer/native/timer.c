#include "sedona.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

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

Cell Timer_DelayOut_a(SedonaVM* vm, Cell* params)
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

Cell Timer_DelayOut_c(SedonaVM* vm, Cell* params)
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

Cell Timer_DelayOut_e(SedonaVM* vm, Cell* params)
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

Cell Timer_DelayOut_d(SedonaVM* vm, Cell* params)
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
