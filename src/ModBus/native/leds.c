#include <stdio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>

#define GPIO_IOC_MAGIC   'G'
#define IOCTL_GPIO_SETPINMUX              _IOW(GPIO_IOC_MAGIC, 0, int)
#define IOCTL_GPIO_REVPINMUX              _IOW(GPIO_IOC_MAGIC, 1, int)
#define IOCTL_GPIO_SETVALUE               _IOW(GPIO_IOC_MAGIC, 2, int)
#define IOCTL_GPIO_GETVALUE               _IOR(GPIO_IOC_MAGIC, 3, int)
#define IOCTL_GPIO_GETVALUE_RT            _IOR(GPIO_IOC_MAGIC, 5, int)

#define GPIO_DRV_NAME "/dev/gpio"

static pthread_t ctx_thread;
static int is_thread_init = 0;
static int fd = -1;

struct as9260_gpio_arg {
    int port;
    int pin;
    int data;
    int pinmuxback;
};

#define LED_BLINK_DELAY_S 0
#define LED_BLINK_DELAY_US 1000
struct led_table {
    int port;
    int pin;
    int pinmuxback;
    int state;
    unsigned long last_s;
    unsigned long last_us;
};

#define LED_COUNT 4
static struct led_table table[LED_COUNT] = {
        {5, 0, 0, -1, 0, 0},
        {5, 2, 0, -1, 0, 0},
        {5, 1, 0, -1, 0, 0},
        {5, 3, 0, -1, 0, 0},
};

static void led_init(void)
{
    struct as9260_gpio_arg localArg;

    fd = open(GPIO_DRV_NAME, O_RDWR);
    if(fd < 0){
        printf("led open err !\n");
        return;
    }

    int i;
    int ret;
    struct timeval tv;
    gettimeofday(&tv, NULL);

    for(i=0;i<LED_COUNT;i++){
        localArg.port = table[i].port;
        localArg.pin = table[i].pin;
        ret = ioctl(fd, IOCTL_GPIO_SETPINMUX, &localArg);
        if(ret < 0){
            printf("ioctl error ! \n");
        }
        table[i].pinmuxback = localArg.pinmuxback;

        localArg.data = 0;
        ret = ioctl(fd, IOCTL_GPIO_SETVALUE, &localArg);
        if(ret < 0){
            printf("ioctl error ! \n");
        }
        table[i].last_s = tv.tv_sec + 1;
        table[i].last_us = tv.tv_usec;
    }
    is_thread_init = 1;
}

static void* thread_led(void* arg)
{
    int i;
    struct timeval tv;
    struct as9260_gpio_arg localArg;

    while(1){
        gettimeofday(&tv, NULL);
        unsigned long long tn = tv.tv_sec * 1000000 + tv.tv_usec;
        for(i=0;i<LED_COUNT;i++){
            if(table[i].state != 1){
                unsigned long long t = table[i].last_s * 1000000 + table[i].last_us;
                if(tn > t || t - tn > 1000000){
                    localArg.port = table[i].port;
                    localArg.pin = table[i].pin;
                    localArg.data = 1;
                    ioctl(fd, IOCTL_GPIO_SETVALUE, &localArg);
                    table[i].state = 1;
                }
            }
        }
        usleep(1000);
    }

    pthread_exit(NULL);
    return (void*)NULL;
}

void led_blink(int led_idx)
{
    if(is_thread_init == 0){
        led_init();
        pthread_create(&ctx_thread, NULL, thread_led, 0);
    } else {
        struct as9260_gpio_arg localArg;
        struct timeval tv;

        gettimeofday(&tv, NULL);

        localArg.port = table[led_idx].port;
        localArg.pin = table[led_idx].pin;
        localArg.data = 0;
        ioctl(fd, IOCTL_GPIO_SETVALUE, &localArg);

        table[led_idx].last_s = tv.tv_sec + LED_BLINK_DELAY_S;
        table[led_idx].last_us = tv.tv_usec + LED_BLINK_DELAY_US;
        table[led_idx].state = 0;
    }
}

void led_on(int led_idx)
{
    struct as9260_gpio_arg localArg;
    localArg.port = table[led_idx].port;
    localArg.pin = table[led_idx].pin;
    localArg.data = 0;
    ioctl(fd, IOCTL_GPIO_SETVALUE, &localArg);
}

void led_off(int led_idx)
{
    struct as9260_gpio_arg localArg;
    localArg.port = table[led_idx].port;
    localArg.pin = table[led_idx].pin;
    localArg.data = 1;
    ioctl(fd, IOCTL_GPIO_SETVALUE, &localArg);
}

