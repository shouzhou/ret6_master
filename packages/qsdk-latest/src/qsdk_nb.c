/*
 * File      : qsdk_nb.c
 * This file is part of nb in qsdk
 * Copyright (c) 2018-2030, longmain Development Team
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-11-14     longmain     first version
 * 2019-04-19     longmain     v1.0.0
 * 2019-05-09     longmain     add m5311 module
 * 2019-05-19     longmain     add psm mode
 * 2019-06-12     longmain     Separated qsdk nb quick connect to net
 * 2019-06-13     longmain     add hexstring to string
 * 2019-06-13     longmain     qsdk environment is auto init by the system app
 * 2019-06-30     longmain     add qsdk_nb_clear_environment
 * 2019-08-30     longmain     Adding PSM support function
 * 2019-09-19     longmain     Adding wakeup_out function
 */

#include "qsdk.h"
#include <at.h>

#ifdef RT_USING_ULOG
#define LOG_TAG              "[QSDK/NB]"
#ifdef QSDK_USING_LOG
#define LOG_LVL              LOG_LVL_DBG
#else
#define LOG_LVL              LOG_LVL_INFO
#endif
#include <ulog.h>
#else
#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME              "[QSDK/NB]"
#ifdef QSDK_USING_LOG
#define DBG_LEVEL                      DBG_LOG
#else
#define DBG_LEVEL                      DBG_INFO
#endif /* QSDK_DEBUG */

#include <rtdbg.h>
#endif

//����event�¼�
#define EVENT_REBOOT 						(1<< 1)
#define EVENT_PSM_UNLOOK_AT		 	        (1<< 2)
#define EVENT_ENTER_PSM 				    (1<< 3)
#define EVENT_EXIT_PSM 					    (1<< 4)
#define EVENT_EXIT_PSM_REBOOT 	            (1<< 5)
#define EVENT_ENTER_RUN 			        (1<< 6)
#define EVENT_PING_OK						(1<< 7)
#define EVENT_PING_ERROR				    (1<< 8)
#define EVENT_GPS_DOWN_OK				    (1<< 9)
#define EVENT_GPS_DOWN_FAILED		        (1<<10)

//����ҵ������
#ifdef QSDK_USING_NET
extern int qsdk_net_clear_environment(void);
extern void net_event_func(char *event);
#endif

#ifdef QSDK_USING_IOT
extern void iot_event_func(char *event);
#endif
			
#ifdef QSDK_USING_ONENET
extern void onenet_event_func(char *event);
extern int qsdk_onenet_clear_environment(void);

#endif

#ifdef QSDK_USING_MQTT
extern void mqtt_event_func(char *event);
#endif

#ifdef QSDK_USING_ME3616_GPS
extern void qsdk_gps_event_func(char *event);
#endif

enum nb_reboot_type
{
	NB_MODULE_NO_INIT=0,
	NB_MODULE_INIT_SUCCESS=4,
	NB_MODULE_REBOOT_BY_PIN=8,
	NB_MODULE_REBOOT_BY_AT=12,
	NB_MODULE_REBOOT_BY_FOTA=16,
	NB_MODULE_REBOOT_BY_OTHER=20,
	NB_MODULE_REBOOT_FOR_PIN=30,
	NB_MODULE_REBOOT_FOR_AT=31,
	NB_MODULE_REBOOT_FOR_FOTA=32,
	NB_MODULE_REBOOT_CLOSE=35
};

struct nb_pin_mode
{
	int pwr_high;
	int pwr_low;
	int rst_high;
	int rst_low;
	int wake_in_high;
	int wake_in_low;
};


//���� NBģ��������Ӧ�ṹ��ָ��
at_client_t nb_client = RT_NULL;
at_response_t nb_resp=RT_NULL;

//����������ƿ�
rt_thread_t qsdk_thread_id=RT_NULL;
rt_thread_t qsdk_psm_id=RT_NULL;

//���� ����
void qsdk_thread_entry(void* parameter);

//����������ƿ�
static rt_mailbox_t nb_mail=RT_NULL;

//�����¼����ƿ�
rt_event_t nb_event=RT_NULL;

static struct nb_pin_mode nb_pin={0};
struct nb_device nb_device_table={0};
static 	int  nbiot_psm_status=0;

/*************************************************************
*	�������ƣ�	nb_io_init
*
*	�������ܣ�	NB-IOT ģ��������ų�ʼ��
*
*	��ڲ�����	��
*
*	���ز�����	0:�ɹ�  1:ʧ��
*
*	˵����		
*************************************************************/
static int nb_io_init(void)
{
#ifdef QSDK_USING_PWRKEY
	rt_pin_mode(QSDK_PWRKEY_PIN,PIN_MODE_OUTPUT);
#endif
	rt_pin_mode(QSDK_RESET_PIN,PIN_MODE_OUTPUT);

	//�������� pwrkey �����ϵ�
#ifdef QSDK_USING_PWRKEY
	rt_pin_write(QSDK_PWRKEY_PIN,nb_pin.pwr_high);
#endif
	rt_pin_write(QSDK_RESET_PIN,nb_pin.rst_high);
	
#if	(defined QSDK_USING_M5311)||(defined QSDK_USING_ME3616)	
	rt_pin_mode(QSDK_WAKEUP_IN_PIN,PIN_MODE_OUTPUT);
	rt_pin_write(QSDK_WAKEUP_IN_PIN,nb_pin.wake_in_high);
	rt_thread_delay(2000);
	rt_pin_write(QSDK_PWRKEY_PIN,nb_pin.pwr_low);
#endif
	rt_thread_delay(500);
	rt_pin_write(QSDK_RESET_PIN,nb_pin.rst_low);

	
	return RT_EOK;
}
#if	(defined QSDK_USING_M5311)||(defined QSDK_USING_ME3616)
/*************************************************************
*	�������ƣ�	nb_io_exit_psm
*
*	�������ܣ�	NB-IOT ģ���˳�psmģʽ
*
*	��ڲ�����	��
*
*	���ز�����	0:�ɹ�  1:ʧ��
*
*	˵����		
*************************************************************/
static int nb_io_exit_psm(void)
{
	rt_pin_write(QSDK_WAKEUP_IN_PIN,nb_pin.wake_in_low);
	rt_thread_delay(200);
	rt_pin_write(QSDK_WAKEUP_IN_PIN,nb_pin.wake_in_high);
	rt_thread_delay(500);

	return RT_EOK;
}

static void nb_wakeup_enable_irq(void)
{
	rt_pin_irq_enable(QSDK_WAKEUP_OUT_PIN,1);
}
static void nb_wakeup_disable_irq(void)
{
	rt_pin_irq_enable(QSDK_WAKEUP_OUT_PIN,0);
}

static void nb_wakeup_event(void *args)
{
	nb_wakeup_disable_irq();
	if(nbiot_psm_status==qsdk_nb_status_enter_psm)
	{
		rt_event_send(nb_event,EVENT_EXIT_PSM);
	}
}

static void nb_wakeup_init(void)
{
	rt_pin_mode(QSDK_WAKEUP_OUT_PIN,PIN_MODE_INPUT);
	if(QSDK_WAKEUP_OUT_PIN_VALUE==1)
	{
		rt_pin_attach_irq(QSDK_WAKEUP_OUT_PIN,PIN_IRQ_MODE_RISING,nb_wakeup_event,RT_NULL);
	}
	else
	{
		rt_pin_attach_irq(QSDK_WAKEUP_OUT_PIN,PIN_IRQ_MODE_FALLING,nb_wakeup_event,RT_NULL);
	}
}
#endif
/*************************************************************
*	�������ƣ�	qsdk_nb_clear_environment
*
*	�������ܣ�	���NBģ�鵱ǰ״̬
*
*	��ڲ�����	��
*
*	���ز�����	��
*
*	˵����		
*************************************************************/
void qsdk_nb_clear_environment(void)
{
	rt_memset(&nb_device_table,0,sizeof(nb_device_table));
}
/*************************************************************
*	�������ƣ�	qsdk_nb_reboot
*
*	�������ܣ�	NB-IOT ģ������
*
*	��ڲ�����	��
*
*	���ز�����	0:�ɹ�  1:ʧ��
*
*	˵����		
*************************************************************/
int qsdk_nb_reboot(void)
{
	rt_uint32_t status;
	rt_memset(&nb_device_table,0,sizeof(nb_device_table));
	nb_device_table.reboot_open=NB_MODULE_REBOOT_FOR_PIN;
	nb_device_table.reboot_type=NB_MODULE_NO_INIT;
	nb_device_table.net_connect_ok=NB_MODULE_NO_INIT;
	LOG_D("now nb-iot rebooting by pin");
	rt_pin_write(QSDK_RESET_PIN,nb_pin.rst_high);
	rt_thread_delay(500);
	rt_pin_write(QSDK_RESET_PIN,nb_pin.rst_low);
	
	if(rt_event_recv(nb_event,EVENT_REBOOT,RT_EVENT_FLAG_AND|RT_EVENT_FLAG_CLEAR,20000,&status)!=RT_EOK)
	{
		LOG_E("nb-iot reboot error\n");
		return  RT_ERROR;
	}
	rt_thread_delay(1000);
	nb_device_table.reboot_type=NB_MODULE_NO_INIT;
	nb_device_table.reboot_open=0;
	if(qsdk_nb_wait_connect()!=RT_EOK)
	{
		LOG_E("nb-iot reboot success,but connect nb-iot error\n");
		return  RT_ERROR;
	}
	return RT_EOK;
}
/*************************************************************
*	�������ƣ�	qsdk_nb_wait_connect
*
*	�������ܣ�	���ģ���Ƿ�����
*
*	��ڲ�����	��
*
*	���ز�����	0:����   1:ģ���쳣
*
*	˵����		
*************************************************************/
int qsdk_nb_wait_connect(void)
{
	if(at_client_obj_wait_connect(nb_client,2000)!=RT_EOK)
	{
		at_delete_resp(nb_resp);							//ɾ�� AT ������Ӧ�ṹ��
		LOG_E("nb-iot module connect error\n");
		return RT_ERROR;
	}

	return RT_EOK;
}

/*************************************************************
*	�������ƣ�	nb_sim_check
*
*	�������ܣ�	���ģ���Ƿ�Ϊ����ģʽ
*
*	��ڲ�����	��
*
*	���ز�����	0���ɹ�   1����С����ģʽ
*
*	˵����		
*************************************************************/
int qsdk_nb_sim_check(void)
{
#if (defined QSDK_USING_DEBUG)
	LOG_D("AT+CFUN?\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CFUN?")!=RT_EOK)	return RT_ERROR;
	at_resp_parse_line_args(nb_resp,2,"+CFUN:%d",&nb_device_table.sim_state);

	return  RT_EOK;
}

/*************************************************************
*	�������ƣ�	nb_get_imsi
*
*	�������ܣ�	��ȡ SIM ���� imsi 
*
*	��ڲ�����	��
*
*	���ز�����	IMSIָ��:�ɹ�    RT_NULL:ʧ��
*
*	˵����		
*************************************************************/
char *qsdk_nb_get_imsi(void)
{
#if (defined QSDK_USING_DEBUG)
	LOG_D("AT+CIMI\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CIMI")!=RT_EOK)	return RT_NULL;	
	
	at_resp_parse_line_args(nb_resp,2,"%s\r\n",nb_device_table.imsi);
	return  nb_device_table.imsi;
}
/*************************************************************
*	�������ƣ�	nb_get_imei
*
*	�������ܣ�	��ȡģ��� imei 
*
*	��ڲ�����	��
*
*	���ز�����	IMEIָ��:�ɹ�    RT_NULL:ʧ��
*
*	˵����		
*************************************************************/
char *qsdk_nb_get_imei(void)
{
#if (defined QSDK_USING_DEBUG)
	LOG_D("AT+CGSN=1\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CGSN=1")!=RT_EOK)	return RT_NULL;	
	
	at_resp_parse_line_args(nb_resp,2,"+CGSN:%s",nb_device_table.imei);
	return  nb_device_table.imei;
}
/*************************************************************
*	�������ƣ�	nb_set_rtc_time
*
*	�������ܣ�	����RTCʱ��Ϊ��ǰʱ��
*
*	��ڲ�����	year:��   month:��	day:��	hour:ʱ	min:��	sec:��
*
*	���ز�����	��
*
*	˵����		
*************************************************************/
static void qsdk_nb_set_rtc_time(int year,int month,int day,int hour,int min,int sec)
{
	int week,lastday;
	hour+=QSDK_TIME_ZONE;
	if ((0==year%4 && 0!=year%100) || 0==year%400)
       	lastday=29;
    else if(month==1||month==3||month==5||month==7||month==8||month==10||month==12)
    	lastday=31;
    else if(month==4||month==6||month==9||month==11)
   		 lastday=30;
     else
       	lastday=28;
   	if(hour>24)
   	{
   		hour-=24;
   		day++;
   		if(day>lastday)
   		{
   			day-=lastday;
   			month++;  			
   		}
   		if(month>12)
   		{
   			month-=12;	
   			year++;
   		}
   	}	
		week=(day+2*month+3*(month+1)/5+year+year/4-year/100+year/400)%7+1;
#ifndef QSDK_USING_ME3616
		year+=2000;
#endif
		
#ifdef QSDK_USING_LOG
		LOG_D("��ǰʱ��:%d-%d-%d,%d-%d-%d,����:%d\n",year,month,day,hour,min,sec,week);
#endif	
		qsdk_rtc_set_time_callback(year,month,day,hour,min,sec,week);

}
/*************************************************************
*	�������ƣ�	qsdk_nb_get_time
*
*	�������ܣ�	��ȡ����ʱ��
*
*	��ڲ�����	��
*
*	���ز�����	0:�ɹ�  1:ʧ��
*
*	˵����		
*************************************************************/
int qsdk_nb_get_time(void)
{
	int year,mouth,day,hour,min,sec;
#if (defined QSDK_USING_DEBUG)
	LOG_D("AT+CCLK?\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CCLK?")!=RT_EOK)	return RT_ERROR;	
	
	at_resp_parse_line_args(nb_resp,2,"+CCLK:%d/%d/%d,%d:%d:%d+",&year,&mouth,&day,&hour,&min,&sec);
	
	qsdk_nb_set_rtc_time(year,mouth,day,hour,min,sec);
	return  RT_EOK;
}

/*************************************************************
*	�������ƣ�	qsdk_nb_get_csq
*
*	�������ܣ�	��ȡ��ǰ�ź�ֵ
*
*	��ڲ�����	��
*
*	���ز�����	0-99:�ɹ�    -1:ʧ��
*
*	˵����		
*************************************************************/
int qsdk_nb_get_csq(void)
{
#if (defined QSDK_USING_DEBUG)
	LOG_D("AT+CSQ\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CSQ")!=RT_EOK)	return -1;	
	
	at_resp_parse_line_args(nb_resp,2,"+CSQ:%d\r\n",&nb_device_table.csq);
	return  nb_device_table.csq;

}
/*************************************************************
*	�������ƣ�	nb_set_net_start
*
*	�������ܣ�	�ֶ���������
*
*	��ڲ�����	��
*
*	���ز�����	0:�ɹ�  1:ʧ��
*
*	˵����		
*************************************************************/
int qsdk_nb_set_net_start(void)
{
#if (defined QSDK_USING_DEBUG)
	LOG_D("AT+CGATT=1\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CGATT=1")!=RT_EOK)	return RT_ERROR;
	
	return  RT_EOK;
}
/*************************************************************
*	�������ƣ�	qsdk_nb_get_net_connect
*
*	�������ܣ�	��ȡ��ǰ����״̬
*
*	��ڲ�����	��
*
*	���ز�����	0:�ɹ�  1:ʧ��
*
*	˵����		
*************************************************************/
int qsdk_nb_get_net_connect(void)
{
	int i;
#if (defined QSDK_USING_DEBUG)
	LOG_D("AT+CEREG?\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CEREG?")!=RT_EOK)
	{
		return  RT_ERROR;
	}
	at_resp_parse_line_args_by_kw(nb_resp,"+CEREG:","+CEREG:%d,%d",&i,&nb_device_table.net_connect_ok);

	if(nb_device_table.net_connect_ok==1||nb_device_table.net_connect_ok==5) 
	{
		return RT_EOK;
	}
	else 
	{
		return  RT_ERROR;
	}
}
/*************************************************************
*	�������ƣ�	qsdk_nb_get_net_connect_status
*
*	�������ܣ�	��ȡ��ѯ��������״̬
*
*	��ڲ�����	��
*
*	���ز�����	0:�ɹ�  1:ʧ��
*
*	˵����		
*************************************************************/
int qsdk_nb_get_net_connect_status(void)
{
	if(nb_device_table.net_connect_ok) return RT_EOK;

	return RT_ERROR;
}
/*************************************************************
*	�������ƣ�	qsdk_nb_get_reboot_event
*
*	�������ܣ�	��ȡnb-iotģ������ԭ��
*
*	��ڲ�����	��
*
*	���ز�����	1:���Ÿ�λ   2:AT��λ		3:FOTA��λ		4���쳣��λ
*
*	˵����		
*************************************************************/
int qsdk_nb_get_reboot_event(void)
{
	if(nb_device_table.reboot_type==NB_MODULE_REBOOT_BY_PIN)		return 1;
	else if(nb_device_table.reboot_type==NB_MODULE_REBOOT_BY_AT)		return 2;
	else if(nb_device_table.reboot_type==NB_MODULE_REBOOT_BY_FOTA)	return 3;
	else 		return 4;
}
/*************************************************************
*	�������ƣ�	nb_set_psm_mode
*
*	�������ܣ�	ģ�� PSM ģʽ����
*
*	��ڲ�����	tau_time	TAU ʱ��		active_time activeʱ��
*
*	���ز�����	0:�ɹ�  1:ʧ��
*
*	˵����		
*************************************************************/
int qsdk_nb_set_psm_mode(int mode,char *tau_time,char *active_time)
{
	if(mode==0)
	{
#if (defined QSDK_USING_DEBUG)
	LOG_D("AT+CPSMS=0\n");
#endif
		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CPSMS=0")!=RT_EOK)	return RT_ERROR;		
	}
	else if(mode==1)
	{
#if (defined QSDK_USING_DEBUG)
	LOG_D("AT+CPSMS=1,,,\"%s\",\"%s\"\n",tau_time,active_time);
#endif
		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CPSMS=1,,,\"%s\",\"%s\"",tau_time,active_time)!=RT_EOK)	return RT_ERROR;	
#if (defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)||(defined QSDK_USING_ME3616)
		if(qsdk_nb_get_psm_status()!=RT_EOK)
		{
#if (defined QSDK_USING_DEBUG)
			LOG_D("AT+CPSMS=0\n");
#endif
			if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CPSMS=0")!=RT_EOK)	return RT_ERROR;	
		}
#endif
	}


	return  RT_EOK;
}
/*************************************************************
*	�������ƣ�	qsdk_nb_enter_psm
*
*	�������ܣ�	nb-iotģ�����PSMģʽ
*
*	��ڲ�����	��
*
*	���ز�����	��
*
*	˵����		
*************************************************************/
void qsdk_nb_enter_psm(void)
{
	rt_uint32_t status;
#ifdef QSDK_USING_DEBUG
	LOG_D("nb-iot will enter psm\n");
#endif
#if (defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)
	//����PSMģʽ
#if (defined QSDK_USING_DEBUG)
	LOG_D("AT+CPSMS=1\n");
#endif
	at_obj_exec_cmd(nb_client,nb_resp,"AT+CPSMS=1");	
#elif (defined QSDK_USING_ME3616)

	//����PSMģʽ
#if (defined QSDK_USING_DEBUG)
	LOG_D("AT+CPSMS=1\n");
#endif
	at_obj_exec_cmd(nb_client,nb_resp,"AT+CPSMS=1");
#if (defined QSDK_USING_DEBUG)
	LOG_D("AT+ZSLR\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+ZSLR")!=RT_EOK)
		LOG_E("nb-iot set psm cmd error\n");
#elif (defined QSDK_USING_M5311)
	qsdk_nb_open_auto_psm();
#if (defined QSDK_USING_DEBUG)
	LOG_D("AT*ENTERSLEEP\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT*ENTERSLEEP")!=RT_EOK)
		LOG_E("nb-iot set psm cmd error\n");
#endif
		//�ȴ��˳�PSM�¼�
		if(rt_event_recv(nb_event,EVENT_ENTER_RUN,RT_EVENT_FLAG_AND|RT_EVENT_FLAG_CLEAR,RT_WAITING_FOREVER,&status)!=RT_EOK)
		{
			LOG_E("nb-iot wait exit psm error\n");
		}
		
	LOG_I("nb-iot is exit psm mode\n");
}

/*************************************************************
*	�������ƣ�	qsdk_nb_exit_psm
*
*	�������ܣ�	nb-iotģ���˳�PSMģʽ
*
*	��ڲ�����	��
*
*	���ز�����	0:�ɹ�    1:ʧ��
*
*	˵����		
*************************************************************/
int qsdk_nb_exit_psm(void)
{
#if (defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)
	rt_event_send(nb_event,EVENT_EXIT_PSM);
#elif	(defined QSDK_USING_M5311)||(defined QSDK_USING_ME3616)
	if(nbiot_psm_status==qsdk_nb_status_enter_psm)
	{
		if(nb_io_exit_psm()!=RT_EOK)
		{
			LOG_E("nb-iot exit psm error\n");
			return RT_ERROR;
		}
	}
	else
	{
		rt_event_send(nb_event,EVENT_EXIT_PSM);
	}
#endif
	return  RT_EOK;
}
/*************************************************************
*	�������ƣ�	qsdk_nb_get_psm_status
*
*	�������ܣ�	��ѯnb-iotģ���Ƿ����psm ģʽ
*
*	��ڲ�����	��
*
*	���ز�����	0����ǰ��psmģʽ      1����ǰδ����psmģʽ
*
*	˵����		
*************************************************************/
int qsdk_nb_get_psm_status(void)
{
	if(nbiot_psm_status==qsdk_nb_status_exit_psm)
	{
		return  RT_ERROR;
	}
	return  RT_EOK;
}
/*************************************************************
*	�������ƣ�	nb_set_edrx_mode
*
*	�������ܣ�	ģ�� EDRX ģʽ����
*
*	��ڲ�����	tau_time	TAU ʱ��		active_time activeʱ��
*
*	���ز�����	0:�ɹ�  1:ʧ��
*
*	˵����		
*************************************************************/
int qsdk_nb_set_edrx_mode(int mode,int act_type,char *edex_value,char *ptw_time)
{
#if (defined QSDK_USING_M5311)

#if (defined QSDK_USING_LOG)
	LOG_D("AT+CEDRXS=%d,%d,\"%s\",\"%s\"\n",mode,act_type,edex_value,ptw_time);
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CEDRXS=%d,%d,\"%s\",\"%s\"",mode,act_type,edex_value,ptw_time)!=RT_EOK)	return RT_ERROR;
	
	if(mode==1)
	{
#if (defined QSDK_USING_LOG)
		LOG_D("AT*EDRXCFG=%d,%d,\"%s\",\"%s\"\r\n",mode,act_type,edex_value,ptw_time);
#endif
		if(at_obj_exec_cmd(nb_client,nb_resp,"AT*EDRXCFG=%d,%d,\"%s\",\"%s\"",mode,act_type,edex_value,ptw_time)!=RT_EOK)	return RT_ERROR;
		qsdk_nb_open_auto_psm();
	}
#endif
	return  RT_EOK;
}
/*************************************************************
*	�������ƣ�	nb_set_edrx_mode
*
*	�������ܣ�	ģ�� EDRX ģʽ����
*
*	��ڲ�����	tau_time	TAU ʱ��		active_time activeʱ��
*
*	���ز�����	0:�ɹ�  1:ʧ��
*
*	˵����		
*************************************************************/
int qsdk_nb_exit_edrx_mode(void)
{
#if (defined QSDK_USING_M5311)
	if(qsdk_nb_wait_connect()!=RT_EOK) return RT_ERROR;
#endif
	return  RT_EOK;
}
/*************************************************************
*	�������ƣ�	qsdk_nb_ping_ip
*
*	�������ܣ�	ping������IP
*
*	��ڲ�����	IP����Ҫping ��IP��ַ
*
*	���ز�����	0:�ɹ�  1:ʧ��
*
*	˵����		
*************************************************************/
int qsdk_nb_ping_ip(char *ip)
{
	rt_uint32_t status;
#if	(defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)	
#if (defined QSDK_USING_LOG)
	LOG_D("AT+NPING=%s\n",ip);
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NPING=%s",ip)!=RT_EOK) return RT_ERROR;
#elif (defined QSDK_USING_M5311)
#if (defined QSDK_USING_LOG)
	LOG_D("AT+PING=%s,,,1,,1\n",ip);
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+PING=%s,,,1,,1",ip)!=RT_EOK) return RT_ERROR;
#endif	
	
	rt_event_recv(nb_event,EVENT_PING_ERROR|EVENT_PING_OK,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,60000,&status);
	if(status==EVENT_PING_OK)
	{
		return RT_EOK;
	}	
	return RT_ERROR;
}

#if (defined QSDK_USING_M5311)||(defined QSDK_USING_ME3616)
/*************************************************************
*	�������ƣ�	qsdk_nb_set_rai_mode
*
*	�������ܣ�	����ģ��������ݷ��ͻ���պ��Ƿ���ٽ��� idle ̬
*
*	��ڲ�����	rai:
								0 �ر� RAI
								1 ������ 1 �����а���ģ�����̽��� idle ̬
								2 ���� 1 �����а����ҽ��յ� 1 �����а���ģ���������� idle ̬
*
*	���ز�����	0:�ɹ�    1:ʧ��
*
*	˵����		
*************************************************************/
int qsdk_nb_set_rai_mode(int rai)
{
#ifdef QSDK_USING_M5311
#if (defined QSDK_USING_LOG)
	LOG_D("AT*NBIOTRAI=%d\n",rai);
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT*NBIOTRAI=%d",rai)!=RT_EOK)	return RT_ERROR;
#else
#if (defined QSDK_USING_LOG)
	LOG_D("AT*MNBIOTRAI=%d\n",rai);
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT*MNBIOTRAI=%d",rai)!=RT_EOK)	return RT_ERROR;
#endif	
	return  RT_EOK;
}
/*************************************************************
*	�������ƣ�	qsdk_nb_open_net_light
*
*	�������ܣ�	������ָʾ��
*
*	��ڲ�����	��
*
*	���ز�����	0:�ɹ�    1:ʧ��
*
*	˵����		
*************************************************************/
int qsdk_nb_open_net_light(void)
{
#ifdef QSDK_USING_M5311
#if (defined QSDK_USING_LOG)
	LOG_D("AT+CMSYSCTRL=0,2\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CMSYSCTRL=0,2")!=RT_EOK)	return RT_ERROR;
#else
#if (defined QSDK_USING_LOG)
	LOG_D("AT+ZCONTLED=1\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+ZCONTLED=1")!=RT_EOK)	return RT_ERROR;
#endif	
	return  RT_EOK;
}
/*************************************************************
*	�������ƣ�	qsdk_nb_close_net_light
*
*	�������ܣ�	�ر�����ָʾ��
*
*	��ڲ�����	��
*
*	���ز�����	0:�ɹ�    1:ʧ��
*
*	˵����		
*************************************************************/
int qsdk_nb_close_net_light(void)
{
#ifdef QSDK_USING_M5311
#if (defined QSDK_USING_LOG)
	LOG_D("AT+CMSYSCTRL=0,0\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CMSYSCTRL=0,0")!=RT_EOK)	return RT_ERROR;
#else
#if (defined QSDK_USING_LOG)
	LOG_D("AT+ZCONTLED=0\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+ZCONTLED=0")!=RT_EOK)	return RT_ERROR;
#endif	
	return  RT_EOK;
}

#ifdef QSDK_USING_M5311

/*************************************************************
*	�������ƣ�	qsdk_nb_open_auto_psm
*
*	�������ܣ�	ģ�������Զ�����PSMģʽ����
*
*	��ڲ�����	��
*
*	���ز�����	0:�ɹ�    1:ʧ��
*
*	˵����		
*************************************************************/
int qsdk_nb_open_auto_psm(void)
{
#ifdef QSDK_USING_M5311
#if (defined QSDK_USING_LOG)
	LOG_D("AT+SM=UNLOCK\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+SM=UNLOCK")!=RT_EOK)	return RT_ERROR;
#endif	
	return  RT_EOK;
}
/*************************************************************
*	�������ƣ�	qsdk_nb_close_auto_psm
*
*	�������ܣ�	ģ��ر��Զ�����PSMģʽ����
*
*	��ڲ�����	��
*
*	���ز�����	0:�ɹ�    1:ʧ��
*
*	˵����		
*************************************************************/
int qsdk_nb_close_auto_psm(void)
{
#ifdef QSDK_USING_M5311
#if (defined QSDK_USING_LOG)
	LOG_D("AT+SM=LOCK\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+SM=LOCK")!=RT_EOK)	return RT_ERROR;
#endif
	return  RT_EOK;
}
#endif	
#elif		(defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)
/*************************************************************
*	�������ƣ�	nb_query_ip
*
*	�������ܣ�	��ѯģ���ں�������IP��ַ
*
*	��ڲ�����	��
*
*	���ز�����	IP: �ɹ�  	RT_NULL:ʧ��
*
*	˵����		
*************************************************************/
char *qsdk_nb_query_ip(void)
{
#if (defined QSDK_USING_DEBUG)
	LOG_D("AT+CGPADDR\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CGPADDR")!=RT_EOK) return RT_NULL;
	
	at_resp_parse_line_args(nb_resp,2,"+CGPADDR:0,%s",nb_device_table.ip);
	return nb_device_table.ip;
}
/*************************************************************
*	�������ƣ�	qsdk_iot_check_address
*
*	�������ܣ�	���iot��������ַ�Ƕ���ȷ
*
*	��ڲ�����	��
*
*	���ز�����	0:��ȷ  1:ʧ��
*
*	˵����		
*************************************************************/
int qsdk_iot_check_address(void)
{
	char str[50];
#if (defined QSDK_USING_DEBUG)
	LOG_D("AT+NCDP?\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NCDP?")!=RT_EOK) return RT_ERROR;
	
	at_resp_parse_line_args(nb_resp,2,"%s",str);


	LOG_D("%s\r\n",str);

#if (defined QSDK_USING_M5310A) &&(defined QSDK_USING_IOT)
	if(rt_strstr(str,QSDK_IOT_ADDRESS)!=RT_NULL) 		return RT_EOK;
#else
	if(rt_strstr(str,"+NCDP:0.0.0.0")!=RT_NULL) 		return RT_EOK;
#endif

	return RT_ERROR;
}
/*************************************************************
*	�������ƣ�	qsdk_iot_set_address
*
*	�������ܣ�	���� NCDP ������
*
*	��ڲ�����	��
*
*	���ز�����	0:�ɹ�  1:ʧ��
*
*	˵����		
*************************************************************/
int qsdk_iot_set_address(void)
{
	LOG_D("AT+CFUN=0\n");

	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CFUN=0")!=RT_EOK)
	{
		LOG_E("set AT+CFUN=0 error\n");
		return RT_ERROR;
	}
	rt_thread_delay(200);
	
#if (defined QSDK_USING_M5310A) &&(defined QSDK_USING_IOT)
	LOG_D("AT+NCDP=%s,%s\n",QSDK_IOT_ADDRESS,QSDK_IOT_PORT);
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NCDP=%s,%s",QSDK_IOT_ADDRESS,QSDK_IOT_PORT)!=RT_EOK)
#else
	LOG_D("AT+NCDP=0.0.0.0,5683\n");
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NCDP=0.0.0.0,5683")!=RT_EOK)
#endif
	{
		LOG_E("set ncdp address error\n");
		return RT_ERROR;
	}
	qsdk_iot_check_address();
	rt_thread_delay(2500);

	if(qsdk_nb_reboot()==RT_EOK)	return RT_EOK;

	return RT_ERROR;	
}
#endif
/*************************************************************
*	�������ƣ�	string_to_hex
*
*	�������ܣ�	�ַ���תhex
*
*	��ڲ�����	*pString:��ת���ַ���  len:��ת���ַ�������   *pHex:ת�����HEX�ַ���
*
*	���ز�����	0:�ɹ�  1:ʧ��
*
*	˵����		
*************************************************************/
int string_to_hex(const char *pString, int len, char *pHex)
{
    int i = 0;
    if (NULL == pString || len <= 0 || NULL == pHex)
    {
        return RT_ERROR;
    }
    for(i = 0; i < len; i++)
    {
        rt_sprintf(pHex+i*2, "%02X", pString[i]);
    }
    return RT_EOK;
}
/************************************************************
*	�������ƣ�	hexstring_to_string
*
*	�������ܣ�	16�����ַ���ת�ַ���
*
*	��ڲ�����	pHex: 16�����ַ���   len: pHex�ַ�������     pString��ת���õ��ַ���
*
*	���ز�����	��
*
*	˵����		
************************************************************/
void hexstring_to_string(char * pHex,int len, char * pString)
{
	int i=0,j=0;
	unsigned char temp[2];
	for ( i = 0; i <len; i++)
	{
			temp[0] = *pHex++;
			temp[1] = *pHex++;
			for (j = 0; j < 2; j++)
			{
					if (temp[j] <= 'F' && temp[j] >= 'A')
							temp[j] = temp[j] - 'A' + 10;
					else if (temp[j] <= 'f' && temp[j] >= 'a')
							temp[j] = temp[j] - 'a' + 10;
					else if (temp[j] >= '0' && temp[j] <= '9')
							temp[j] = temp[j] - '0';
			}   
			pString[i] = temp[0] << 4;   
			pString[i] |= temp[1]; 
	}   
}
/*************************************************************
*	�������ƣ�	nb_reboot_func
*
*	�������ܣ�	ģ�������·������¼�
*
*	��ڲ�����	��
*
*	���ز�����	��
*
*	˵����		
*************************************************************/
void nb_reboot_func(char *data)
{
	if(qsdk_nb_get_psm_status()==RT_EOK)
	{
		rt_event_send(nb_event,EVENT_EXIT_PSM_REBOOT);
	}
/************************M5310A��λ����*****************************/
#if	(defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)
	//�ж��Ƿ���δ�˳�PSMǰ��λ

	if(rt_strstr(data,"REBOOT_CAUSE_SECURITY_RESET_PIN")!=RT_NULL)
	{
		if(nb_device_table.reboot_open==NB_MODULE_REBOOT_FOR_PIN)
		{
			rt_event_send(nb_event,EVENT_REBOOT);
		}			
		else 
		{
			LOG_E("%s\n reboot by pin\n",data);
			nb_device_table.reboot_type=NB_MODULE_REBOOT_BY_PIN;
			qsdk_nb_reboot_callback();
		}
	}
	else 	if(rt_strstr(data,"REBOOT_CAUSE_APPLICATION_AT")!=RT_NULL)
	{	
		if(nb_device_table.reboot_open==NB_MODULE_REBOOT_FOR_AT)
		{
			rt_event_send(nb_event,EVENT_REBOOT);
		}
		else 
		{
			LOG_E("%s\n reboot by at\n",data);
			nb_device_table.reboot_type=NB_MODULE_REBOOT_BY_AT;	
			qsdk_nb_reboot_callback();
		}
	}
	else 	if(rt_strstr(data,"REBOOT_CAUSE_SECURITY_FOTA_UPGRADE")!=RT_NULL)
	{
		LOG_E("%s\n reboot by fota\n",data);

		if(nb_device_table.fota_open==NB_MODULE_REBOOT_FOR_FOTA) nb_device_table.fota_open=0;
		nb_device_table.reboot_type=NB_MODULE_REBOOT_BY_FOTA;
		qsdk_onenet_fota_callback();

	}
	else
	{
		LOG_E("%s\n reboot by other\n",data);

		nb_device_table.reboot_type=NB_MODULE_REBOOT_BY_OTHER;
		qsdk_nb_reboot_callback();
	}
#elif (defined QSDK_USING_M5311)

/************************M5311��λ����*****************************/
	if(rt_strstr(data,"*ATREADY:")!=RT_NULL)
	{
		if(nb_device_table.reboot_open==NB_MODULE_REBOOT_FOR_PIN||nb_device_table.reboot_open==NB_MODULE_REBOOT_FOR_AT)
		{
			rt_event_send(nb_event,EVENT_REBOOT);
		}			
		else
		{
			LOG_E("%s\n nb-iot reboot\n",data);
			qsdk_nb_reboot_callback();
		}
	}
#elif (defined QSDK_USING_ME3616)
	
/************************ME3616��λ����*****************************/
	if(rt_strstr(data,"*MATREADY:")!=RT_NULL)
	{
		if(nb_device_table.reboot_open==NB_MODULE_REBOOT_FOR_PIN||nb_device_table.reboot_open==NB_MODULE_REBOOT_FOR_AT)
		{
			rt_event_send(nb_event,EVENT_REBOOT);
		}			
		else
		{
			LOG_E("%s\n nb-iot reboot\n",data);
			qsdk_nb_reboot_callback();
		}
	}
#endif
		//���nbģ����Ϣ
		qsdk_nb_clear_environment();
#ifdef QSDK_USING_NET
		//���net client��Ϣ
		qsdk_net_clear_environment();
#endif
#ifdef QSDK_USING_ONENET
	qsdk_onenet_clear_environment();
#endif
}

/*************************************************************
*	�������ƣ�	qsdk_psm_entry
*
*	�������ܣ�	ģ�������ϱ����ݴ�����
*
*	��ڲ�����	��
*
*	���ز�����	��
*
*	˵����		
*************************************************************/
void qsdk_psm_entry(void* parameter)
{
	rt_uint32_t status;
	while(1)
	{
		//�ȴ�����PSMָʾ
		rt_event_recv(nb_event,EVENT_ENTER_PSM|EVENT_EXIT_PSM|EVENT_EXIT_PSM_REBOOT,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,RT_WAITING_FOREVER,&status);
		if(status==EVENT_ENTER_PSM)
		{
			//����QSDK�ڲ�����PSMָʾ
			nbiot_psm_status=qsdk_nb_status_enter_psm;

			//����wakeup_out�����ж�
#if	(defined QSDK_USING_M5311)||(defined QSDK_USING_ME3616)
			nb_wakeup_enable_irq();
#endif
			//��סAT��ֹͣ������AT����
			rt_mutex_take(nb_client->lock,RT_WAITING_FOREVER);
            
            
            ///////////////////
            
            qsdk_nb_exit_psm();
            
            
            /////////////////////////
		}
		else if(status==EVENT_EXIT_PSM||status==EVENT_EXIT_PSM_REBOOT)
		{
			if(status==EVENT_EXIT_PSM_REBOOT)
			{
				nbiot_psm_status=qsdk_nb_status_psm_run;
				//�ͷ�AT���ͷź������Է���AT����
				rt_mutex_release(nb_client->lock);
				
#if (defined QSDK_USING_M5310A)
				qsdk_nb_wait_connect();
#if (defined QSDK_USING_DEBUG)
				LOG_D("AT+CPSMS=0\n");
#endif
				at_obj_exec_cmd(nb_client,nb_resp,"AT+CPSMS=0");
#endif
			}
			if(nbiot_psm_status==qsdk_nb_status_enter_psm)
			{
				//�ͷ�AT���ͷź������Է���AT����
				rt_mutex_release(nb_client->lock);
				//ģ�黽�Ѻ���Ҫ�ر��Զ�������������
#if (defined QSDK_USING_M5311)
				qsdk_nb_close_auto_psm();
#endif
#if (defined QSDK_USING_M5310A)
				qsdk_nb_wait_connect();
#if (defined QSDK_USING_DEBUG)
				LOG_D("AT+CPSMS=0\n");
#endif
				at_obj_exec_cmd(nb_client,nb_resp,"AT+CPSMS=0");
#endif
			}
			//�����˳�PSMʱ��
			if(status==EVENT_EXIT_PSM)
			{
				if(qsdk_nb_get_psm_status()==RT_EOK)
				{
					//����QSDK�ڲ��˳�PSMָʾ
					nbiot_psm_status=qsdk_nb_status_exit_psm;
					rt_event_send(nb_event,EVENT_ENTER_RUN);
				}
			}
		}
      //  rt_thread_mdelay(1000);

	}
}
/*************************************************************
*	�������ƣ�	qsdk_thread_entry
*
*	�������ܣ�	ģ�������ϱ����ݴ�����
*
*	��ڲ�����	��
*
*	���ز�����	��
*
*	˵����		
*************************************************************/
void qsdk_thread_entry(void* parameter)
{
	rt_err_t status=RT_EOK;
	char *event;
	
	while(1)
	{
		//�ȴ��¼��ʼ� event_mail
		status=rt_mb_recv(nb_mail,(rt_ubase_t *)&event,RT_WAITING_FOREVER);
		//�ж��Ƿ���ճɹ�
		if(status==RT_EOK)
		{
#if	(defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)
/*********************��λ������*************************/
			if(rt_strstr(event,"REBOOT_")!=RT_NULL)
#elif (defined QSDK_USING_M5311)
			if(rt_strstr(event,"*ATREADY")!=RT_NULL)
#elif (defined QSDK_USING_ME3616)
			if(rt_strstr(event,"*MATREADY")!=RT_NULL)
#endif
			{
				nb_reboot_func(event);
			}
/*******************ping������*********************/
#if	(defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)||(defined QSDK_USING_M5311)
			
#if	(defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)
			else if(rt_strstr(event,"+NPING:")!=RT_NULL)
				
#elif (defined QSDK_USING_M5311)
			else if(rt_strstr(event,"+PING:")!=RT_NULL)
#endif
			{
				LOG_D("%s\r\n",event);
				rt_event_send(nb_event,EVENT_PING_OK);		
			}
#if (defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)
			else if(rt_strstr(event,"+NPINGERR:")!=RT_NULL)
//#endif
#elif	(defined QSDK_USING_M5311)
			else if(rt_strstr(event,"+PINGERR:")!=RT_NULL)	
#endif
			{
				LOG_D("%s\r\n",event);
				rt_event_send(nb_event,EVENT_PING_ERROR);
			}
#endif
/********************����PSM������*************************/
#if (defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)
			else if(rt_strstr(event,"+NPSMR:1")!=RT_NULL)
//#endif
#elif	(defined QSDK_USING_M5311)
			else if(rt_strstr(event,"*GOTOSLEEP")!=RT_NULL)	
#elif (defined QSDK_USING_ME3616)
			else if(rt_strstr(event,"*MNBIOTEVENT: \"ENTER PSM\"")!=RT_NULL)	
#endif
			{
				LOG_I("nb-iot enter psm\n");
#ifdef QSDK_USING_LOG
				LOG_D("%s\r\n",event);
#endif
				rt_event_send(nb_event,EVENT_ENTER_PSM);
                
               
			}
/*********************ME3616 GPS������**************************/
#if (defined QSDK_USING_ME3616_GPS)
			else if(rt_strstr(event,"+ZGPS:")!=RT_NULL)	
			{
				qsdk_gps_event_func(event);
			}
			else if(rt_strstr(event,"$GNRMC")!=RT_NULL)	
			{
				qsdk_gps_event_func(event);
			}
#endif
/*****************************************************************/
			else
			{
			//NET������
#ifdef QSDK_USING_NET
			net_event_func(event);
#endif
			//IOT������
#ifdef QSDK_USING_IOT
			iot_event_func(event);
#endif
			//ONENET������			
#ifdef QSDK_USING_ONENET
			onenet_event_func(event);
#endif
			//MQTT������
#ifdef QSDK_USING_MQTT
			mqtt_event_func(event);
#endif			
			}
			
		}
		else	LOG_E("event_mail recv error\n");
		// rt_thread_mdelay(500);

	}
}

/*************************************************************
*	�������ƣ�	qsdk_nb_event_func
*
*	�������ܣ�	ģ�������·������¼�
*
*	��ڲ�����	��
*
*	���ز�����	��
*
*	˵����		
*************************************************************/
void nb_event_func(struct at_client *client, const char *data, rt_size_t size)
{
	rt_err_t status;
	if(client==nb_client)
	{
		status=rt_mb_send(nb_mail,(rt_uint32_t)data);
		if(status!=RT_EOK)
			LOG_E("mb send error\n");
	}
	else LOG_E("nb event fun error\r\n");	
}


/*************************************************************
*
*	˵����		ģ�������·���Ϣʶ��ṹ��
*
*************************************************************/
static struct at_urc nb_urc_table[]={
//ģ�鿪���������
#if	(defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)
	{"REBOOT_",           "\r",nb_event_func},
	{"+NPING:",           "\r",nb_event_func},
	{"+NPINGERR:",           "\r",nb_event_func},
	//PSMָʾ
	{"+NPSMR:1",           "\r",nb_event_func},
//	{"+NPSMR:0",           "\r",nb_event_func},
//�������TCP/UDP֧�֣�����NET��������
#ifdef QSDK_USING_NET
	{"+NSONMI:",           "\r",nb_event_func},
	{"+NSOCLI:",           "\r",nb_event_func},
	{"CONNECT OK",           "\r",nb_event_func},
#endif
	
//�������iot֧�֣�����iot��������
#ifdef QSDK_USING_IOT
	{"+NNMI:",				 			"\r",nb_event_func},
	{"+NSMI:",							"\r",nb_event_func},
#endif
	
//�������mqtt֧�֣�����mqtt��������
#ifdef QSDK_USING_MQTT
	{"+MQTTOPEN:",				"\r",nb_event_func},
	{"+MQTTSUBACK:",			"\r",nb_event_func},
	{"+MQTTPUBLISH:",			"\r",nb_event_func},
	{"+MQTTPUBACK:",			"\r",nb_event_func},
	{"+MQTTUNSUBACK:",		"\r",nb_event_func},
	{"+MQTTDISC:",				"\r",nb_event_func},
	{"+MQTTPUBREC:",			"\r",nb_event_func},
	{"+MQTTPUBCOMP:",			"\r",nb_event_func},
	{"+MQTTPINGRESP:",		"\r",nb_event_func},
	{"+MQTTTO:",					"\r",nb_event_func},
	{"+MQTTREC:",					"\r",nb_event_func},
#endif

#elif (defined QSDK_USING_M5311)
	{"*ATREADY:",           "\r",nb_event_func},
	//PSMָʾ
	{"*GOTOSLEEP",           "\r",nb_event_func},
//�������TCP/UDP֧�֣�����NET��������
#ifdef QSDK_USING_NET
	{"+IPRD:",              "\r\n",nb_event_func},
	{"+IPCLOSE:",           "\r\n",nb_event_func},
	{"CONNECT OK",          "\r\n",nb_event_func},
#endif
#elif (defined QSDK_USING_ME3616)
	{"*MATREADY:",           "\r",nb_event_func},
	//PSMָʾ
	{"*MNBIOTEVENT: \"ENTER PSM\"",           "\r",nb_event_func},
#ifdef QSDK_USING_IOT
	{"+M2MCLI",				  	 "\r\n",nb_event_func},	
	{"+M2MCLIRECV",				 "\r\n",nb_event_func},		
#endif

#ifdef QSDK_USING_ME3616_GPS
	{"+ZGPS:",				  	 "\r\n",nb_event_func},	
  {"$GNRMC",				  	 "\r\n",nb_event_func},	
#endif
#endif	
	

//�������onenet֧�֣�����onenet��������
#ifdef QSDK_USING_ONENET
	{"+MIPLREAD:",         "\r",nb_event_func},
	{"+MIPLWRITE:",        "\r",nb_event_func},
	{"+MIPLEXECUTE:",      "\r",nb_event_func},
	{"+MIPLOBSERVE:",      "\r",nb_event_func},
	{"+MIPLDISCOVER:",     "\r",nb_event_func},
	{"+MIPLEVENT:",				"\r",nb_event_func},
#endif
};
/*************************************************************
*	�������ƣ�	qsdk_init_environment
*
*	�������ܣ�	QSDK ���л�����ʼ��
*
*	��ڲ�����	��
*
*	���ز�����	0���ɹ�   1��ʧ��
*
*	˵����		
*************************************************************/
int qsdk_init_environment(void)
{
	rt_uint32_t status;
	rt_device_t uart_device;
	struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT; 
	
	LOG_I("\r\nbegin to init m5310A enviroment\r\n");
	
	//set nb pin mode


#ifdef QSDK_USING_PWRKEY
	if(QSDK_PWRKEY_PIN_VALUE==PIN_HIGH)
	{
		nb_pin.pwr_high=PIN_HIGH;
		nb_pin.pwr_low=PIN_LOW;
	}
	else
	{
		nb_pin.pwr_high=PIN_LOW;
		nb_pin.pwr_low=PIN_HIGH;
	}
#endif
	if(QSDK_RESET_PIN_VALUE==PIN_HIGH)
	{
		nb_pin.rst_high=PIN_HIGH;
		nb_pin.rst_low=PIN_LOW;
	}
	else
	{
		nb_pin.rst_high=PIN_LOW;
		nb_pin.rst_low=PIN_HIGH;
	}
#if	(defined QSDK_USING_M5311)||(defined QSDK_USING_ME3616)

	if(QSDK_WAKEUP_IN_PIN_VALUE==PIN_HIGH)
	{
		nb_pin.wake_in_high=PIN_HIGH;
		nb_pin.wake_in_low=PIN_LOW;
	}
	else
	{
		nb_pin.wake_in_high=PIN_LOW;
		nb_pin.wake_in_low=PIN_HIGH;
	}
#endif
	//set uart buad
	uart_device=rt_device_find(QSDK_UART);

	config.baud_rate=QSDK_UART_BAUDRATE;
	rt_device_control(uart_device, RT_DEVICE_CTRL_CONFIG, &config);
	//at client init
	at_client_init(QSDK_UART,QSDK_CMD_REV_MAX_LEN*2+100);
	//create at resp
	nb_client = at_client_get(QSDK_UART);
	nb_resp=at_create_resp(QSDK_CMD_REV_MAX_LEN*2+100,0,5000);
	if (nb_resp == RT_NULL)
	{
		LOG_E("create at resp error\n");
		return RT_ERROR;
	}	
	at_obj_set_urc_table(nb_client,nb_urc_table,sizeof(nb_urc_table)/sizeof(nb_urc_table[0]));
	//create mail
	nb_mail=rt_mb_create("qsdk_mb",
													10,
													RT_IPC_FLAG_FIFO);
	if(nb_mail==RT_NULL)
	{
		LOG_E("create mail error\n");
		return RT_ERROR;
	}
	//create event
	nb_event=rt_event_create("nb_event",RT_IPC_FLAG_PRIO);
	if(nb_event==RT_NULL)
	{
		LOG_E("create event error\n");
		return RT_ERROR;
	}
	//create event hand fun
	qsdk_thread_id=rt_thread_create("qsdk_thread",
																	qsdk_thread_entry,
																	RT_NULL,
																	1024 + 512,
																	7,
																	50);
	if(qsdk_thread_id!=RT_NULL)
		rt_thread_startup(qsdk_thread_id);
	else
	{
		LOG_E("create event hand fun error\n");
		return RT_ERROR;
	}
		//create psm hand fun
	qsdk_psm_id=rt_thread_create("qsdk_psm",
																	qsdk_psm_entry,
																	RT_NULL,
																	512,
																	12,
																	50);
	if(qsdk_psm_id!=RT_NULL)
		rt_thread_startup(qsdk_psm_id);
	else
	{
		LOG_E("create qsdk psm fun error\n");
		return RT_ERROR;
	}
	rt_memset(&nb_device_table,0,sizeof(nb_device_table));
	nb_device_table.reboot_open=NB_MODULE_REBOOT_FOR_PIN;
	nb_device_table.reboot_type=NB_MODULE_NO_INIT;
	nb_device_table.net_connect_ok=NB_MODULE_NO_INIT;
	nbiot_psm_status=qsdk_nb_status_exit_psm;

	//��ʼ��wakeuo_out����
#if	(defined QSDK_USING_M5311)||(defined QSDK_USING_ME3616)
	nb_wakeup_init();
#endif

	//nb-iot gpio init
	if(nb_io_init()!=RT_EOK)
	{
		LOG_E("nb-iot gpio init error\n");
		return RT_ERROR;
	}		
	if(rt_event_recv(nb_event,EVENT_REBOOT,RT_EVENT_FLAG_AND|RT_EVENT_FLAG_CLEAR,20000,&status)!=RT_EOK)
	{
		LOG_E("nb-iot power on reset error,Check the reset pin or qsdk config\n");
		return RT_ERROR;
	}
	nb_device_table.reboot_type=NB_MODULE_NO_INIT;
	nb_device_table.reboot_open=0;
	rt_thread_delay(1000);
	
	if(qsdk_nb_wait_connect()!=RT_EOK)
	{
		LOG_E("nb-iot wait connect error,no find nb-iot module\n");
		return RT_ERROR;
	}

	return RT_EOK;
}
/*************************************************************
*	�������ƣ�	qsdk_nb_quick_connect
*
*	�������ܣ�	NB-IOT ģ��һ��������ʼ��
*
*	��ڲ�����	��
*
*	���ز�����	0���ɹ�   1��ʧ��
*
*	˵����		
*************************************************************/
int qsdk_nb_quick_connect(void)
{
	rt_uint32_t i=5;
/*********************ģ���ϵ�ǿ������**************************/

#if (defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)
	//��������PSM��ʾ
#if (defined QSDK_USING_DEBUG)
	LOG_D("AT+NPSMR=1\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NPSMR=1")!=RT_EOK)	return RT_ERROR;
	
	if(qsdk_nb_get_psm_status()!=RT_EOK)
	{
		qsdk_nb_set_psm_mode(0,RT_NULL,RT_NULL);
	}
#elif (defined QSDK_USING_M5311)
	//�رջ���
#if (defined QSDK_USING_DEBUG)
	LOG_D("ATE0\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"ATE0")!=RT_EOK)	return RT_ERROR;

	//��������PSM��ʾ
#if (defined QSDK_USING_DEBUG)
	LOG_D("AT*SLEEP=1\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT*SLEEP=1")!=RT_EOK)	return RT_ERROR;
	
		//�����˳�PSM��ʾ
#if (defined QSDK_USING_DEBUG)
	LOG_D("AT*MATWAKEUP=1\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT*MATWAKEUP=1")!=RT_EOK)	return RT_ERROR;

		//����wakeup_out ��ʾ
#if (defined QSDK_USING_DEBUG)
	LOG_D("AT+CMSYSCTRL=1,1\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CMSYSCTRL=1,1")!=RT_EOK)	return RT_ERROR;	

	if(qsdk_nb_get_psm_status()!=RT_EOK)
	{
		qsdk_nb_close_auto_psm();
	}
	qsdk_nb_open_net_light();
	
#if (defined QSDK_USING_NET)
		qsdk_net_set_out_format();
#endif

#elif (defined QSDK_USING_ME3616)
	//�رջ���
#if (defined QSDK_USING_DEBUG)
	LOG_D("ATE0\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"ATE0")!=RT_EOK)	return RT_ERROR;
	
		//��������PSM��ʾ
#if (defined QSDK_USING_DEBUG)
	LOG_D("AT*MNBIOTEVENT=1,1\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT*MNBIOTEVENT=1,1")!=RT_EOK)	return RT_ERROR;
	
		qsdk_nb_open_net_light();

	if(qsdk_nb_get_psm_status()!=RT_EOK)
	{
		qsdk_nb_set_psm_mode(0,RT_NULL,RT_NULL);
	}
#if (defined QSDK_USING_IOT)	
	//�������ݷ���ģʽ
#if (defined QSDK_USING_DEBUG)
	LOG_D("AT+M2MCLICFG=1,0\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+M2MCLICFG=1,0")!=RT_EOK) return RT_ERROR;
#endif
#endif

#ifdef QSDK_USING_M5310A
//�������M5310����IOTƽ̨
	if(qsdk_iot_check_address()!=RT_EOK)
	{
		LOG_D("ncdp error,now to set ncdp address\n");
		
		if(qsdk_iot_set_address()==RT_EOK)
		{
			if(qsdk_iot_check_address()!=RT_EOK)
				LOG_E("set ncdp address error\n");
		}
		else	return RT_ERROR;
	}
#endif	
	rt_thread_delay(1000);
//����ȷ��ģ���Ƿ񿪻�	
	do{
			i--;
			if(qsdk_nb_sim_check()!=RT_EOK)
			{
				rt_thread_delay(500);
			}
			LOG_D("+CFUN=%d\n",nb_device_table.sim_state);

			if(nb_device_table.sim_state!=1)
				rt_thread_delay(1000);

		}	while(nb_device_table.sim_state==0&&i>0);
		
		if(i<=0)
		{
			LOG_E("NB-IOT boot failure, please check SIM card\n");
			return RT_ERROR;
		}
		else{
			i=5;
			rt_thread_delay(1000);
		}	
//��ȡSIM����IMSI����		
		do{
				i--;
				if(qsdk_nb_get_imsi()==RT_NULL)
				{
					rt_thread_delay(500);				
				}
				else
				{
					LOG_D("IMSI=%s\n",nb_device_table.imsi);
					break;
				}
			}while(i>0);
		
			if(i<=0)
			{
				LOG_E("No SIM card found\n");
				return RT_ERROR;
			}
			else
			{
				i=15;
				rt_thread_delay(100);
			}

//��ȡģ��IMEI
			if(qsdk_nb_get_imei()==RT_NULL)
			{
				LOG_E("Nb-iot IMEI not foundr\n");
				return RT_ERROR;				
			}
			else
			{
				LOG_D("IMEI=%s\n",nb_device_table.imei);
			}

//�������IOTƽ̨֧��
#if (defined QSDK_USING_IOT)&&(defined QSDK_USING_M5310A)

//�������M5310����IOTƽ̨
	
		rt_thread_delay(100);
		if(qsdk_iot_open_update_status()!=RT_EOK)
		{
			LOG_E("open iot update status error\n");
			return RT_ERROR;
		}
		else LOG_D("open iot update status success\n");
		rt_thread_delay(100);
		if(qsdk_iot_open_down_date_status()!=RT_EOK)
		{
			LOG_E("open iot down date status error\n");
			return RT_ERROR;
		}
			else LOG_D("open iot down date status success\n");	
#endif
//��ȡ�ź�ֵ
		do{
				i--;
				if(qsdk_nb_get_csq()==-1)
					{
						rt_thread_delay(500);
					}
					else if(nb_device_table.csq!=99&&nb_device_table.csq!=0)
					{
						break;
					}
					else
						{
							LOG_D("CSQ=%d\n",nb_device_table.csq);	
							rt_thread_delay(3000);
						}
		
			}while(i>0);
			
			if(i<=0)
			{
				LOG_E("nb-iot not find the signal\n");
				return RT_ERROR;
			}
			else
				{
					LOG_D("CSQ=%d\n",nb_device_table.csq);
					i=40;
					rt_thread_delay(100);
				}
		do{
				i--;
				if(qsdk_nb_get_net_connect()==RT_EOK) break;
				LOG_D("CEREG=%d\n",nb_device_table.net_connect_ok);
				rt_thread_delay(1000);
			}while(i>0);
		
		if(i<=0)
		{
			LOG_E("nb-iot connect network failurer\n");
			return RT_ERROR;
		}	
		rt_thread_delay(2000);			
		LOG_D("CEREG=%d\n",nb_device_table.net_connect_ok);

//��ȡntp������ʱ��

		if(qsdk_nb_get_time()!=RT_EOK)
		{
			LOG_E("getting network time errorsr\n");
		}
		LOG_D("nb-iot connect network success\n");		
				
		return RT_EOK;	
}

INIT_APP_EXPORT(qsdk_init_environment);


#ifdef QSDK_USING_FINSH_CMD
#include <string.h>
#include "stdlib.h"

void qsdk_nb(int argc,char**argv)
{
	if (argc > 1)
	{
		if (!strcmp(argv[1], "quick_connect"))
		{
			//nb-iotģ�����ٳ�ʼ������
			rt_kprintf("Please wait for nb-iot module networking\n");
			if(qsdk_nb_quick_connect()!=RT_EOK)
				rt_kprintf("module init failure\n");
			else rt_kprintf("nb-iot module connect to Network success\n");				
		}
		else if (!strcmp(argv[1], "reboot"))
		{
			if(qsdk_nb_reboot()!=RT_EOK)	rt_kprintf("nb-iot reboot error\n");
			rt_kprintf("nb-iot reboot success\n");				
		}
		else if (!strcmp(argv[1], "get_imsi"))
		{
			rt_kprintf("imsi=%s\n",qsdk_nb_get_imsi());				
		}
		else if (!strcmp(argv[1], "get_imei"))
		{
			rt_kprintf("imei=%s\n",qsdk_nb_get_imei());				
		}
		else if (!strcmp(argv[1], "get_csq"))
		{
			rt_kprintf("csq=%d\n",qsdk_nb_get_csq());				
		}
		else if (!strcmp(argv[1], "get_net_connect"))
		{
			if(qsdk_nb_get_net_connect()==RT_EOK)
				rt_kprintf("nb-iot module networking  successful\n");
			else rt_kprintf("nb-iot module networking  error\n");
		}
#if (defined QSDK_USING_M5311)||(defined QSDK_USING_ME3616)
		else if (!strcmp(argv[1], "open_net_light"))
		{
			qsdk_nb_open_net_light();
		}
		else if (!strcmp(argv[1], "close_net_light"))
		{
			qsdk_nb_close_net_light();
		}
#if (defined QSDK_USING_M5311)
		else if (!strcmp(argv[1], "open_auto_psm"))
		{
			qsdk_nb_open_auto_psm();
		}
		else if (!strcmp(argv[1], "close_auto_psm"))
		{
			qsdk_nb_close_auto_psm();
		}
#endif

#endif
		else if (!strcmp(argv[1], "exit_psm"))
		{
			qsdk_nb_exit_psm();
		}
		else
		{
				rt_kprintf("Unknown command. Please enter 'qsdk_nb' for help\n");
		}
	}
	else
	{
		rt_kprintf("Usage:\n");
		rt_kprintf("qsdk_nb quick_connect         - Fast init and network attachment\n");
		rt_kprintf("qsdk_nb reboot                - Restart nb-iot module\n");
		rt_kprintf("qsdk_nb get_imsi              - Getting SIM card imsi\n");
		rt_kprintf("qsdk_nb get_imei              - Get the nb-iot module imei\n");
		rt_kprintf("qsdk_nb get_csq               - Get nb-iot Module Signal Value\n");
		rt_kprintf("qsdk_nb get_net_connect       - Getting the networking status of nb-iot module\n");
#if (defined QSDK_USING_M5311)||(defined QSDK_USING_ME3616)
		rt_kprintf("qsdk_nb open_net_light        - nb-iot module enable network led\n");
		rt_kprintf("qsdk_nb close_net_light       - nb-iot module disable network led\n");
#if (defined QSDK_USING_M5311)
		rt_kprintf("qsdk_nb open_auto_psm         - nb-iot module enable auto psm sleep\n");
		rt_kprintf("qsdk_nb close_auto_psm        - nb-iot module disable auto psm sleep\n");
#endif

#endif
		rt_kprintf("qsdk_nb exit_psm              - nb-iot module exit psm mode\n");
	}	
}


MSH_CMD_EXPORT(qsdk_nb, Basic functions supported by nb-iot);

#endif
