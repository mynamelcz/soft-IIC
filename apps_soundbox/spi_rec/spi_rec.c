/***********************************Jieli tech************************************************
  File : spi_rec.c
  By   : Huxi
  Email: xi_hu@zh-jieli.com
  date : 2016-11-30 14:30
********************************************************************************************/
#include "sdk_cfg.h"
#include "common/app_cfg.h"
#include "spi/spi1.h"
#include "spi/nor_fs.h"
#include "common/msg.h"
/* #include "task_manage.h" */
#include "cbuf/circular_buf.h"
#include "cpu/dac_param.h"
#include "dac/dac_track.h"
#include "cpu/crc_api.h"
#include "spi_rec/spi_rec.h"
#ifdef TOYS_EN
#include "encode/encode_flash.h"
#endif /*TOYS_EN*/


#ifndef AT_ENC_ISR
#define AT_ENC_ISR
#endif

#if SPI_REC_EN
extern void adpcm_coder_ms(char indata[], char outdata[], int *index, int *valpred);


//#define SPI_REC_USE_ZEBRA_RAM_ENABLE

#ifdef SPI_REC_USE_ZEBRA_RAM_ENABLE

extern void *zebra_malloc(u32 size);
extern void zebra_free(void *p);

#define spi_rec_malloc	zebra_malloc
#define spi_rec_free	zebra_free
#else
#define spi_rec_malloc	malloc
#define spi_rec_free	free
#endif//SPI_REC_USE_ZEBRA_RAM_ENABLE

#if (SPI1_FLASH_CHIP==NAND_FLASH)
#define SPI_NAND_PAGE_SIZE      2048L
#define SPI_NAND_SPARE_SIZE     128L
#define SPI_NAND_BLOCK_PAGE     64L
#define SPI_NAND_CHIP_BLOCK     2048L
#define SPI_NAND_BLOCK_SIZE     (SPI_NAND_PAGE_SIZE*SPI_NAND_BLOCK_PAGE)
#define SPI_NAND_CHIP_SIZE      (SPI_NAND_BLOCK_SIZE*SPI_NAND_CHIP_BLOCK)

#define NOR_FS_SECTOR_S     SPI_NAND_BLOCK_SIZE
#define NOR_FS_S_SECTOR     (1)
#define NOR_FS_E_SECTOR     (SPI_NAND_CHIP_SIZE/SPI_NAND_BLOCK_SIZE-1)

#else
#define NOR_FS_SECTOR_S     (4*1024L)
#define NOR_FS_S_SECTOR     (1)
#define NOR_FS_E_SECTOR     (1*1024L*1024L/NOR_FS_SECTOR_S-1)
#endif


RECFILESYSTEM recfs;
REC_FILE recfile;
u32 spirec_filenum;

static OS_EVENT spienc_sem;

#if (SPI1_EN==0)
extern void delay(u32 d);
static void spi1_cs_api(u8 cs)
{
    JL_PORTC->DIR &= ~BIT(2);
    cs ? (JL_PORTC->OUT |= BIT(2)) : (JL_PORTC->OUT &= ~BIT(2));
    delay(10);
}
#if (SPI1_FLASH_CHIP==NAND_FLASH)
static u8 *nand_getinfo_api(u16 len, u8 *iniok)
{
    u8 *buf;
    *iniok = 0;
    buf = malloc(len + 6);
    printf("\n** nand_getinfo_api **\n");
    memset(buf, 0, len + 6);
    *iniok = 1;
    return buf;
}
static void nand_saveinfo_api(u8 *buf, u16 len)
{
    printf("\n** nand_saveinfo_api **\n");
    printf_buf(buf, len + 6);
}

static bool nand_idinfo_api(u32 nandid, NANDFLASH_ID_INFO *info)
{
    printf("nandflash id = 0x%x \n", nandid);
    info->page_size          = SPI_NAND_PAGE_SIZE;
    info->page_spare_size    = SPI_NAND_SPARE_SIZE;
    info->block_page_num     = SPI_NAND_BLOCK_PAGE;
    info->chip_block_num     = SPI_NAND_CHIP_BLOCK;
    return true;
}
static bool nand_blockok_api(void (*readspare)(u16 block, u8 shift, u8 *buf, u8 len), u16 block)
{
    return true;
}
#endif
#endif

static void hook_spirec_eraser(u32 address)
{
#if (SPI1_FLASH_CHIP==NAND_FLASH)
    spi1flash_eraser(BLOCK_ERASER, address);
#else
    spi1flash_eraser(SECTOR_ERASER, address);
#endif
}
static s32 hook_spirec_read(u8 *buf, u32 addr, u32 len)
{
    return spi1flash_read(buf, addr, len);
}
static s32 hook_spirec_write(u8 *buf, u32 addr, u32 len)
{
    return spi1flash_write(buf, addr, len);
}


static u8 get_sec_size(u32 sec)
{
    u8 i = 0;
    while ((sec >>= 1) != 0) {
        i++;
    }
    return i;
}

static u32 init_recfs(void)
{
    memset(&recfs, 0, sizeof(RECFILESYSTEM));
    recfs.eraser = hook_spirec_eraser;
    recfs.read  = hook_spirec_read;
    recfs.write = hook_spirec_write;

    init_nor_fs(&recfs, NOR_FS_S_SECTOR, NOR_FS_E_SECTOR, get_sec_size(NOR_FS_SECTOR_S));
    recfs_scan(&recfs);
    return  recfs.index.index;
}

/*----------------------------------------------------------------------------*/
/**@brief   spiflash录音初始化
   @param   void
   @return  void
   @author  huxi
   @note    void spiflash_rec_init()
*/
/*----------------------------------------------------------------------------*/
void spiflash_rec_init(void)
{
#if (SPI1_EN==0)
    spi1flash_set_chip(SPI1_FLASH_CHIP); //0-norflash, 1-nandflash
#if (SPI1_FLASH_CHIP==NAND_FLASH)
    nandflash_info_register(nand_getinfo_api, nand_saveinfo_api, nand_idinfo_api, nand_blockok_api);
#endif

    spi1_io_set(SPI1_POC);
    spi1_remap_cs(spi1_cs_api);
    /* spi1flash_init(SPI_2WIRE_MODE, 0, SPI_CLK_DIV16); */
    printf("\n--func=%s, line=%d\n", __FUNCTION__, __LINE__);
    u8 err = spi1flash_init(SPI_ODD_MODE, 0, SPI_CLK_DIV10);
    /* if (err == SPI_OFFLINE) { */
    /* return; */
    /* } */
#endif

    spirec_filenum = init_recfs();

    os_sem_create(&spienc_sem, 0);
}

#define SPIREC_TASK_NAME        "SpiRecTask"
#define ENC_RUN_TASK_NAME       "EncRunTask"
#define ENC_WFILE_TASK_NAME     "EncWFileTask"
#if 1
#define spirec_puts         puts
#define spirec_printf       printf
#else
#define spirec_puts(...)
#define spirec_printf(...)
#endif

typedef struct _SPIFS_API_IO {
    REC_FILE *f_p;
    u8(*seek)(REC_FILE *f_p, u8 type,  u32 offsize);
    u16(*read)(REC_FILE *f_p, u8 *buff, u16 len);
    u16(*write)(REC_FILE *f_p, u8 *buff, u16 len);
    u32(*close)(REC_FILE *f_p);
    void(*save_sr)(REC_FILE *f_p, u16 sr);
} spi_fs_io_t;

typedef struct {
    cbuffer_t   cbuf;
    u8          *buf;
    u32         buflen;
    u32         oncemin;
    int         index;
    int         valpred;
} spi_adpcm_t;

typedef struct {
    spi_fs_io_t     io;
    dac_save_t      dac;
    spi_adpcm_t     adpcm;
    u32             lost;
    u16				sr;
    u8              createfile;
    u8              w_once_flag;
    u8              err;
    volatile u8     stu;
    volatile u8     busy;
    volatile u8     w_busy;
} spirec_op_t;

enum {
    SPIREC_STOP,
    SPIREC_WORK,
    SPIREC_PAUSE,
};
#define SPIREC_ERR_W_END    BIT(0)


static void spi_save_dac_data(void *hdl, void *buf, u32 len)
{
    dac_save_t *p_dac = hdl;
    if (!hdl) {
        return;
    }
    spirec_op_t *p_spi_info = (spirec_op_t *)p_dac->priv;

    if (p_spi_info->stu != SPIREC_WORK) {
        return;
    }

//    if (cbuf_write(&(p_dac->cbuf),buf,len)==0)
//    {
//        p_spi_info->lost ++;
//    }
    cbuffer_t *cbuffer = &(p_dac->cbuf);
    if ((cbuffer->total_len - cbuffer->data_len) < len) {
        p_spi_info->lost ++;
    } else {
        cbuf_write(&(p_dac->cbuf), buf, len);
    }
    os_sem_set(&spienc_sem, 0);
    os_sem_post(&spienc_sem);
}


static void spirec_close(spirec_op_t *p_spi_info)
{
    if (p_spi_info->createfile) {
        p_spi_info->createfile = 0;
        p_spi_info->io.save_sr(p_spi_info->io.f_p, p_spi_info->sr);
        p_spi_info->io.close(p_spi_info->io.f_p);
        spirec_printf("\n recfile close.. lost=%d\n", p_spi_info->lost);
    }
}

static void spirec_init(spirec_op_t *p_spi_info)
{
    cbuf_init(&(p_spi_info->dac.cbuf), p_spi_info->dac.buf, p_spi_info->dac.buflen);
    p_spi_info->dac.save_buf = spi_save_dac_data;
    p_spi_info->dac.priv = p_spi_info;

    cbuf_init(&(p_spi_info->adpcm.cbuf), p_spi_info->adpcm.buf, p_spi_info->adpcm.buflen);
    p_spi_info->adpcm.index = 0;
    p_spi_info->adpcm.valpred = 0;

    spirec_filenum = create_recfile(&recfs, &recfile);
    spirec_printf("spirec_filenum=%d \n", spirec_filenum);

    p_spi_info->createfile = 1;
    p_spi_info->w_once_flag = 0;
    p_spi_info->err = 0;
    p_spi_info->lost = 0;

    p_spi_info->io.f_p = &recfile;

    p_spi_info->io.seek = recf_seek;
    p_spi_info->io.read = recf_read;
    p_spi_info->io.write = recf_write;
    p_spi_info->io.close = close_recfile;
    p_spi_info->io.save_sr = recf_save_sr;
}

static u16 spienc_adpcm_opt(spirec_op_t *p_spi_info)
{
//    u32 len;
    u16 dac_buf_tmp[DAC_SAMPLE_POINT];

    if (DAC_SAMPLE_POINT > (p_spi_info->adpcm.cbuf.total_len - p_spi_info->adpcm.cbuf.data_len)) {
        return 0;
    }
    if (0 == cbuf_read(&(p_spi_info->dac.cbuf), dac_buf_tmp, DAC_SAMPLE_POINT * 2)) {
        return 0;
    }

//    printf("\n\nwrite1,idx=%d,val=%d\n\n",p_spi_info->adpcm.index,p_spi_info->adpcm.valpred);
//    printf_buf(dac_buf_tmp,DAC_SAMPLE_POINT*2);

    adpcm_coder_ms((char *)dac_buf_tmp, (char *)dac_buf_tmp, &(p_spi_info->adpcm.index), &(p_spi_info->adpcm.valpred));


    if (cbuf_is_write_able(&(p_spi_info->adpcm.cbuf), DAC_SAMPLE_POINT / 2) >= (DAC_SAMPLE_POINT / 2)) {
        cbuf_write(&(p_spi_info->adpcm.cbuf), dac_buf_tmp, DAC_SAMPLE_POINT / 2);
    } else {
        //putchar('L');
    }

    if (p_spi_info->adpcm.cbuf.data_len >= p_spi_info->adpcm.oncemin) {
        os_taskq_post_msg(ENC_WFILE_TASK_NAME, 1, (void *)p_spi_info);
    }
    return DAC_SAMPLE_POINT / 2;
}

static u16 spienc_write_flash(spirec_op_t *p_spi_info, u16 limit_len)
{
    u32 len = 0;
    u8 *addr = 0;

    len = p_spi_info->adpcm.cbuf.data_len;
    len = (len / p_spi_info->adpcm.oncemin) * p_spi_info->adpcm.oncemin;
    if (len < limit_len) {
        return 0;
    }

    addr = cbuf_read_alloc(&(p_spi_info->adpcm.cbuf), &len);
    if (addr == NULL || len == 0) {
        return 0;
    }

//    printf("write\n\n");
//    printf_buf(addr,len);
    if (p_spi_info->io.write(p_spi_info->io.f_p, addr, len) == 0) {
        p_spi_info->err |= SPIREC_ERR_W_END;
    }

    cbuf_read_updata(&(p_spi_info->adpcm.cbuf), len);

    return len;
}


static void spienc_run_task(void *p)
{
    spirec_op_t *p_spi_info = 0;

    while (1) {
        if (p_spi_info) {
            p_spi_info->busy = 0;
        }
        os_taskq_pend(0, 1, (int *)&p_spi_info);

        if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
            os_task_del_res_name(OS_TASK_SELF);
        }

        if (p_spi_info) {
            p_spi_info->busy = 1;
            while (1) {
                if (p_spi_info->stu == SPIREC_STOP) {
                    break;
                }
                if (spienc_adpcm_opt(p_spi_info) == 0) {
                    os_sem_pend(&spienc_sem, 0);
                }
            }
        }
    }

}

static void spienc_write_file_task(void *p)
{
    spirec_op_t *p_spi_info = 0;
    u16 len;

    while (1) {
        if (p_spi_info) {
            p_spi_info->w_busy = 0;
        }
        os_taskq_pend(0, 1, (int *)&p_spi_info);

        if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
            os_task_del_res_name(OS_TASK_SELF);
        }

        if (p_spi_info) {
            p_spi_info->w_busy = 1;
            while (1) {
                if (p_spi_info->stu == SPIREC_STOP) {
                    break;
                }
                if (p_spi_info->w_once_flag == 0) {
                    len = spienc_write_flash(p_spi_info, p_spi_info->adpcm.oncemin - 0x10);
                } else {
                    len = spienc_write_flash(p_spi_info, p_spi_info->adpcm.oncemin);
                }

                if (len) {
                    p_spi_info->w_once_flag = 1;
                } else {
                    if (p_spi_info->err) {
                        p_spi_info->stu = SPIREC_STOP;
                        //                    os_taskq_post_event((char *)SPIREC_TASK_NAME, 1, MSG_ENCODE_ERR);
//                        os_taskq_post_event((char *)"ScriptTask", 1, MSG_ENCODE_ERR);
                    }
                    break;
                }
            }
        }
    }

}


static void spienc_run_creat(void)
{
    u32 err;
    //初始化ENCODE RUN任务

    err = os_task_create_ext(spienc_run_task, (void *)0, TaskEncRunPrio, 10, ENC_RUN_TASK_NAME, 2 * 1024);
//   err = os_task_create(spienc_run_task,
//						(void *)0,
//						TaskEncRunPrio,
//						10
//#if OS_TIME_SLICE_EN > 0
//						, 1
//#endif
//						, ENC_RUN_TASK_NAME);

    if (err) {
        spirec_puts("spienc_run_task err\n");
    }

    //初始化写文件任务
    err = os_task_create_ext(spienc_write_file_task, (void *)0, TaskEncWFPrio, 10, ENC_WFILE_TASK_NAME, 2 * 1024);
//   err = os_task_create(spienc_write_file_task,
//						(void *)0,
//						TaskEncWFPrio,
//						30
//#if OS_TIME_SLICE_EN > 0
//						, 1
//#endif
//						, ENC_WFILE_TASK_NAME);

    if (err) {
        spirec_puts("spienc_write_file_task err\n");
    }
}



void os_task_exit(s8 *name)
{
    if (os_task_del_req(name) != OS_TASK_NOT_EXIST) {
        os_taskq_post_event(name, 1, SYS_EVENT_DEL_TASK);
        do {
            OSTimeDly(1);
        } while (os_task_del_req(name) != OS_TASK_NOT_EXIST);
    }
}


static void spienc_run_exit(void)
{
    os_task_exit(ENC_RUN_TASK_NAME);
    os_task_exit(ENC_WFILE_TASK_NAME);
}


static void spienc_stop(spirec_op_t *p_spi_info)
{
    /* u32 cpu_sr; */
    /* OS_ENTER_CRITICAL_FUN(&cpu_sr); */

    OS_ENTER_CRITICAL();
    g_p_adc = NULL;
    OS_EXIT_CRITICAL();
    /* OS_EXIT_CRITICAL_FUN(&cpu_sr); */

    p_spi_info->stu = SPIREC_STOP;
    if (p_spi_info->busy || p_spi_info->w_busy) {
        os_sem_set(&spienc_sem, 0);
        os_sem_post(&spienc_sem);

        while (p_spi_info->busy || p_spi_info->w_busy) {
            os_time_dly(1);
        }
    }
    spirec_close(p_spi_info);
}

static void spienc_start(spirec_op_t *p_spi_info)
{
    spienc_stop(p_spi_info);
    spirec_init(p_spi_info);
    p_spi_info->stu = SPIREC_WORK;
    os_taskq_post_event(ENC_RUN_TASK_NAME, 1, (void *)p_spi_info);

    /* u32 cpu_sr; */
    /* OS_ENTER_CRITICAL_FUN(&cpu_sr); */
    OS_ENTER_CRITICAL();
    g_p_adc = &(p_spi_info->dac);
    OS_EXIT_CRITICAL();
    /* OS_EXIT_CRITICAL_FUN(&cpu_sr); */

    while (1) {
        if (p_spi_info->stu != SPIREC_WORK) {
            break;
        }
        if (p_spi_info->busy) {
            break;
        }
        os_time_dly(1);
    }
}

static volatile u8 spirec_alive;

static void spirec_task(void *p)
{
    int msg[3];

    u16 cur_dac_sr;
    spirec_puts("\n************************SPIREC TASK********************\n");

    spirec_op_t *p_spi_info = p;

    malloc_stats();

    p_spi_info->dac.buflen = SPIREC_DAC_BUF_LEN;

    p_spi_info->dac.buf = spi_rec_malloc(p_spi_info->dac.buflen);
    /* p_spi_info->dac.buf = malloc(p_spi_info->dac.buflen); */
    ASSERT(p_spi_info->dac.buf);

    cur_dac_sr = dac_get_samplerate();
    printf("cur dac sr ================%d\n", cur_dac_sr);
    if (cur_dac_sr > (16000L)) {
        p_spi_info->adpcm.buflen = SPI_ADPCM_HIGH_SAMPLERAT_W_BUF_LEN;
    } else {
        p_spi_info->adpcm.buflen = SPI_ADPCM_LOW_SAMPLERATE_W_BUF_LEN;
    }

    p_spi_info->adpcm.buf = malloc(p_spi_info->adpcm.buflen);
    ASSERT(p_spi_info->adpcm.buf);

    p_spi_info->adpcm.oncemin = SPI_ADPCM_W_BUF_MIN;

    spienc_run_creat();

    os_taskq_post_msg((char *)SPIREC_TASK_NAME, 1, MSG_REC_START);
    while (1) {
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            spirec_puts("ENCODE_SYS_EVENT_DEL_TASK\n");
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                spienc_stop(p_spi_info);
                spienc_run_exit();
                spi_rec_free(p_spi_info->dac.buf);
                /* free(p_spi_info->dac.buf); */
                free(p_spi_info->adpcm.buf);
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;
        case MSG_REC_START:
            spirec_puts("MSG_REC_START\n");
            spienc_start(p_spi_info);
            break;
        case MSG_REC_PP:
            spirec_puts("MSG_REC_PP\n");
            if (p_spi_info->stu == SPIREC_WORK) {
                p_spi_info->stu = SPIREC_PAUSE;
            } else if (p_spi_info->stu == SPIREC_PAUSE) {
                p_spi_info->stu = SPIREC_WORK;
            }
            break;
        case MSG_ENCODE_ERR:
        case MSG_REC_STOP:
            spirec_puts("MSG_REC_STOP\n");
            spienc_stop(p_spi_info);
            break;
        }
    }
}


static spirec_op_t *spirec_ctrl = NULL;
/* static spirec_op_t spirec_ctrl; */
static void spirec_task_init(u16 sr)
{
    u32 err;

    spirec_alive = 1;

    printf("---------------------------spi rec handle size = %d\n", sizeof(spirec_op_t));
    spirec_ctrl = spi_rec_malloc(sizeof(spirec_op_t));
    if (spirec_ctrl == NULL) {
        return ;
    }
    memset((u8 *)spirec_ctrl, 0, sizeof(spirec_op_t));
    /* memset(spirec_ctrl, 0, sizeof(spirec_op_t)); */

    spirec_ctrl->sr = sr;
    //初始化ENCODE APP任务
    err = os_task_create(spirec_task,
                         /* (void *)&spirec_ctrl, */
                         (void *)spirec_ctrl,
                         TaskEncodePrio,
                         30
#if OS_TIME_SLICE_EN > 0
                         , 1
#endif
                         , SPIREC_TASK_NAME);

    if (err) {
        spirec_alive = 0;
        spi_rec_free(spirec_ctrl);
        spirec_printf("\n rec_task_init err = %x\n", err);
    }
}

static void spirec_task_exit(void)
{
    os_task_exit(SPIREC_TASK_NAME);
    spi_rec_free(spirec_ctrl);
}


/* TASK_REGISTER(spirec_task_info) = { */
/* .name = SPIREC_TASK_NAME, */
/* .init = spirec_task_init, */
/* .exit = spirec_task_exit, */
/* }; */

/*----------------------------------------------------------------------------*/
/**@brief   spiflash开始录音
   @param   void
   @return  void
   @author  huxi
   @note    void spirec_start(u16 sr)
*/
/*----------------------------------------------------------------------------*/
void spirec_start(u16 sr)
{
    if (spirec_alive) {
        spirec_task_exit();
        spirec_alive = 0;
    }

    spirec_task_init(sr);
}

/*----------------------------------------------------------------------------*/
/**@brief   spiflash结束录音
   @param   void
   @return  void
   @author  huxi
   @note    void spirec_stop()
*/
/*----------------------------------------------------------------------------*/
void spirec_stop(void)
{
    if (spirec_alive) {
        spirec_printf("spirec stop ~\r");
        spirec_task_exit();
        spirec_alive = 0;
    }
}

extern u8 dac_buffer[DAC_BUF_LEN];

volatile dac_save_t *g_p_adc = NULL;
volatile u8 spirec_save_type = 0;

/*----------------------------------------------------------------------------*/
/**@brief   spiflash录音写数据（写buf）
   @param   *buf    写buf
   @param   len     写buf长度
   @return  void
   @author  huxi
   @note    void spirec_save_buf(void *buf, u16 len)
*/
/*----------------------------------------------------------------------------*/
AT_ENC_ISR void spirec_save_buf(void *buf, u16 len)
{
    if ((spirec_save_type == SAVE_TYPE_BUF) && (NULL != g_p_adc) && g_p_adc->save_buf) {
        g_p_adc->save_buf((void *)g_p_adc, buf, len);
    }
}
/*----------------------------------------------------------------------------*/
/**@brief   spiflash录音写数据（写dacbuf，固定长度DAC_BUF_LEN）
   @param   *dacbuf    写dacbuf
   @return  void
   @author  huxi
   @note    void spirec_save_dac(void *dacbuf)
*/
/*----------------------------------------------------------------------------*/
AT_ENC_ISR void spirec_save_dac(void *dacbuf)
{
    if ((spirec_save_type == SAVE_TYPE_DAC) && (NULL != g_p_adc) && g_p_adc->save_buf) {
#if (SPIREC_DAC_TACK==1)
        dual_to_single(dac_buffer, dacbuf, DAC_BUF_LEN);
        g_p_adc->save_buf((void *)g_p_adc, dac_buffer, DAC_BUF_LEN / 2);
#else
        g_p_adc->save_buf((void *)g_p_adc, dacbuf, DAC_BUF_LEN);
#endif
    }
}
/*----------------------------------------------------------------------------*/
/**@brief   spiflash录音写数据（写ladcbuf，每个buf固定长度DAC_BUF_LEN/2）
   @param   *buf0    ladcbuf0
   @param   *buf1    ladcbuf1
   @return  void
   @author  huxi
   @note    void spirec_save_ladc(void *buf0, void *buf1)
*/
/*----------------------------------------------------------------------------*/
AT_ENC_ISR void spirec_save_ladc(void *buf0, void *buf1)
{
    if ((spirec_save_type == SAVE_TYPE_LADC) && (NULL != g_p_adc) && g_p_adc->save_buf) {
        single_l_r_2_dual(dac_buffer, buf0, buf1, DAC_BUF_LEN / 2);
#if (SPIREC_DAC_TACK==1)
        dual_to_single(dac_buffer, dac_buffer, DAC_BUF_LEN);
        g_p_adc->save_buf((void *)g_p_adc, dac_buffer, DAC_BUF_LEN / 2);
#else
        g_p_adc->save_buf((void *)g_p_adc, dac_buffer, DAC_BUF_LEN);
#endif
    }
}

#endif

