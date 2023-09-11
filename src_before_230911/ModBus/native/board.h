#define MAX_MODBUSRTU_NUM 2
#define MAX_MODBUSTCPMASTER_NUM 1
#define MAX_MODBUSTCPSLAVE_NUM 1

int acquire_uart(int idx, char *name);
void release_uart(int idx);

void led_blink(int led_idx);
void led_on(int led_idx);
void led_off(int led_idx);
