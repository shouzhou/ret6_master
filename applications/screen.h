/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-04-14     CRJ       the first version
 */
#ifndef APPLICATIONS_APP_UART4_H_
#define APPLICATIONS_APP_UART4_H_


void write_register_80_1byte(uint8_t address,uint8_t data);                                //0x80指令-写一个寄存器单元   
void write_register_80_2byte(uint8_t address,uint8_t data1,uint8_t data2);                 //0x80指令-连续写两个寄存器单元
void write_register_80_3byte(uint8_t address,uint8_t data1,uint8_t data2,uint8_t data3);   //0x80指令-连续写三个寄存器单元
void write_multiple_register_80(uint8_t address,uint8_t data_length,uint8_t *data);        //0x80指令-连续写多个寄存器单元

void write_variable_store_82_1word(uint16_t address,uint16_t data);                           //0x82指令-写一个变量存储单元
void write_variable_store_82_2word(uint16_t address,uint16_t data1,uint16_t data2);           //0x82指令-连续写两个变量存储单元
void write_multiple_variable_store_82(uint16_t address,uint8_t data_length,uint16_t *data);   //0x82指令-连续写多个变量存储单元

extern void bsp_uart4_init(void);
void bsp_ScreenjumpToPage(uint8_t page);
void bsp_ScreenDataAnalyze(uint8_t *rx,uint32_t rx_len);
void bsp_GetCurrentPage(void);
#endif /* APPLICATIONS_APP_UART2_H_ */
