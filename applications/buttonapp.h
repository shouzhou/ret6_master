#ifndef __QSDK_KEY__H_
#define __QSDK_KEY__H_

#include "button.h"
#include <board.h>

#define KEY_0		GET_PIN(B, 13)
#define	KEY_1		GET_PIN(B, 14)

//#define KEY_PRESS  	PIN_HIGH
//#define KEY_RELEASE PIN_LOW

#define KEY_PRESS  	1
#define KEY_RELEASE 0

//定义key 状态结构体
struct key_state_type
{
	int down_state;
	int double_state;
	int long_state;
};

void bsp_key_process(void);

#endif
