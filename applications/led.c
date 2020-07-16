#include "led.h"

int bsp_led_init(void)
{
	rt_pin_mode(LED0,PIN_MODE_OUTPUT);
	rt_pin_write(LED0,PIN_HIGH);
	

	
	return RT_EOK;
}

void bsp_led_on(rt_base_t pin)
{
    rt_pin_write(pin,LED_ON);
}


void bsp_led_off(rt_base_t pin)
{
	rt_pin_write(pin,LED_OFF);
}

INIT_APP_EXPORT(bsp_led_init);
