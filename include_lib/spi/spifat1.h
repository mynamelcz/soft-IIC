
#ifndef __SPIFAT_1_H
#define __SPIFAT_1_H

#include "typedef.h"
//#include "clock.h"
#include "mango_dev_spi.h"
#include "peripherals/winbond_flash.h"
// #include "spiflash.h"
#include "spi/fat12.h"

//for spifat1 driver
extern u8 spifat1_eraser(FLASH_ERASER eraser, u32 addr);
extern void spifat1_write(void *buf, u32 addr, tu16 len);
extern void spifat1_read(void *buf, u32 addr, tu16 len);

//for spifat1 mango device
extern const struct DEV_IO *dev_reg_spi1(void *parm);
extern void spifat1_mutex_enter();
extern void spifat1_mutex_exit();
extern void set_spifat1_cap(u32 base, u32 lenght);
extern void set_spifat1_start(void);
extern u8 get_spifat1_working(void);

//for spifat1 ctrl
typedef struct __spifat1_ctrl {
    u32     base;
    u8      flag;
    void    *dc_buf;
    struct __fat12_ops  *ops;
    struct __fat12_io   io;
} spifat1_ctrl;

extern volatile spifat1_ctrl *g_p_spifat1;

extern void spifat1_ctrl_close(void);
extern void *spifat1_ctrl_open(u8 reset, u8 auto_sync);

extern u8 spifat1_ctrl_init(void);
extern u32 spifat1_ctrl_read(void *buf, u32 lba);
extern u32 spifat1_ctrl_write(void *buf, u32 lba);
extern u32 spifat1_ctrl_get_capacity(void);
extern u8 spifat1_ctrl_wiat(u8 op_sd, u8 retry);
extern void spifat1_ctrl_sync(void);

#endif
