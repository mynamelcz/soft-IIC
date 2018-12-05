#ifndef _RFSPI_H
#define _RFSPI_H

#include "sdk_cfg.h"
void cs_control(tu8 cs);
void clk_control(tu8 clk);
void reset_control(tu8 reset);
void MOSI_control(tu8 mosi_control);
tu8 MISO_status(void);
#ifdef RF_HW_SPI
void SPI1_INIT(u8 speed);
void spi_write(unsigned   char   spi_bValue);
unsigned char spi_read(void);
#else
void spi_write(unsigned   char   spi_bValue);
unsigned  char spi_read();
#endif
void Reg_write16(unsigned char addr, unsigned char v1, unsigned char v2);
unsigned int Reg_read16(unsigned char addr);

#endif
