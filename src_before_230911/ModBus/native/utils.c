#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <errno.h>
#include <sys/time.h>

unsigned int get_tick_s(void)
{
    unsigned int ret = 0;
    struct timeval tv;

    if(sizeof(ret) == 4 && gettimeofday(&tv, NULL) == 0){
        ret = (unsigned int)tv.tv_sec;
    }

    return ret;
}

unsigned long long get_tick_ms(void)
{
    unsigned long long ret = 0;
    struct timeval tv;

    if(sizeof(ret) == 8 && gettimeofday(&tv, NULL) == 0){
        unsigned long long s = (unsigned long long)tv.tv_sec;
        unsigned long long us = (unsigned long long)tv.tv_usec;
        ret = s * 1000 + us / 1000;
    }

    return ret;
}

int date_check(unsigned int *date)
{
    int result = 0;

    int year = (*date / 10000) % 10000;
    int month = (*date / 100) % 100;
    int day = *date % 100;

    if (year < 2019) {
        year = 2019;
        result = 1;
    }

    if (year > 2099) {
        year = 2099;
        result = 1;
    }

    if (month < 1) {
        month = 1;
        result = 1;
    }

    if (month > 12) {
        month = 12;
        result = 1;
    }

    if (day < 1) {
        day = 1;
        result = 1;
    }

    if (day > 31) {
        day = 31;
        result = 1;
    }

    if (result == 1) {
        *date = (unsigned int)(year * 10000 + month * 100 + day);
    }

    return result;
}

int time_check(int *time)
{
    int result = 0;

    int hour = (*time / 10000) % 100;
    int min = (*time / 100) % 100;
    int sec = *time % 100;

    if (hour < 0) {
        hour = 0;
        result = 1;
    }

    if (hour > 23) {
        hour = 23;
        result = 1;
    }

    if (min < 0) {
        min = 0;
        result = 1;
    }

    if (min > 59) {
        min = 59;
        result = 1;
    }

    if (sec < 0) {
        sec = 0;
        result = 1;
    }

    if (sec > 59) {
        sec = 59;
        result = 1;
    }

    if (result == 1) {
        *time = hour * 10000 + min * 100 + sec;
    }

    return result;
}
