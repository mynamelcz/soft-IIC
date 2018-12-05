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
#include "rfspi.h"

#if PLL1167_EN

const u8 init_rf_table[][3] = {
    {0x00, 0x6f, 0xe0},
    {0x01, 0x56, 0x81},
    {0x02, 0x66, 0x17},
    {0x04, 0x9c, 0xc9},
    {0x05, 0x66, 0x37}, //
    {0x07, 0x00, 0x30}, //
    {0x08, 0x6c, 0x90}, //
    {0x08, 0x6c, 0x90}, //
    {0x09, 0x48, 0x00}, // 48 00    18 40 ,5.5db, 2db
    {0x0a, 0x7f, 0xfd}, //
    {0x0b, 0x00, 0x08}, //
    {0x0c, 0x00, 0x00}, //
    {0x0d, 0x48, 0xbd}, //
    {0x16, 0x00, 0xff}, //
    {0x17, 0x80, 0x05}, //
    {0x18, 0x00, 0x67}, //
    {0x19, 0x16, 0x59}, //
    {0x1a, 0x19, 0xe0}, //
    {0x1b, 0x13, 0x00}, //
    {0x1c, 0x18, 0x00}, //
    {0x20, 0x48, 0x00}, //
    {0x20, 0x48, 0x00}, //
    {0x21, 0x3f, 0xc7}, //
    {0x21, 0x3f, 0xc7}, //
    {0x22, 0x20, 0x00}, //
//    {0x23,0x03,0x80},//
    {0x23, 0x00, 0x80}, //

    {0x24, 0x42, 0x31}, //
    {0x25, 0x86, 0x75}, //
    {0x26, 0x9a, 0x0b}, //
    {0x27, 0xde, 0xcf}, //
    {0x28, 0x44, 0x01}, //
    {0x29, 0xb0, 0x00}, //
//    {0x2a,0xfd,0xb0},//
    {0x2a, 0xfd, 0x00}, //
    {0x2b, 0x00, 0x0f} //
};



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

unsigned char RX_packet(void);

void PORT_Init(void)
{
    cs_control(1);
    clk_control(0);
    reset_control(0);
}

void Auto_Ack(void)
{
    unsigned int j;
    j = Reg_read16(0x29);
    Reg_write16(0x29, (j >> 8) & 0xf7, j & 0xff);			//Close ACK Function;
}

void enter_RX_mode(void)
{
    Reg_write16(0x34, 0, 0x80);									  // reset RX FIFO point
    Reg_write16(0x07, (0x00 & 0xfe), (0x30 | 0x80));  	//enter RX mode
}

void disable_TXRX_mode(void)
{
    Reg_write16(0x07, (0x00 & 0xfe), (0x30 & 0x7f));  	//dis  RX mod
}

void wait_PTK_low(void)
{
    PTK_PORT_DIR |= PTK_BIT;//ptk检测
    PTK_PORT_PU &= ~PTK_BIT;
    PTK_PORT_PD |= PTK_BIT;
    while (PTK_PORT_IN & PTK_BIT) {
        enter_RX_mode();
    }
}

tu8 PTK_status(void)
{
    PTK_PORT_DIR |= PTK_BIT;//ptk检测
    PTK_PORT_PU &= ~PTK_BIT;
    PTK_PORT_PD |= PTK_BIT;
    if (PTK_PORT_IN & PTK_BIT) {
//        putchar('c');
        return 1;
    } else {
        return 0;
    }
}

//*================================================================================
//*Fuction  : TX packet via FIFO
//*Parameter: packet data and length of payload sent
//return    : none
//*================================================================================
tu8 TX_packet(unsigned char *ptr, unsigned char bytes) //only tx loop
{

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////Because MCU is too fast,MCU write 60 bytes every time instead of 64 bytes lest the FIFO isn't over written
///////Pls refer to register56[15:12]:TX_FIFO_threshold
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    unsigned char i;
    u32 g_rev_info;
    static u32 j;

    Reg_write16(0x34, 0x80, 0); // reset TX FIFO point

    extern void close_wdt(void);
    close_wdt();
    spi_write(0x32);

    // spi_write(5);
    printf_buf(ptr, bytes);
    for (i = 0; i < bytes; i++) {
        spi_write(*ptr++);

    }
    cs_control(1);
    Reg_write16(0x07, (0x00 | 0x01), 0x30 & 0x7f);//TX Enable//Cancel 0706 by Jerry
    puts("wait ptk high");
    while (PTK_status() == 0) {
        printf(".");
    }
    puts("send data end \n");
    for (j = 0; j < 100; j++);
    Reg_write16(0x07, (0x00 & 0xfe), 0x30 & 0x7f);   //TX Disable
//    puts("S");
    return 1;
}

//*=============================================================================================================
//* PS: First byte received may be wrong, so FIFO_flag may not turn high as expectation
//* Function: RX packet from FIFO
//* Parameter: two;EnableTimeout=1, enable timeout function; bytes, how much bytes you wanted to receive
//* return   : error bits counted
//*=============================================================================================================
unsigned char RX_packet(void)
{

    unsigned char i, j;
    unsigned char Temp[6];
    u32 g_rev_info;
    i = PTK_status();
    if (!i) {
        //puts(".");
        return false;
    }


    spi_write(0x32 | REG_RD);

    for (i = 0; i < 5; i++) {
        Temp[i] = spi_read();
        //put_u32hex0(Temp[i]);
    }
    cs_control(1);
    printf_buf(Temp, 5);
    Reg_write16(0x07, (0x00 & 0xfe), (0x30 & 0x7f));
    if (Temp[4] != (Temp[1] + Temp[2])) {
        return false;
    }
    printf("temp:%d \n", Temp[1]);
    g_rev_info = (Temp[1] << 24) | (Temp[2] << 16) | (Temp[3] << 8) | Temp[4];
    //put_u32hex0(g_rev_info);
    disable_TXRX_mode();
    enter_RX_mode();
    return Temp[1];
}

void pll_1167_init0(void)
{
    int i;
#ifdef RF_HW_SPI
    SPI1_INIT(0x08);
#endif
    PORT_Init();
    reset_control(0);
    delay_ms(100);
    reset_control(1);//Enable PL1167
    delay_ms(60);//delay 5ms to let PL1167 stable
    clk_control(0);//set SPI clock to low
    for (i = 0; i < sizeof(init_rf_table) / sizeof(init_rf_table[0]); i++) {
        Reg_write16(init_rf_table[i][0], init_rf_table[i][1], init_rf_table[i][2]);
    }

    delay_ms(100);
    Auto_Ack();
    PTK_PORT_DIR |= PTK_BIT;//ptk检测
    PTK_PORT_PU &= ~PTK_BIT;
    PTK_PORT_PD |= PTK_BIT;

    enter_RX_mode();
}

unsigned char rf_buffer[6];
void send_rf_cmd(unsigned char cmd)
{
    rf_buffer[0] = 5;
    rf_buffer [1] = cmd;
    rf_buffer [2] = cmd;
    rf_buffer [3] = cmd;
    rf_buffer [4] = rf_buffer[1] + rf_buffer[2];
    rf_buffer[5] = rf_buffer[1] + rf_buffer[2] + rf_buffer[3] + rf_buffer[4];
    disable_TXRX_mode();
    TX_packet(rf_buffer, 6);
    enter_RX_mode();
}

void check_rf(void)
{
    if (PTK_status()) {
        os_taskq_post_msg(RF_TASK_NAME, 1, MSG_READ_CMD);
    }
}

void send_cmd(void)
{
    send_rf_cmd(0x33);
}

#endif
