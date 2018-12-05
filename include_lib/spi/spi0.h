#ifndef _SPI0_H_
#define _SPI0_H_

#include "typedef.h"
//#include "clock.h"
#include "mango_dev_spi.h"

#define SPI0_POB       0
#define SPI0_POD       1

extern void spi0_io_set(u8 spi_io);

extern s32 spi0_init(spi_mode mode, spi_clk clk);
extern void spi0_cs(u8 cs);
extern void spi0_set_clk(spi_clk clk);
extern void spi0_write_byte(u8 dat);
extern u8 spi0_read_byte(void);
extern s32 spi0_write(u8 *buf, u16 len);
extern s32 spi0_dec_write(u8 *buf, u16 len);
extern s32 spi0_read(u8 *buf, u16 len) ;
extern s32 spi0_dec_read(u8 *buf, u16 len);
extern s32 spi0_write_2bitMode(u8 *buf, u16 len);
void spi0_remap_cs(void (*cs_func)(u8 cs));

#endif  //_SPI0_H_
