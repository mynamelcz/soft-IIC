#include "ui/led/led.h"
#include "timer.h"
#include "rtc/rtc_api.h"

extern tbool is_sniff_mode();
u32 g_led_fre = 0; ///<led fre
u8 g_led_flash = 0;///<b_led and r_led flash

/*----------------------------------------------------------------------------*/
/**@brief   LED初始化函数
   @param   void
   @return  void
   @note    void led_init(void)
*/
/*----------------------------------------------------------------------------*/
void led_init(void)
{
#if LED_EN
    LED_INIT_EN();
    //B_LED_ON();
    //R_LED_ON();
    s32 ret;
#ifndef POWER_TIMER_HW_ENABLE
    ret = timer_reg_isr_fun(timer0_hl, 5, led_scan, NULL);
    if (ret != TIMER_NO_ERR) {
        printf("led_scan err = %x\n", ret);
    }
#endif
#endif
}

/*----------------------------------------------------------------------------*/
/**@brief   LED关闭函数
   @param   void
   @return  void
   @note    void led_close(void)
*/
/*----------------------------------------------------------------------------*/
void led_close(void)
{
    LED_INIT_DIS();
}

/*----------------------------------------------------------------------------*/
/**@brief   LED打开函数
   @param   void
   @return  void
   @note    void led_open(void)
*/
/*----------------------------------------------------------------------------*/
void led_open(void)
{
    LED_INIT_EN();
}

/*----------------------------------------------------------------------------*/
/**@brief   LED频率设置函数(50ms*fre = 实际闪烁频率)
   @param   fre:led闪烁频率 （注：当fre = 0时，表示led常亮）
            led_flash:表示B_led和R_led是否互闪
   @return  void
   @note    实际闪烁频率 = fre*5*scan_fre
            例如scan_fre = 10ms
                led_fre_set(10,0)
            则：
                led实际闪烁频率 = 10*5*10 = 500ms = 0.5s
*/
/*----------------------------------------------------------------------------*/
void led_fre_set(u32 fre, u8 led_flash)
{
#ifdef POWER_TIMER_HW_ENABLE
    g_led_fre = fre;
#else
    g_led_fre = fre * 5;
#endif
    g_led_flash = led_flash;
}
volatile u8 led_rg_flag = 0;
u32 led_on_sniff(u32 led_flash)
{
    if (!g_led_fre) { /*红蓝灯常亮*/
        B_LED_ON();
        R_LED_ON();

    } else if (led_flash) { /*红蓝灯互闪*/
        if (led_rg_flag) {
            R_LED_ON();
            B_LED_OFF();
            led_rg_flag = 0;
        } else {
            B_LED_ON();
            R_LED_OFF();
            led_rg_flag = 1;
        }

    } else {
        /*蓝灯独闪*/
        B_LED_ON();
        R_LED_OFF();

    }
    return 1;
}
u32 led_off_sniff(u32 led_flash)
{
    if (!g_led_fre) { /*红蓝灯常亮*/
        B_LED_ON();
        R_LED_ON();

    } else if (led_flash) { /*红蓝灯互闪*/
        B_LED_OFF();
        R_LED_OFF();

    } else { /*蓝灯独闪*/
        B_LED_OFF();
        R_LED_OFF();
    }
    return 1;
}


/*----------------------------------------------------------------------------*/
/**@brief   led扫描函数
   @param   void
   @return  void
   @note    void led_scan(void *param)
*/
/*----------------------------------------------------------------------------*/
extern u8 is_pwrtimer_en();
extern volatile u8 low_power_flag;
extern void pwr_timer_control_led(u8 led_flash_mode, u32 led_on_ms, u32 led_off_ms, u32(*led_off)(u32 mode), u32(*led_on)(u32 mode));
extern void pwr_timer_control_led_close();
void led_scan(void *param)
{
#if LED_EN
    static u32 led_cnt;

#ifndef POWER_TIMER_HW_ENABLE
    if (is_sniff_mode() || is_pwrtimer_en()) {
        if (!g_led_fre) { /*红蓝灯常亮*/
            /*
                     led_on_sniff     led_off_sniff    led_on_sniff
            	  |_______________|_________________|_____________
            	  |     0(20)     |     0(20)       |   0(20)
                  |               |                 |

            */
            pwr_timer_control_led(0, 0, 0, led_off_sniff, led_on_sniff);

        } else if (g_led_flash) { /*红蓝灯互闪*/
            /*
                   led_on_sniff(红)                    led_on_sniff(蓝)
                   _________________                  ___________________
            	  |(g_led_fre*2*10) |led_off_sniff 50|  (g_led_fre*2*10  |led_off_sniff 50
            ______|                 |________________|                   |__________________

            */
            pwr_timer_control_led(g_led_flash, (g_led_fre * 2 * 10), 50, led_off_sniff, led_on_sniff); ///
        } else { /*蓝灯独闪*/

            /*
                   led_on_sniff(蓝)                                  led_on_sniff(蓝)
                   _______________                                 _________________
            	  |     (10*10)   |  led_off_sniff (g_led_fre*10) |   (10*10)
            ______|               |_______________________________|

            */
            pwr_timer_control_led(g_led_flash, 10 * 10, g_led_fre * 10, led_off_sniff, led_on_sniff);
        }
        return;
    } else {
        pwr_timer_control_led_close();
    }
#endif
    if (!g_led_fre) { /*红蓝灯常亮*/
        led_cnt = 0;
        B_LED_ON();
        R_LED_ON();
    } else if (g_led_flash) { /*红蓝灯互闪*/
        led_cnt++;
        if (led_cnt > g_led_fre) {
            if (led_cnt >= g_led_fre * 2) {
                led_cnt = 0;
            }
            B_LED_ON();
            R_LED_OFF();
        } else if (led_cnt > 0) {
            B_LED_OFF();
            R_LED_ON();
        }
    } else { /*蓝灯独闪*/
        led_cnt++;
        R_LED_OFF();
        if (led_cnt > g_led_fre) {
            if (led_cnt >= g_led_fre + 10) {
                led_cnt = 0;
            }
            B_LED_ON();
        } else if (led_cnt > 0) {
            B_LED_OFF();
        }
    }
#endif
}




