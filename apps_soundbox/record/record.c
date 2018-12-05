#include "sdk_cfg.h"
#include "common/app_cfg.h"
#include "common/msg.h"
#include "common/includes.h"
#include "common/common.h"
#include "rtos/task_manage.h"
/* #include "play_sel/play_sel.h" */
#include "sdmmc/sd_host_api.h"
#include "encode/dev_write_api.h"
#include "encode/mp2_encode_api.h"
#include "encode/encode.h"
#include "record.h"
#include "dac/dac_api.h"
#include "ui/ui_api.h"
#include "rec_key.h"
#include "sys_detect.h"
#include "rec_ui.h"
#include "key_drv/key.h"
#include "clock_api.h"
#include "clock.h"
#include "toy_rec_module.h"
#include "vm/vm_api.h"
/* #include "music_ui.h" */
/* #include "music.h" */
/* #include "linein.h" */
#include "notice/notice_player.h"
#include "script_switch.h"
#ifdef TOYS_EN
#include "encode/encode_flash.h"
#include "irq_api.h"
#endif /*TOYS_EN*/


#ifdef ENCODE_FLASH_API
#include "common/flash_cfg.h"
/* #define FLASH_VM_START		app_use_flash_cfg.vmif_addr */
/* #define REC_FLASH_S_ADDR   (((FLASH_VM_START-64*1024*2+64*1024-1)/(64*1024))*(64*1024)) */
/* #define REC_FLASH_MAX_LEN  (((FLASH_VM_START-REC_FLASH_S_ADDR+4*1024-1)/(4*1024))*(4*1024)) */
volatile u8 enc_flash_work = 0;
volatile u16 enc_flash_key = 0xffff;
#endif

#define BT_REC_SR		8000L		//不能修改
#define FM_REC_SR		32000L		//
#define AUX_REC_SR		48000L
#define MIX_REC_SR		48000L

extern void *malloc_fun(void *ptr, u32 len, char *info);
extern void free_fun(void **ptr);
extern volatile u8 new_lg_dev_let;

#if REC_EN

//录音格式选择
#define	REC_WAV  0
#define	REC_MP3  1
///K歌宝录音，只支持WAV
#define REC_FILE_TYPE    REC_MP3//REC_MP3 1：录MP3文件      REC_WAV 0：录WAV文件

//是否开启循环录音
#define RECYCLE_REC_EN   0

#if RECYCLE_REC_EN
//开启循环录音的循环个数
#define RECYCLE_REC_MAX 10
#else
#define RECYCLE_REC_MAX 0xffff
#endif


const u16 enc_bitrate_h[] = {
    ///>(44100), (48000), (32000) 采样率可配置以下比特率
    32, 48, 56, 64, 80, 96, 112, 128
};
const u16 enc_bitrate_l[] = {
    ///>(44.1K/2,/4),  (48k/2,/4),  (32K/2,/4) 采样率可配置以下比特率
    8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128
};
const char rec_folder_name[8] = "/JL_REC";                   //录音文件夹  //仅支持一层目录

#if REC_FILE_TYPE
const char rec_file_name[21] =   "/JL_REC/FILE0000.MP3";      //MP3录音文件名（含路径）
#else
const char rec_file_name[21] =   "/JL_REC/FILE0000.WAV";      //ADPCM录音文件名（含路径）
#endif

u32 rec_fname_cnt;       //录音文件名计数
u32 rec_total_number;    //录音文件数计数

REC_CTL *g_rec_ctl;
RECORD_OP_API *rec_mic_api;

/* #if REC_BT_EN */
/* s16 *g_bt_buf_ptr; */
/* #endif */

extern ENC_OPS *get_mpl3_ops();
extern ENC_OPS *get_mp2_ops();
extern ENC_OPS *get_enadpcm_ops();
/* extern ENC_OPS_t *get_mp2_opst(); */
/* extern ENC_OPS_t *get_enadpcm_opst(); */

ENC_OPS *(*enc_ops_list[MAX_FORMAT])(void) AT(.const_tab);
ENC_OPS *(*enc_ops_list[MAX_FORMAT])(void) = {
    NULL,       //标记

#if ENCODE_MP2_EN
//    get_mpl3_ops,
    get_mp2_ops,
#endif

#if ENCODE_ADPCM_EN
    get_enadpcm_ops,
#endif
};

/* ENC_OPS_t *(*enc_ops_list_t[MAX_FORMAT])(void) AT(.const_tab); */
/* ENC_OPS_t *(*enc_ops_list_t[MAX_FORMAT])(void) = */
/* { */
/* NULL,       //标记 */

/* #if ENCODE_MP2_EN */
/* get_mp2_opst, */
/* #endif */

/* #if ENCODE_ADPCM_EN */
/* get_enadpcm_opst, */
/* #endif */
/* }; */

#if BT_REC_EN
extern RECORD_OP_API *rec_bt_api ;
cbuffer_t bt_adc_cb;
u8 *bt_adc_cbuf;
#define BT_REC_BUF_LEN 1024
void bt_rec_buf_init(void)
{
    u32 bt_adc_cbuf_len;
    enc_puts("bt_rec buf init\n");

    bt_adc_cbuf_len = BT_REC_BUF_LEN;
    bt_adc_cbuf = malloc(BT_REC_BUF_LEN);
    ASSERT(bt_adc_cbuf);
    cbuf_init(&bt_adc_cb, bt_adc_cbuf, bt_adc_cbuf_len);
}
void bt_rec_buf_release(void)
{
    enc_printf("temp = %x", bt_adc_cbuf);
    if (bt_adc_cbuf != NULL) {
        free(bt_adc_cbuf);
    }
    bt_adc_cbuf = NULL;
    cbuf_clear(&bt_adc_cb);
}
void bt_rec_buf_write(s16 *buf, u32 len)
{
#if BT_KTV_EN
    return;//
#else
    if ((rec_bt_api != NULL) && (rec_bt_api->rec_ctl != NULL) && (rec_bt_api->rec_ctl->enable == ENC_STAR)) {
        if (cbuf_is_write_able(&bt_adc_cb, len)) {
            /* putchar('g'); */
            cbuf_write(&bt_adc_cb, buf, len);
        }
    }
#endif
}
/*----------------------------------------------------------------------------*/
/**@brief  蓝牙录音数据源获取
   @param  无
   @return 无
   @note   void bt_rec_get_data(s16 *buf,u32 len,)//录音获取pcm数据函数
*/
/*----------------------------------------------------------------------------*/
void rec_get_dac_data(RECORD_OP_API *rec_api, s16 *buf, u32 len) //录音获取pcm数据函数
{
    u8 bt_rec_buf[0x40];//dac_buf_len
    s16 *dp = (s16 *)bt_rec_buf;
    s16 adc_buf[0x20];
    u32 rec_point;

    s32 valuetemp;

    if ((rec_api != NULL) && (rec_api->rec_ctl != NULL) && (rec_api->rec_ctl->enable == ENC_STAR)) {
#if BT_KTV_EN
        memset((u8 *)adc_buf, 0x00, len);
#else
        if (cbuf_read(&bt_adc_cb, (u8 *)adc_buf, len / 2) != (len / 2)) {
            /* enc_printf("no_enough\n"); */
            memset((u8 *)adc_buf, 0x00, len);
        } else {
        }
#endif

        if (rec_api->rec_ctl->enable == ENC_STAR) {
            for (rec_point = 0; rec_point < 0x20; rec_point++) {
                valuetemp = adc_buf[rec_point] + buf[rec_point * 2]; //all left channel data
                if (valuetemp > 32767) {
                    valuetemp = 32767;
                } else if (valuetemp < -32768) {
                    valuetemp = -32768;
                }
                dp[rec_point] = (s16)valuetemp;
            }
            rec_api->rec_ctl->ladc.save_ladc_buf(&rec_api->rec_ctl->ladc, dp, 0, 0x40);

            for (rec_point = 0; rec_point < 0x20; rec_point++) {
                valuetemp = adc_buf[rec_point] + buf[rec_point * 2 + 1]; //all right channel data
                if (valuetemp > 32767) {
                    valuetemp = 32767;
                } else if (valuetemp < -32768) {
                    valuetemp = -32768;
                }
                dp[rec_point] = (s16)valuetemp;
            }
            rec_api->rec_ctl->ladc.save_ladc_buf(&rec_api->rec_ctl->ladc, dp, 1, 0x40);
        }
    }
}
#endif

extern s32 get_dev_next(u8 drv, u8 dev_type);

/*----------------------------------------------------------------------------*/
/**@brief  录音通道初始化、文件初始化
   @param  mode: 录制通道
   @return 无
   @note   RECORD_OP_API *rec_init(u8 mode)
*/
/*----------------------------------------------------------------------------*/
static RECORD_OP_API *rec_init(u8 mode)
{
    u8 emu_lg_dev = 0;//枚举的设备数，0，第一次枚举，1，第二次枚举，超过1不再枚举退出。
    s16 ret;
    u32 rec_file_num;
    u32 dev_next = 0;
    RECORD_OP_API *rec_op_api = NULL;

    rec_op_api = malloc_fun(rec_op_api, sizeof(RECORD_OP_API), 0);
    ASSERT(rec_op_api);

    rec_op_api->fop_api = enc_fop_api_init();   //文件操作信息初始化
    enc_printf("-------enc_fop_api_init %x------\n", rec_op_api->fop_api);

    rec_op_api->fw_api = fw_api_init();     //读写接口初始化
    enc_printf("------fw_api_init %x------\n", rec_op_api->fw_api);
    enc_printf("------new_lg_dev_let%x------\n", new_lg_dev_let);
    dev_next = get_dev_next(new_lg_dev_let, DEV_TYPE_MASS);
    enc_printf("------lg_dev_let%x------\n", dev_next);

_file_init:
    enc_puts("------encode_fs_open-----\n");
    ret = encode_fs_open(rec_op_api, DEV_SEL_SPEC, new_lg_dev_let, (char *)rec_folder_name, FA_CREATE_NEW);  //初始化文件系统
    enc_printf("--encode_fs_open ret = %x  \n", ret);

    if (-FILE_OP_ERR_LGDEV_NO_FIND == ret) {
        enc_printf("--DEV_SEL_FIRST--\n");
        ret = encode_fs_open(rec_op_api, DEV_SEL_FIRST, new_lg_dev_let, (char *)rec_folder_name, FA_CREATE_NEW);  //初始化文件系统
    }

    if (ret == FR_EXIST) {
#if REC_FILE_TYPE
        encode_get_fileinfo(rec_op_api, (char *)rec_folder_name, "MP3", &rec_file_num, &rec_total_number);
#else
        encode_get_fileinfo(rec_op_api, (char *)rec_folder_name, "WAV", &rec_file_num, &rec_total_number);
#endif
        enc_printf("rec_total_number = %d\n", rec_total_number);
        enc_printf("rec_file_num = %d\n", rec_file_num);
        //rec_fname_cnt = rec_file_num + 1;

        if ((rec_fname_cnt >= 9999) && (rec_total_number >= 9999)) { //以超出可使用文件名数
            goto _err_exit;
        }
    } else if (ret != FR_OK) { //初始化失败，释放资源
        if ((emu_lg_dev < 2) && (dev_next != 0)) {
            enc_printf("rec_folder_creater fail,nex_dev\n");
            emu_lg_dev ++;
            new_lg_dev_let = dev_next;
            goto _file_init;
        }
        enc_printf("rec_folder_creater fail,ret = %d\n", ret);
        goto _err_exit;
    } else {
        enc_printf("rec_folder_creater succ\n");
        rec_fname_cnt = 0;       //文件夹不存在，文件数清零
    }

    enc_puts("----enc_run_creat----\n");
    enc_run_creat();        //创建录音线程

    enc_puts("----rec_enc_init----\n");
    if (mode == ENC_MIC_CHANNEL) {
#if REC_FILE_TYPE
        rec_op_api->rec_ctl = init_enc(rec_op_api, mode, enc_bitrate_h[7], LADC_SR48000, MP2_FORMAT);    //申请录音资源并启动录音
#else
        rec_op_api->rec_ctl = init_enc(rec_op_api, mode, 1024, LADC_SR48000, ADPCM_FORMAT);    //申请录音资源并启动录音
#endif
    } else if (mode == ENC_FM_CHANNEL) {
        ///FM录音必须32K采样率

#if REC_FILE_TYPE
        rec_op_api->rec_ctl = init_enc(rec_op_api, mode, enc_bitrate_h[7], LADC_SR32000, MP2_FORMAT);    //申请录音资源并启动录音
#else
        rec_op_api->rec_ctl = init_enc(rec_op_api, mode, 1024, LADC_SR32000, ADPCM_FORMAT);    //申请录音资源并启动录音
#endif
    } else if (mode == ENC_LINE_LR_CHANNEL) {
        enc_printf("--------------ENC_ENC_LINE_LR_CHANNEL\n");
#if REC_FILE_TYPE
        rec_op_api->rec_ctl = init_enc(rec_op_api, mode, enc_bitrate_h[7], LADC_SR48000, MP2_FORMAT);    //申请录音资源并启动录音
#else
        rec_op_api->rec_ctl = init_enc(rec_op_api, mode, 1024, LADC_SR48000, ADPCM_FORMAT);    //申请录音资源并启动录音
#endif
    } else if (mode == ENC_BT_CHANNEL) {
        ///蓝牙通话录音必须8K采样率
#if REC_FILE_TYPE
        rec_op_api->rec_ctl = init_enc(rec_op_api, mode, enc_bitrate_l[5], dac_get_samplerate(), MP2_FORMAT);    //申请录音资源并启动录音
#else
        rec_op_api->rec_ctl = init_enc(rec_op_api, mode, 1024, dac_get_samplerate(), ADPCM_FORMAT);    //申请录音资源并启动录音
#endif
    }
    if (rec_op_api->rec_ctl != NULL) {
        rec_op_api->rec_ctl->file_info.cut_head_ms = (toy_rec_parameter->rec_head_cut_ms) * 5; //这里5倍是因为下面乘了5下面的单位是0.2ms
        rec_op_api->rec_ctl->file_info.cut_tail_ms = (toy_rec_parameter->rec_tail_cut_ms) / 10; //下面的单位是10ms
    }

    g_rec_ctl = rec_op_api->rec_ctl;
    enc_printf("cut head=%d*0.2ms tail=%d*10ms\n", g_rec_ctl->file_info.cut_head_ms, g_rec_ctl->file_info.cut_tail_ms);

    enc_printf("----rec_op_api %x----\n", rec_op_api);
    return rec_op_api;

_err_exit:
    enc_printf("----err_exit----\n");
    encode_fs_close(rec_op_api);
    enc_printf("----encode_fs_close----\n");
    free_fop(rec_op_api);
    enc_printf("----free_fop----\n");
    free(rec_op_api);
    enc_printf("----return NULL;----\n");
    return NULL;
}

#ifdef ENCODE_FLASH_API
static RECORD_OP_API *rec_init_flash(u8 mode)
{
    u8 emu_lg_dev = 0;//枚举的设备数，0，第一次枚举，1，第二次枚举，超过1不再枚举退出。
    s16 ret;
    u32 rec_file_num;
    RECORD_OP_API *rec_op_api = NULL;


    if (mode != ENC_MIC_CHANNEL) {
        return NULL;
    }

    rec_op_api = malloc_fun(rec_op_api, sizeof(RECORD_OP_API), 0);
    ASSERT(rec_op_api);

    rec_op_api->fop_api = enc_fop_api_init();   //文件操作信息初始化
    enc_printf("-------enc_fop_api_init %x------\n", rec_op_api->fop_api);

    rec_op_api->fw_api = fw_api_init();     //读写接口初始化
    enc_printf("------fw_api_init %x------\n", rec_op_api->fw_api);

    enc_puts("------encode_fs_open-----\n");
    ret = encode_fs_open(rec_op_api, DEV_SEL_SPEC, 'A', (char *)rec_folder_name, FA_CREATE_NEW);  //初始化文件系统
    enc_printf("--encode_fs_open ret = %x  \n", ret);

    if (-FILE_OP_ERR_LGDEV_NO_FIND == ret) {
        goto _err_exit;
    }

    if (ret == FR_EXIST) {
    } else if (ret != FR_OK) { //初始化失败，释放资源
        enc_printf("rec_folder_creater fail,ret = %d\n", ret);
        goto _err_exit;
    } else {
        enc_printf("rec_folder_creater succ\n");
        rec_fname_cnt = 0;       //文件夹不存在，文件数清零
    }


    enc_puts("----rec_enc_init----\n");
    if (mode == ENC_MIC_CHANNEL) {
        rec_op_api->rec_ctl = init_enc(rec_op_api, mode, 1024, LADC_SR8000, ADPCM_FORMAT);    //申请录音资源并启动录音
    }
    if (rec_op_api->rec_ctl != NULL) {
        rec_op_api->rec_ctl->file_info.cut_head_ms = (toy_rec_parameter->rec_head_cut_ms) * 5; //这里5倍是因为下面乘了5下面的单位是0.2ms
        rec_op_api->rec_ctl->file_info.cut_tail_ms = (toy_rec_parameter->rec_tail_cut_ms) / 10; //下面的单位是10ms
    }

    g_rec_ctl = rec_op_api->rec_ctl;
    enc_printf("cut head=%d*0.2ms tail=%d*10ms\n", g_rec_ctl->file_info.cut_head_ms, g_rec_ctl->file_info.cut_tail_ms);

    enc_printf("----rec_op_api %x----\n", rec_op_api);
    return rec_op_api;

_err_exit:
    enc_printf("----err_exit----\n");
    encode_fs_close(rec_op_api);
    enc_printf("----encode_fs_close----\n");
    free_fop(rec_op_api);
    enc_printf("----free_fop----\n");
    free(rec_op_api);
    enc_printf("----return NULL;----\n");
    return NULL;
}

static tbool enc_flash_exit(void)
{
    if ((enc_flash_key != 0xffff) && (enc_flash_key != 0xff)) {

        if (enc_flash_key == MSG_REC_STOP || enc_flash_key == MSG_REC_PLAYBACK) {
            enc_printf("\n rec end key down %x\n", enc_flash_key);
            return true;
        } else {
            enc_flash_key = 0xffff;
            return false;
        }
    }
    return false;
}


static u32 g_rec_flash_s_addr;
static u32 g_rec_flash_max_len;
void encode_flash_zone(u32 addr, u32 len)
{
    g_rec_flash_s_addr = addr;
    g_rec_flash_max_len = len;
}

static bool start_rec_flash(RECORD_OP_API **rec_api)
{
    RECORD_OP_API *rec_op_api;
    s16 ret;
    u32 tmp;

    if (!rec_api) {
        return false;
    }

    rec_op_api = *rec_api;

    ret = encode_file_open(rec_op_api, (char *)rec_file_name, FA_CREATE_NEW | FA_WRITE); //初始化文件

    if (FR_OK == ret) { //文件创建成功
        rec_op_api->rec_ctl->curr_device = (u8)file_operate_ctl(FOP_GET_PHYDEV_INFO, rec_op_api->fop_api, 0, 0);
        enc_printf("--start_flash_rec-- device %d--- \n", rec_op_api->rec_ctl->curr_device);
        rec_op_api->rec_ctl->file_info.file_del_size = 1; ///0:不自动删除文件 , 非0:删除文件 删除成功自动归0

        int var_ie0 = 0;
        int var_ie1 = 0;
        int var_tmp = 0;

        CPU_INT_DIS();

        enc_flash_key = 0xffff;
        enc_flash_work = 1;

        irq_read_all(&var_ie0, &var_ie1);
        var_tmp = (BIT(IRQ_DAC_IDX) | BIT(IRQ_TIME0_IDX));
        irq_write_all(var_tmp, 0);
        CPU_INT_EN();

        encode_flash_main(rec_op_api, g_rec_flash_s_addr, g_rec_flash_max_len, 1, enc_flash_exit);

        toy_rec_destroy(rec_api); //停止录音和释放资源

        enc_flash_work = 0;

        CPU_INT_DIS();
        irq_write_all(var_ie0, var_ie1);
        CPU_INT_EN();

        if (enc_flash_key != 0xffff) {
            u16 msg = enc_flash_key;
            enc_printf("\n++++++++ msg = %x\n", msg);
#if 0
            os_taskq_post_msg("MainTask", 1, msg);
#else
            extern void key_msg_sender(char *name, u16 msg);
            if (msg < MSG_MAIN_MAX) {
                /* os_taskq_post_msg(MAIN_TASK_NAME, 1, msg); */
                key_msg_sender(MAIN_TASK_NAME, msg);
            } else if (keymsg_task_name) {
                /* os_taskq_post_msg(keymsg_task_name, 1, msg); */
                key_msg_sender(keymsg_task_name, msg);
            }
#endif
        }

        enc_printf("\n encode_flash_main end \n\n");

#if 0
        u32 addr = g_rec_flash_s_addr;
        u8 buf[512];
        sfc_read(buf, addr, 512);
        enc_printf_buf(buf, 512);
        addr += 512;
        sfc_read(buf, addr, 512);
        enc_printf_buf(buf, 512);
        while (1);
#endif
    } else { //其他错误
        enc_printf("file_open ret :%x\n", ret);
        toy_rec_destroy(rec_api); //停止录音和释放资源
        return false;
    }
    return true;
}
#endif


//从vm获取最后一个录音文件序号
u16 get_last_rec_filenum_from_VM(void)
{
    u16 last_rec_file_num = 0;
    vm_read_api(VM_REC_FILE_NUM, &last_rec_file_num);
    if (last_rec_file_num > 0xfff0) {
        return 0;
    } else {
        return last_rec_file_num;
    }
}

void set_rec_name(char *name, u8 cnt)
{
    if (name == NULL) {
        enc_puts("\n rec file name addr err");
        return;
    }

    char cnt1 = (cnt / 1000) % 10;
    char cnt2 = (cnt / 100) % 10;
    char cnt3 = (cnt / 10) % 10;
    char cnt4 = (cnt / 1) % 10;
    name[12] = (char)(cnt1 + '0');
    name[13] = (char)(cnt2 + '0');
    name[14] = (char)(cnt3 + '0');
    name[15] = (char)(cnt4 + '0');
}




static bool  start_rec(RECORD_OP_API **rec_api)
{
    RECORD_OP_API *rec_op_api;
    s16 ret;
    u16 err_cnt;
    u32 tmp;
    u16 rec_cycle_num;

    char rec_file_name_temp[sizeof(rec_file_name)];

    if (!rec_api) {
        return false;
    }

    memcpy(rec_file_name_temp, rec_file_name, sizeof(rec_file_name));

    rec_op_api = *rec_api;
    err_cnt = 0;    //err_reset


#if RECYCLE_REC_EN
    if (rec_fname_cnt >= RECYCLE_REC_MAX) {
        vm_read_api(VM_REC_FILE_NUM, &rec_cycle_num);
        enc_printf("rec_cycle_num = %d\n", rec_cycle_num);
        rec_cycle_num += 1;
        if (rec_cycle_num >= RECYCLE_REC_MAX) {
            rec_cycle_num = 0;
        }
        rec_fname_cnt = rec_cycle_num;
    }
#endif

_get_file_name:
    if (rec_fname_cnt > 9999) {
        rec_fname_cnt = 0;
    }

    tmp = rec_fname_cnt / 1000;
    rec_file_name_temp[12] = (char)(tmp + '0');

    tmp = rec_fname_cnt % 1000 / 100;
    rec_file_name_temp[13] = (char)(tmp + '0');

    tmp = rec_fname_cnt % 100 / 10;
    rec_file_name_temp[14] = (char)(tmp + '0');

    tmp = rec_fname_cnt % 10;
    rec_file_name_temp[15] = (char)(tmp + '0');

    enc_printf("--  encode_file_open %s  --\n", rec_file_name_temp);
    ret = encode_file_open(rec_op_api, rec_file_name_temp, FA_CREATE_NEW | FA_WRITE); //初始化文件

    if (FR_OK == ret) { //文件创建成功
#if RECYCLE_REC_EN
        rec_cycle_num = (u16)rec_fname_cnt;
        vm_write_api(VM_REC_FILE_NUM, &rec_cycle_num);
#else
        vm_write_api(VM_REC_FILE_NUM, &rec_fname_cnt); //记录录音序号
#endif
        rec_op_api->rec_ctl->curr_device = (u8)file_operate_ctl(FOP_GET_PHYDEV_INFO, rec_op_api->fop_api, 0, 0);
//        music_ui.ui_curr_device = (u32)file_operate_ctl(FOP_GET_PHYDEV_INFO,rec_op_api->fop_api,0,0);
//        printf("--start_rec-- device %d--- \n",music_ui.ui_curr_device);
        rec_op_api->rec_ctl->file_info.file_del_size = 1; ///0:不自动删除文件 , 非0:删除文件 删除成功自动归0
        os_taskq_post_msg(ENC_RUN_TASK_NAME, 1, rec_op_api); //启动录音
        while (rec_op_api->rec_ctl->enable != ENC_STAR) { //等待录音启动线程
            os_time_dly(1);
        }
        UI_menu(MENU_REFRESH);
        return true;
    } else if (FR_EXIST == ret) { ////文件已经存在
        enc_printf("--%s__FR_EXIST--\n", rec_file_name_temp);

        err_cnt++;
        if (err_cnt >= 9999) {
            enc_printf("file_open err 9999\n");
            exit_rec_api(rec_api); //停止录音和释放资源
            return false;
        }
#if RECYCLE_REC_EN
        ret = rec_file_delete(rec_op_api);
        if (ret != FR_OK) {
            enc_printf("rec_delete err ret = %x\n", ret);
        }
        rec_file_close(rec_op_api);
        if (ret != FR_OK) {
            enc_printf("rec_close err ret = %x\n", ret);
        }
#else
        rec_fname_cnt++;
#endif

        goto _get_file_name;
    } else { //其他错误
        enc_printf("file_openerr ret :%x\n", ret);
        exit_rec_api(rec_api); //停止录音和释放资源
        return false;
    }
}


/*----------------------------------------------------------------------------*/
/**@brief  RECORD 消息处理流程
   @param  rec_api_p：record 参数指针
   @param  msg：record 消息
   @return 无
   @note   void rec_msg_deal(RECORD_OP_API **rec_api_p, int *msg)
*/
/*----------------------------------------------------------------------------*/
void rec_msg_deal(RECORD_OP_API **rec_api_p, int *msg)
{
    switch (msg[0]) {
    case MSG_REC_START:
        enc_puts("MSG_REC_START\n");
#ifdef ENCODE_FLASH_API
        u8 encode_dev = new_lg_dev_let;
        if (encode_dev == 0) {
            encode_dev = 'A';
        }
#endif
        if (*rec_api_p == NULL) {
            enc_printf("curr_task_id %d\n", script_get_id());

            if (unlikely(0))
                ;
#ifdef ENCODE_FLASH_API
            else if (encode_dev == 'A') {
#if MIC_REC_EN
                /* if (compare_task_name(REC_TASK_NAME)) { */
                if (script_get_id() == SCRIPT_ID_REC) {
                    (*rec_api_p) = rec_init_flash(ENC_MIC_CHANNEL);
                }
#endif
            }
#endif

#if MIC_REC_EN
            /* else if (compare_task_name(REC_TASK_NAME)) { */
#if(SCRIPT_REC_EN)
            else if (script_get_id() == SCRIPT_ID_REC) {
                (*rec_api_p) = rec_init(ENC_MIC_CHANNEL);
            }
#endif
#endif

#if FM_REC_EN
            /* else if (compare_task_name(FM_TASK_NAME)) { */
            else if (script_get_id() == SCRIPT_ID_FM) {
                (*rec_api_p) = rec_init(ENC_FM_CHANNEL);
            }
#endif

#if AUX_REC_EN
            /* else if (compare_task_name(LINEIN_TASK_NAME)) { */
#if(SCRIPT_LINEIN_EN)
            else if (script_get_id() == SCRIPT_ID_LINEIN) {
                (*rec_api_p) = rec_init(ENC_LINE_LR_CHANNEL);
            }
#endif
#endif

#if BT_REC_EN
            /* else if (compare_task_name("BtStackTask")) { */
            /* bt_rec_buf_init(); */
            else if (script_get_id() == SCRIPT_ID_BT) {
                (*rec_api_p) = rec_init(ENC_BT_CHANNEL);
            }
#endif

            else {
                (*rec_api_p) = NULL;    //err
            }
        }

        if (*rec_api_p) {
            if ((*rec_api_p)->rec_ctl->enable == ENC_STOP) {
#if BT_REC_EN
                /* if (compare_task_name("BtStackTask")) { */
                if (script_get_id() == SCRIPT_ID_BT) {
                    bt_rec_buf_init();
                } else
#endif
#if FM_REC_EN
                    /* if (compare_task_name(FM_TASK_NAME)) { */
                    if (script_get_id() == SCRIPT_ID_FM) {
                        set_sys_freq(FM_REC_Hz);
                    }
#endif

                SET_UI_REC_OPT(UI_REC_OPT_START);
#ifdef ENCODE_FLASH_API
                if (encode_dev == 'A') {
                    start_rec_flash(rec_api_p); //启动录音
                } else
#endif
                {
                    start_rec(rec_api_p); //启动录音
                }
            }
        }
        UI_menu(MENU_REFRESH);
        break;


    case MSG_REC_STOP:
        enc_puts("--REC_MSG_REC_STOP\n");
        if (*rec_api_p) {
#if 0	//停止录音，不释放录音资源
            stop_rec(*rec_api_p);
            (*rec_api_p)->rec_ctl->lost_frame.black_lost_frame = 0;
            (*rec_api_p)->rec_ctl->lost_frame.front_lost_frame = 0;
#else	//停止录音释放所有录音资源
            exit_rec_deal(rec_api_p); //停止录音和释放资源
#endif

            rec_fname_cnt++;//录音文件名序号
        }
//            UI_menu(MENU_MAIN);
        SET_UI_REC_OPT(UI_REC_OPT_STOP);
        UI_menu(MENU_REFRESH);
        break;

    case MSG_REC_PP:
        if (*rec_api_p) {
            if ((*rec_api_p)->rec_ctl->enable == ENC_STAR) {
                (*rec_api_p)->rec_ctl->enable = ENC_PAUS;
                enc_puts("enc_pause\n");
                SET_UI_REC_OPT(UI_REC_OPT_PAUSE);
            } else if ((*rec_api_p)->rec_ctl->enable == ENC_PAUS) {
                (*rec_api_p)->rec_ctl->enable = ENC_STAR;
                enc_puts("enc_star\n");
                SET_UI_REC_OPT(UI_REC_OPT_START);
            } else {
                enc_puts("--MSG_ENCODE_PP_err\n");
            }
        }
        UI_menu(MENU_REFRESH);
        break;

    case MSG_ENCODE_ERR:
        enc_puts("--REC_MSG_ENCODE_ERR\n");
        exit_rec_api(rec_api_p); //停止录音和释放资源
        SET_UI_REC_OPT(UI_REC_OPT_ERR);
        UI_menu(MENU_REFRESH);
        break;

    case SYS_EVENT_DEV_OFFLINE:
        enc_puts("--enc_SYS_EVENT_DEV_OFFLINE--\n");
        if (*rec_api_p) {
            enc_puts("DEV_OFFLINE-00\n");
            if ((*rec_api_p)->fop_api->cur_lgdev_info->lg_hdl->phydev_item == (void *)msg[1]) {
                enc_puts("DEV_OFFLINE-01\n");

                /* if((*rec_api_p)->rec_ctl->enable != ENC_STOP) */
                /* { */
                /* (*rec_api_p)->rec_ctl->enable = ENC_STOP; */
                /* SET_UI_REC_OPT(UI_REC_OPT_ERR); */
                /* } */

                /* UI_menu(MENU_REFRESH); */

                SET_UI_REC_OPT(UI_REC_OPT_ERR);
                UI_menu(MENU_REFRESH);
                exit_rec_api(rec_api_p); //停止录音和释放资源
#if BT_REC_EN
                /* if (compare_task_name("BtStackTask")) { */
                if (script_get_id() == SCRIPT_ID_BT) {
                    bt_rec_buf_release();
                }
#endif
            } else {
                ///<非解码设备，删除该逻辑设备
                enc_puts("DEV_OFFLINE-02\n");
                break;
            }
        } else {
            UI_menu(MENU_REFRESH);
        }

        break;

    default:
        break;
    }
}

void exit_rec_deal(void *p)
{
    exit_rec(p);

#if BT_REC_EN
    /* if (compare_task_name("BtStackTask")) { */
    if (script_get_id() == SCRIPT_ID_BT) {
        bt_rec_buf_release();
    }
#endif

    set_sys_freq(SYS_Hz);
    SET_UI_REC_OPT(UI_REC_OPT_STOP);
}

/*----------------------------------------------------------------------------*/
/**@brief  RECORD 控制任务
   @param  p：任务间参数传递指针
   @return 无
   @note   static void rec_task(void *p)
*/
/*----------------------------------------------------------------------------*/
//static void rec_task(void *p)
//{
//    int msg[3];
//    u32 tone_flag = 0;
//
//    puts("\n************************RECORD TASK********************\n");
//
//    /* tone_play_by_name(REC_TASK_NAME, 1, BPF_REC_MP3); //RECORD模式提示音播放 */
//    notice_player_play_by_path(REC_TASK_NAME, BPF_REC_MP3, NULL, NULL);
//
//    while (1) {
//        memset(msg, 0x00, sizeof(msg));
//        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
//        if (MSG_HALF_SECOND != msg[0]) {
//            printf("rec_task get_msg, 0x%x\n", msg[0]);
//        }
//        clear_wdt();
//        switch (msg[0]) {
//        case SYS_EVENT_DEL_TASK:
//            enc_puts("RECORD_SYS_EVENT_DEL_TASK\n");
//            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
////                UI_menu(MENU_WAIT);
//                ui_close_rec();
//                exit_rec_deal(&rec_mic_api); //停止录音和释放资源
//                /* play_sel_stop();   //关闭提示音 */
//                notice_player_stop();
//                dac_channel_off(DAC_DIGITAL_AVOL, FADE_ON);
//
//                os_task_del_res_name(OS_TASK_SELF);
//            }
//            break;
//
//        case SYS_EVENT_PLAY_SEL_END: //提示音结束
//            enc_puts("RECORD_SYS_EVENT_PLAY_SEL_END\n");
//            /* play_sel_stop(); //关闭提示音 */
//            notice_player_stop();
//            tone_flag = 1;
//
//        case MSG_REC_INIT:
//            enc_puts("MSG_REC_INIT\n");
//            dac_channel_on(DAC_DIGITAL_AVOL, FADE_ON);
//            toy_rec_parameter_set(ENC_MIC_CHANNEL, 600, 50); //设置录音通道、开头结尾要去掉的数据
//            printf("cut_head=%dms,cut_tail=%dms\n", toy_rec_parameter->rec_head_cut_ms, toy_rec_parameter->rec_tail_cut_ms);
//            // ui_open_rec(&rec_mic_api, sizeof(RECORD_OP_API**));
//            break;
//        case MSG_REC_START:
//            puts("toy rec init nad start\n");
//            rec_mic_api = toy_rec_hdl_create_and_start(); //创建录音句柄
//            if (rec_mic_api == NULL) {
//                puts("start_rec_err!!!! !!!!\n");
//            }
//            printf("cut_head=%dms,cut_tail=%dms\n", rec_mic_api->rec_ctl->file_info.cut_head_ms, rec_mic_api->rec_ctl->file_info.cut_tail_ms);
//            puts("toy rec run!!\n");
//            //toy_rec_run(&rec_mic_api);
//            break;
//        case MSG_REC_STOP:
//            puts("toy rec stop!!\n");
//            toy_rec_destroy(&rec_mic_api);
//            break;
//        case MSG_REC_PP:
//            puts("toy rec pp!!\n");
//            toy_rec_pp(&rec_mic_api);
//            break;
//#if LCD_SUPPORT_MENU
//        case MSG_ENTER_MENULIST:
//            UI_menu_arg(MENU_LIST_DISPLAY, UI_MENU_LIST_ITEM);
//            break;
//#endif
//
//        case MSG_HALF_SECOND:
//            if (updata_enc_time(rec_mic_api)) {
//                UI_menu(MENU_HALF_SEC_REFRESH);
//            }
//            break;
//
//        default:
//            if (tone_flag) {
//                rec_msg_deal(&rec_mic_api, msg);    //record 流程
//            }
//            break;
//        }
//    }
//}

/*----------------------------------------------------------------------------*/
/**@brief  ENCODE任务创建
   @param  priv：任务间参数传递指针
   @return 无
   @note   static void encode_task_init(void *priv)
*/
/*----------------------------------------------------------------------------*/
//static void rec_task_init(void *priv)
//{
//    u32 err;
//
//    //初始化ENCODE APP任务
//    err = os_task_create_ext(rec_task,
//                             (void *)0,
//                             TaskEncodePrio,
//                             30
//#if OS_TIME_SLICE_EN > 0
//                             , 1
//#endif
//                             , REC_TASK_NAME
//                             , 1024 * 4
//                            );
//
//    //按键映射
//    if (OS_NO_ERR == err) {
//        key_msg_reg(REC_TASK_NAME, &rec_mode_key);
//        // key_msg_register(REC_TASK_NAME, adkey_msg_rec_table, iokey_msg_rec_table, irff00_msg_rec_table, NULL);
//    }
//}


/*----------------------------------------------------------------------------*/
/**@brief  ENCODE 任务删除退出
   @param  无
   @return 无
   @note   static void encode_task_exit(void)
*/
/*----------------------------------------------------------------------------*/
//static void rec_task_exit(void)
//{
//    if (os_task_del_req(REC_TASK_NAME) != OS_TASK_NOT_EXIST) {
//        os_taskq_post_event(REC_TASK_NAME, 1, SYS_EVENT_DEL_TASK);
//        do {
//            OSTimeDly(1);
//        } while (os_task_del_req(REC_TASK_NAME) != OS_TASK_NOT_EXIST);
//        enc_puts("del_encode_task: succ\n");
//    }
//}


/*----------------------------------------------------------------------------*/
/**@brief  ENCODE 任务信息
   @note   const struct task_info rec_task_info
*/
/*----------------------------------------------------------------------------*/
//TASK_REGISTER(rec_task_info) = {
//    .name = REC_TASK_NAME,
//    .init = rec_task_init,
//    .exit = rec_task_exit,
//};




/*----------------------------------------------------------------------------*/
/**@brief  录音初始化及启动
   @param
   @param
   @return 录音开始成功:录音句柄;录音开始失败:NULL
   @note
*/
/*----------------------------------------------------------------------------*/
RECORD_OP_API *toy_rec_hdl_create_and_start(void)
{
    RECORD_OP_API *rec_op_api;

#ifdef ENCODE_FLASH_API
    u8 encode_dev = 0;
    if (file_operate_get_total_phydev() == 0) {
        encode_dev = 'A';
    }
    if (encode_dev == 'A') {
        rec_op_api =  rec_init_flash(toy_rec_parameter->rec_mode);
    } else
#endif
        rec_op_api =  rec_init(toy_rec_parameter->rec_mode);

    if (rec_op_api) {
        if (rec_op_api->rec_ctl->enable == ENC_STOP) {
            //bt 模式

#if BT_REC_EN
            if (toy_rec_parameter->rec_mode == ENC_BT_CHANNEL) {
                bt_rec_buf_init();
            }
#endif

#if FM_REC_EN
            if (toy_rec_parameter->rec_mode == ENC_FM_CHANNEL) { //FM 模式
                set_sys_freq(FM_REC_Hz);
            }
#endif
#ifdef ENCODE_FLASH_API
            if (encode_dev == 'A') {
                if (!start_rec_flash(&rec_op_api)) {
                    return NULL;
                }
            } else
#endif
                if (!start_rec(&rec_op_api)) {
                    return NULL;
                }

        }
    }
    return rec_op_api;
}



/*----------------------------------------------------------------------------*/
/**@brief  录音暂停/启动
   @param  rec_api :当前录句柄地址
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void toy_rec_pp(RECORD_OP_API **rec_api)
{
    if (*rec_api) {
        if ((*rec_api)->rec_ctl->enable == ENC_STAR) {
            (*rec_api)->rec_ctl->enable = ENC_PAUS;
        } else if ((*rec_api)->rec_ctl->enable == ENC_PAUS) {
            (*rec_api)->rec_ctl->enable = ENC_STAR;
        } else {
            enc_puts("toy_rec_pp err!!!\n");
        }
    }
}



/*----------------------------------------------------------------------------*/
/**@brief  录音停止
   @param  rec_api :当前录句柄地址
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void toy_rec_destroy(RECORD_OP_API **rec_api)
{
    if (*rec_api == NULL) {
        return;
    }
    exit_rec(rec_api);

#if BT_REC_EN
    if (toy_rec_parameter->rec_mode == ENC_BT_CHANNEL) {
        bt_rec_buf_release();
    }
#endif
}


//flash rec play
void toy_rec_file_play(void)
{
#ifdef ENCODE_FLASH_API
    enc_printf("flash rec addr = 0x%x \n", g_rec_flash_s_addr);
    notice_player_play_by_flash_rec_addr(0, g_rec_flash_s_addr, 0, 0);
#endif
}

#endif
