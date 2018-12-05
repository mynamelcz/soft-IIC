#ifndef _PLL1167_H
#define _PLL1167_H

#include "sdk_cfg.h"

#if PLL1167_EN
/*
 注意：2.4g对晶振要求很高，用一种较大的晶振有问题，用小晶振就ok。
 * */
//硬件spi控制
#define RF_HW_SPI

#define	REG_RD	0x80
#define	REG_WR	0x7F
#define RF_GAP	150					//30=3.5us, time constant between succesive RF register accesses

#define CS_PORT_DIR JL_PORTA->DIR
#define CS_PORT_OUT JL_PORTA->OUT
#define CS_BIT BIT(9)

#define RST_PORT_DIR JL_PORTA->DIR
#define RST_PORT_OUT JL_PORTA->OUT
#define RST_BIT BIT(8)

#define PTK_PORT_DIR JL_PORTA->DIR
#define PTK_PORT_IN JL_PORTA->IN
#define PTK_PORT_PU JL_PORTA->PU
#define PTK_PORT_PD JL_PORTA->PD
#define PTK_BIT BIT(10)

#define CLK_PORT_DIR JL_PORTC->DIR
#define CLK_PORT_OUT JL_PORTC->OUT
#define CLK_BIT BIT(4)

#define DI_PORT_DIR JL_PORTC->DIR
#define DI_PORT_IN JL_PORTC->IN
#define DI_BIT BIT(3)

#define DO_PORT_DIR JL_PORTC->DIR
#define DO_PORT_OUT JL_PORTC->OUT
#define DO_BIT BIT(5)

#define RF_TASK_NAME "RfTask"

tu8 TX_packet(unsigned char *ptr, unsigned char bytes); //only tx loop
unsigned char RX_packet(void);
void pll_1167_init0(void);
void send_rf_cmd(unsigned char cmd);
void check_rf(void);
void send_cmd(void);

#endif
#endif
