#ifndef _SPI1_H_
#define _SPI1_H_

#include "typedef.h"
//#include "clock.h"
#include "spi/mango_dev_spi.h"
// #include "spi/spiflash.h"
#include "peripherals/winbond_flash.h"

#define SPI1_POB       0
#define SPI1_POC       1

extern s32 spi1_init(spi_mode mode, spi_clk clk);
extern void spi1_cs(u8 cs);
extern void spi1_io_set(u8 spi_io);
extern void spi1_set_clk(spi_clk clk);
extern void spi1_write_byte(u8 dat);
extern u8 spi1_read_byte(void);
extern s32 spi1_write(u8 *buf, u16 len);
extern s32 spi1_dec_write(u8 *buf, u16 len);
extern s32 spi1_read(u8 *buf, u16 len) ;
extern s32 spi1_dec_read(u8 *buf, u16 len);
extern s32 spi1_write_2bitMode(u8 *buf, u16 len);
void spi1_remap_cs(void (*cs_func)(u8 cs));

///////////////////////////////////////////////////////////
typedef struct __spi1_close_sd {
    void (*spi1_close_sd1_controller)(void);
    void (*spi1_close_sd0_controller)(void);
} _spi1_close_sd;
typedef struct _SPIFLASH {
    u32(*read_id)(void);
    s32(*read)(u8 *buf, u32 addr, u32 len);
    void (*eraser)(FLASH_ERASER eraser, u32 address);
    s32(*write)(u8 *buf, u32 addr, u32 len);
    void (*write_protect)(u8 pro_cmd);
    void (*reset)(void);
    s32(*init)(spi_mode mode0, FLASH_MODE mode1, spi_clk clk);
    _spi1_close_sd *spi_close_sd;
} SPIFLASH;
SPIFLASH *get_spi1_norflash(void);
SPIFLASH *get_spi1_nandflash(void);

//nor flash
void norflash_4Bflash_set_ID(u32 id);

//nand flash
typedef struct _NANDFLASH_ID_INFO {
    u16 page_size;
    u16 page_spare_size;
    u16 block_page_num;
    u16 chip_block_num;
} NANDFLASH_ID_INFO;
void nand_get_retry(u16 times);
void nandflash_info_register(u8 * (*getinfo)(u16 len, u8 *iniok),
                             void (*saveinfo)(u8 *infobuf, u16 len),
                             bool (*idinfo)(u32 nandid, NANDFLASH_ID_INFO *info),
                             bool (*blockok)(void (*readspare)(u16 block, u8 shift, u8 *buf, u8 len), u16 block)
                            );
void nand_read_block_spare_area(u16 block, u8 shift, u8 *buf, u8 len);
void nand_write_block_spare_area(u16 block, u8 shift, u8 *buf, u8 len);

///////////////////////////////////////////////////////////
void spi1flash_set_chip(u8 chipmode);//0-norflash, 1-nandflash
s32 spi1flash_init(spi_mode mode0, FLASH_MODE mode1, spi_clk clk);
u32	spi1flash_read_id();
void spi1flash_eraser(FLASH_ERASER eraser, u32 address);
void spi1flash_write_protect(u8 pro_cmd);
s32 spi1flash_read(u8 *buf, u32 addr, u32 len);
s32 spi1flash_write(u8 *buf, u32 addr, u32 len);
void set_sd_spi1_reuse(u8 onoff);//sd spi1的io复用开关
u8 sd_spi_pc_mode_enter(u8 sd_type);
void sd_spi_pc_mode_exit(u8 sd_type);
#endif  //_SPI0_H_
