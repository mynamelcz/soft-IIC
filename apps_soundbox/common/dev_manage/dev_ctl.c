#include "sdk_cfg.h"
#include "dev_manage/dev_ctl.h"
#include "dev_manage/drv_manage.h"
#include "sdmmc/sd_host_api.h"
#include "dev_manage/mango_dev_cache.h"
#include "common/app_cfg.h"
#include "file_operate/logic_dev_list.h"
#include "linein/linein.h"
#include "usb_device/dev_pc.h"
#include "sys_detect.h"
#include "linein/dev_linein.h"
#include "irq_api.h"
#include "crc_api.h"
#include "update.h"
#include "spi/spi1.h"
#include "spi/spifat0.h"
#include "spi/spifat1.h"
#include "spi/mango_dev_spi.h"

#define UDISK_READ_PAGE    512

extern tbool syd_test(void *p_hdev);
extern const struct DEV_IO *dev_reg_usb(void *parm);
extern const struct DEV_IO *dev_reg_usb_mult(void *parm);
extern void set_usb_mult_mount_delay(u8 cnt);
//extern OS_EVENT  sys_clk_chg_sem;
extern void delay(u32 d);

DEV_HANDLE volatile sd0;        ///>设备句柄
DEV_HANDLE volatile sd1;        ///>设备句柄
DEV_HANDLE volatile usb;        ///>设备句柄
DEV_HANDLE volatile cache;      ///>设备句柄
DEV_HANDLE volatile spi1;        ///>设备句柄
DEV_HANDLE volatile spifat0;        ///>设备句柄
DEV_HANDLE volatile spifat1;        ///>设备句柄

static u32 dev_cache_flash_header_base = 0;
void dev_cache_set_header_base(u32 base)
{
    dev_cache_flash_header_base = base;
}
static u32 dev_cache_get_flase_header_base(void)
{
    return dev_cache_flash_header_base;
}

void register_spi_close_sd_handle(void *handle, u8 sdcontroller);

#if SDMMC0_EN
///sd0 io检测函数
u8 sd0_io_check(void)
{
    return TRUE;  //检测到
    return FALSE; //没检测到
}
#endif


#if SDMMC1_EN
///sd1 io检测函数
u8 sd1_io_check(void)
{
    return TRUE;  //检测到
    return FALSE; //没检测到
}
#endif

#if SPI1_EN
void spi1_cs_api(u8 cs)
{
    /* JL_PORTB->DIR&= ~BIT(9); */
    /* cs ? (JL_PORTB->OUT |= BIT(9)) : (JL_PORTB->OUT &= ~BIT(9)); */
    JL_PORTC->DIR &= ~BIT(2);
    cs ? (JL_PORTC->OUT |= BIT(2)) : (JL_PORTC->OUT &= ~BIT(2));
    delay(10);
}
#if (SPI1_FLASH_CHIP==NAND_FLASH)
#include "vm_api.h"
#define VM_NAND_ONCE_LEN    200
extern u16 CRC16(void *ptr, u32  len);
u8 *nand_getinfo_api(u16 len, u8 *iniok)
{
    u8 *buf;
    *iniok = 0;
    buf = malloc(len + 6);
    printf("\n** nand_getinfo_api **\n");

    if (buf) {
        u8 i = 0;
        u16 total = len + 6;
        u16 rlen;
        vm_err err = VM_ERR_NONE;
        while (total) {
            rlen = total;
            if (rlen > VM_NAND_ONCE_LEN) {
                rlen = VM_NAND_ONCE_LEN;
            }
            err = AD200_vm_read(VM_NANDFLASH + i, &buf[i * VM_NAND_ONCE_LEN], rlen);
            if (VM_ERR_NONE != err) {
                break;
            }
            i++;
            total -= rlen;
        }
        if (VM_ERR_NONE == err) {
            if ((buf[len + 0] == 'n')
                && (buf[len + 1] == 'a')
                && (buf[len + 2] == 'n')
                && (buf[len + 3] == 'd')
               ) {
                u16 vmcrc, bufcrc;
                vmcrc   = ((u16)buf[len + 4]) | ((u16)buf[len + 5] << 8);
                bufcrc  = CRC16(buf, len + 4);
                printf("vmcrc = 0x%x \n", vmcrc);
                printf("bufcrc = 0x%x \n", bufcrc);
                if (vmcrc == bufcrc) {
                    *iniok = 1;
                }
            }
        }
    }
    printf_buf(buf, len + 6);
    return buf;
}
void nand_saveinfo_api(u8 *buf, u16 len)
{
    printf("\n** nand_saveinfo_api **\n");

    buf[len + 0] = 'n';
    buf[len + 1] = 'a';
    buf[len + 2] = 'n';
    buf[len + 3] = 'd';

    u16 bufcrc = CRC16(buf, len + 4);
    printf("bufcrc = 0x%x \n", bufcrc);

    buf[len + 4]  = bufcrc;
    buf[len + 5]  = bufcrc >> 8;

    u8 i = 0;
    u16 total = len + 6;
    u16 rlen;
    vm_err err = VM_ERR_NONE;
    while (total) {
        rlen = total;
        if (rlen > VM_NAND_ONCE_LEN) {
            rlen = VM_NAND_ONCE_LEN;
        }
        err = AD200_vm_write(VM_NANDFLASH + i, &buf[i * VM_NAND_ONCE_LEN], rlen);
        if (VM_ERR_NONE != err) {
            break;
        }
        i++;
        total -= rlen;
    }
    printf_buf(buf, len + 6);
}

bool nand_idinfo_api(u32 nandid, NANDFLASH_ID_INFO *info)
{
    printf("nandflash id = 0x%x \n", nandid);
    info->page_size          = 2048;
    info->page_spare_size    = 128;
    info->block_page_num     = 64;
    info->chip_block_num     = 2048;
    return true;
}
bool nand_blockok_api(void (*readspare)(u16 block, u8 shift, u8 *buf, u8 len), u16 block)
{
    u8 buf[1];
//{
//    //test
//    u8 tbuf[64];
//    u8 i;
//    readspare(block,0,tbuf,64);
//    printf("\n block = 0x%x \n", block);
//    printf_buf(tbuf, 64);
//    for (i=0; i<64; i++)
//    {
//        if (tbuf[i] != 0xff)
//        {
//            printf("\n %d not 0xff \n", i);
//            break;
//        }
//    }
//}

    readspare(block, 0, buf, 1);
    if (buf[0] == 0xff) {
        return true;
    }
    printf("\n errblock = %d, var=0x%x \n", block, buf[0]);
    return false;
}
#endif
#endif

static void dev_reg_ctrl(void)
{

    u32 dev_parm = 0;

#if SDMMC0_EN
    extern void spi1_close_sd0_controller(void);
    sSD_API_SET sd0_api_set;
    memset(&sd0_api_set, 0x0, sizeof(sSD_API_SET));
    sd0_api_set.controller_io = SD0_IO_A;        //SD0_IO_A：SD0控制器B出口，SD0_IO_B：SD0控制器B出口
    sd0_api_set.online_check_way = SD_CLK_CHECK; //CMD检测方式：SD_CMD_CHECK，CLK检测方式：SD_CLK_CHECK，IO检测方式：SD_IO_CHECK
    sd0_api_set.max_data_baud = 5;               //数据传输波特率(0为最快速度)
    sd0_api_set.hc_mode = SD_1WIRE_MODE;         //1线模式：SD_1WIRE_MODE，4线模式：SD_4WIRE_MODE，高速模式：SD_HI_SPEED
    //--(SD_1WIRE_MODE|SD_HI_SPEED 表示1线高速模式)
    if (SD_IO_CHECK == sd0_api_set.online_check_way) {
        sd0_api_set.io_det_func = sd0_io_check;  //传入io检测函数
    }
#if USB_SD0_MULT_EN
    DEVICE_REG(sd0_mult, &sd0_api_set);               ///<注册sd0_usb_复用到系统
    sd0 = dev_open("sd0mult", 0);
#else
    DEVICE_REG(sd0, &sd0_api_set);               ///<注册sd0到系统
    sd0 = dev_open("sd0", 0);
#endif
    register_spi_close_sd_handle(spi1_close_sd0_controller, 0);

#endif




#if SDMMC1_EN
    extern void spi1_close_sd1_controller(void);
    sSD_API_SET sd1_api_set;
    memset(&sd1_api_set, 0x0, sizeof(sSD_API_SET));

    sd1_api_set.controller_io = SD1_IO_B;        //SD1_IO_A：SD1控制器B出口，SD1_IO_B：SD1控制器B出口
    sd1_api_set.online_check_way = SD_CLK_CHECK; //CMD检测方式：SD_CMD_CHECK，CLK检测方式：SD_CLK_CHECK，IO检测方式：SD_IO_CHECK
    sd1_api_set.max_data_baud = 5;               //数据传输波特率(0为最快速度)
    sd1_api_set.hc_mode = SD_1WIRE_MODE;         //1线模式：SD_1WIRE_MODE，4线模式：SD_4WIRE_MODE，高速模式：SD_HI_SPEED
    //--(SD_1WIRE_MODE|SD_HI_SPEED 表示1线高速模式)
    if (SD_IO_CHECK == sd1_api_set.online_check_way) {
        sd1_api_set.io_det_func = sd1_io_check;  //传入io检测函数
    }
#if USB_SD1_MULT_EN
    DEVICE_REG(sd1_mult, &sd1_api_set);               ///<注册sd0_usb_复用到系统
    sd1 = dev_open("sd1mult", 0);
#else
    DEVICE_REG(sd1, &sd1_api_set);               ///<注册sd1到系统
    sd1 = dev_open("sd1", 0);
#endif
    register_spi_close_sd_handle(spi1_close_sd1_controller, 1);
#endif

#if USB_DISK_EN

#if(USB_SD0_MULT_EN == 1)||(USB_SD1_MULT_EN == 1)
    dev_parm = UDISK_READ_PAGE;
    DEVICE_REG(usb_mult, &dev_parm); ///<注册usb到系统
    usb = dev_open("usbmult", 0);
#else
    dev_parm = UDISK_READ_PAGE;
    DEVICE_REG(usb, &dev_parm); ///<注册usb到系统
    usb = dev_open("usb", 0);
#endif

#endif

#if VR_SPI0_EN
    DEVICE_REG(spifat0, NULL);
    spifat0 = dev_open("spifat0", 0);
#endif

#if VR_SPI1_EN
    DEVICE_REG(spifat1, NULL);
    spifat1 = dev_open("spifat1", 0);
#endif

#if SPI1_EN
    u32 spi1parm = 0;

    spi1flash_set_chip(SPI1_FLASH_CHIP); //0-norflash, 1-nandflash
#if (SPI1_FLASH_CHIP==NAND_FLASH)
    nandflash_info_register(nand_getinfo_api, nand_saveinfo_api, nand_idinfo_api, nand_blockok_api);
#endif

    spi1parm = SPI_ODD_MODE | SPI_CLK_DIV16;
    /* spi1parm = SPI_DUAL_MODE | SPI_CLK_DIV16; */
    /* spi1parm = SPI_2WIRE_MODE | SPI_CLK_DIV16; */
    spi1_io_set(SPI1_POC);
    spi1_remap_cs(spi1_cs_api);
    DEVICE_REG(spi1, &spi1parm);
    spi1 = dev_open("spi1", 0);
#endif

    DEVICE_REG(cache, NULL);
    /* cache = dev_open("cache", 0); */
    cache = dev_open("cache", (void *)dev_cache_get_flase_header_base());

#if(USB_SD0_MULT_EN == 1)||(USB_SD1_MULT_EN == 1)
    usb_sd_mult_init();
    set_usb_mult_mount_delay(10);
#endif
}



u32 dev_detect_fun(u32 info)
{
#if SD_DADA_MULT
    sd_data_multiplex();
#endif

#if AUX_DETECT_EN
    aux_check_api(info); //linein检测
#endif

#if(BT_MODE==NORMAL_MODE)
#if USB_PC_EN
    pc_check_api(info);  //PC检测
#endif
#endif

#if(USB_SD0_MULT_EN == 1)||(USB_SD1_MULT_EN == 1)
    usb_sd_detect_mult_api();
#endif
    return 0;
}

extern u32 lg_dev_list_init();
void dev_ctrl_sys_init(void *parm)
{
    ///逻辑设备链表初始化
    lg_dev_list_init();

    ///物理设备链表初始化
    phy_dev_list_init();

    ///注册设备(所有设备先在此处注册)
    dev_reg_ctrl();

    ///启动设备管理器线程
    dev_init(TaskDevCheckPrio, TaskDevMountPrio, (void *)dev_detect_fun);

//    os_mutex_create(&sys_clk_chg_sem);

}

u32 dev_updata_cfg(void)
{
    JL_AUDIO->DAC_CON = 0;
    irq_global_disable();
    irq_clear_all_ie();
    clr_PINR_ctl();
    led_update_start();
    return 1; //非软复位升级
}


