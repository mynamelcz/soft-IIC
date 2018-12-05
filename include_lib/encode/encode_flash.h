#ifndef _ENCODE_FLASH_H
#define _ENCODE_FLASH_H

#include "typedef.h"
#include "rtos/os_api.h"
// #include "sys_cfg.h"


/*
 * 打开flash录音需要编译dev_manage、dec、sydf和encode库。
 * 并把(.enc_spi_loop)和(.enc_spi_isr)加入到ram.ld中，
 * 当aoto_eraser==1时,中断代码、数据等一定要放在芯片内部ram中
 */
typedef tbool(*ENCODE_FLASH_EXIT_FUN)(void);
void encode_flash_main(void *rec_api, u32 addr, u32 len, u32 aoto_eraser, ENCODE_FLASH_EXIT_FUN exit_fun);
extern volatile u8 vm_eraser_noclosecpu_flag;

extern volatile u8 enc_flash_ladc_data;
extern volatile u8 enc_flash_enc_data;
extern volatile u8 enc_flash_enc_end;
extern volatile u8 enc_flash_flag;

#ifndef ENCODE_FLASH
#define ENCODE_FLASH        //录flash使能，库
#endif

#ifdef ENCODE_FLASH
#define AT_ENC_ISR 				AT(.enc_spi_isr)
#define AT_ENC_ISR_KEY 			AT(.enc_spi_isr_key)
#define AT_ENC_ISR_AD_KEY 		AT(.enc_spi_isr_ad_key)
#define AT_ENC_ISR_IO_KEY 		AT(.enc_spi_isr_io_key)
#define AT_ENC_ISR_IR_KEY 		AT(.enc_spi_isr_ir_key)
#define AT_ENC_ISR_TCH_KEY 		AT(.enc_spi_isr_tch_key)
#else
#define AT_ENC_ISR
#define AT_ENC_ISR_KEY
#define AT_ENC_ISR_AD_KEY
#define AT_ENC_ISR_IO_KEY
#define AT_ENC_ISR_IR_KEY
#define AT_ENC_ISR_TCH_KEY
#endif

///////////////////////////////////////////////////////////////////////////////////////

// #define ENCODE_FLASH_API        //录flash使能

//使能这个需要把ram.ld中的几个东西放开

/***** flash encode ram *********/
/*    *(.enc_spi_isr)           */	//录音过程中用到的中断函数
/*	  *(.enc_spi_isr_key)       */	//录音过程中用到的按键
/*	  *(.enc_spi_isr_ad_key)    */	//录音过程中用到的AD按键
/*	  *(.enc_spi_isr_ir_key)    */	//录音过程中用到的IR按键
/*	  *(.enc_spi_isr_tch_key)   */	//录音过程中用到的触摸按键
/*	  *(.enc_spi_isr_io_key)    */	//录音过程中用到的io按键
/*	  *(.rtc_io_ram)            */	//录音过程中的中断函数有用到PR口
/*	  *(.uart_to_ram)           */	//录音过程中的中断函数有用到uart
/***** flash encode ram end******/

extern volatile u8 enc_flash_work;
extern volatile u16 enc_flash_key;


#endif
