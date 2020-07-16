#ifndef BSP_SLAVE__H_
#define BSP_SLAVE__H_

#include "rtthread.h"
#include "board.h"
#define SLAVE_NUM  10


//����led ״̬�ṹ��
typedef struct 
{
    uint8_t  slaveaddr; //�ӻ���
    uint8_t  slaveonoff;//�����ر�
    uint8_t  slaveconnect;//modbus������
    uint8_t  slavemode; //����ģʽ  �Զ� �ֶ� Ѭ��
    uint16_t  slavelegth;//��
    uint16_t  slavewidth;//��
    uint16_t  slaveheight;//��
    uint16_t  slavetemp; //�¶�
    uint16_t  slavehum; //ʪ��
    uint16_t  slaveo3; //����Ũ��
    uint16_t  slavenh3; //����Ũ��

}SLAVE_T;

void bsp_setSlaveOnOff(uint8_t slave,uint8_t status);
void bsp_setSlaveMode(uint8_t slave,uint8_t mode);
void bsp_setSlaveDefault(void);
extern SLAVE_T g_Slave[SLAVE_NUM];
#endif

