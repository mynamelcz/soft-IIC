#include "notice_player.h"
#include "wav_play.h"
#include "script_switch.h"
#include "common/app_cfg.h"
#include "rtos/os_api.h"
#include "rtos/os_cfg.h"
#include "dac/dac_api.h"
#include "sound_mix.h"
#include "spi_rec.h"

//#define NOTICE_PLAYER_DEBUG_ENABLE

#ifdef NOTICE_PLAYER_DEBUG_ENABLE
#define notice_player_printf printf
#else
#define notice_player_printf(...)
#endif


#define NOTICE_PLAYER_PHY_TASK_NAME	"NOTICE_PLAYER_DEC_TASK"

#define NOTICE_MIX_KEY							"NOTICE_MIX"
#define NOTICE_WAV_PLAYER_MIX_BUF_SIZE			(2*1024L)
#define NOTICE_WAV_PLAYER_MIX_LIMIT				(50)

#define NOTICE_NORMAL_PLAYER_MIX_BUF_SIZE		(4*1024L)
#define NOTICE_NORMAL_PLAYER_MIX_LIMIT			(50)


typedef struct __NOTICE_PLAYER_OBJ {
    NOTICE_PLAYER_METHOD method;
    u32 total_file;
    u32 *file_p;
    void *father_name;
    u16 samp_backup;
    u8  manul_msg_post;
    u8  msg_post;
    u32 dev_let;
    u32 rpt_time;
    u32 delay;
    _DECODE_STATUS	status;
    SOUND_MIX_OBJ *mix_obj;
} NOTICE_PLAYER_OBJ;


typedef struct __NOTICE_PLAYER_P {
    NOTICE_PLAYER_METHOD method;
    void *father_name;
    void *priv;
    msg_cb _cb;
    u32 dev_let;
    u32 rpt_time;
    u32 delay;
    SOUND_MIX_OBJ *mix_obj;
} NOTICE_PLAYER_P;


static u8 notice_player_active = 0;
static NOTICE_PLAYER_OBJ *g_notice_obj = NULL;

#define SIZEOF_ALIN(var,al)     ((((var)+(al)-1)/(al))*(al))



//#define MUSIC_PLAYER_DEBUG_ENABLE

#ifdef MUSIC_PLAYER_DEBUG_ENABLE
#define music_player_printf printf
#else
#define music_player_printf(...)
#endif

#define SIZEOF_ALIN(var,al)     ((((var)+(al)-1)/(al))*(al))

//#define NOTICE_PLAYER_USE_ZEBRA_MEMERY_ENABLE
#ifdef NOTICE_PLAYER_USE_ZEBRA_MEMERY_ENABLE

extern void *zebra_malloc(u32 size);
extern void zebra_free(void *p);

void *notice_player_zebra_calloc(u32 count, u32 size)
{
    void *p;
    p = zebra_malloc(count * size);
    if (p) {
        /* zero the memory */
        memset(p, 0, count * size);
    }
    return p;
}

void notice_player_zebra_free(void *p)
{
    zebra_free(p);
}

#define notice_player_calloc		notice_player_zebra_calloc
#define notice_player_free			notice_player_zebra_free
#else
#define notice_player_calloc		calloc
#define notice_player_free			free
#endif//NOTICE_PLAYER_USE_ZEBRA_MEMERY_ENABLE


static OS_MUTEX notice_player_mutex;
static void notice_player_mutex_init(void)
{
    static u8 init = 0;
    if (init == 0) {
        init = 1;
        os_mutex_create(&notice_player_mutex);
    }
}
static inline int notice_player_mutex_enter(void)
{
    notice_player_mutex_init();
    os_mutex_pend(&notice_player_mutex, 0);
    return 0;
}
static inline int notice_player_mutex_exit(void)
{
    return os_mutex_post(&notice_player_mutex);
}


static void notice_player_end_msg_post(NOTICE_PLAYER_OBJ *notice_obj, u32 err)
{
    if ((notice_obj->father_name != NULL) && (notice_obj->msg_post == 0)) {
        os_taskq_post_event(notice_obj->father_name, 2, SYS_EVENT_PLAY_SEL_END, err);
        notice_obj->msg_post = 1;
        notice_obj->status = DECODER_STOP;
    }
}

static tbool notice_player_repeat_deal(NOTICE_PLAYER_OBJ *notice_obj, u32 *cur_file_cnt, u32 *cur_rpt_time)
{
    if (notice_obj == NULL) {
        return false;
    }
    if (notice_obj->total_file > 1) {
        (*cur_file_cnt)++;
        if ((*cur_file_cnt) >= notice_obj->total_file) {
            notice_player_printf("notice player player over !!\n");
            notice_player_end_msg_post(notice_obj, 0);
            /* if (notice_obj->father_name != NULL) { */
            /* os_taskq_post_event(notice_obj->father_name, 2, SYS_EVENT_PLAY_SEL_END, 0); */
            /* } */
            return false;
        } else {
            if (notice_obj->delay) {
                OSTimeDly(notice_obj->delay);
            }
        }
    } else if (notice_obj->total_file == 1) {
        u8 rpt = 0;
        if (notice_obj->rpt_time == 0) {
            rpt = 1;
        }
        if ((*cur_rpt_time)) {
            (*cur_rpt_time)--;
            if ((*cur_rpt_time)) {
                rpt = 1;
            }
        }
        if (rpt) {
            (*cur_file_cnt) = 0;
            if (notice_obj->delay) {
                OSTimeDly(notice_obj->delay);
            }
        } else {
            notice_player_end_msg_post(notice_obj, 0);
            /* if (notice_obj->father_name != NULL) { */
            /* os_taskq_post_event(notice_obj->father_name, 2, SYS_EVENT_PLAY_SEL_END, 0); */
            /* } */
            return false;
        }
    } else {
        notice_player_end_msg_post(notice_obj, (u32) - 1);
        /* if (notice_obj->father_name != NULL) { */
        /* os_taskq_post_event(notice_obj->father_name, 2, SYS_EVENT_PLAY_SEL_END, ERR_MUSIC_START_DEC); */
        /* } */
        return false;
    }
    return true;
}



static void notice_wav_player_task_deal(NOTICE_PLAYER_OBJ *notice_obj)
{
    u32 i, rpt_time;
    u32 file_cnt = 0;
    char *dir_path = NULL;
    int msg[6] = {0};
    tbool ret;
    notice_obj->method &= 0xffff;

    notice_player_printf("post msg flag = %d, method = %d\n", notice_obj->manul_msg_post, notice_obj->method);

    if (notice_obj->method == NOTICE_PLAYER_METHOD_BY_NUM
        || 	notice_obj->method == NOTICE_PLAYER_METHOD_BY_NUM_ARRAY) {
        dir_path = (char *)notice_obj->file_p[0];
        notice_obj->file_p++;
        notice_obj->total_file--;
    }

    rpt_time = notice_obj->rpt_time;

    WAV_PLAYER *w_play = NULL;
    w_play = wav_player_creat();
    ASSERT(w_play);

    if (__wav_player_set_output_to_mix(
            w_play,
            notice_obj->mix_obj,
            NOTICE_MIX_KEY,
            NOTICE_WAV_PLAYER_MIX_BUF_SIZE,
            NOTICE_WAV_PLAYER_MIX_LIMIT,
            DAC_DIGIT_TRACK_DEFAULT_VOL,
            DAC_DIGIT_TRACK_MAX_VOL) == false) {
        notice_player_printf("notice_player mix add fail!! %d\n", __LINE__);
    }

    os_taskq_post(NOTICE_PLAYER_TASK_NAME, 1, SYS_EVENT_BEGIN_DEC);
    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        if ((w_play != NULL) && (wav_player_get_status(w_play) == WAV_PLAYER_ST_PLAY)) {
            if (OSTaskQAccept(ARRAY_SIZE(msg), msg) == OS_Q_EMPTY) {
                ret = wav_player_decode(w_play);
                if (ret == false) {
                    msg[0] = SYS_EVENT_DEC_END;
                } else {
                    continue;
                }
            }
        } else {
            os_taskq_pend(0, ARRAY_SIZE(msg), msg);
            /* if(msg[0] != SYS_EVENT_DEL_TASK) */
            /* continue; */
        }

        /* need to deal msg */
        switch (msg[0]) {
        case SYS_EVENT_DEC_END:
            if (notice_player_repeat_deal(notice_obj, &file_cnt, &rpt_time) == false) {
                break;
            }
        case SYS_EVENT_BEGIN_DEC:
            if (notice_obj->method == NOTICE_PLAYER_METHOD_BY_NUM
                || notice_obj->method == NOTICE_PLAYER_METHOD_BY_NUM_ARRAY) {
                ret = wav_player_set_path(w_play, dir_path, (u32)notice_obj->file_p[file_cnt]);
            } else if (notice_obj->method == NOTICE_PLAYER_METHOD_BY_PATH
                       || notice_obj->method == NOTICE_PLAYER_METHOD_BY_PATH_ARRAY) {
                ret = wav_player_set_path(w_play, (const char *)notice_obj->file_p[file_cnt], 0);
            } else if (notice_obj->method == NOTICE_PLAYER_METHOD_BY_SPI_REC_NUM) {
                ret = spirec_wav_play(w_play, (u32)notice_obj->file_p[file_cnt]);
            } else {
                break;
            }
            if (ret == false) {
                notice_player_printf("notice player wav player fail\n");
                notice_player_end_msg_post(notice_obj, (u32) - 1);
                /* if (notice_obj->father_name != NULL) { */
                /* os_taskq_post_event(notice_obj->father_name, 2, SYS_EVENT_PLAY_SEL_END, ERR_MUSIC_START_DEC); */
                /* } */
            } else {
                ret = wav_player_play(w_play);
            }
            notice_obj->status = (_DECODE_STATUS)wav_player_get_status(w_play);
            break;

        case MSG_MUSIC_PP:
            notice_obj->status = (_DECODE_STATUS)wav_player_pp(w_play);
            break;

        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                notice_player_printf("wav_player_task_del \r");
                wav_player_destroy(&w_play);
                if (notice_obj->manul_msg_post) {
                    notice_player_end_msg_post(notice_obj, 0);
                }
                /* if (manul_stop_post_msg_flag == 1 && notice_obj->father_name != NULL) { */
                /* os_taskq_post_event(notice_obj->father_name, 2, SYS_EVENT_PLAY_SEL_END, 0); */
                /* } */
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;

        default:
            break;
        }
    }
}

static void notice_normal_player_task_deal(NOTICE_PLAYER_OBJ *notice_obj)
{
    u32 err;
    u32 i, rpt_time;
    u32 file_cnt = 0;
    int msg[3];
    lg_dev_info *cur_lgdev_info = NULL;
    char *dir_path = NULL;
    MUSIC_PLAYER_OBJ *mapi = NULL;

    if (notice_obj != NULL) {

        notice_obj->method &= 0xffff;

        notice_player_printf("post msg flag = %d, method = %d\n", notice_obj->manul_msg_post, notice_obj->method);

        if (notice_obj->method == NOTICE_PLAYER_METHOD_BY_NUM
            || 	notice_obj->method == NOTICE_PLAYER_METHOD_BY_NUM_ARRAY) {
            dir_path = (char *)notice_obj->file_p[0];
            notice_obj->file_p++;
            notice_obj->total_file--;
        }

        rpt_time = notice_obj->rpt_time;

        mapi = music_player_creat();
        if (mapi != NULL) {

            __music_player_set_dec_father_name(mapi, NOTICE_PLAYER_TASK_NAME);
            /* __music_player_set_dec_phy_name(mapi, NOTICE_PLAYER_PHY_TASK_NAME); */
            __music_player_set_decoder_type(mapi, DEC_PHY_MP3 | DEC_PHY_WAV);
            __music_player_set_file_ext(mapi, "MP3WAV");
            __music_player_set_dev_sel_mode(mapi, DEV_SEL_SPEC);
            /* __music_player_set_path(mapi, path); */
            /* __music_player_set_file_sel_mode(mapi, PLAY_FILE_BYPATH); */
            __music_player_set_play_mode(mapi, REPEAT_ALL);
            /* __music_player_set_play_mode(mapi, REPEAT_FOLDER); */
            __music_player_set_dev_let(mapi, notice_obj->dev_let);
            /* __music_player_set_dev_let(mapi, 'B'); */
            if (notice_obj->mix_obj != NULL) {
                if (__music_player_set_output_to_mix(
                        mapi,
                        notice_obj->mix_obj,
                        NOTICE_MIX_KEY,
                        NOTICE_NORMAL_PLAYER_MIX_BUF_SIZE,
                        NOTICE_NORMAL_PLAYER_MIX_LIMIT,
                        DAC_DIGIT_TRACK_DEFAULT_VOL,
                        DAC_DIGIT_TRACK_MAX_VOL) == false) {
                    notice_player_printf("notice_player mix add fail!!\n");
                }
            }

            if (music_player_process(mapi, NOTICE_PLAYER_PHY_TASK_NAME, TaskPselPhyDecPrio) == true) {
                os_taskq_post(NOTICE_PLAYER_TASK_NAME, 1, SYS_EVENT_BEGIN_DEC);
            } else {
                notice_player_printf("notice_player_play fail!!\n");
                music_player_destroy(&mapi);
            }
        }

    }

    /* dac_channel_on(MUSIC_CHANNEL, FADE_ON); */

    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        if (notice_obj == NULL) {
            if (msg[0] != SYS_EVENT_DEL_TASK) {
                continue;
            }
        }
        switch (msg[0]) {
        case SYS_EVENT_DEC_FR_END:
        case SYS_EVENT_DEC_FF_END:
        case SYS_EVENT_DEC_END:
            if (mapi == NULL) {
                break;
            }

            notice_player_printf("notice play end!!\n");
            if (notice_player_repeat_deal(notice_obj, &file_cnt, &rpt_time) == false) {
                break;
            }

        case SYS_EVENT_BEGIN_DEC:
            if (mapi != NULL) {
                if (notice_obj->method == NOTICE_PLAYER_METHOD_BY_NUM
                    || notice_obj->method == NOTICE_PLAYER_METHOD_BY_NUM_ARRAY) {
                    __music_player_set_path(mapi, dir_path);
                    __music_player_set_file_index(mapi, (u32)notice_obj->file_p[file_cnt]);
                    __music_player_set_file_sel_mode(mapi, PLAY_FILE_BYPATH);
                } else if (notice_obj->method == NOTICE_PLAYER_METHOD_BY_FLASH_REC_ADDR) {
                    __music_player_set_file_index(mapi, (u32)notice_obj->file_p[file_cnt]);
                    __music_player_set_file_sel_mode(mapi, PLAY_SEL_FLASH_REC);
                } else {
                    __music_player_set_path(mapi, (const char *)notice_obj->file_p[file_cnt]);
                    __music_player_set_file_sel_mode(mapi, PLAY_FILE_BYPATH);
                }

                err = music_player_play(mapi);
                notice_obj->status = __music_player_get_status(mapi);
                if (err != SUCC_MUSIC_START_DEC) {
                    notice_player_printf("notice_player_play fail 1 err = %x !!\n", err);
                    music_player_close(mapi);
                    notice_player_end_msg_post(notice_obj, err);
                } else {
                    notice_player_printf("notice player ok filenum = %d!!\n", __music_player_get_cur_file_index(mapi, 0));
                }
            }
            break;

        case MSG_MUSIC_PP:
            notice_obj->status = music_player_pp(mapi);
            break;

        case SYS_EVENT_DEV_OFFLINE:
            notice_player_printf("notice player dev offline err !! line %d\n", __LINE__);
            cur_lgdev_info = __music_player_get_lg_dev_info(mapi);
            if ((cur_lgdev_info != NULL) && (cur_lgdev_info->lg_hdl->phydev_item == (void *)msg[1])) {
                if (__music_player_get_status(mapi) != DECODER_PAUSE) {
                    break;//不是解码暂停状态，有下一个消息SYS_EVENT_DEC_DEVICE_ERR处理
                }
            } else {
                break;
            }
        case SYS_EVENT_DEC_DEVICE_ERR:
            notice_player_printf("notice player dev offline err !! line %d\n", __LINE__);
            music_player_close(mapi);
            notice_player_end_msg_post(notice_obj, (u32) - 1);
            break;
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                music_player_destroy(&mapi);
                if (notice_obj->manul_msg_post) {
                    notice_player_end_msg_post(notice_obj, 0);
                }
                /* dac_channel_off(MUSIC_CHANNEL, FADE_ON); */
                notice_player_printf("notice player del ok\n");
                os_task_del_res_name(OS_TASK_SELF); 	//确认可删除，此函数不再返回
            }

            break;

        default:
            break;
        }
    }
}


void notice_player_task_deal(void *parg)
{
    u32 err;
    u32 i, rpt_time;
    u32 file_cnt = 0;
    u8 manul_stop_post_msg_flag = 0;
    NOTICE_PLAYER_OBJ *notice_obj = (NOTICE_PLAYER_OBJ *)parg;
    NOTICE_PLAYER_METHOD method;
    char *dir_path = NULL;
    if (notice_obj != NULL) {

        method = notice_obj->method & 0xffff;
        notice_player_printf(" method = %d\n", method);
        notice_player_printf(" dev_let = %c\n", (char)notice_obj->dev_let);

        ///* 以下是debug信息 */
        if (method == NOTICE_PLAYER_METHOD_BY_NUM
            || 	method == NOTICE_PLAYER_METHOD_BY_NUM_ARRAY) {
            dir_path = (char *)notice_obj->file_p[0];
        }

        switch (method) {
        case NOTICE_PLAYER_METHOD_BY_PATH:
            notice_player_printf("NOTICE_PLAYER_METHOD_BY_PATH\n");
            for (i = 0; i < notice_obj->total_file; i++) {
                notice_player_printf("path = %s\n", (const char *)notice_obj->file_p[i]);
            }
            break;
        case NOTICE_PLAYER_METHOD_BY_NUM:
            notice_player_printf("NOTICE_PLAYER_METHOD_BY_NUM\n");
            notice_player_printf("path = %s\n", (const char *)dir_path);
            for (i = 1; i < notice_obj->total_file; i++) {
                notice_player_printf("num = %d\n", (u32)notice_obj->file_p[i]);
            }
            break;
        case NOTICE_PLAYER_METHOD_BY_PATH_ARRAY:
            notice_player_printf("NOTICE_PLAYER_METHOD_BY_PATH_ARRAY\n");
            notice_player_printf("total = %d\n", notice_obj->total_file);
            for (i = 0; i < notice_obj->total_file; i++) {
                notice_player_printf("path = %s\n", (const char *)notice_obj->file_p[i]);
            }
            break;
        case NOTICE_PLAYER_METHOD_BY_NUM_ARRAY:
            notice_player_printf("NOTICE_PLAYER_METHOD_BY_NUM_ARRAY\n");
            notice_player_printf("dir_path  = %s\n", dir_path);
            for (i = 1; i < notice_obj->total_file; i++) {
                notice_player_printf("num = %d \n", (u32)notice_obj->file_p[i]);
            }
            break;
        case NOTICE_PLAYER_METHOD_BY_FLASH_REC_ADDR:
            notice_player_printf("NOTICE_PLAYER_METHOD_BY_FLASH_REC_ADDR\n");
            for (i = 0; i < notice_obj->total_file; i++) {
                notice_player_printf("addr = 0x%x\n", (u32)notice_obj->file_p[i]);
            }
            break;
        case NOTICE_PLAYER_METHOD_BY_SPI_REC_NUM:
            notice_player_printf("NOTICE_PLAYER_METHOD_BY_SPI_REC_NUM\n");
            notice_player_printf("num = %d\n", (u32)notice_obj->file_p[0]);
            break;
        default:
            break;
        }
        ///* 以上是debug信息 */

        if ((notice_obj->method & NOTICE_USE_WAV_PLAYER) != 0) {
            notice_wav_player_task_deal(notice_obj);
        } else {
            notice_normal_player_task_deal(notice_obj);
        }
    } else {
        notice_player_printf("notice obj is null err!\n");
        int msg[6] = {0};
        while (1) {
            os_taskq_pend(0, ARRAY_SIZE(msg), msg);
            if (msg[0] != SYS_EVENT_DEL_TASK) {
                continue;
            }
            switch (msg[0]) {
            case SYS_EVENT_DEL_TASK:
                if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                    notice_player_printf("notice_player_task_del 1\r");
                    os_task_del_res_name(OS_TASK_SELF);
                }
                break;
            }
        }
    }
}



/* static NOTICE_PlAYER_ERR notice_player_play_phy(void *father_name, msg_cb _cb, void *priv, u32 dev_let, u32 rpt_time, u32 delay, NOTICE_PLAYER_METHOD method, int argc, va_list argptr) */
static NOTICE_PlAYER_ERR notice_player_play_phy(NOTICE_PLAYER_P *param, NOTICE_PLAYER_METHOD method, int argc, va_list argptr)
{
    NOTICE_PlAYER_ERR n_err = NOTICE_PLAYER_NO_ERR;
    NOTICE_PLAYER_METHOD notice_player_method = method & 0xffff;
    u32 num, i;
    u32 *array_ptr = NULL;
    const char *dir_path = "/";
    /* va_list argptr; */
    /* va_start(argptr, argc); */
    if (notice_player_active) {
        /* return NOTICE_PLAYER_IS_PLAYING_ERR; */
        notice_player_stop();
    }


    notice_player_mutex_enter();
    notice_player_active = 1;
    if (notice_player_method == NOTICE_PLAYER_METHOD_BY_PATH
        || notice_player_method == NOTICE_PLAYER_METHOD_BY_NUM
        || notice_player_method == NOTICE_PLAYER_METHOD_BY_SPI_REC_NUM
        || notice_player_method == NOTICE_PLAYER_METHOD_BY_FLASH_REC_ADDR) {
        num = argc;
    } else {
        if (notice_player_method == NOTICE_PLAYER_METHOD_BY_NUM_ARRAY) {
            if (argc == 2) {
                num = (u32)va_arg(argptr, int);
                array_ptr = (u32 *)va_arg(argptr, int);
            } else if (argc == 3) {
                dir_path = (const char *)va_arg(argptr, int);
                num = (u32)va_arg(argptr, int);
                array_ptr = (u32 *)va_arg(argptr, int);
            } else {
                notice_player_printf("err param!!!\n");
                n_err = NOTICE_PLAYER_PLAY_ERR;
                goto __exit;
            }
            num ++;/*file_p need record dir path, sould ++*/
        } else {
            num = (u32)va_arg(argptr, int);
            array_ptr = (u32 *)va_arg(argptr, int);
        }
    }

    u32 ctr_buf_len = SIZEOF_ALIN(sizeof(NOTICE_PLAYER_OBJ), 4) + sizeof(u32) * num;
    notice_player_printf("notice player buf size = %d\n",  ctr_buf_len);
    u8 *ctr_buf = (u8 *)notice_player_calloc(1, ctr_buf_len);
    if (ctr_buf == NULL) {
        notice_player_printf("no space for notice_obj err !!!\n");
        n_err = NOTICE_PLAYER_PLAY_ERR;
        notice_player_active = 0;
        goto __exit;
    }
    NOTICE_PLAYER_OBJ *notice_obj = (NOTICE_PLAYER_OBJ *)ctr_buf;
    notice_obj->file_p = (u32 *)(ctr_buf + SIZEOF_ALIN(sizeof(NOTICE_PLAYER_OBJ), 4));

    notice_obj->samp_backup = dac_get_samplerate();///backup samplerate, when notice stop revert
    notice_obj->total_file = num;
    notice_obj->father_name = param->father_name;
    notice_obj->method = method;
    notice_obj->dev_let = param->dev_let;
    notice_obj->delay = param->delay;
    notice_obj->rpt_time = param->rpt_time;
    notice_obj->mix_obj = param->mix_obj;
    notice_obj->msg_post = 0;
    notice_obj->status = DECODER_STOP;

    if ((method & NOTICE_STOP_WITH_BACK_END_MSG) != 0) {
        notice_obj->manul_msg_post = 1;
    } else {
        notice_obj->manul_msg_post = 0;
    }


    if (notice_player_method == NOTICE_PLAYER_METHOD_BY_PATH
        || notice_player_method == NOTICE_PLAYER_METHOD_BY_NUM
        || notice_player_method == NOTICE_PLAYER_METHOD_BY_SPI_REC_NUM
        || notice_player_method == NOTICE_PLAYER_METHOD_BY_FLASH_REC_ADDR) {
        for (i = 0; i < num; i++) {
            notice_obj->file_p[i] = (u32)va_arg(argptr, int);
        }
    } else {
        if (notice_player_method == NOTICE_PLAYER_METHOD_BY_NUM_ARRAY) {
            notice_obj->file_p[0] = (u32)dir_path;
            memcpy((u8 *) & (notice_obj->file_p[1]), (u8 *)array_ptr, (num - 1)*sizeof(u32));
        } else {
            memcpy((u8 *) & (notice_obj->file_p[0]), (u8 *)array_ptr, num * sizeof(u32));
        }
    }

    g_notice_obj = notice_obj;

    if (OS_NO_ERR == os_task_create_ext(notice_player_task_deal, notice_obj, TaskPselPrio, 10, NOTICE_PLAYER_TASK_NAME, 2 * 1024)) {

        notice_player_printf("build play sel succ!!\n");
    } else {
        notice_player_mutex_exit();
        n_err = NOTICE_PLAYER_PLAY_ERR;
        notice_player_stop();
        goto __exit;
    }

    notice_player_mutex_exit();

    if (param->_cb) {
        u8 exit = 0;
        int msg[6] = {0};
        while (1) {
            memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
            /* os_taskq_pend(0, ARRAY_SIZE(msg), msg); */
            os_taskq_pend_no_clean(0, ARRAY_SIZE(msg), msg);

            if (msg[0] != MSG_HALF_SECOND) {
                notice_player_printf("notice msg=0x%x \n", msg[0]);
            }
            clear_wdt();
            switch (msg[0]) {
            case SYS_EVENT_DEL_TASK:
                exit = 1;
                n_err = NOTICE_PLAYER_TASK_DEL_ERR;
                /* os_taskq_post_event(notice_obj->father_name, 1, SYS_EVENT_DEL_TASK); */
                /* if(os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) */
                /* { */
                /* notice_player_printf("notice task del \r"); */
                /* os_task_del_res_name(OS_TASK_SELF); */
                /* } */
                break;
            case SYS_EVENT_PLAY_SEL_END:
                exit = 1;
                if (msg[1] != 0) {
                    notice_player_printf("notice player play err = %x\n", msg[1]);
                    n_err = NOTICE_PLAYER_PLAY_ERR;
                }
                /* os_taskq_last_msg_clean(ARRAY_SIZE(msg), msg); */

                os_taskq_msg_clean(msg[0], 1);
                break;

            default:
                if (param->_cb(msg, param->priv) == true) {
                    exit = 1;
                    n_err = NOTICE_PLAYER_MSG_BREAK_ERR;
                    /* os_taskq_post_event(notice_obj->father_name, 1, msg[0]); */
                } else {
                    os_taskq_msg_clean(msg[0], 1);
                }
                break;
            }
            if (exit) {
                notice_player_stop();
                break;
            }
        }
    } else {
        n_err = NOTICE_PLAYER_NO_ERR_WITHOUT_MSG_DEAL;
    }

__exit:
    va_end(argptr);
    return n_err;
}
/*----------------------------------------------------------------------------*/
/**@brief   停止提示音播放接口
   @param   void
   @return  void
   @note
*/
/*----------------------------------------------------------------------------*/
void notice_player_stop(void)
{
    u8 is_mix = 0;
    notice_player_mutex_enter();
    if (g_notice_obj == NULL) {
        notice_player_mutex_exit();
        return ;
    }

    if (g_notice_obj->mix_obj) {
        is_mix = 1;
    }

    if (os_task_del_req(NOTICE_PLAYER_TASK_NAME) != OS_TASK_NOT_EXIST) {
        os_taskq_post_event(NOTICE_PLAYER_TASK_NAME, 1, SYS_EVENT_DEL_TASK);
        do {
            OSTimeDly(1);
        } while (os_task_del_req(NOTICE_PLAYER_TASK_NAME) != OS_TASK_NOT_EXIST);
        /* puts("del notice player  succ\n"); */
    }

    if (is_mix == 0) {
        dac_set_samplerate(g_notice_obj->samp_backup, 1);
    }

    notice_player_free(g_notice_obj);
    g_notice_obj = NULL;

    notice_player_active = 0;

    notice_player_mutex_exit();
    /* puts("del notice player  ok\n"); */
}


/*----------------------------------------------------------------------------*/
/**@brief   提示音播放/暂停接口
   @param   void
   @return  是否正在播放
   @note
*/
/*----------------------------------------------------------------------------*/
_DECODE_STATUS notice_player_pp(void)
{
    if (g_notice_obj == NULL) {
        return DECODER_STOP;
    }
    if (g_notice_obj->status == DECODER_STOP) {
        return DECODER_STOP;
    }

    _DECODE_STATUS wait_status = DECODER_STOP;
    if (g_notice_obj->status == DECODER_PAUSE) {
        wait_status = DECODER_PLAY;
    } else {
        wait_status = DECODER_PAUSE;
    }

    os_taskq_post(NOTICE_PLAYER_TASK_NAME, 1, MSG_MUSIC_PP);
    while (g_notice_obj->status != wait_status) {
        OSTimeDly(1);
        /* if(g_notice_obj->status == DECODER_STOP) */
        /* break; */
    }

    return g_notice_obj->status;
}


/*----------------------------------------------------------------------------*/
/**@brief   获取提示音播放状态接口
   @param   void
   @return  是否正在播放
   @note
*/
/*----------------------------------------------------------------------------*/
u8 notice_player_get_status(void)
{
    return notice_player_active;
}

/*----------------------------------------------------------------------------*/
/**@brief   多方法提示音播放接口，支持可变参数(默认‘A‘设备)
   @param   father_name:控制线程名称
   @param   _cb:播放过程中消息处理回调接口， 提示音在播放过程中的消息处理，由使用者根据流程需求编写,主要是打断当前提示音及显示相关等刷新处理
   @param   priv:_cb私有属性参数， 会被传递到_cb中使用
   @param   rpt_time:0无限循环,非0按照指定值循环rpt_time次
   @param   delay:一次循环之后停顿时间间隔，单位单位10ms
   @param   method:提示音播放方法，具体查看NOTICE_PLAYER_METHOD定义
   @param   argc:参数个数
   @param   可变参数(遵循method传递可变参数):
			NOTICE_PLAYER_METHOD_BY_PATH:例如，argc为3，其后的可变参数为:"/0.mp3","/1.mp3","/2.mp3"
			NOTICE_PLAYER_METHOD_BY_NUM:例如，argc为2，其后的可变参数为:3,4
			NOTICE_PLAYER_METHOD_BY_PATH_ARRAY:argc固定2, 其后参数为路径数组的路径个数及路径数组的指针地址, 数组大小及根据应用填写
			NOTICE_PLAYER_METHOD_BY_NUM_ARRAY:argc 为2, 其后参数为文件号数组的文件个数及数组的指针地址, 数组大小及根据应用填写, 由于没有指定dirpath， 默认从根目录指定文件号
											  argc 为3, 其后参数为指定播放目录、文件号数组的文件个数及数组的指针地址, 数组大小及根据应用填写
			NOTICE_PLAYER_METHOD_BY_FLASH_REC_ADDR:argc为1， 可变参数为指定的物理地址
   @return  查看提示音返回错误列表
   @note	注意返回值的处理
*/
/*----------------------------------------------------------------------------*/
NOTICE_PlAYER_ERR notice_player_play(void *father_name, msg_cb _cb, void *priv, u32 rpt_time, u32 delay, NOTICE_PLAYER_METHOD method, int argc, ...)
{
    NOTICE_PLAYER_P param;
    va_list args;
    va_start(args, argc);
    memset((u8 *)&param, 0x0, sizeof(NOTICE_PLAYER_P));
    param.father_name = father_name;
    param._cb = _cb;
    param.priv = priv;
    param.dev_let = 'A';
    param.rpt_time = rpt_time;
    param.delay = delay;
    param.mix_obj = NULL;
    return notice_player_play_phy(&param, method, argc, args);
    /* return notice_player_play_phy(father_name, _cb, priv, 'A', rpt_time, delay, method, argc, args); */
}


/*----------------------------------------------------------------------------*/
/**@brief   指定文件路径播放(默认‘A‘设备)
   @param   father_name:控制线程名称
   @param   dev_let:指定逻辑设备盘符
   @param   path:指定文件路径
   @param   _cb:播放过程中消息处理回调接口， 提示音在播放过程中的消息处理，由使用者根据流程需求编写,主要是打断当前提示音及显示相关等刷新处理
   @param   priv:_cb私有属性参数， 会被传递到_cb中使用
   @return  查看提示音返回错误列表
   @note	注意返回值的处理
*/
/*----------------------------------------------------------------------------*/
NOTICE_PlAYER_ERR notice_player_play_by_path(void *father_name, const char *path, msg_cb _cb, void *priv)
{
    return notice_player_play(father_name, _cb, priv, 1, 0, NOTICE_PLAYER_METHOD_BY_PATH, 1, path);
}
/*----------------------------------------------------------------------------*/
/**@brief   指定文件夹的文件号播放提示音(默认‘A’设备)
   @param   father_name:控制线程名称
   @param   dir_path:指定文件夹路径
   @param   num:指定文件号
   @param   _cb:播放过程中消息处理回调接口， 提示音在播放过程中的消息处理，由使用者根据流程需求编写,主要是打断当前提示音及显示相关等刷新处理
   @param   priv:_cb私有属性参数， 会被传递到_cb中使用
   @return  查看提示音返回错误列表
   @note	注意返回值的处理
*/
/*----------------------------------------------------------------------------*/
NOTICE_PlAYER_ERR notice_player_play_by_num(void *father_name, const char *dir_path, u32 num, msg_cb _cb, void *priv)
{
    return notice_player_play(father_name, _cb, priv, 1, 0, NOTICE_PLAYER_METHOD_BY_NUM, 2, dir_path, num);
}
/*----------------------------------------------------------------------------*/
/**@brief   指定物理地址播放录音内容(‘A’设备)
   @param   father_name:控制线程名称
   @param   flash_addr:物理地址
   @param   _cb:播放过程中消息处理回调接口， 提示音在播放过程中的消息处理，由使用者根据流程需求编写,主要是打断当前提示音及显示相关等刷新处理
   @param   priv:_cb私有属性参数， 会被传递到_cb中使用
   @return  查看提示音返回错误列表
   @note	注意返回值的处理
*/
/*----------------------------------------------------------------------------*/
NOTICE_PlAYER_ERR notice_player_play_by_flash_rec_addr(void *father_name, u32 flash_addr, msg_cb _cb, void *priv)
{
    return notice_player_play(father_name, _cb, priv, 1, 0, NOTICE_PLAYER_METHOD_BY_FLASH_REC_ADDR, 1, flash_addr);
}
/*----------------------------------------------------------------------------*/
/**@brief   指定设备多方法提示音播放接口，支持可变参数
   @param   father_name:控制线程名称
   @param   _cb:播放过程中消息处理回调接口， 提示音在播放过程中的消息处理，由使用者根据流程需求编写,主要是打断当前提示音及显示相关等刷新处理
   @param   priv:_cb私有属性参数， 会被传递到_cb中使用
   @param   dev_let:指定逻辑设备盘符
   @param   rpt_time:0无限循环,非0按照指定值循环rpt_time次
   @param   delay:一次循环之后停顿时间间隔，单位单位10ms
   @param   method:提示音播放方法，具体查看NOTICE_PLAYER_METHOD定义
   @param   argc:参数个数
   @param   可变参数(遵循method传递可变参数):
			NOTICE_PLAYER_METHOD_BY_PATH:例如，argc为3，其后的可变参数为:"/0.mp3","/1.mp3","/2.mp3"
			NOTICE_PLAYER_METHOD_BY_NUM:例如，argc为2，其后的可变参数为:3,4
			NOTICE_PLAYER_METHOD_BY_PATH_ARRAY:argc固定2, 其后参数为路径数组的路径个数及路径数组的指针地址, 数组大小及根据应用填写
			NOTICE_PLAYER_METHOD_BY_NUM_ARRAY:argc 为2, 其后参数为文件号数组的文件个数及数组的指针地址, 数组大小及根据应用填写, 由于没有指定dirpath， 默认从根目录指定文件号
											  argc 为3, 其后参数为指定播放目录、文件号数组的文件个数及数组的指针地址, 数组大小及根据应用填写
			NOTICE_PLAYER_METHOD_BY_FLASH_REC_ADDR:argc为1， 可变参数为指定的物理地址
   @return  查看提示音返回错误列表
   @note	注意返回值的处理
*/
/*----------------------------------------------------------------------------*/
NOTICE_PlAYER_ERR notice_player_play_spec_dev(void *father_name, msg_cb _cb, void *priv, u32 dev_let, u32 rpt_time, u32 delay, NOTICE_PLAYER_METHOD method, int argc, ...)
{
    NOTICE_PLAYER_P param;
    va_list args;
    va_start(args, argc);
    memset((u8 *)&param, 0x0, sizeof(NOTICE_PLAYER_P));
    param.father_name = father_name;
    param._cb = _cb;
    param.priv = priv;
    param.dev_let = dev_let;
    param.rpt_time = rpt_time;
    param.delay = delay;
    param.mix_obj = NULL;
    return notice_player_play_phy(&param, method, argc, args);
    /* return notice_player_play_phy(father_name, _cb, priv, dev_let, rpt_time, delay, method, argc, args); */
}

/*----------------------------------------------------------------------------*/
/**@brief   指定设备的文件路径播放
   @param   father_name:控制线程名称
   @param   dev_let:指定逻辑设备盘符
   @param   path:指定文件路径
   @param   _cb:播放过程中消息处理回调接口， 提示音在播放过程中的消息处理，由使用者根据流程需求编写,主要是打断当前提示音及显示相关等刷新处理
   @param   priv:_cb私有属性参数， 会被传递到_cb中使用
   @return  查看提示音返回错误列表
   @note	注意返回值的处理
*/
/*----------------------------------------------------------------------------*/
NOTICE_PlAYER_ERR notice_player_play_spec_dev_by_path(void *father_name, u32 dev_let, const char *path, msg_cb _cb, void *priv)
{
    return notice_player_play_spec_dev(father_name, _cb, priv, dev_let, 1, 0, NOTICE_PLAYER_METHOD_BY_PATH, 1, path);
}
/*----------------------------------------------------------------------------*/
/**@brief   指定设备的对应文件夹的文件号播放提示音
   @param   father_name:控制线程名称
   @param   dev_let:指定逻辑设备盘符
   @param   dir_path:指定文件夹路径
   @param   num:指定文件号
   @param   _cb:播放过程中消息处理回调接口， 提示音在播放过程中的消息处理，由使用者根据流程需求编写,主要是打断当前提示音及显示相关等刷新处理
   @param   priv:_cb私有属性参数， 会被传递到_cb中使用
   @return  查看提示音返回错误列表
   @note	注意返回值的处理
*/
/*----------------------------------------------------------------------------*/
NOTICE_PlAYER_ERR notice_player_play_spec_dev_by_num(void *father_name, u32 dev_let, const char *dir_path, u32 num, msg_cb _cb, void *priv)
{
    return notice_player_play_spec_dev(father_name, _cb, priv, dev_let, 1, 0, NOTICE_PLAYER_METHOD_BY_NUM, 2, dir_path, num);
}
/*----------------------------------------------------------------------------*/
/**@brief   精简wavplay通过指定文件路径(省资源)
   @param   father_name:控制线程名称
   @param   path:指定文件路径
   @param   _cb:wavplay播放过程中消息处理回调接口， 提示音在播放过程中的消息处理，由使用者根据流程需求编写,主要是打断当前提示音及显示相关等刷新处理
   @param   priv:_cb私有属性参数， 会被传递到_cb中使用
   @return  查看提示音返回错误列表
   @note	注意只能播放flash内部指定格式的wav文件，注意返回值的处理
*/
/*----------------------------------------------------------------------------*/
NOTICE_PlAYER_ERR wav_play_by_path(void *father_name, const char *path, msg_cb _cb, void *priv)
{
    return notice_player_play(father_name, _cb, priv, 1, 0, NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_USE_WAV_PLAYER, 1, path);
}
/*----------------------------------------------------------------------------*/
/**@brief   精简wavplay通过指定文件夹内的文件号播放(省资源）
   @param   father_name:控制线程名称
   @param   dir_path:指定文件夹路径
   @param   num:指定文件号
   @param   _cb:wavplay播放过程中消息处理回调接口， 提示音在播放过程中的消息处理，由使用者根据流程需求编写,主要是打断当前提示音及显示相关等刷新处理
   @param   priv:_cb私有属性参数， 会被传递到_cb中使用
   @return  查看提示音返回错误列表
   @note	注意只能播放flash内部指定格式的wav文件，注意返回值的处理
*/
/*----------------------------------------------------------------------------*/
NOTICE_PlAYER_ERR wav_play_by_num(void *father_name, const char *dir_path, u32 num, msg_cb _cb, void *priv)
{
    return notice_player_play(father_name, _cb, priv, 1, 0, NOTICE_PLAYER_METHOD_BY_NUM | NOTICE_USE_WAV_PLAYER, 2, dir_path, num);
}


/*----------------------------------------------------------------------------*/
/**@brief   指定输出到叠加处理的多方法提示音播放接口，支持可变参数(默认‘A‘设备)
   @param   father_name:控制线程名称
   @param   _cb:播放过程中消息处理回调接口， 提示音在播放过程中的消息处理，由使用者根据流程需求编写,主要是打断当前提示音及显示相关等刷新处理
   @param   priv:_cb私有属性参数， 会被传递到_cb中使用
   @param   rpt_time:0无限循环,非0按照指定值循环rpt_time次
   @param   delay:一次循环之后停顿时间间隔，单位单位10ms
   @param   mix_obj:叠加控制句柄
   @param   method:提示音播放方法，具体查看NOTICE_PLAYER_METHOD定义
   @param   argc:参数个数
   @param   可变参数(遵循method传递可变参数):
			NOTICE_PLAYER_METHOD_BY_PATH:例如，argc为3，其后的可变参数为:"/0.mp3","/1.mp3","/2.mp3"
			NOTICE_PLAYER_METHOD_BY_NUM:例如，argc为2，其后的可变参数为:3,4
			NOTICE_PLAYER_METHOD_BY_PATH_ARRAY:argc固定2, 其后参数为路径数组的路径个数及路径数组的指针地址, 数组大小及根据应用填写
			NOTICE_PLAYER_METHOD_BY_NUM_ARRAY:argc 为2, 其后参数为文件号数组的文件个数及数组的指针地址, 数组大小及根据应用填写, 由于没有指定dirpath， 默认从根目录指定文件号
											  argc 为3, 其后参数为指定播放目录、文件号数组的文件个数及数组的指针地址, 数组大小及根据应用填写
			NOTICE_PLAYER_METHOD_BY_FLASH_REC_ADDR:argc为1， 可变参数为指定的物理地址
   @return  查看提示音返回错误列表
   @note	注意返回值的处理
*/
/*----------------------------------------------------------------------------*/
NOTICE_PlAYER_ERR notice_player_play_to_mix(void *father_name, msg_cb _cb, void *priv, u32 rpt_time, u32 delay, SOUND_MIX_OBJ *mix_obj, NOTICE_PLAYER_METHOD method, int argc, ...)
{
    NOTICE_PLAYER_P param;
    va_list args;
    va_start(args, argc);
    memset((u8 *)&param, 0x0, sizeof(NOTICE_PLAYER_P));
    param.father_name = father_name;
    param._cb = _cb;
    param.priv = priv;
    param.dev_let = 'A';
    param.rpt_time = rpt_time;
    param.delay = delay;
    param.mix_obj = mix_obj;
    return notice_player_play_phy(&param, method, argc, args);
    /* return notice_player_play_phy(father_name, _cb, priv, 'A', rpt_time, delay, method, argc, args); */
}


/*----------------------------------------------------------------------------*/
/**@brief   指定输出到叠加处理的指定文件路径播放(默认‘A‘设备)
   @param   father_name:控制线程名称
   @param   dev_let:指定逻辑设备盘符
   @param   path:指定文件路径
   @param   _cb:播放过程中消息处理回调接口， 提示音在播放过程中的消息处理，由使用者根据流程需求编写,主要是打断当前提示音及显示相关等刷新处理
   @param   priv:_cb私有属性参数， 会被传递到_cb中使用
   @param   mix_obj:叠加控制句柄
   @return  查看提示音返回错误列表
   @note	注意返回值的处理
*/
/*----------------------------------------------------------------------------*/
NOTICE_PlAYER_ERR notice_player_play_by_path_to_mix(void *father_name, const char *path, msg_cb _cb, void *priv, SOUND_MIX_OBJ *mix_obj)
{
    return notice_player_play_to_mix(father_name, _cb, priv, 1, 0, mix_obj, NOTICE_PLAYER_METHOD_BY_PATH, 1, path);
}
/*----------------------------------------------------------------------------*/
/**@brief   指定输出到叠加处理的指定文件夹的文件号播放提示音(默认‘A’设备)
   @param   father_name:控制线程名称
   @param   dir_path:指定文件夹路径
   @param   num:指定文件号
   @param   _cb:播放过程中消息处理回调接口， 提示音在播放过程中的消息处理，由使用者根据流程需求编写,主要是打断当前提示音及显示相关等刷新处理
   @param   priv:_cb私有属性参数， 会被传递到_cb中使用
   @param   mix_obj:叠加控制句柄
   @return  查看提示音返回错误列表
   @note	注意返回值的处理
*/
/*----------------------------------------------------------------------------*/
NOTICE_PlAYER_ERR notice_player_play_by_num_to_mix(void *father_name, const char *dir_path, u32 num, msg_cb _cb, void *priv, SOUND_MIX_OBJ *mix_obj)
{
    return notice_player_play_to_mix(father_name, _cb, priv, 1, 0, mix_obj, NOTICE_PLAYER_METHOD_BY_NUM, 2, dir_path, num);
}
/*----------------------------------------------------------------------------*/
/**@brief   指定输出到叠加处理的指定物理地址播放录音内容(‘A’设备)
   @param   father_name:控制线程名称
   @param   flash_addr:物理地址
   @param   _cb:播放过程中消息处理回调接口， 提示音在播放过程中的消息处理，由使用者根据流程需求编写,主要是打断当前提示音及显示相关等刷新处理
   @param   priv:_cb私有属性参数， 会被传递到_cb中使用
   @return  查看提示音返回错误列表
   @note	注意返回值的处理
*/
/*----------------------------------------------------------------------------*/
NOTICE_PlAYER_ERR notice_player_play_by_flash_rec_addr_to_mix(void *father_name, u32 flash_addr, msg_cb _cb, void *priv, SOUND_MIX_OBJ *mix_obj)
{
    return notice_player_play_to_mix(father_name, _cb, priv, 1, 0, mix_obj, NOTICE_PLAYER_METHOD_BY_FLASH_REC_ADDR, 1, flash_addr);
}
/*----------------------------------------------------------------------------*/
/**@brief   指定输出到叠加处理的指定设备多方法提示音播放接口，支持可变参数
   @param   father_name:控制线程名称
   @param   _cb:播放过程中消息处理回调接口， 提示音在播放过程中的消息处理，由使用者根据流程需求编写,主要是打断当前提示音及显示相关等刷新处理
   @param   priv:_cb私有属性参数， 会被传递到_cb中使用
   @param   dev_let:指定逻辑设备盘符
   @param   rpt_time:0无限循环,非0按照指定值循环rpt_time次
   @param   delay:一次循环之后停顿时间间隔，单位单位10ms
   @param   mix_obj:叠加控制句柄
   @param   method:提示音播放方法，具体查看NOTICE_PLAYER_METHOD定义
   @param   argc:参数个数
   @param   可变参数(遵循method传递可变参数):
			NOTICE_PLAYER_METHOD_BY_PATH:例如，argc为3，其后的可变参数为:"/0.mp3","/1.mp3","/2.mp3"
			NOTICE_PLAYER_METHOD_BY_NUM:例如，argc为2，其后的可变参数为:3,4
			NOTICE_PLAYER_METHOD_BY_PATH_ARRAY:argc固定2, 其后参数为路径数组的路径个数及路径数组的指针地址, 数组大小及根据应用填写
			NOTICE_PLAYER_METHOD_BY_NUM_ARRAY:argc 为2, 其后参数为文件号数组的文件个数及数组的指针地址, 数组大小及根据应用填写, 由于没有指定dirpath， 默认从根目录指定文件号
											  argc 为3, 其后参数为指定播放目录、文件号数组的文件个数及数组的指针地址, 数组大小及根据应用填写
			NOTICE_PLAYER_METHOD_BY_FLASH_REC_ADDR:argc为1， 可变参数为指定的物理地址
   @return  查看提示音返回错误列表
   @note	注意返回值的处理
*/
/*----------------------------------------------------------------------------*/
NOTICE_PlAYER_ERR notice_player_play_spec_dev_to_mix(void *father_name, msg_cb _cb, void *priv, u32 dev_let, u32 rpt_time, u32 delay, SOUND_MIX_OBJ *mix_obj, NOTICE_PLAYER_METHOD method, int argc, ...)
{
    NOTICE_PLAYER_P param;
    va_list args;
    va_start(args, argc);
    memset((u8 *)&param, 0x0, sizeof(NOTICE_PLAYER_P));
    param.father_name = father_name;
    param._cb = _cb;
    param.priv = priv;
    param.dev_let = dev_let;
    param.rpt_time = rpt_time;
    param.delay = delay;
    param.mix_obj = mix_obj;
    return notice_player_play_phy(&param, method, argc, args);
    /* return notice_player_play_phy(father_name, _cb, priv, dev_let, rpt_time, delay, method, argc, args); */
}
/*----------------------------------------------------------------------------*/
/**@brief   指定输出到叠加处理的指定设备的文件路径播放
   @param   father_name:控制线程名称
   @param   dev_let:指定逻辑设备盘符
   @param   path:指定文件路径
   @param   _cb:播放过程中消息处理回调接口， 提示音在播放过程中的消息处理，由使用者根据流程需求编写,主要是打断当前提示音及显示相关等刷新处理
   @param   priv:_cb私有属性参数， 会被传递到_cb中使用
   @param   mix_obj:叠加控制句柄
   @return  查看提示音返回错误列表
   @note	注意返回值的处理
*/
/*----------------------------------------------------------------------------*/
NOTICE_PlAYER_ERR notice_player_play_spec_dev_by_path_to_mix(void *father_name, u32 dev_let, const char *path, msg_cb _cb, void *priv, SOUND_MIX_OBJ *mix_obj)
{
    return notice_player_play_spec_dev_to_mix(father_name, _cb, priv, dev_let, 1, 0, mix_obj, NOTICE_PLAYER_METHOD_BY_PATH, 1, path);
}
/*----------------------------------------------------------------------------*/
/**@brief   指定输出到叠加处理的指定设备的对应文件夹的文件号播放提示音
   @param   father_name:控制线程名称
   @param   dev_let:指定逻辑设备盘符
   @param   dir_path:指定文件夹路径
   @param   num:指定文件号
   @param   _cb:播放过程中消息处理回调接口， 提示音在播放过程中的消息处理，由使用者根据流程需求编写,主要是打断当前提示音及显示相关等刷新处理
   @param   priv:_cb私有属性参数， 会被传递到_cb中使用
   @param   mix_obj:叠加控制句柄
   @return  查看提示音返回错误列表
   @note	注意返回值的处理
*/
/*----------------------------------------------------------------------------*/
NOTICE_PlAYER_ERR notice_player_play_spec_dev_by_num_to_mix(void *father_name, u32 dev_let, const char *dir_path, u32 num, msg_cb _cb, void *priv, SOUND_MIX_OBJ *mix_obj)
{
    return notice_player_play_spec_dev_to_mix(father_name, _cb, priv, dev_let, 1, 0, mix_obj, NOTICE_PLAYER_METHOD_BY_NUM, 2, dir_path, num);
}
/*----------------------------------------------------------------------------*/
/**@brief   指定输出到叠加处理的精简wavplay通过指定文件路径(省资源)
   @param   father_name:控制线程名称
   @param   path:指定文件路径
   @param   _cb:wavplay播放过程中消息处理回调接口， 提示音在播放过程中的消息处理，由使用者根据流程需求编写,主要是打断当前提示音及显示相关等刷新处理
   @param   priv:_cb私有属性参数， 会被传递到_cb中使用
   @param   mix_obj:叠加控制句柄
   @return  查看提示音返回错误列表
   @note	注意只能播放flash内部指定格式的wav文件，注意返回值的处理
*/
/*----------------------------------------------------------------------------*/
NOTICE_PlAYER_ERR wav_play_by_path_to_mix(void *father_name, const char *path, msg_cb _cb, void *priv, SOUND_MIX_OBJ *mix_obj)
{
    return notice_player_play_to_mix(father_name, _cb, priv, 1, 0, mix_obj, NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_USE_WAV_PLAYER, 1, path);
}
/*----------------------------------------------------------------------------*/
/**@brief   指定输出到叠加处理的精简wavplay通过指定文件夹内的文件号播放(省资源）
   @param   father_name:控制线程名称
   @param   dir_path:指定文件夹路径
   @param   num:指定文件号
   @param   _cb:wavplay播放过程中消息处理回调接口， 提示音在播放过程中的消息处理，由使用者根据流程需求编写,主要是打断当前提示音及显示相关等刷新处理
   @param   priv:_cb私有属性参数， 会被传递到_cb中使用
   @param   mix_obj:叠加控制句柄
   @return  查看提示音返回错误列表
   @note	注意只能播放flash内部指定格式的wav文件，注意返回值的处理
*/
/*----------------------------------------------------------------------------*/
NOTICE_PlAYER_ERR wav_play_by_num_to_mix(void *father_name, const char *dir_path, u32 num, msg_cb _cb, void *priv, SOUND_MIX_OBJ *mix_obj)
{
    return notice_player_play_to_mix(father_name, _cb, priv, 1, 0, mix_obj, NOTICE_PLAYER_METHOD_BY_NUM | NOTICE_USE_WAV_PLAYER, 2, dir_path, num);
}

/*----------------------------------------------------------------------------*/
/**@brief   根据数字获取数字提示音对应的字符串
   @param   数字， 0~9
   @return  播放路径
   @note
*/
/*----------------------------------------------------------------------------*/
void *tone_number_get_name(u32 number)
{
    const char *num_list[] = {
        NUMBER_PATH_LIST,
    };
    return (void *)((number <= 9) ? num_list[number] : 0);
}

