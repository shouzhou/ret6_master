/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-11-06     SummerGift   first version
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <buttonapp.h>
#include <led.h>
#include <screen.h>
#include "slave.h"
#include "m5310a.h"
#include "at24cxx.h"
extern struct key_state_type key0;
extern struct key_state_type key1;
extern int mb_master_samlpe(void);
int main(void)
{
    rt_uint16_t count =0;
	rt_kprintf("main function runing\r\n");
    bsp_setSlaveDefault();
    bsp_at24cxx_init();
    rt_thread_mdelay(6000);
    bsp_uart4_init();
    rt_kprintf("begin runing\r\n");
   // bsp_setSlaveDefault();
    mb_master_samlpe();
    bsp_m5310a_init();
	while(1)
	{
        count++;
        if(count<25) 
        {
            bsp_led_on(LED0);
        }
        else if(count<50)
        {
           bsp_led_off(LED0); 
        }
        else 
        {
           count =0;
         
        }
		bsp_key_process();
		//按键处理函数
		if(key0.down_state==KEY_PRESS)
		{
			rt_kprintf("key0 down press \r\n");
			key0.down_state=KEY_RELEASE;
            //void write_variable_store_82_1word(uint16_t address,uint16_t data) 
            write_variable_store_82_1word(0x10,0x01);            
		}
//		else if(key0.double_state==KEY_PRESS)
//		{
//			rt_kprintf("key0 double press \r\n");
//			key0.double_state=KEY_RELEASE;
//		}
//		else if(key0.long_state==KEY_PRESS)
//		{
//			rt_kprintf("key0 long press \r\n");
//			key0.long_state=KEY_RELEASE;
//		}
		
		else if(key1.down_state==KEY_PRESS)
		{
			rt_kprintf("key1 down press \r\n");
			key1.down_state=KEY_RELEASE;
             write_variable_store_82_1word(0x11,0x03); 
		}
//		else if(key1.double_state==KEY_PRESS)
//		{
//			rt_kprintf("key1 double press \r\n");
//			key1.double_state=KEY_RELEASE;
//		}			
//		else if(key1.long_state==KEY_PRESS)
//		{
//			rt_kprintf("key1 long press \r\n");
//			key1.long_state=KEY_RELEASE;
//		}	
		rt_thread_mdelay(20); 
	}
}
