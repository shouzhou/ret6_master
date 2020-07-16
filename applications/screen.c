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

//全局变量
#define array_length          20   //定义数组长度 100
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
//uint8_t tftcmd[10]={0xA5,0X5A,0X03,0X80,0X4f,0x01};     //模拟按键控制

/* 串口接收消息结构*/
struct rx_msg
{
    rt_device_t dev;
    rt_size_t size;
};
/* 串口设备句柄 */
static rt_device_t serial;
struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;  /* 初始化配置参数 ，用于配置波特率等*/
/* 消息队列控制块 */
static struct rt_messagequeue rx_mq;

uint8_t g_pagenum=0;
uint8_t g_slavenum=0;
extern SLAVE_T g_Slave[SLAVE_NUM];
/* 接收数据回调函数 */
static rt_err_t uart_input(rt_device_t dev, rt_size_t size)
{
    struct rx_msg msg;
    rt_err_t result;
    msg.dev = dev;
    msg.size = size;

    result = rt_mq_send(&rx_mq, &msg, sizeof(msg));
    if ( result == -RT_EFULL)
    {
        /* 消息队列满 */
        rt_kprintf("message queue full！\n");
    }
    return result;
}
//串口屏刷新进程
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
//串口屏接收数据接收线程
static void serial_thread_entry(void *parameter)
{
    struct rx_msg msg;
        rt_err_t result;
        rt_uint32_t rx_length;
        static char rx_buffer[RT_SERIAL_RB_BUFSZ + 1];

        while (1)
        {
            rt_memset(&msg, 0, sizeof(msg));
            /* 从消息队列中读取消息*/
            result = rt_mq_recv(&rx_mq, &msg, sizeof(msg), RT_WAITING_FOREVER);
            if (result == RT_EOK)
            {
                /* 从串口读取数据*/
                rx_length = rt_device_read(msg.dev, 0, rx_buffer, msg.size);
                rx_buffer[rx_length] = '\0';
                /* 通过串口设备 serial 输出读取到的消息 */
               // rt_device_write(serial, 0, rx_buffer, rx_length);
                /* 打印数据 */
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
        if(rx[5]==0x04) //从机号 1， 100
        {
            
            if((rx[8]<=SLAVE_NUM)&&(rx[8]>0))    //需要判断从机号是否有效 1~ SLAVE_NUM ,开始保存数据
            {
                g_slavenum= rx[8];
                rt_kprintf("slave number =%d\r\n",g_slavenum);
                rt_event_send(screen_event,EVENT_SCREEN_CHANGE_SLAVENUM);	
                
            }
        }
        else if(rx[5] == 0x10) //开启关闭 0，1 
        {
            rt_kprintf("slave on off =%d\r\n",rx[8]);
            g_Slave[g_slavenum-1].slaveonoff = rx[8];
        }
        else if(rx[5] == 0x11) //工作模式  2 ，3，4
        {
            rt_kprintf("slave mode =%d\r\n",rx[8]);
            g_Slave[g_slavenum-1].slavemode = rx[8];
        }
        else if(rx[5] == 0x01) //密码验证 
        {
            rt_kprintf("password  =%d\r\n",rx[8]*65536+rx[9]*256+rx[10]);
            if((rx[8]*65536+rx[9]*256+rx[10] )==123456)
                bsp_ScreenjumpToPage(7);
            else
                bsp_ScreenjumpToPage(1);
        }
        else if(rx[5] ==0x05) //长度
        {
            rt_kprintf("legth  =%d\r\n",rx[8]);
        }
        else if(rx[5] ==0x06) //宽度
        {
            rt_kprintf("width  =%d\r\n",rx[8]);
        }
        else if(rx[5] ==0x07) //高度
        {
            rt_kprintf("height =%d\r\n",rx[8]);
        }
        
        else if(rx[5] ==0x0a) //a5  5a  06  83  00  0a  01  00  01 启动 系统 跳转到监控页面
        {
            rt_kprintf("start all system \r\n");
        }
        else if(rx[5] ==0x0b) //a5  5a  06  83  00  0b  01  00  01 停止 系统 不跳转
        {
            rt_kprintf("stop all system \r\n");
        }
        
    }
    else if((rx[0]== 0xA5)&&(rx[1] == 0x5A)&&(rx[3]==0x81)&&(rx[4]==0x03)) //获取页面
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
    /* 查找串口设备 */
       serial = rt_device_find(SAMPLE_UART_NAME);
       if (!serial)
       {
           rt_kprintf("find %s failed!\n", SAMPLE_UART_NAME);
       }
       else
       {
           rt_kprintf("find %s successful!\n", SAMPLE_UART_NAME);
       }

       /* step：修改串口配置参数 */
       config.baud_rate = BAUD_RATE_115200;        //修改波特率为 9600
       config.data_bits = DATA_BITS_8;           //数据位 8
       config.stop_bits = STOP_BITS_1;           //停止位 1
       config.bufsz     = 128;                   //修改缓冲区 buff size 为 128
       config.parity    = PARITY_NONE;           //无奇偶校验位
       /* step：控制串口设备。通过控制接口传入命令控制字，与控制参数 */
       rt_device_control(serial, RT_DEVICE_CTRL_CONFIG, &config);
       /* 初始化消息队列 */
       rt_mq_init(&rx_mq, "rx_mq",
                  msg_pool,                 /* 存放消息的缓冲区 */
                  sizeof(struct rx_msg),    /* 一条消息的最大长度 */
                  sizeof(msg_pool),         /* 存放消息的缓冲区大小 */
                  RT_IPC_FLAG_FIFO);        /* 如果有多个线程等待，按照先来先得到的方法分配消息 */

       /* 以 DMA 接收及轮询发送方式打开串口设备 */
       rt_device_open(serial, RT_DEVICE_FLAG_DMA_RX);
       //MX_USART2_UART_Init();
       /* 设置接收回调函数 */
       rt_device_set_rx_indicate(serial, uart_input);
                  
       /* 发送字符串 */
       rt_device_write(serial, 0, str, (sizeof(str) - 1));
      // rt_device_write(serial, 0, tftcmd, 6);
       bsp_ScreenjumpToPage(0); //跳转到1号页面  
       rt_thread_mdelay(2000);
       bsp_ScreenjumpToPage(1); //跳转到1号页面
       g_pagenum =1;
       /* 创建 serial 线程 */
       rt_thread_t thread = rt_thread_create("serial4_recv", serial_thread_entry, RT_NULL, 1024, 25, 10);
       
       /* 创建成功则启动线程 */
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


//0x80指令-写一个寄存器
//地址范围为0x00~0xff,所以address是8位的
//参考开发指南-2.2寄存器读写指令 0x80 、0x81
void write_register_80_1byte(uint8_t address,uint8_t data)    
 {
    
   uint8_t array[array_length];
	 
   array[0]=USER_R3;
   array[1]=USER_RA;
   array[2]=0x03;               //指令长度
   array[3]=0x80;
   array[4]=address;
   array[5]=data;               //数值 
     
     
   rt_device_write(serial, 0, array, 6);

 }

//0x80指令-连续写两个寄存器单元 
void write_register_80_2byte(uint8_t address,uint8_t data1,uint8_t data2)                 
 {
    
   uint8_t array[array_length];
	 
   array[0]=USER_R3;           
   array[1]=USER_RA;          
   array[2]=0x04;              //指令长度
   array[3]=0x80;
   array[4]=address;
   array[5]=data1;             //数值1
   array[6]=data2;             //数值2

	 
    rt_device_write(serial, 0, array, 7);
 }

 
 //切换到指定的页面
void bsp_ScreenjumpToPage(uint8_t page)
 {
     write_register_80_2byte(0x03,0x00,page) ;
 }
void bsp_GetCurrentPage(void)
 {
    uint8_t array[array_length];

    array[0]=USER_R3;           
    array[1]=USER_RA;          
    array[2]=0x03;              //指令长度
    array[3]=0x81;
    array[4]=0x03;
    array[5]=0x02;             //数值1
    rt_device_write(serial, 0, array, 6);
 }

//0x80指令-连续写三个寄存器单元 
void write_register_80_3byte(uint8_t address,uint8_t data1,uint8_t data2,uint8_t data3)
 {
    
   uint8_t array[array_length];
	 
   array[0]=USER_R3;           
   array[1]=USER_RA;          
   array[2]=0x05;              //指令长度
   array[3]=0x80;
   array[4]=address;
   array[5]=data1;             //数值1
   array[6]=data2;             //数值2
   array[7]=data3;             //数值3

	 
    rt_device_write(serial, 0, array, 8);
 } 
 
 
//0x80指令-写多个寄存器
//地址范围为0x00~0xff,所以address是8位的
//参考开发指南-2.2寄存器读写指令 0x80 、0x81
void write_multiple_register_80(uint8_t address,uint8_t data_length,uint8_t *data)    
 {
   rt_uint8_t i,nDataLen;
   uint8_t array[array_length];
	 
   array[0]=USER_R3;
   array[1]=USER_RA;
   array[2]=1+1+data_length;   //命令占一字节，地址占一字节，再加上数据长度
   array[3]=0x80;
   array[4]=address;
	 
   for(i=0;i<data_length;i++)
    {
      array[5+i]=data[i]; 
    }
	 
   nDataLen=array[2]+3;        //有效命令的长度
	 
    rt_device_write(serial, 0, array, nDataLen);
 }



//0x82指令-写一个变量存储单元
void write_variable_store_82_1word(uint16_t address,uint16_t data)     
{
    
   uint8_t array[array_length];
	 
   array[0]=USER_R3;
   array[1]=USER_RA;
   array[2]=1+2+2;                   //命令占1字节，地址占2字节，再加上数据长度2字节
   array[3]=0x82;
   array[4]=(address&0xFF00)>>8;     //取地址的高8位
   array[5]=address&0x00FF;          //取地址的低8位
   array[6]=(data&0xFF00)>>8;        //取数据的高8位
   array[7]=data&0x00FF;             //取数据的低8位
	 
    rt_device_write(serial, 0, array, 8);
 }


//0x82指令-连续写两个变量存储单元                    
void write_variable_store_82_2word(uint16_t address,uint16_t data1,uint16_t data2)           
{
    
   uint8_t array[array_length];
	 
   array[0]=USER_R3;
   array[1]=USER_RA;
   array[2]=1+2+4;                   //命令占1字节，地址占2字节，再加上数据长度4字节
   array[3]=0x82;
   array[4]=(address&0xFF00)>>8;     //取地址的高8位
   array[5]=address&0x00FF;          //取地址的低8位
   array[6]=(data1&0xFF00)>>8;       //取数据的高8位
   array[7]=data1&0x00FF;            //取数据的低8位
   array[8]=(data2&0xFF00)>>8;       //取数据的高8位
   array[9]=data2&0x00FF;            //取数据的低8位
   	 
    rt_device_write(serial, 0, array, 10);
 } 
 
  
//0x82指令-连续写多个变量存储单元
//地址范围为0x0000~0xffff,所以address是16位的
//数据内容是2N字节，因为每个变量存储单元包含2个字节。
//参考开发指南-2.3变量存储器读写指令 0x82 、0x83
void write_multiple_variable_store_82(uint16_t address,uint8_t data_length,uint16_t *data)    
 {
   rt_uint8_t i,nDataLen;
   uint8_t array[array_length];
	 
   array[0]=USER_R3;
   array[1]=USER_RA;
   array[2]=1+2+data_length*2;  //命令占1字节，地址占2字节，再加上数据长度(数据是双字节)
   array[3]=0x82;
   array[4]=(address&0xFF00)>>8;         //取地址的高8位
   array[5]=address&0x00FF;              //取地址的低8位

   for(i=0;i<data_length;i++)
    {
      array[6+2*i]=(data[i]&0xFF00)>>8;  //取数据的高8位
      array[7+2*i]=data[i]&0x00FF;       //取数据的低8位
    }

   nDataLen=array[2]+3;                  //有效命令的长度
	 
    rt_device_write(serial, 0, array, nDataLen);
 }
