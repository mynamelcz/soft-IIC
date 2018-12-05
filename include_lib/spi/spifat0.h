
#ifndef __SPIFAT_0_H
#define __SPIFAT_0_H

#include "typedef.h"
//#include "clock.h"
#include "mango_dev_spi.h"
// #include "spi/spiflash.h"
#include "peripherals/winbond_flash.h"
#include "spi/fat12.h"

//for spifat0 driver
extern u8 spifat0_eraser(FLASH_ERASER eraser, u32 addr);
extern void spifat0_write(void *buf, u32 addr, tu16 len);
extern void spifat0_read(void *buf, u32 addr, tu16 len);

//for spifat0 mango device
extern const struct DEV_IO *dev_reg_spi1(void *parm);
extern void spifat0_mutex_enter();
extern void spifat0_mutex_exit();
extern void set_spifat0_cap(u32 base, u32 lenght);
extern void set_spifat0_start(void);
extern u8 get_spifat0_working(void);

//for spifat0 ctrl
typedef struct __spifat0_ctrl {
    u32     base;
    u8      flag;
    void    *dc_buf;
    struct __fat12_ops  *ops;
    struct __fat12_io   io;
} spifat0_ctrl;

extern volatile spifat0_ctrl *g_p_spifat0;

extern void spifat0_ctrl_close(void);
extern void *spifat0_ctrl_open(u8 reset, u8 auto_sync);

extern u8 spifat0_ctrl_init(void);
extern u32 spifat0_ctrl_read(void *buf, u32 lba);
extern u32 spifat0_ctrl_write(void *buf, u32 lba);
extern u32 spifat0_ctrl_get_capacity(void);
extern u8 spifat0_ctrl_wiat(u8 op_sd, u8 retry);
extern void spifat0_ctrl_sync(void);

#endif
