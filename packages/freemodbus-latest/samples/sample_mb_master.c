/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-06-21     flybreak     first version
 */

#include <rtthread.h>

#include "mb.h"
#include "mb_m.h"
#include "user_mb_app.h"
#ifdef PKG_MODBUS_MASTER_SAMPLE
#define SLAVE_ADDR      MB_SAMPLE_TEST_SLAVE_ADDR
#define PORT_NUM        MB_MASTER_USING_PORT_NUM
#define PORT_BAUDRATE   MB_MASTER_USING_PORT_BAUDRATE
#else
#define SLAVE_ADDR      0x01
#define PORT_NUM        3
#define PORT_BAUDRATE   115200
#endif
#define PORT_PARITY     MB_PAR_NONE

#define MB_POLL_THREAD_PRIORITY  9 
#define MB_SEND_THREAD_PRIORITY  30
//#define MB_READ_THREAD_PRIORITY  19
//写寄存器的起始地址和数据个数
#define MB_SEND_REG_START  20
#define MB_SEND_REG_NUM    10

//读取的数据的起始地址和个数
#define MB_READ_REG_START 0
#define MB_READ_REG_NUM   20

#define MB_POLL_CYCLE_MS   500
extern USHORT   usMRegHoldBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_REG_HOLDING_NREGS];


static void send_thread_entry(void *parameter)
{
    eMBMasterReqErrCode error_code = MB_MRE_NO_ERR;
    rt_uint16_t error_count = 0;
    USHORT data[20] = {0};
    rt_uint8_t  i =0;
    USHORT test = 0x0000;
     rt_kprintf("enter modbus thread\r\n");
    while (1)
    {
       // rt_kprintf("enter while\r\n");
        error_code =  eMBMasterReqReadHoldingRegister( 
                                             SLAVE_ADDR,
                                             MB_READ_REG_START, 
                                             MB_READ_REG_NUM, 
                                             RT_WAITING_FOREVER);
        


        /* Record the number of errors */
        if (error_code != MB_MRE_NO_ERR)
        {
            error_count++;
            rt_kprintf("READ fail= %d\r\n",error_code);
        }
        else
        {
          //  rt_kprintf("READ ok\r\n");
            test++;
        }
        
        data[0] = usMRegHoldBuf[0][0];
        data[1] = usMRegHoldBuf[0][1];
        data[2] = usMRegHoldBuf[0][2];
        data[3] = usMRegHoldBuf[0][3];
        data[4] = usMRegHoldBuf[0][4];
        data[5] = usMRegHoldBuf[0][5];
        data[6] = usMRegHoldBuf[0][6];
        data[7] = usMRegHoldBuf[0][7];
        data[8] = usMRegHoldBuf[0][8];
        data[9] = usMRegHoldBuf[0][9];
//        
//        for(i=0;i<10;i++)
//         data[i]= test+i;
        error_code = eMBMasterReqWriteMultipleHoldingRegister(SLAVE_ADDR,          /* salve address */
                                                              MB_SEND_REG_START,   /* register start address */
                                                              MB_SEND_REG_NUM,     /* register total number */
                                                              data,                /* data to be written */
                                                              RT_WAITING_FOREVER); /* timeout */
        if (error_code != MB_MRE_NO_ERR)
        {
            error_count++;
             rt_kprintf("WRITE fail= %d\r\n",error_code);
        }
        else
        {
            test++;
            if(test%10 ==0)
            rt_kprintf("MODBUS ok\r\n");
        }

      //  test = test +0x0100;
        rt_thread_mdelay(5000);
    } 
}

static void mb_master_poll(void *parameter)
{
    eMBMasterInit(MB_RTU, PORT_NUM, PORT_BAUDRATE, PORT_PARITY);
    eMBMasterEnable();

    while (1)
    {
        eMBMasterPoll();
        rt_thread_mdelay(MB_POLL_CYCLE_MS);
    }
}

//static int mb_master_samlpe(int argc, char **argv)
 int mb_master_samlpe(void)
{
    static rt_uint8_t is_init = 0;
    rt_thread_t tid1 = RT_NULL, tid2 = RT_NULL;//,tid3= RT_NULL;

    if (is_init > 0)
    {
        rt_kprintf("sample is running\n");
        return -RT_ERROR;
    }
    tid1 = rt_thread_create("md_m_poll", mb_master_poll, RT_NULL, 512, MB_POLL_THREAD_PRIORITY, 50);
    if (tid1 != RT_NULL)
    {
        rt_thread_startup(tid1);
    }
    else
    {
        goto __exit;
    }

    tid2 = rt_thread_create("md_m_send", send_thread_entry, RT_NULL, 512, MB_SEND_THREAD_PRIORITY, 50);
    if (tid2 != RT_NULL)
    {
        rt_thread_startup(tid2);
    }
    else
    {
        goto __exit;
    }

    
//    tid3 = rt_thread_create("md_m_read", read_thread_entry, RT_NULL, 512, MB_READ_THREAD_PRIORITY, 20);
//    if (tid3 != RT_NULL)
//    {
//        rt_thread_startup(tid3);
//    }
//    else
//    {
//        goto __exit;
//    }
    
    is_init = 1;
    return RT_EOK;

__exit:
    if (tid1)
        rt_thread_delete(tid1);
    if (tid2)
        rt_thread_delete(tid2);
//    if (tid3)
//        rt_thread_delete(tid3);

    return -RT_ERROR;
}
//MSH_CMD_EXPORT(mb_master_samlpe, run a modbus master sample);
//INIT_APP_EXPORT(mb_master_samlpe);
