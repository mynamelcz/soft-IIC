#include "wav_play.h"
#include "sdk_cfg.h"
#include "common/msg.h"
#include "dec/sup_dec_op.h"
#include "dev_manage/dev_ctl.h"
#include "fat/tff.h"
#include "file_operate/file_op.h"
#include "fat/fs_io.h"
#include "fat/tff.h"
#include "common/app_cfg.h"
#include "file_operate/logic_dev_list.h"
#include "dac/dac_api.h"
#include "vm/vm_api.h"
#include "timer.h"
#include "hw_cpu.h"
#include "sdk_cfg.h"
#include "pll1167.h"

#if PLL1167_EN


static void __delay(u32 t)
{
    while (t--) {
        asm("nop");
    }
}
static void delay(u32 d)
{
    __delay(d);
}
static void delay_1us(void)
{
    volatile int t;
    t = 30;
    while (t != 0) {
        t--;
    }
}

static void pdelay(unsigned char t)
{
    volatile int i;
    for (i = 0; i < t; i++) {
        delay_1us();
    }
}
static void delay_1ms(void)
{
    volatile int i;
    for (i = 0; i < 0x7900; i++);

}
static void delay_ms(tu8 ms)
{
    volatile int i;
    for (i = 0; i < ms; i++) {
        delay_1ms();
    }
}

void cs_control(tu8 cs)
{
    CS_PORT_DIR &= ~CS_BIT;
    delay(6);//ray debug
    if (cs) {
        CS_PORT_OUT |= CS_BIT;
        //delay(5);//ray debug
    } else {
        CS_PORT_OUT &= ~CS_BIT;
        //delay(5);
    }
}
void clk_control(tu8 clk)
{
    CLK_PORT_DIR &= ~CLK_BIT;
//    delay(2);//ray debug
    if (clk) {
        CLK_PORT_OUT |= CLK_BIT;

    } else {
        CLK_PORT_OUT &= ~CLK_BIT;

    }
}
void reset_control(tu8 reset)
{
    RST_PORT_DIR &= ~RST_BIT;
//    delay(5);//ray debug
    if (reset) {
        RST_PORT_OUT |= RST_BIT;
        delay(120);
    } else {
        RST_PORT_OUT &= ~RST_BIT;
        delay(120);
    }
}
void MOSI_control(tu8 mosi_control)
{
    //data in
    DO_PORT_DIR &= ~DO_BIT;
    delay(2);//ray debug
    if (mosi_control) {
        DO_PORT_OUT |= DO_BIT;

    } else {
        DO_PORT_OUT &= ~DO_BIT;

    }
}

tu8 MISO_status(void)
{
    //data out
    DI_PORT_DIR |= DI_BIT;
    delay(2);//ray debug
    if (DI_PORT_IN & DI_BIT) {
        return 1;
    } else {
        return 0;
    }
}
#ifdef RF_HW_SPI
#define WAIT_SPI1_OK()  while(!(JL_SPI1->CON & BIT(15))); JL_SPI1->CON |= BIT(14)
void SPI1_INIT(u8 speed)
{
    JL_SPI1->BAUD = speed;
    JL_SPI1->CON = 0X0098;

    JL_IOMAP->CON1 |= BIT(4);
    DI_PORT_DIR |= DI_BIT;
    CLK_PORT_DIR &= ~CLK_BIT;
    DO_PORT_DIR &= ~DO_BIT;

    JL_SPI1->CON |= BIT(0);

    cs_control(1);
}
void spi_write(unsigned   char   spi_bValue)
{
    cs_control(0);
    JL_SPI1->CON &= ~BIT(12);
    JL_SPI1->BUF = spi_bValue;
    WAIT_SPI1_OK();
}
unsigned char spi_read(void)
{
    cs_control(0);
    JL_SPI1->CON |= BIT(12);
    JL_SPI1->BUF = 0xff;
    WAIT_SPI1_OK();
    return JL_SPI1->BUF;
}
#else
//-----------------------------------------------------------------------------
// SPI 写操作
//-----------------------------------------------------------------------------
void spi_write(unsigned   char   spi_bValue)
{
    unsigned   char   no;
    cs_control(0);
    delay(4);//ray debug
    for (no = 0; no < 4; no++) {
        clk_control(0);
        if ((spi_bValue & 0x80) == 0x80) {	//SPIMOSI   =1 ;
            MOSI_control(1);//数据保持一段时间

        } else {
            MOSI_control(0);

        }
        //delay(2);//ray debug
        clk_control(1);
        //delay(2);//ray debug
        spi_bValue   = (spi_bValue   << 1);

        clk_control(0);
        if ((spi_bValue & 0x80) == 0x80) {	//SPIMOSI   =1 ;
            MOSI_control(1);//数据保持一段时间

        } else {
            MOSI_control(0);

        }
        //delay(2);//ray debug
        clk_control(1);
        //delay(2);//ray debug
        spi_bValue   = (spi_bValue   << 1);
    }
    clk_control(0);
}
//-----------------------------------------------------------------------------
// SPI 读操作
//-----------------------------------------------------------------------------
unsigned  char spi_read()
{
    unsigned   char   no, spi_bValue = 0;

    cs_control(0);
    delay(2);//ray debug
    for (no = 0; no < 8; no++) {
        clk_control(0);
//        delay(2);//ray debug
        spi_bValue   <<= 1;
        clk_control(1);
//        delay(2);//ray debug
        if (MISO_status()) {
            spi_bValue++;
        }
    }

    clk_control(0);
    return   spi_bValue;
}
#endif
//-----------------------------------------------------------------------------
// SPI 16位寄存器写操作
//-----------------------------------------------------------------------------
void Reg_write16(unsigned char addr, unsigned char v1, unsigned char v2)
{
    spi_write(addr);
    //delay(130);//ray debug
    spi_write(v1);
    //delay(130);
    spi_write(v2);
    //delay(130);
    cs_control(1);
    pdelay(RF_GAP);			// RF register DBUS propergation time
}

//-----------------------------------------------------------------------------
// SPI 16位寄存器读操作
//-----------------------------------------------------------------------------
unsigned int Reg_read16(unsigned char addr)
{
    unsigned int value = 0;
    spi_write(addr | REG_RD);
    //delay(130);//ray debug

    pdelay(RF_GAP);				// RF register DBUS propergation time from sclk falling edge

    value = spi_read();
    //delay(130);
    value <<= 8;
    value |= spi_read();
    //delay(130);
    cs_control(1);


    return value;
}

#endif
