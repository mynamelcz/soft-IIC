#ifndef _LED_H_
#define _LED_H_

#include "includes.h"
#include "sys_detect.h"
#include "sdk_cfg.h"

#define LED_POARA  BIT(28)
#define LED_POARB  BIT(29)
#define LED_POARC  BIT(30)
#define LED_POARD  BIT(31)

#define LED_B_BIT                       BIT(11) ///PB11
#define LED_R_BIT                       BIT(12) ///PB12


#ifdef POWER_TIMER_HW_ENABLE
#define LEDB_PORT     (LED_POARB|LED_B_BIT)//选择那组IO口和对应的IO口
#define LEDR_PORT     0//(LED_POARB|LED_R_BIT)
#else
#define LEDB_PORT     0
#define LEDR_PORT     0
#endif

#define LED_INIT_EN()     {JL_PORTB->PU &= ~LED_B_BIT;JL_PORTB->PD &= ~LED_B_BIT;JL_PORTB->DIR &= ~LED_B_BIT;}
#define LED_INIT_DIS()    {JL_PORTB->PU &= ~LED_B_BIT;JL_PORTB->PD &= ~LED_B_BIT;JL_PORTB->DIR |= LED_B_BIT;}
#define B_LED_ON()        JL_PORTB->OUT |= LED_B_BIT;
#define B_LED_OFF()       JL_PORTB->OUT &= ~LED_B_BIT;
#define R_LED_ON(...)
#define R_LED_OFF(...)

extern u32 g_led_fre;

void led_init(void);
void led_close(void);
void led_open(void);
void led_fre_set(u32 fre, u8 led_flash);
void led_scan(void *param);

#endif/*_LED_H_*/

