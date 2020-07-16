#ifndef __QSDK_LED__H_
#define __QSDK_LED__H_

#include "rtthread.h"
#include "board.h"

#define LED0 GET_PIN(A, 5)


#define LED_ON	PIN_HIGH
#define LED_OFF  PIN_LOW

//定义led 状态结构体
struct led_state_type
{
	int current_state;
	int last_state;
};

void bsp_led_on(rt_base_t pin);
void bsp_led_off(rt_base_t pin);



#endif

