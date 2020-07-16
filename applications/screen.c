/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-04-14     David       the first version
 */
#include "stm32f1xx_hal.h"
#include <rtthread.h>
#include <rtdevice.h>
#include "screen.h"
#include "slave.h"

//ȫ�ֱ���
#define array_length          20   //�������鳤�� 100
#define USER_R3 0xA5
#define USER_RA 0X5A


#define EVENT_SCREEN_CHANGE_SLAVENUM 				(1<< 1)
#define EVENT_SCREEN_CHANGE_HEIGHT		 	        (1<< 2)
#define EVENT_SCREEN_CHANGE_WEIGHT 				    (1<< 3)
#define EVENT_SCREEN_CHANGE_LENGTH 					(1<< 4)
#define EVENT_SCREEN_CHANGE_MODE 	                (1<< 5)
#define EVENT_SCREEN_CHANGE_ONOFF 	                (1<< 6)




rt_event_t screen_event=RT_NULL;

//void bsp_jumpToPage(uint8_t page);

#define SAMPLE_UART_NAME       "uart4"
//uint8_t tftcmd[10]={0xA5,0X5A,0X03,0X80,0X4f,0x01};     //ģ�ⰴ������

/* ���ڽ�����Ϣ�ṹ*/
struct rx_msg
{
    rt_device_t dev;
    rt_size_t size;
};
/* �����豸��� */
static rt_device_t serial;
struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;  /* ��ʼ�����ò��� ���������ò����ʵ�*/
/* ��Ϣ���п��ƿ� */
static struct rt_messagequeue rx_mq;

uint8_t g_pagenum=0;
uint8_t g_slavenum=0;
extern SLAVE_T g_Slave[SLAVE_NUM];
/* �������ݻص����� */
static rt_err_t uart_input(rt_device_t dev, rt_size_t size)
{
    struct rx_msg msg;
    rt_err_t result;
    msg.dev = dev;
    msg.size = size;

    result = rt_mq_send(&rx_mq, &msg, sizeof(msg));
    if ( result == -RT_EFULL)
    {
        /* ��Ϣ������ */
        rt_kprintf("message queue full��\n");
    }
    return result;
}
//������ˢ�½���
static void screen_thread_entry(void *parameter)
{
    rt_uint8_t i,count;
    rt_uint32_t status;
    rt_uint16_t tempdata[5];
    while(1)
    {
        
        
    if(rt_event_recv(screen_event,EVENT_SCREEN_CHANGE_SLAVENUM,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,RT_WAITING_FOREVER,&status)==RT_EOK)
	{
		rt_kprintf("receive change slave num event\n");
        tempdata[0] = g_Slave[g_slavenum-1].slaveonoff;
        tempdata[1] = g_Slave[g_slavenum-1].slavemode;
        
    //    write_variable_store_82_2word(uint16_t address,uint16_t data1,uint16_t data2)   
        write_variable_store_82_2word(0x10,tempdata[0],tempdata[1]);
		//write_variable_store_82_1word(0x10,tempdata[0]); //onoff
      //  rt_thread_mdelay(1000);
       // write_variable_store_82_1word(0x11,tempdata[1]); //mode
      //  write_variable_store_82_1word(0x11,0x03); 
        rt_thread_mdelay(100);
	}
//       if(g_pagenum != 9)
//       {
//       count++;
//       if(count%50 ==0)
//            bsp_GetCurrentPage();
//        }
//       else
//       {
//           for(i=0;i<5;i++)
//           tempdata[i] = g_Slave[i].slavetemp;
//          // void write_multiple_variable_store_82(uint16_t address,uint8_t data_length,uint16_t *data)  
//           write_multiple_variable_store_82(0x30,5,tempdata);
//       }
//       rt_thread_mdelay(1000);
    }
}
//�������������ݽ����߳�
static void serial_thread_entry(void *parameter)
{
    struct rx_msg msg;
        rt_err_t result;
        rt_uint32_t rx_length;
        static char rx_buffer[RT_SERIAL_RB_BUFSZ + 1];

        while (1)
        {
            rt_memset(&msg, 0, sizeof(msg));
            /* ����Ϣ�����ж�ȡ��Ϣ*/
            result = rt_mq_recv(&rx_mq, &msg, sizeof(msg), RT_WAITING_FOREVER);
            if (result == RT_EOK)
            {
                /* �Ӵ��ڶ�ȡ����*/
                rx_length = rt_device_read(msg.dev, 0, rx_buffer, msg.size);
                rx_buffer[rx_length] = '\0';
                /* ͨ�������豸 serial �����ȡ������Ϣ */
               // rt_device_write(serial, 0, rx_buffer, rx_length);
                /* ��ӡ���� */
               // rt_kprintf("%s\n",rx_buffer);
                
                bsp_ScreenDataAnalyze((uint8_t *)rx_buffer,rx_length);
            }
        }
}

void bsp_ScreenDataAnalyze(uint8_t *rx,uint32_t rx_len)
{
  //  uint8_t rev[50];
    uint8_t i;
     for(i=0;i<rx_len;i++)
            rt_kprintf("  %02x",rx[i]);
    rt_kprintf("\r\n");
    if((rx[0]== 0xA5)&&(rx[1] == 0x5A)&&(rx[3]==0x83))
    {
        if(rx[5]==0x04) //�ӻ��� 1�� 100
        {
            
            if((rx[8]<=SLAVE_NUM)&&(rx[8]>0))    //��Ҫ�жϴӻ����Ƿ���Ч 1~ SLAVE_NUM ,��ʼ��������
            {
                g_slavenum= rx[8];
                rt_kprintf("slave number =%d\r\n",g_slavenum);
                rt_event_send(screen_event,EVENT_SCREEN_CHANGE_SLAVENUM);	
                
            }
        }
        else if(rx[5] == 0x10) //�����ر� 0��1 
        {
            rt_kprintf("slave on off =%d\r\n",rx[8]);
            g_Slave[g_slavenum-1].slaveonoff = rx[8];
        }
        else if(rx[5] == 0x11) //����ģʽ  2 ��3��4
        {
            rt_kprintf("slave mode =%d\r\n",rx[8]);
            g_Slave[g_slavenum-1].slavemode = rx[8];
        }
        else if(rx[5] == 0x01) //������֤ 
        {
            rt_kprintf("password  =%d\r\n",rx[8]*65536+rx[9]*256+rx[10]);
            if((rx[8]*65536+rx[9]*256+rx[10] )==123456)
                bsp_ScreenjumpToPage(7);
            else
                bsp_ScreenjumpToPage(1);
        }
        else if(rx[5] ==0x05) //����
        {
            rt_kprintf("legth  =%d\r\n",rx[8]);
        }
        else if(rx[5] ==0x06) //���
        {
            rt_kprintf("width  =%d\r\n",rx[8]);
        }
        else if(rx[5] ==0x07) //�߶�
        {
            rt_kprintf("height =%d\r\n",rx[8]);
        }
        
        else if(rx[5] ==0x0a) //a5  5a  06  83  00  0a  01  00  01 ���� ϵͳ ��ת�����ҳ��
        {
            rt_kprintf("start all system \r\n");
        }
        else if(rx[5] ==0x0b) //a5  5a  06  83  00  0b  01  00  01 ֹͣ ϵͳ ����ת
        {
            rt_kprintf("stop all system \r\n");
        }
        
    }
    else if((rx[0]== 0xA5)&&(rx[1] == 0x5A)&&(rx[3]==0x81)&&(rx[4]==0x03)) //��ȡҳ��
    {
        g_pagenum = rx[7];
        rt_kprintf("current page =%d\r\n",g_pagenum);
    }
    
    else 
    {
       
    }
    
}

void bsp_uart4_init(void)
{
    static char msg_pool[256];
    char str[] = "Init Screen serial begin\r\n";
    screen_event=rt_event_create("screen_event",RT_IPC_FLAG_PRIO);
    if(screen_event==RT_NULL)
	{
		rt_kprintf("create event error\n");
		//return RT_ERROR;
	}
    /* ���Ҵ����豸 */
       serial = rt_device_find(SAMPLE_UART_NAME);
       if (!serial)
       {
           rt_kprintf("find %s failed!\n", SAMPLE_UART_NAME);
       }
       else
       {
           rt_kprintf("find %s successful!\n", SAMPLE_UART_NAME);
       }

       /* step���޸Ĵ������ò��� */
       config.baud_rate = BAUD_RATE_115200;        //�޸Ĳ�����Ϊ 9600
       config.data_bits = DATA_BITS_8;           //����λ 8
       config.stop_bits = STOP_BITS_1;           //ֹͣλ 1
       config.bufsz     = 128;                   //�޸Ļ����� buff size Ϊ 128
       config.parity    = PARITY_NONE;           //����żУ��λ
       /* step�����ƴ����豸��ͨ�����ƽӿڴ�����������֣�����Ʋ��� */
       rt_device_control(serial, RT_DEVICE_CTRL_CONFIG, &config);
       /* ��ʼ����Ϣ���� */
       rt_mq_init(&rx_mq, "rx_mq",
                  msg_pool,                 /* �����Ϣ�Ļ����� */
                  sizeof(struct rx_msg),    /* һ����Ϣ����󳤶� */
                  sizeof(msg_pool),         /* �����Ϣ�Ļ�������С */
                  RT_IPC_FLAG_FIFO);        /* ����ж���̵߳ȴ������������ȵõ��ķ���������Ϣ */

       /* �� DMA ���ռ���ѯ���ͷ�ʽ�򿪴����豸 */
       rt_device_open(serial, RT_DEVICE_FLAG_DMA_RX);
       //MX_USART2_UART_Init();
       /* ���ý��ջص����� */
       rt_device_set_rx_indicate(serial, uart_input);
                  
       /* �����ַ��� */
       rt_device_write(serial, 0, str, (sizeof(str) - 1));
      // rt_device_write(serial, 0, tftcmd, 6);
       bsp_ScreenjumpToPage(0); //��ת��1��ҳ��  
       rt_thread_mdelay(2000);
       bsp_ScreenjumpToPage(1); //��ת��1��ҳ��
       g_pagenum =1;
       /* ���� serial �߳� */
       rt_thread_t thread = rt_thread_create("serial4_recv", serial_thread_entry, RT_NULL, 1024, 25, 10);
       
       /* �����ɹ��������߳� */
       if (thread != RT_NULL)
       {
           rt_thread_startup(thread);
           rt_kprintf("Create %s Entry successful!\n", SAMPLE_UART_NAME);
       }
       else
       {
           rt_kprintf("Create %s Entry failed!\n", SAMPLE_UART_NAME);
       }
       
       rt_thread_t thread2 = rt_thread_create("screen_fresh", screen_thread_entry, RT_NULL, 1024, 24, 10);
       if (thread2 != RT_NULL)
       {
           rt_thread_startup(thread2);
           rt_kprintf("Create screen fresh Entry successful!\n");
       }
       else
       {
           rt_kprintf("Create screen fresh Entry failed!\n");
       }

}


//0x80ָ��-дһ���Ĵ���
//��ַ��ΧΪ0x00~0xff,����address��8λ��
//�ο�����ָ��-2.2�Ĵ�����дָ�� 0x80 ��0x81
void write_register_80_1byte(uint8_t address,uint8_t data)    
 {
    
   uint8_t array[array_length];
	 
   array[0]=USER_R3;
   array[1]=USER_RA;
   array[2]=0x03;               //ָ���
   array[3]=0x80;
   array[4]=address;
   array[5]=data;               //��ֵ 
     
     
   rt_device_write(serial, 0, array, 6);

 }

//0x80ָ��-����д�����Ĵ�����Ԫ 
void write_register_80_2byte(uint8_t address,uint8_t data1,uint8_t data2)                 
 {
    
   uint8_t array[array_length];
	 
   array[0]=USER_R3;           
   array[1]=USER_RA;          
   array[2]=0x04;              //ָ���
   array[3]=0x80;
   array[4]=address;
   array[5]=data1;             //��ֵ1
   array[6]=data2;             //��ֵ2

	 
    rt_device_write(serial, 0, array, 7);
 }

 
 //�л���ָ����ҳ��
void bsp_ScreenjumpToPage(uint8_t page)
 {
     write_register_80_2byte(0x03,0x00,page) ;
 }
void bsp_GetCurrentPage(void)
 {
    uint8_t array[array_length];

    array[0]=USER_R3;           
    array[1]=USER_RA;          
    array[2]=0x03;              //ָ���
    array[3]=0x81;
    array[4]=0x03;
    array[5]=0x02;             //��ֵ1
    rt_device_write(serial, 0, array, 6);
 }

//0x80ָ��-����д�����Ĵ�����Ԫ 
void write_register_80_3byte(uint8_t address,uint8_t data1,uint8_t data2,uint8_t data3)
 {
    
   uint8_t array[array_length];
	 
   array[0]=USER_R3;           
   array[1]=USER_RA;          
   array[2]=0x05;              //ָ���
   array[3]=0x80;
   array[4]=address;
   array[5]=data1;             //��ֵ1
   array[6]=data2;             //��ֵ2
   array[7]=data3;             //��ֵ3

	 
    rt_device_write(serial, 0, array, 8);
 } 
 
 
//0x80ָ��-д����Ĵ���
//��ַ��ΧΪ0x00~0xff,����address��8λ��
//�ο�����ָ��-2.2�Ĵ�����дָ�� 0x80 ��0x81
void write_multiple_register_80(uint8_t address,uint8_t data_length,uint8_t *data)    
 {
   rt_uint8_t i,nDataLen;
   uint8_t array[array_length];
	 
   array[0]=USER_R3;
   array[1]=USER_RA;
   array[2]=1+1+data_length;   //����ռһ�ֽڣ���ַռһ�ֽڣ��ټ������ݳ���
   array[3]=0x80;
   array[4]=address;
	 
   for(i=0;i<data_length;i++)
    {
      array[5+i]=data[i]; 
    }
	 
   nDataLen=array[2]+3;        //��Ч����ĳ���
	 
    rt_device_write(serial, 0, array, nDataLen);
 }



//0x82ָ��-дһ�������洢��Ԫ
void write_variable_store_82_1word(uint16_t address,uint16_t data)     
{
    
   uint8_t array[array_length];
	 
   array[0]=USER_R3;
   array[1]=USER_RA;
   array[2]=1+2+2;                   //����ռ1�ֽڣ���ַռ2�ֽڣ��ټ������ݳ���2�ֽ�
   array[3]=0x82;
   array[4]=(address&0xFF00)>>8;     //ȡ��ַ�ĸ�8λ
   array[5]=address&0x00FF;          //ȡ��ַ�ĵ�8λ
   array[6]=(data&0xFF00)>>8;        //ȡ���ݵĸ�8λ
   array[7]=data&0x00FF;             //ȡ���ݵĵ�8λ
	 
    rt_device_write(serial, 0, array, 8);
 }


//0x82ָ��-����д���������洢��Ԫ                    
void write_variable_store_82_2word(uint16_t address,uint16_t data1,uint16_t data2)           
{
    
   uint8_t array[array_length];
	 
   array[0]=USER_R3;
   array[1]=USER_RA;
   array[2]=1+2+4;                   //����ռ1�ֽڣ���ַռ2�ֽڣ��ټ������ݳ���4�ֽ�
   array[3]=0x82;
   array[4]=(address&0xFF00)>>8;     //ȡ��ַ�ĸ�8λ
   array[5]=address&0x00FF;          //ȡ��ַ�ĵ�8λ
   array[6]=(data1&0xFF00)>>8;       //ȡ���ݵĸ�8λ
   array[7]=data1&0x00FF;            //ȡ���ݵĵ�8λ
   array[8]=(data2&0xFF00)>>8;       //ȡ���ݵĸ�8λ
   array[9]=data2&0x00FF;            //ȡ���ݵĵ�8λ
   	 
    rt_device_write(serial, 0, array, 10);
 } 
 
  
//0x82ָ��-����д��������洢��Ԫ
//��ַ��ΧΪ0x0000~0xffff,����address��16λ��
//����������2N�ֽڣ���Ϊÿ�������洢��Ԫ����2���ֽڡ�
//�ο�����ָ��-2.3�����洢����дָ�� 0x82 ��0x83
void write_multiple_variable_store_82(uint16_t address,uint8_t data_length,uint16_t *data)    
 {
   rt_uint8_t i,nDataLen;
   uint8_t array[array_length];
	 
   array[0]=USER_R3;
   array[1]=USER_RA;
   array[2]=1+2+data_length*2;  //����ռ1�ֽڣ���ַռ2�ֽڣ��ټ������ݳ���(������˫�ֽ�)
   array[3]=0x82;
   array[4]=(address&0xFF00)>>8;         //ȡ��ַ�ĸ�8λ
   array[5]=address&0x00FF;              //ȡ��ַ�ĵ�8λ

   for(i=0;i<data_length;i++)
    {
      array[6+2*i]=(data[i]&0xFF00)>>8;  //ȡ���ݵĸ�8λ
      array[7+2*i]=data[i]&0x00FF;       //ȡ���ݵĵ�8λ
    }

   nDataLen=array[2]+3;                  //��Ч����ĳ���
	 
    rt_device_write(serial, 0, array, nDataLen);
 }
