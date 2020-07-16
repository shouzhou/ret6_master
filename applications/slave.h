#ifndef BSP_SLAVE__H_
#define BSP_SLAVE__H_

#include "rtthread.h"
#include "board.h"
#define SLAVE_NUM  10


//定义led 状态结构体
typedef struct 
{
    uint8_t  slaveaddr; //从机号
    uint8_t  slaveonoff;//开启关闭
    uint8_t  slaveconnect;//modbus连接上
    uint8_t  slavemode; //运行模式  自动 手动 熏蒸
    uint16_t  slavelegth;//长
    uint16_t  slavewidth;//宽
    uint16_t  slaveheight;//高
    uint16_t  slavetemp; //温度
    uint16_t  slavehum; //湿度
    uint16_t  slaveo3; //臭氧浓度
    uint16_t  slavenh3; //氨气浓度

}SLAVE_T;

void bsp_setSlaveOnOff(uint8_t slave,uint8_t status);
void bsp_setSlaveMode(uint8_t slave,uint8_t mode);
void bsp_setSlaveDefault(void);
extern SLAVE_T g_Slave[SLAVE_NUM];
#endif

