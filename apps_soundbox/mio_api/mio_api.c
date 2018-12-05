/***********************************Jieli tech************************************************
  File : mio_api.c
  By   : Huxi
  Email: xi_hu@zh-jieli.com
  date : 2016-09-26 15:30
********************************************************************************************/
#include "sdk_cfg.h"
#include "mio/mio_ctrl.h"
#include "mio_api/mio_api.h"
/* #include "os_api.h" */
#include "fat/tff.h"
/* #include "fs_io.h" */
#include "cbuf/circular_buf.h"
#include "common/msg.h"
#include "mcpwm_api.h"
#include "clock_api.h"

#ifdef MUSIC_IO_ENABLE

#define MIO_API_DEBUG
#ifdef MIO_API_DEBUG
#define mio_api_printf  printf
#define mio_api_puts    puts
#else
#define mio_api_printf(...)
#define mio_api_puts(...)
#endif

#define MIO_FILE_TASK_NAME	"ScriptTask"

#define MIO_API_INPUT_BUF_SIZE  600

#define MIO_API_IO_OFFSET       0
#define MIO_API_IO_PORT_DIR     JL_PORTC->DIR
#define MIO_API_IO_PORT_DIE     JL_PORTC->DIE
#define MIO_API_IO_PORT_OUT     JL_PORTC->OUT
#define MIO_API_IO_PORT_PU      JL_PORTC->PU
#define MIO_API_IO_PORT_PD      JL_PORTC->PD
#define MIO_API_IO_USE			0x3f


/* MCPWMCH0_H  PA5     MCPWMCH0_L   PA6  */
/* MCPWMCH1_H  PB1     MCPWMCH1_L   PB2  */
/* MCPWMCH2_H  PB5     MCPWMCH2_L   PB6  */
/* MCPWMCH3_H  PB7     MCPWMCH3_L   PB8  */
/* MCPWMCH4_H  PA10    MCPWMCH4_L   PA11 */
/* MCPWMCH5_H  PC1     MCPWMCH5_L   PC2  */

#define MIO_PWM_FRE				120000L

static void mio_ctrl_pwm_init(u8 chl)
{
    mio_api_printf("pwm init chl%d \n", chl);

    static u8 flag = 0;
    if (flag == 0) {
        flag = 1;
        mcpwm_module_on(MCPWM_INCREASE_REDUCE_MODE, clock_get_sys_freq(), MCPWM_CLK_DIV8, 255);
    }

    switch (chl) {
    case 0:
        mcpwm4_init(MCPWMCH4_L_OPPOSITE, MCPWMCH4_L); //PA11
        break;
    case 1:
        mcpwm1_init(MCPWMCH1_L_OPPOSITE, MCPWMCH1_L); //PB2
        break;
    case 2:
        mcpwm2_init(MCPWMCH2_H_OPPOSITE, MCPWMCH2_H);
        break;
    case 3:
        mcpwm3_init(MCPWMCH3_H_OPPOSITE, MCPWMCH3_H);
        break;
    }
}
static void mio_ctrl_pwm_run(u8 chl, u8 pwm_var)
{
    /* mio_api_printf("-p%d=%x,",chl,pwm_var); */
    switch (chl) {
    case 0:
        set_mcpwm4(MIO_PWM_FRE, pwm_var);
        break;
    case 1:
        set_mcpwm1(MIO_PWM_FRE, pwm_var);
        break;
    case 2:
        set_mcpwm2(MIO_PWM_FRE, pwm_var);
        break;
    case 3:
        set_mcpwm3(MIO_PWM_FRE, pwm_var);
        break;
    }
}

static void mio_ctrl_io_init(u16 mask)
{
    mio_api_printf("io init mask = 0x%x\n", mask);
//test
    MIO_API_IO_PORT_PU  &= ~((mask << MIO_API_IO_OFFSET) & (MIO_API_IO_USE));
    MIO_API_IO_PORT_PD  &= ~((mask << MIO_API_IO_OFFSET) & (MIO_API_IO_USE));
    MIO_API_IO_PORT_OUT &= ~((mask << MIO_API_IO_OFFSET) & (MIO_API_IO_USE));
    MIO_API_IO_PORT_DIR &= ~((mask << MIO_API_IO_OFFSET) & (MIO_API_IO_USE));
    MIO_API_IO_PORT_DIE |= ((mask << MIO_API_IO_OFFSET) & (MIO_API_IO_USE));
}
static void mio_ctrl_io_run(u16 mask, u16 io_var)
{
    /* mio_api_printf("-i%x,",io_var); */
//test
    MIO_API_IO_PORT_OUT &= ~((mask << MIO_API_IO_OFFSET) & (MIO_API_IO_USE));
    MIO_API_IO_PORT_OUT |= ((io_var << MIO_API_IO_OFFSET) & (MIO_API_IO_USE));
}

static void mio_ctrl_need_input(void *mio_hdl)
{
    mio_ctrl_t *p_mio_var = mio_hdl;
    os_taskq_post_msg(p_mio_var->father_name, 1, MSG_MIO_READ);
}

static mio_ctrl_t *mio_api_start0(mio_file_t *file, u32 start_addr, u8 checkID3, void *father_name)
{
    u32 len;
    mio_ctrl_t *p_mio_var;
    p_mio_var = malloc(sizeof(mio_ctrl_t));
    if (!p_mio_var) {
        mio_api_puts("miostart malloc err0 \n");
        goto _mio_api_init_err;
    }
    memset(p_mio_var, 0, sizeof(mio_ctrl_t));

    p_mio_var->inbuf = malloc(MIO_API_INPUT_BUF_SIZE);
    if (!p_mio_var->inbuf) {
        mio_api_puts("miostart malloc err1 \n");
        goto _mio_api_init_err;
    }
    cbuf_init(&p_mio_var->cbuf, p_mio_var->inbuf, MIO_API_INPUT_BUF_SIZE);

    p_mio_var->file.filehdl          = file->filehdl;
    p_mio_var->file.seek             = file->seek;
    p_mio_var->file.read             = file->read;
    p_mio_var->file.tell             = file->tell;

    p_mio_var->start_addr       = start_addr;
    p_mio_var->father_name      = father_name;
    p_mio_var->need_input       = mio_ctrl_need_input;

    p_mio_var->dec_io.priv      = p_mio_var;
    p_mio_var->dec_io.input     = mio_ctrl_input;

    p_mio_var->dec_io.pwm_init  = mio_ctrl_pwm_init;
    p_mio_var->dec_io.pwm_run   = mio_ctrl_pwm_run;
    p_mio_var->dec_io.io_init   = mio_ctrl_io_init;
    p_mio_var->dec_io.io_run    = mio_ctrl_io_run;

    p_mio_var->dec      = get_mio_ops();
    len                 = p_mio_var->dec->need_workbuf_size();
    p_mio_var->dcbuf    = malloc(len);
    if (!p_mio_var->dcbuf) {
        mio_api_puts("miostart malloc err2 \n");
        goto _mio_api_init_err;
    }
    memset(p_mio_var->dcbuf, 0, len);

    if (p_mio_var->dec->open(p_mio_var->dcbuf, &p_mio_var->dec_io)) {
        mio_api_puts("miostart open \n");
        goto _mio_api_init_err;
    }
    p_mio_var->status |= MIO_API_STU_CHECK;
    if (p_mio_var->dec->check(p_mio_var->dcbuf, checkID3)) {
        mio_api_puts("miostart check \n");
        goto _mio_api_init_err;
    }
    p_mio_var->status &= ~MIO_API_STU_CHECK;

    mio_api_puts("miostart ok \n");
    return p_mio_var;

_mio_api_init_err:
    if (p_mio_var) {
        if (p_mio_var->dcbuf) {
            free(p_mio_var->dcbuf);
        }
        if (p_mio_var->inbuf) {
            free(p_mio_var->inbuf);
        }
        free(p_mio_var);
    }
    return NULL;
}

extern u16 fs_read(void *p_f_hdl, u8 _xdata *buff, u16 len);
extern bool fs_seek(void *p_f_hdl, u8 type, u32 offsize);
extern u32 fs_tell(void *p_f_hdl);

mio_ctrl_t *mio_api_start(void *file_hdl, u32 start_addr)
{
    mio_file_t miofile;

    miofile.filehdl     = file_hdl;
    miofile.seek        = fs_seek;
    miofile.read        = fs_read;
    miofile.tell        = fs_tell;

    return mio_api_start0(&miofile, start_addr, 1, MIO_FILE_TASK_NAME);
}
void mio_api_close(mio_ctrl_t **priv)
{
    mio_ctrl_t *p_mio_var;
    if (priv && (*priv)) {
        p_mio_var = *priv;
        if (p_mio_var->dcbuf) {
            free(p_mio_var->dcbuf);
        }
        if (p_mio_var->inbuf) {
            free(p_mio_var->inbuf);
        }
        free(p_mio_var);
        *priv = NULL;
    }
}


void mio_api_run(mio_ctrl_t *p_mio_var)
{
    if (p_mio_var) {
        p_mio_var->dec->run(p_mio_var->dcbuf);
    }
}

u8 mio_api_get_status(mio_ctrl_t *p_mio_var)
{
    u8 ret = MIO_ERR_NO_INIT;
    if (p_mio_var) {
        ret = p_mio_var->dec->get_status(p_mio_var->dcbuf);
        mio_api_printf("mio_status = 0x%x \n", ret);
    }
    return ret;
}

u32 mio_api_get_size(mio_ctrl_t *p_mio_var)
{
    if (p_mio_var) {
        mio_api_printf("miolen = 0x%x \n", p_mio_var->dec->get_total_size(p_mio_var->dcbuf));
        return p_mio_var->dec->get_total_size(p_mio_var->dcbuf);
    }
    return 0;
}

u8 mio_api_set_offset(mio_ctrl_t *p_mio_var, u32 offset)
{
    u8 ret = MIO_ERR_NO_INIT;
    if (p_mio_var) {
        ret = p_mio_var->dec->set_dac_offset(p_mio_var->dcbuf, offset);
        mio_api_printf("mio_offset = 0x%x \n", ret);
    }
    return ret;
}


#ifdef MIO_USB_DEBUG
#include "dev_manage/drv_manage.h"
#include "usb_device.h"
/* #include "sdmmc/sd_host_api.h" */
#include "dev_pc.h"
#include "vm/vm.h"
#include "dac/dac_api.h"

#define MIO_USB_TASK_NAME	"ScriptTask"

typedef struct {
    u8 buf[512];
    u32 sector;
} flashdata_t;

typedef struct __mio_usb_file {
    u32 start_addr;
    u32 fptr;
    u32 fsize;
    u32 offset;
    u16 rate;
    flashdata_t win;
} mio_usb_file_t;


#define FLASH_BLOCK_SIZE        (64*1024)
#define FLASH_SECTOR_SIZE       (4*1024)

u8 hook_block_erase(u32 addr)
{
    sfc_erase(BLOCK_ERASER, addr);
    return 0;
}
u8 hook_sector_erase(u32 addr)
{
//    printf("erase=0x%x \n", addr);

    sfc_erase(SECTOR_ERASER, addr);
    return 0;
}
void hook_write_data(void *buf, u32 addr, tu16 len)
{
//    printf("write=0x%x \n", addr);
//    printf_buf(buf,len);

    sfc_write(buf, addr, len);
}
void hook_read_data(void *buf, u32 addr, tu16 len)
{
    sfc_read(buf, addr, len);

//    printf("read=0x%x \n", addr);
//    printf_buf(buf,len);
}

static FRESULT move_window_flashdata(u32 sector, flashdata_t *win_buf)
{
    FRESULT res = FR_OK;
    //mbr_deg("move_window : %x  %x\n",win_buf->sector,sector);
    if (win_buf->sector != sector) {
        ///CLR_WDT();
        if (res == FR_OK) {
//            printf("\n win_buf->buf = %x \n", win_buf->buf);
//            printf("\n win_buf->sector = %x \n", win_buf->sector);
//            printf("\n sector = %x \n", sector);
//			res = ( FRESULT)win_buf->fs->disk_read(win_buf->fs->hdev,win_buf->start, sector);
#if 0
            res = fs_api_read(cache, win_buf->buf, sector);
#else
            hook_read_data(win_buf->buf, sector * 512, 512);
            res = FR_OK;
#endif
//			printf_buf(win_buf->buf, 512);
            if (res != FR_OK) {
                return res;
            }
            win_buf->sector = sector;
        }
    }
    return res;
}

static void phy_flashdata_read(void *hdl, u8 *buf, u32 addr, u16 length)
{
    mio_usb_file_t *pmiofile = hdl;
    u64 t_addr = 0;
    u32 sector;
    u32 sec_offset;
    u32 len0;
    u32 len = length;
    t_addr = addr;
    u8 _xdata *ptr;
    ptr = buf;
//    printf("buf : %08x\r",(u32)buf);
    while (len) {
        sector = t_addr / 512;
//        printf("\n sector = %x \n", sector);
//        printf("\n pmiofile->win.sector = %x \n", pmiofile->win.sector);
//        printf("\n pmiofile->win.buf = %x \n", pmiofile->win.buf);
        move_window_flashdata(sector, &pmiofile->win);
        sec_offset = t_addr % 512;
        if (len > (512 - sec_offset)) {
            len0 = 512 - sec_offset;
        } else {
            len0 = len;
        }
        memcpy(ptr, &pmiofile->win.buf[sec_offset], len0);
        len -= len0;
        t_addr += len0;
        ptr += len0;
    }
}


static u16 mio_usb_fs_read(void *p_f_hdl, u8 *buff, u16 btr)
{
    u16 rlen;
    u32 clust;
    mio_usb_file_t *pmiofile = p_f_hdl;

    clust = (pmiofile->fsize - pmiofile->fptr);
    if (btr > clust) {
        rlen = clust;
        memset(buff + rlen, 0, btr - rlen);            //将不够的部分清0
        btr = clust;		/* Truncate btr by remaining bytes */
    }
    rlen = btr;
    phy_flashdata_read(p_f_hdl, buff, pmiofile->fptr + pmiofile->start_addr, rlen);

    pmiofile->fptr += rlen;

    return rlen;
}
static bool mio_usb_fs_seek(void *p_f_hdl, u8 type, u32 offsize)
{
    mio_usb_file_t *pmiofile = p_f_hdl;
    if (type == SEEK_SET) {
        if (offsize > pmiofile->fsize) {
            return FR_RW_ERROR;
        }
        pmiofile->fptr = offsize;
    } else {
        if (pmiofile->fsize < (offsize + pmiofile->fptr)) {
            return FR_RW_ERROR;
        }
        pmiofile->fptr += offsize;
    }
    return FR_OK;
}
static u32 mio_usb_fs_tell(void *p_f_hdl)
{
    mio_usb_file_t *pmiofile = p_f_hdl;
    return pmiofile->fptr;
}

static mio_ctrl_t *mio_usb_api_start(void *file_hdl, u32 start_addr)
{
    mio_file_t miofile;

    miofile.filehdl     = file_hdl;
    miofile.seek        = mio_usb_fs_seek;
    miofile.read        = mio_usb_fs_read;
    miofile.tell        = mio_usb_fs_tell;

    return mio_api_start0(&miofile, start_addr, 0, MIO_USB_TASK_NAME);
}
static u8 mio_usb_set_sr(u16 rate)
{
    switch (rate) {
    case 44100:
    case 48000:
    case 32000:
    case 22050:
    case 24000:
    case 16000:
    case 11025:
    case 12000:
    case 8000:
        dac_set_samplerate(rate, 0);
        return 0;
    default:
        break;
    }
    return -1;
}

OS_MUTEX mio_usb_ctrl_mutex;
static inline int mio_usb_ctrl_mutex_init(void)
{
    static u8 mio_api_flag = 0;
    if (mio_api_flag) {
        return 0;
    }
    mio_api_flag = 1;
    return os_mutex_create(&mio_usb_ctrl_mutex);
}
int mio_usb_ctrl_mutex_enter(void)      ///<申请信号量
{
    mio_usb_ctrl_mutex_init();
    return os_mutex_pend(&mio_usb_ctrl_mutex, 0);
}
int mio_usb_ctrl_mutex_exit(void)       ///<释放信号量
{
    mio_usb_ctrl_mutex_init();
    return os_mutex_post(&mio_usb_ctrl_mutex);
}

/* #define SPIFLASH_ALIGN_SIZE     (64*1024) */
/* #define VM_START_ADDR_ALIGN     (((FLASH_VM_START)/SPIFLASH_ALIGN_SIZE)*SPIFLASH_ALIGN_SIZE) */
/* #define MIO_USB_FLASH_LEN       (SPIFLASH_ALIGN_SIZE*8) */
/* #define MIO_USB_FLASH_START     (VM_START_ADDR_ALIGN-MIO_USB_FLASH_LEN) */

static u32 MIO_USB_FLASH_START = 0;
static u32 MIO_USB_FLASH_LEN = 0;

void music_usb_mio_cap(u32 start, u32 len)
{
    MIO_USB_FLASH_START	= ((start + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;
    MIO_USB_FLASH_LEN 	= (len / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;
}

volatile mio_ctrl_t *g_mio_var;
mio_usb_file_t mio_usb_file;
void music_usb_mio_close(void)
{
    mio_ctrl_t *p_mio = (mio_ctrl_t *)g_mio_var;
//    mio_usb_ctrl_mutex_enter();
    if (g_mio_var) {
        g_mio_var = NULL;
        mio_api_close(&p_mio);
    }
//    mio_usb_ctrl_mutex_exit();
}
u8 music_usb_mio_start(u32 dac_offset)
{
    u8 ret = MIO_ERR_NO_INIT;
    mio_ctrl_t *p_mio;
    music_usb_mio_close();

    memset(&mio_usb_file, 0, sizeof(mio_usb_file_t));
    mio_usb_file.win.sector = (u32) - 1;
    mio_usb_file.start_addr = MIO_USB_FLASH_START;

    mio_usb_file.fsize = MIO_USB_FLASH_LEN;//init

    if (MIO_USB_FLASH_LEN < (FLASH_BLOCK_SIZE * 8)) {
        printf(" music usb mio flash size not enough \n");
        printf("cur:0x%x , need:0x%x \n", MIO_USB_FLASH_LEN, (FLASH_BLOCK_SIZE * 8));
        while (1);
    }

    p_mio = mio_usb_api_start(&mio_usb_file, 0);
    if (p_mio) {
        mio_usb_file.fsize = p_mio->dec->get_total_size(p_mio->dcbuf);
        mio_usb_file.rate  = p_mio->dec->get_rate(p_mio->dcbuf);
        mio_api_printf("fsize = 0x%x \n", mio_usb_file.fsize);
        mio_api_printf("rate = %d \n", mio_usb_file.rate);
        if ((mio_usb_file.fsize == 0)
            || (mio_usb_file.fsize > (MIO_USB_FLASH_LEN))
            || (mio_usb_set_sr(mio_usb_file.rate))
           ) {
            mio_api_printf("fsize or rate err \n");
            mio_api_close(&p_mio);
            return ret;
        }
        ret = mio_api_set_offset(p_mio, dac_offset);
        if (ret) {
            mio_api_printf("set_offset err \n");
            mio_api_close(&p_mio);
            return ret;
        }
    }
    p_mio->status |= MIO_API_STU_USB_DEG;
    g_mio_var = p_mio;
    return ret;
}
void music_usb_read(void)
{
    mio_usb_ctrl_mutex_enter();
    if (g_mio_var) {
        mio_ctrl_read_data((mio_ctrl_t *)g_mio_var);
    }
    mio_usb_ctrl_mutex_exit();
}
void music_usb_start(void)
{
    mio_api_printf("start=%d \n", mio_usb_file.offset);
    mio_usb_ctrl_mutex_enter();
    music_usb_mio_start(mio_usb_file.offset);
    mio_usb_ctrl_mutex_exit();
}
void music_usb_stop(void)
{
    mio_usb_ctrl_mutex_enter();
    music_usb_mio_close();
    mio_usb_ctrl_mutex_exit();
}
void music_usb_tell_start(u32 offset)
{
    mio_usb_file.offset = offset;
    os_taskq_post_msg(MIO_USB_TASK_NAME, 1, MSG_MIO_START);
}
void music_usb_tell_close(void)
{
    os_taskq_post_msg(MIO_USB_TASK_NAME, 1, MSG_MIO_STOP);
}

/* #define USB_SLAVE_PUTS */
#ifdef USB_SLAVE_PUTS
#define susb_puts           puts
#define susb_printf         printf
#define susb_printf_buf     printf_buf
#define susb_put_u32hex0    put_u32hex0
#else
#define susb_puts(...)
#define susb_printf(...)
#define susb_printf_buf(...)
#define susb_put_u32hex0(...)
#endif

//一级命令
#define SCSI_MIO_DEBUG      0XFE

//二级命令
enum {
    USB_MIO_GET_SPACE = 0xf1,
    USB_MIO_DATA,
    USB_MIO_START,
    USB_MIO_STOP,
    USB_MIO_DATA_START,
};

#define GET_ADDR_BIG_ENDIAN(addr,pRx)    \
    ((u8 *) &addr)[0] = pRx->CBW.LBA[0];\
    ((u8 *) &addr)[1] = pRx->CBW.LBA[1];\
    ((u8 *) &addr)[2] = pRx->CBW.LBA[2];\
    ((u8 *) &addr)[3] = pRx->CBW.LBA[3];

#define GET_ADDR_LITTLE_ENDIAN(addr,pRx)    \
    ((u8 *) &addr)[0] = pRx->CBW.LBA[3];\
    ((u8 *) &addr)[1] = pRx->CBW.LBA[2];\
    ((u8 *) &addr)[2] = pRx->CBW.LBA[1];\
    ((u8 *) &addr)[3] = pRx->CBW.LBA[0];

#define GET_LEN_BIG_ENDIAN(len, pRx)    \
    ((u8 *) &len)[2] = pRx->CBW.Reserved;\
    ((u8 *) &len)[3] = pRx->CBW.LengthH;

#define GET_LEN_LITTLE_ENDIAN(len, pRx)    \
    ((u8 *) &len)[1] = pRx->CBW.Reserved;\
    ((u8 *) &len)[0] = pRx->CBW.LengthH;

u8 mio_flash_buf[512] AT(.data_usb);
#define ep1_rx_buffer1  mio_flash_buf
#define ep1_rx_buffer2  mio_flash_buf

extern u8 MCU_SRAMToUSB(void *pBuf, u16 uCount);
extern u8 MCU_USBToSRAM(void *pBuf, u16 uCount, sUSB_SLAVE_MASS *mass_var);
#define hook_MCU_SRAMToUSB  MCU_SRAMToUSB
#define hook_MCU_USBToSRAM  MCU_USBToSRAM

static u32 usb_flash_addr;

void hook_flash_write(void *buf, tu16 len)
{
    u32 rlen = 0;
    while (len) {
        rlen = usb_flash_addr % FLASH_SECTOR_SIZE;
        if (rlen + len > FLASH_SECTOR_SIZE) {
            rlen = FLASH_SECTOR_SIZE - rlen;
        } else {
            rlen = len;
        }
        if (usb_flash_addr % FLASH_SECTOR_SIZE == 0) {
//            susb_printf("erase 0x%x \n", usb_flash_addr);
            hook_sector_erase(usb_flash_addr);
        }
//        susb_printf("write 0x%x \n", usb_flash_addr);
        hook_write_data(buf, usb_flash_addr, rlen);
        usb_flash_addr += rlen;
        buf += rlen;
        len -= rlen;
    }
}


void RBC_GetMioSpace(sUSB_BULK_ONLY_RX *pRx, sUSB_BULK_ONLY_S *pS)
{
    u8 ret;
    u8 cnt = 0;
    u32 len = MIO_USB_FLASH_LEN;

    ep1_rx_buffer2[cnt++] = SCSI_MIO_DEBUG ;
    ep1_rx_buffer2[cnt++] = USB_MIO_GET_SPACE;

#ifdef BIG_ENDIAN
    ep1_rx_buffer2[cnt++] = len;
    ep1_rx_buffer2[cnt++] = len >> 8;
    ep1_rx_buffer2[cnt++] = len >> 16;
    ep1_rx_buffer2[cnt++] = len >> 24;
#elif defined(LITTLE_ENDIAN)
    ep1_rx_buffer2[cnt++] = len >> 24;
    ep1_rx_buffer2[cnt++] = len >> 16;
    ep1_rx_buffer2[cnt++] = len >> 8;
    ep1_rx_buffer2[cnt++] = len;
#endif

    ret = hook_MCU_SRAMToUSB(ep1_rx_buffer2, 16);
    susb_printf("SramToUsb=%d\n", ret);
}
void RBC_MioDataStart(sUSB_BULK_ONLY_RX *pRx, sUSB_BULK_ONLY_S *pS)
{
    u8 ret;
    usb_flash_addr = MIO_USB_FLASH_START;

    ep1_rx_buffer2[0] = SCSI_MIO_DEBUG ;
    ep1_rx_buffer2[1] = USB_MIO_DATA_START;
    ep1_rx_buffer2[2] = 0;

    ret = hook_MCU_SRAMToUSB(ep1_rx_buffer2, 16);
    susb_printf("SramToUsb=%d\n", ret);
}
void RBC_MioData(void *var)
{
    sUSB_BULK_ONLY_RX   *pRx = &(((sUSB_SLAVE_MASS *)var)->USB_bulk_only_Rx);
    sUSB_BULK_ONLY_S    *pS  = &(((sUSB_SLAVE_MASS *)var)->USB_bulk_only_S);

    u8 ret;
    tu16 crc;
    tu16 len = 0;
    tu16 crc1;

#ifdef BIG_ENDIAN
    GET_LEN_BIG_ENDIAN(len, pRx);
#endif

#ifdef  LITTLE_ENDIAN
    GET_LEN_LITTLE_ENDIAN(len, pRx);
#endif
    susb_printf("len=%d\n", len);

    crc = (u16)pRx->CBW.Null[0] << 8 | pRx->CBW.XLength;

    ret = hook_MCU_USBToSRAM(ep1_rx_buffer1, len, var);
//    susb_printf_buf(ep1_rx_buffer1, len);
//    susb_printf("SramToUsb=%d\n", ret);

//    crc1 = CRC16(ep1_rx_buffer1, len);
//    if(crc != crc1)
//    {
//        susb_printf("CRC ERROR, crc0=0x%x, crc1=0x%x \n", crc, crc1);
//    }
    hook_flash_write(ep1_rx_buffer1, len);
}

void RBC_MioStart(sUSB_BULK_ONLY_RX *pRx, sUSB_BULK_ONLY_S *pS)
{
    u8 ret;
    u32 offset ;

#ifdef BIG_ENDIAN
    GET_ADDR_BIG_ENDIAN(offset, pRx);
#endif

#ifdef  LITTLE_ENDIAN
    GET_ADDR_LITTLE_ENDIAN(offset, pRx);
#endif

    susb_printf("offset=0x%x\n", offset);

    ep1_rx_buffer2[0] = SCSI_MIO_DEBUG ;
    ep1_rx_buffer2[1] = USB_MIO_START;
    ep1_rx_buffer2[2] = 0;

    ret = hook_MCU_SRAMToUSB(ep1_rx_buffer2, 16);
    susb_printf("SramToUsb=%d\n", ret);

//    music_usb_mio_start(offset);
    music_usb_tell_start(offset);
}

void RBC_MioStop(sUSB_BULK_ONLY_RX *pRx, sUSB_BULK_ONLY_S *pS)
{
    u8 ret;

    ep1_rx_buffer2[0] = SCSI_MIO_DEBUG ;
    ep1_rx_buffer2[1] = USB_MIO_STOP;
    ep1_rx_buffer2[2] = 0;

    ret = hook_MCU_SRAMToUSB(ep1_rx_buffer2, 16);
    susb_printf("SramToUsb=%d\n", ret);

//    music_usb_mio_close();
    music_usb_tell_close();
}

// SCSI_INTERCEPT_SUCC  ok
// SCSI_INTERCEPT_FAIL  other cmd
// SCSI_INTERCEPT_UNKNOW_WRITE
SUSB_SCSI_INTERCEPT_RESULT mio_scsi_intercept(void *var)
{
    sUSB_BULK_ONLY_RX   *pRx = &(((sUSB_SLAVE_MASS *)var)->USB_bulk_only_Rx);
    sUSB_BULK_ONLY_S    *pS  = &(((sUSB_SLAVE_MASS *)var)->USB_bulk_only_S);

    if (pRx->CBW.operationCode != 0xfe) {
        return SCSI_INTERCEPT_FAIL;
    }

    switch (pRx->CBW.LUN) {
    case USB_MIO_GET_SPACE:
        susb_puts("USB_MIO_GET_SPACE\n");
        RBC_GetMioSpace(pRx, pS);
        susb_puts("a_USB_MIO_GET_SPACE\n");
        break;

    case USB_MIO_DATA:
        susb_puts("USB_MIO_DATA\n");
        RBC_MioData(var);
        susb_puts("a_USB_MIO_DATA\n");
        break;

    case USB_MIO_DATA_START:
        susb_puts("USB_MIO_DATA_START\n");
        RBC_MioDataStart(pRx, pS);
        susb_puts("a_USB_MIO_DATA_START\n");
        break;

    case USB_MIO_START:
        susb_puts("USB_MIO_START\n");
        RBC_MioStart(pRx, pS);
        susb_puts("a_USB_MIO_START\n");
        break;

    case USB_MIO_STOP:
        susb_puts("USB_MIO_STOP\n");
        RBC_MioStop(pRx, pS);
        susb_puts("a_USB_MIO_STOP\n");
        break;

    default:
        return SCSI_INTERCEPT_FAIL;
//            break;
    }

    return SCSI_INTERCEPT_SUCC;
//    return SCSI_INTERCEPT_UNKNOW_WRITE;
}


#endif


#endif

