/*
 * File      : qsdk_callback.c
 * This file is part of callback in qsdk
 * Copyright (c) 2018-2030, longmain Development Team
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-11-14     longmain     first version
 * 2018-12-13     longmain     update fun
 * 2019-06-13     longmain     add net close callback
 */

#include "qsdk.h"
#include "stdio.h"
#include "stdlib.h"

#ifdef RT_USING_ULOG
#define LOG_TAG              "[QSDK/CALLBACK]"
#ifdef QSDK_USING_LOG
#define LOG_LVL              LOG_LVL_DBG
#else
#define LOG_LVL              LOG_LVL_INFO
#endif
#include <ulog.h>
#else
#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME              "[QSDK/CALLBACK]"
#ifdef QSDK_USING_LOG
#define DBG_LEVEL                      DBG_LOG
#else
#define DBG_LEVEL                      DBG_INFO
#endif /* QSDK_DEBUG */

#include <rtdbg.h>
#endif


#include "qsdk_beep.h"
#include "qsdk_led.h"

//����EVENT �¼�
#define NB_REBOOT			(1<<1)
#define SEND_ERROR		(1<<2)
#define NET_CLOSE			(1<<3)
#define ONENET_CLOSE  (1<<4)
#define UPDATE_ERROR	(1<<5)

//����led ���ƿ�
extern struct led_state_type led0;
extern struct led_state_type led1;
extern struct led_state_type led2;
extern struct led_state_type led3;

//������ʪ�Ȼ������
extern float humidity;
extern float temperature;

#ifdef QSDK_USING_NET
//����net client
extern qsdk_net_client_t net_client;
#elif (defined QSDK_USING_IOT)||(defined QSDK_USING_ONENET)
//����ƽ̨�ϱ���־
extern rt_uint16_t update_status;
#if (defined QSDK_USING_ONENET)
//����onenet stream
extern qsdk_onenet_stream_t temp_object;
extern qsdk_onenet_stream_t hump_object;
extern qsdk_onenet_stream_t light0_object;
extern qsdk_onenet_stream_t light1_object;
extern qsdk_onenet_stream_t light2_object;
#endif


#endif

//���ô����¼����
extern rt_event_t net_error;

/****************************************************
* �������ƣ� qsdk_rtc_set_time_callback
*
* �������ã� RTC���ûص�����
*
* ��ڲ����� year�����		month: �·�		day: ����
*
*							hour: Сʱ		min: ����		sec: ��		week: ����
*
* ����ֵ�� 0 ����ɹ�	1 ����ʧ��
*****************************************************/
void qsdk_rtc_set_time_callback(int year,char month,char day,char hour,char min,char sec,char week)
{ 
#ifdef RT_USING_RTC
	set_date(year,month,day);
	set_time(hour,min,sec);
#endif
}

/*************************************************************
*	�������ƣ�	qsdk_net_close_callback
*
*	�������ܣ�	TCP�쳣�Ͽ��ص�����
*
*	��ڲ�����	��
*
*	���ز�����	��
*
*	˵����		
*************************************************************/
void qsdk_net_close_callback(void)
{
	LOG_E("now the network is abnormally disconnected\r\n");
	
#ifdef QSDK_USING_NET
		rt_event_send(net_error,NET_CLOSE);
#endif
	
}
/****************************************************
* �������ƣ� qsdk_net_data_callback
*
* �������ã� TCP/UDP �������·����ݻص�����
*
* ��ڲ�����	data: �����׵�ַ
*
*							len: ���ݳ���
*
* ����ֵ�� 0 ����ɹ�	1 ����ʧ��
*****************************************************/
int qsdk_net_data_callback(char *data,int len)
{
	LOG_I("enter net callback\r\n");
#ifdef QSDK_USING_NET
	LOG_D("net client rev data=%d,%s\r\n",len,data);
	
	if(rt_strstr(data,"cmd:")!=RT_NULL)
	{
		//����Ƿ���led0 ����ָ��
		if(rt_strstr(data,"led0:1")!=RT_NULL)
		{
			led0.current_state=LED_ON;
		}
		else if(rt_strstr(data,"led0:0")!=RT_NULL)
		{
			led0.current_state=LED_OFF;
		}
		
		//����Ƿ���led1 ����ָ��
		if(rt_strstr(data,"led1:1")!=RT_NULL)
		{
			led1.current_state=LED_ON;
		}
		else if(rt_strstr(data,"led1:0")!=RT_NULL)
		{
			led1.current_state=LED_OFF;
		}
		
		//����Ƿ���led2 ����ָ��
		if(rt_strstr(data,"led2:1")!=RT_NULL)
		{
			led2.current_state=LED_ON;
		}
		else if(rt_strstr(data,"led2:0")!=RT_NULL)
		{
			led2.current_state=LED_OFF;
		}
		
		//����Ƿ���led3 ����ָ��
		if(rt_strstr(data,"led3:1")!=RT_NULL)
		{
			led3.current_state=LED_ON;
		}
		else if(rt_strstr(data,"led3:0")!=RT_NULL)
		{
			led3.current_state=LED_OFF;
		}
		
		//����Ƿ���beep ����ָ��
		if(rt_strstr(data,"beep:1")!=RT_NULL)
		{
			qsdk_beep_on();
		}
		else if(rt_strstr(data,"beep:0")!=RT_NULL)
		{
			qsdk_beep_off();
		}
		
		//����Ƿ���sht20 ��ȡָ��
		if(rt_strstr(data,"read:sht20")!=RT_NULL)
		{
			char *buf=rt_calloc(1,len*2+50);
			if(buf==RT_NULL)
			{
				LOG_E("net create resp buf error\r\n");
			}
			else
			{
				sprintf(buf,"Now the board temperature is:%0.2f,humidity is:%0.2f",temperature,humidity);
				qsdk_net_send_data(net_client,buf);
			
			}
			rt_free(buf);
		}
	}
#endif
	return RT_EOK;
}

/****************************************************
* �������ƣ� qsdk_iot_data_callback
*
* �������ã� IOTƽ̨�·����ݻص�����
*
* ��ڲ����� data���·������׵�ַ
*
*							len	:	�·����ݳ���
*
* ����ֵ�� 0 ����ɹ�	1 ����ʧ��
*****************************************************/
int qsdk_iot_data_callback(char *data,int len)
{
	LOG_I("enter iot callback\r\n");
	
#ifdef QSDK_USING_IOT
	LOG_D("rev data=%d,%s\r\n",len,data);
	if(rt_strstr(data,"cmd:")!=RT_NULL)
	{
		//����Ƿ���led0 ����ָ��
		if(rt_strstr(data,"led0:1")!=RT_NULL)
		{
			led0.current_state=LED_ON;
		}
		else if(rt_strstr(data,"led0:0")!=RT_NULL)
		{
			led0.current_state=LED_OFF;
		}
		
		//����Ƿ���led1 ����ָ��
		if(rt_strstr(data,"led1:1")!=RT_NULL)
		{
			led1.current_state=LED_ON;
		}
		else if(rt_strstr(data,"led1:0")!=RT_NULL)
		{
			led1.current_state=LED_OFF;
		}
		
		//����Ƿ���led2 ����ָ��
		if(rt_strstr(data,"led2:1")!=RT_NULL)
		{
			led2.current_state=LED_ON;
		}
		else if(rt_strstr(data,"led2:0")!=RT_NULL)
		{
			led2.current_state=LED_OFF;
		}
		
		//����Ƿ���led3 ����ָ��
		if(rt_strstr(data,"led3:1")!=RT_NULL)
		{
			led3.current_state=LED_ON;
		}
		else if(rt_strstr(data,"led3:0")!=RT_NULL)
		{
			led3.current_state=LED_OFF;
		}
		
		//����Ƿ���beep ����ָ��
		if(rt_strstr(data,"beep:1")!=RT_NULL)
		{
			qsdk_beep_on();
		}
		else if(rt_strstr(data,"beep:0")!=RT_NULL)
		{
			qsdk_beep_off();
		}
		
		//����Ƿ���sht20 ��ȡָ��
		if(rt_strstr(data,"read:sht20")!=RT_NULL)
		{
			update_status=10;
		}
	}
#endif
	return RT_EOK;
}	
/****************************************************
* �������ƣ� qsdk_onenet_close_callback
*
* �������ã� onenetƽ̨ǿ�ƶϿ����ӻص�����
*
* ��ڲ����� ��
*
* ����ֵ�� 0 ����ɹ�	1 ����ʧ��
*****************************************************/
int qsdk_onenet_close_callback()
{
	LOG_I("enter close onenent callback\r\n");
#ifdef QSDK_USING_ONENET
	rt_event_send(net_error,ONENET_CLOSE);
#endif
	return RT_EOK;
}
/****************************************************
* �������ƣ� qsdk_onenet_read_rsp_callback
*
* �������ã� onenetƽ̨ read�����ص�����
*
* ��ڲ����� msgid����ϢID	insid��instance id	resid: resource id
*
* ����ֵ�� 0 ����ɹ�	1 ����ʧ��
*****************************************************/
int qsdk_onenet_read_rsp_callback(int msgid,int insid,int resid)
{
	LOG_I("enter read dsp callback\r\n");
#ifdef QSDK_USING_ONENET
	if(insid== -1&& resid== -1)
	{
		//�ж��Ƿ�Ϊ��ȡ�¶�ֵ
		if(qsdk_onenet_get_object_read(temp_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,temp_object,0,(qsdk_onenet_value_t)&temperature,0,0);
		}		
		//�ж��Ƿ�Ϊ��ȡʪ��ֵ
		else if(qsdk_onenet_get_object_read(hump_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,hump_object,0,(qsdk_onenet_value_t)&humidity,0,0);
		}
		//�ж��Ƿ�Ϊ��ȡlight
		else if(qsdk_onenet_get_object_read(light0_object)==RT_EOK)
		{
			if(qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,light0_object,0,(qsdk_onenet_value_t)&led0.current_state,2,1)==RT_ERROR)
			{
				LOG_E("onenet read rsp light0 error\r\n");
				return RT_ERROR;
			}
			if(qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,light1_object,0,(qsdk_onenet_value_t)&led1.current_state,1,2)==RT_ERROR)
			{
				LOG_E("onenet read rsp light1 error\r\n");
				return RT_ERROR;
			}
			if(qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,light2_object,0,(qsdk_onenet_value_t)&led2.current_state,0,0)==RT_ERROR)
			{
				LOG_E("onenet read rsp light2 error\r\n");
				return RT_ERROR;
			}
		}
	}
	else if(resid== -1)
	{
		//�ж��Ƿ�Ϊ��ȡ�¶�ֵ
		if(qsdk_onenet_get_object_read(temp_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,temp_object,0,(qsdk_onenet_value_t)&temperature,0,0);
		}
		//�ж��Ƿ�Ϊ��ȡʪ��ֵ
		else if(qsdk_onenet_get_object_read(hump_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,hump_object,0,(qsdk_onenet_value_t)&humidity,0,0);
		}
		//�ж��Ƿ�Ϊ��ȡled0 ״̬
		else if(qsdk_onenet_get_object_read(light0_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,light0_object,0,(qsdk_onenet_value_t)&led0.current_state,0,0);
		}
		//�ж��Ƿ�Ϊ��ȡled1 ״̬
		else if(qsdk_onenet_get_object_read(light1_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,light1_object,0,(qsdk_onenet_value_t)&led1.current_state,0,0);
		}
		//�ж��Ƿ�Ϊ��ȡled2 ״̬
		else if(qsdk_onenet_get_object_read(light2_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,light2_object,0,(qsdk_onenet_value_t)&led2.current_state,0,0);
		}
	}
	else
	{
		//�ж��Ƿ�Ϊ��ȡ�¶�ֵ
		if(qsdk_onenet_get_object_read(temp_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,temp_object,0,(qsdk_onenet_value_t)&temperature,0,0);
		}
		//�ж��Ƿ�Ϊ��ȡʪ��ֵ
		else if(qsdk_onenet_get_object_read(hump_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,hump_object,0,(qsdk_onenet_value_t)&humidity,0,0);
		}
		//�ж��Ƿ�Ϊ��ȡled0 ״̬
		else if(qsdk_onenet_get_object_read(light0_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,light0_object,0,(qsdk_onenet_value_t)&led0.current_state,0,0);
		}
		//�ж��Ƿ�Ϊ��ȡled1 ״̬
		else if(qsdk_onenet_get_object_read(light1_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,light1_object,0,(qsdk_onenet_value_t)&led1.current_state,0,0);
		}
		//�ж��Ƿ�Ϊ��ȡled2 ״̬
		else if(qsdk_onenet_get_object_read(light2_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,light2_object,0,(qsdk_onenet_value_t)&led2.current_state,0,0);
		}
	}
#endif
	return RT_EOK;
}
/****************************************************
* �������ƣ� qsdk_onenet_write_rsp_callback
*
* �������ã� onenetƽ̨ write�����ص�����
*
* ��ڲ����� len:	��Ҫд������ݳ���
*
*						 value:	��Ҫд�����������
*
* ����ֵ�� 0 ����ɹ�	1 ����ʧ��
*****************************************************/
int qsdk_onenet_write_rsp_callback(int len,char* value)
{
	LOG_I("enter write dsp callback\r\n");	
#ifdef QSDK_USING_ONENET
	if(qsdk_onenet_get_object_write(light0_object)==RT_EOK)
	{
		led0.current_state=atoi(value);
	}
	else if(qsdk_onenet_get_object_write(light1_object)==RT_EOK)
	{
		led1.current_state=atoi(value);
	}
	else if(qsdk_onenet_get_object_write(light2_object)==RT_EOK)
	{
		led2.current_state=atoi(value);
	}
#endif
	return RT_EOK;
}
/****************************************************
* �������ƣ� qsdk_onenet_exec_rsp_callback
*
* �������ã� onenetƽ̨ exec�����ص�����
*
* ��ڲ����� len:	ƽ̨exec�����·����ݳ���
*
*						 cmd:	ƽ̨exec�����·���������
*
* ����ֵ�� 0 ����ɹ�	1 ����ʧ��
*****************************************************/
int qsdk_onenet_exec_rsp_callback(int len,char* cmd)
{
	LOG_I("enter exec dsp callback\r\n");
#ifdef QSDK_USING_ONENET
	if(rt_strstr(cmd,"cmd:")!=RT_NULL)
	{
		//����Ƿ���led0 ����ָ��
		if(rt_strstr(cmd,"led0:1")!=RT_NULL)
		{
			led0.current_state=LED_ON;
		}
		else if(rt_strstr(cmd,"led0:0")!=RT_NULL)
		{
			led0.current_state=LED_OFF;
		}
		
		//����Ƿ���led1 ����ָ��
		if(rt_strstr(cmd,"led1:1")!=RT_NULL)
		{
			led1.current_state=LED_ON;
		}
		else if(rt_strstr(cmd,"led1:0")!=RT_NULL)
		{
			led1.current_state=LED_OFF;
		}
		
		//����Ƿ���led2 ����ָ��
		if(rt_strstr(cmd,"led2:1")!=RT_NULL)
		{
			led2.current_state=LED_ON;
		}
		else if(rt_strstr(cmd,"led2:0")!=RT_NULL)
		{
			led2.current_state=LED_OFF;
		}
		
		//����Ƿ���led3 ����ָ��
		if(rt_strstr(cmd,"led3:1")!=RT_NULL)
		{
			led3.current_state=LED_ON;
		}
		else if(rt_strstr(cmd,"led3:0")!=RT_NULL)
		{
			led3.current_state=LED_OFF;
		}
		
		//����Ƿ���beep ����ָ��
		if(rt_strstr(cmd,"beep:1")!=RT_NULL)
		{
			qsdk_beep_on();
		}
		else if(rt_strstr(cmd,"beep:0")!=RT_NULL)
		{
			qsdk_beep_off();
		}
		
		//����Ƿ���sht20 ��ȡָ��
		if(rt_strstr(cmd,"read:sht20")!=RT_NULL)
		{
			update_status=10;
		}
	}
#endif
	return RT_EOK;
}



/****************************************************
* �������ƣ� qsdk_onenet_fota_callback
*
* �������ã� onenet ƽ̨FOTA�����ص�����
*
* ��ڲ����� ��
*
* ����ֵ�� 	 ��
*****************************************************/
void qsdk_onenet_fota_callback(void)
{
	LOG_I("enter fota callback\r\n");
}

/****************************************************
* �������ƣ� qsdk_mqtt_data_callback
*
* �������ã� MQTT �������·����ݻص�����
*
* ��ڲ�����topic������    mesg��ƽ̨�·���Ϣ    mesg_len���·���Ϣ����
*
* ����ֵ�� 	0 ����ɹ�	1 ����ʧ��
*****************************************************/


int qsdk_mqtt_data_callback(char *topic,char *mesg,int mesg_len)
{
	LOG_I("enter mqtt callback  mesg:%s,len:%d\r\n",mesg,mesg_len);

	return RT_EOK;
}


/****************************************************
* �������ƣ� qsdk_nb_reboot_callback
*
* �������ã� nb-iotģ�����⸴λ�ص�����
*
* ��ڲ�������
*
* ����ֵ��  ��
*****************************************************/
void qsdk_nb_reboot_callback(void)
{
	LOG_I("enter reboot callback\r\n");
	
	if(rt_event_send(net_error,NB_REBOOT)==RT_ERROR)
		LOG_I("event reboot send error\r\n");
}





