#include "slave.h"

SLAVE_T g_Slave[SLAVE_NUM];

void bsp_setSlaveDefault(void)
{
    uint8_t i;
    for(i=0;i<SLAVE_NUM;i++)
     {
        g_Slave[i].slaveaddr  = i+1;
        g_Slave[i].slaveconnect = 0; //Ĭ��û��������
        g_Slave[i].slaveonoff = 0;  //Ĭ�Ϲر�
        g_Slave[i].slavemode = 2;  //Ĭ���Զ�ģʽ
        g_Slave[i].slavelegth = 1000;
        g_Slave[i].slavewidth = 1000;
        g_Slave[i].slaveheight = 1000;
        g_Slave[i].slavenh3 = 1000;
        g_Slave[i].slaveo3 = 1000;
        g_Slave[i].slavetemp = 2000;
        g_Slave[i].slavehum = 5000;
     }
 
}
//���� ĳ���ӻ��Ŀ��� �ر� ״̬
void bsp_setSlaveOnOff(uint8_t slave,uint8_t status)
{
 if((slave>0)&&(slave<100))
 {
     if(status ==0)
     g_Slave[slave-1].slaveonoff = 0;
     else
     g_Slave[slave-1].slaveonoff = 1;
 }
}

void bsp_setSlaveMode(uint8_t slave,uint8_t mode)
{
if((slave>0)&&(slave<100))
 {
     if(mode ==0)
     g_Slave[slave-1].slavemode = 0;
     else if(mode ==1)
     g_Slave[slave-1].slavemode = 1;
     else if(mode ==2)
     g_Slave[slave-1].slavemode = 2;
 }
}




