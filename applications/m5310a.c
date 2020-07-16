

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>

#include "qsdk_led.h"
#include "qsdk.h"
#include "at24cxx.h"
#include "screen.h"
#include <at.h>
#include "m5310a.h"



//设置ulog 参数
#define LOG_TAG              "[main]"
//调试期间开启
#define LOG_LVL              LOG_LVL_DBG

//正式期间开启此项，关闭上方的调试选项
//#define LOG_LVL              LOG_LVL_INFO
#include <ulog.h>

//定义EVENT 事件
#define EVENT_NB_REBOOT			(1<<1)
#define EVENT_SEND_ERROR		(1<<2)
#define EVENT_NET_CLOSE			(1<<3)
#define EVENT_ONENET_CLOSE  (1<<4)
#define EVENT_UPDATE_ERROR	(1<<5)

//定义网络错误事件句柄
rt_event_t net_error=RT_NULL;

//定义线程控制句柄
rt_thread_t data_up_thread_id=RT_NULL;

//static rt_thread_t sht20_thread_id=RT_NULL;
//static rt_thread_t key_thread_id=RT_NULL;
static rt_thread_t net_thread_id=RT_NULL;

//预定义线程运行函数

//void sht20_entry(void *parameter);
//void key_entry(void *parameter);
void net_entry(void *parameter);
void data_up_entry(void *parameter);

//定义onenet stream
qsdk_onenet_stream_t temp_object;
qsdk_onenet_stream_t hump_object;
qsdk_onenet_stream_t light0_object;
qsdk_onenet_stream_t light1_object;
qsdk_onenet_stream_t light2_object;







//定义温湿度缓存变量
float humidity=00.0;
float temperature=00.0;

//定义平台读数据标志
rt_uint16_t update_status;

extern int mb_master_samlpe(void);
 

extern at_client_t nb_client;
extern at_response_t nb_resp;
void bsp_m5310a_init(void)
{
   
	//创建事件 
	net_error=rt_event_create("net_error",RT_IPC_FLAG_FIFO);
	if(net_error==RT_NULL)
	{
		LOG_E("create net error event failure\r\n");
	}
    		
	//创建net 线程
	net_thread_id=rt_thread_create("net_entry",
																 net_entry,
																 RT_NULL,
																 1500,
																 10,
																 20);
		if(net_thread_id!=RT_NULL)
			rt_thread_startup(net_thread_id);
		else
			LOG_E("failure\r\n");

	  //创建data up 线程
	  data_up_thread_id=rt_thread_create("data_up_entry",
																 data_up_entry,
																 RT_NULL,
																 1000,
																 12,
																 50);
		if(data_up_thread_id==RT_NULL)
			LOG_E("failure\r\n");
        
        
   
    
}

void net_entry(void *parameter)
{
	rt_uint16_t recon_count=0,recon_onenet_count=0;
	rt_uint32_t event_status=0;
net_recon:	
	//nb-iot模块快快速初始化联网
	if(qsdk_nb_quick_connect()!=RT_EOK)
	{
		LOG_E("module init failure\r\n");	
		goto net_recon;
	}
		//如需测试PSM模式可以打开   进入PSM模式之后需要在finsh 输入qsdk_nb exit_psm 来退出PSM
		//qsdk_nb_set_psm_mode(1,"01101010","00000101");
onenet_recon:	
	//onenet stream 初始化
	temp_object=qsdk_onenet_object_init(3303,0,5700,1,"1",1,0,qsdk_onenet_value_float);
	if(temp_object==RT_NULL)
	{
		LOG_E("temp object create failure\r\n");
	}
	hump_object=qsdk_onenet_object_init(3304,0,5700,1,"1",1,0,qsdk_onenet_value_float);
	if(hump_object==RT_NULL)
	{
		LOG_E("hump object create failure\r\n");
	}
	light0_object=qsdk_onenet_object_init(3311,0,5850,3,"111",3,0,qsdk_onenet_value_bool);
	if(light0_object==RT_NULL)
	{
		LOG_E("light object create failure\r\n");
	}
		light1_object=qsdk_onenet_object_init(3311,1,5850,3,"111",3,0,qsdk_onenet_value_bool);
	if(light1_object==RT_NULL)
	{
		LOG_E("light object create failure\r\n");
	}
		light2_object=qsdk_onenet_object_init(3311,2,5850,3,"111",3,0,qsdk_onenet_value_bool);
	if(light2_object==RT_NULL)
	{
		LOG_E("light object create failure\r\n");
	}
	
	//快速连接到onenet
	if(qsdk_onenet_quick_start()!=RT_EOK)
	{
		recon_onenet_count++;
		LOG_E("onenet quick start failure\r\n");
		rt_thread_delay(30000);
		if(qsdk_onenet_open()!=RT_EOK)
		{
			qsdk_onenet_delete_instance();
			goto onenet_recon;
		}
	}
//	qsdk_onenet_notify(light0_object,1,(qsdk_onenet_value_t)&led0.current_state,0);
//	qsdk_onenet_notify(light1_object,1,(qsdk_onenet_value_t)&led1.current_state,0);
//	qsdk_onenet_notify(light2_object,1,(qsdk_onenet_value_t)&led2.current_state,0);
	//连接网络成功，beep提示一声
//	qsdk_beep_on();
//	rt_thread_delay(500);
//	qsdk_beep_off();
	if(recon_count==0)
		rt_thread_startup(data_up_thread_id);
	else
	{
		//如果在PSM模式，需要挂起线程
		if(qsdk_nb_get_psm_status()!=RT_EOK)
		{
			rt_thread_resume(data_up_thread_id);
		}
		recon_count=0;
	//	led3.current_state=LED_OFF;
	}

	while (1)
	{
		//等待网络错误事件
		if(rt_event_recv(net_error,EVENT_SEND_ERROR|EVENT_UPDATE_ERROR|EVENT_ONENET_CLOSE|EVENT_NB_REBOOT,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,RT_WAITING_FOREVER,&event_status)==RT_EOK)
		{
			//如果在PSM模式，需要挂起线程
			if(qsdk_nb_get_psm_status()!=RT_EOK)
			{
				//首先挂起数据上报线程
				rt_thread_suspend(data_up_thread_id);
			}
			
	//		led3.current_state=LED_ON;
			
			//判断当前是否为数据发送错误或者更新在线时间错误
			if(event_status==EVENT_SEND_ERROR||event_status==EVENT_UPDATE_ERROR)
			{
				if(qsdk_nb_wait_connect()!=RT_EOK)
				{
					LOG_E("Failure to communicate with module now\r\n");
					recon_count=1;
					qsdk_nb_reboot();
					goto net_recon;
				}
				if(qsdk_nb_get_net_connect()!=RT_EOK)
				{
					LOG_E("Now Internet has failed\r\n");
					recon_count=1;
					qsdk_nb_reboot();
					goto net_recon;
				}
				if(qsdk_onenet_get_connect()!=RT_EOK)
				{
				  LOG_E("Now the onenet link is disconnected\r\n");
					if(qsdk_onenet_open()!=RT_EOK)
					{
						LOG_E("Now onenet reconnection\r\n");
						qsdk_onenet_delete_instance();
						recon_count=1;
						goto onenet_recon;
					}
				}			
			}
			//判断当前是否为onenet异常关闭
			else if(event_status==EVENT_ONENET_CLOSE)
			{
				LOG_E("Now the onenet link is disconnected\r\n");
				if(qsdk_onenet_open()!=RT_EOK)
				{
					LOG_E("Now onenet reconnection\r\n");
					qsdk_onenet_delete_instance();
					recon_count=1;
					goto onenet_recon;
				}
			}
			//判断当前是否为模组异常重启
			else if(event_status==EVENT_NB_REBOOT)
			{
				if(qsdk_nb_get_net_connect_status()!=RT_EOK)
				{
					LOG_E("Now the nb-iot is reboot,will goto init\r\n");
					recon_count=1;
					goto net_recon;	
				}
			}
		//	led3.current_state=LED_OFF;
			rt_thread_resume(data_up_thread_id);	
		}
        
		
	}
}

void data_up_entry(void *parameter)
{
	int lifetime=0;
	char *buf=rt_calloc(1,200);
	if(buf==RT_NULL)
	{
		LOG_E("net create buf error\r\n");
	}
	while(1)
	{
        humidity = humidity +0.01;
        temperature = temperature +0.01;
		//上报数据前首先判断当前lwm2m 协议是否正常连接
		if(qsdk_onenet_get_connect()==RT_EOK)
		{
			LOG_D("data UP is open\r\n");
retry:			
			if(qsdk_onenet_notify(temp_object,0,(qsdk_onenet_value_t)&temperature,0)!=RT_EOK)
			{
				LOG_E("onenet notify error\r\n");
				rt_event_send(net_error,EVENT_SEND_ERROR);
				goto retry;
			}
			rt_thread_delay(100);
			if(qsdk_onenet_notify(hump_object,0,(qsdk_onenet_value_t)&humidity,0)!=RT_EOK)
			{
				LOG_E("onenet notify error\r\n");
				rt_event_send(net_error,EVENT_SEND_ERROR);
				goto retry;
			}
			
			//每40分钟定时更新一次(设备在线时间) lwm2m lifetime
            //#define QSDK_ONENET_LIFE_TIME 3000  3000/60 = 50分钟
			if(lifetime>10)
			{
retry_update:
				if(qsdk_onenet_update_time(0)!=RT_EOK)
				{
					LOG_E("onenet lifetime update error");
					rt_thread_delay(5000);
					if(qsdk_onenet_update_time(0)!=RT_EOK)
					{
						LOG_E("Two Update Errors in OneNet Lifetime");
						rt_event_send(net_error,EVENT_UPDATE_ERROR);
						goto retry_update;
					}
				}
				lifetime=0;
			}
		}
		else LOG_E("Now lwm2m is no connect\r\n");
		rt_memset(buf,0,sizeof(buf));
		lifetime++;
		//如需测试PSM模式可以打开   进入PSM模式之后需要在finsh 输入qsdk_nb exit_psm 来退出PSM
		qsdk_nb_enter_psm();
        // at_obj_exec_cmd(nb_client,nb_resp,"AT");
        //at_obj_exec_cmd(nb_client,nb_resp,"AT+CPSMS=0");
		rt_thread_delay(180000);
        
      
	}
}
