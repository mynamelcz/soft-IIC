#include "music_player.h"
#include "common/msg.h"
#include "common/app_cfg.h"
#include "fat/fs_io.h"
#include "fat/tff.h"
#include "speed_pitch_api.h"

//#define MUSIC_PLAYER_DEBUG_ENABLE

#ifdef MUSIC_PLAYER_DEBUG_ENABLE
#define music_player_printf printf
#else
#define music_player_printf(...)
#endif

#define SIZEOF_ALIN(var,al)     ((((var)+(al)-1)/(al))*(al))

#define MUSIC_PLAYER_USE_ZEBRA_MEMERY_ENABLE
#ifdef MUSIC_PLAYER_USE_ZEBRA_MEMERY_ENABLE

extern void *zebra_malloc(u32 size);
extern void zebra_free(void *p);

void *music_player_zebra_calloc(u32 count, u32 size)
{
    void *p;
    p = zebra_malloc(count * size);
    if (p) {
        /* zero the memory */
        memset(p, 0, count * size);
    }
    return p;
}

void music_player_zebra_free(void *p)
{
    zebra_free(p);
}

#define music_player_calloc		music_player_zebra_calloc
#define music_player_free		music_player_zebra_free
#else
#define music_player_calloc		calloc
#define music_player_free		free
#endif//MUSIC_PLAYER_USE_ZEBRA_MEMERY_ENABLE

/*----------------------------------------------------------------------------*/
/**@brief 音乐播放器资源申请函数（只管资源申请）
   @param void
   @return 音乐播放器控制句柄, 失败返回NULL
   @note
*/
/*----------------------------------------------------------------------------*/
MUSIC_PLAYER_OBJ *music_player_creat(void)
{
    u32 err;
    tbool ret;
    MUSIC_PLAYER_OBJ *mapi;
    u8 *mapi_buf;
    u32 mapi_buf_len;
    u32 mapi_buf_index = 0;

    mapi_buf_len = (SIZEOF_ALIN(sizeof(MUSIC_PLAYER_OBJ), 4)
                    + SIZEOF_ALIN(sizeof(_MUSIC_API), 4)
                    + SIZEOF_ALIN(sizeof(FILE_OPERATE), 4)
                    + SIZEOF_ALIN(sizeof(DEC_API_IO), 4)
                    + SIZEOF_ALIN(sizeof(FILE_OPERATE_INIT), 4)
                    + SIZEOF_ALIN(sizeof(FS_BRK_POINT), 4));
    music_player_printf("music player need buf size = %d\n", mapi_buf_len);
    mapi_buf = (u8 *)music_player_calloc(1, mapi_buf_len);
    if (mapi_buf == false) {
        music_player_printf("no space for rpt dec!!!\r");
        return NULL;
    }

    mapi_buf_index = 0;
    mapi = (MUSIC_PLAYER_OBJ *)(mapi_buf + mapi_buf_index);

    mapi_buf_index += SIZEOF_ALIN(sizeof(MUSIC_PLAYER_OBJ), 4);
    mapi->dop_api = (_MUSIC_API *)(mapi_buf + mapi_buf_index);

    mapi_buf_index += SIZEOF_ALIN(sizeof(_MUSIC_API), 4);
    mapi->fop_api = (FILE_OPERATE *)(mapi_buf + mapi_buf_index);

    mapi_buf_index += SIZEOF_ALIN(sizeof(FILE_OPERATE), 4);
    mapi->dop_api->io = (DEC_API_IO *)(mapi_buf + mapi_buf_index);

    mapi_buf_index += SIZEOF_ALIN(sizeof(DEC_API_IO), 4);
    mapi->fop_api->fop_init = (FILE_OPERATE_INIT *)(mapi_buf + mapi_buf_index);

    mapi_buf_index += SIZEOF_ALIN(sizeof(FILE_OPERATE_INIT), 4);
    mapi->dop_api->brk = (FS_BRK_POINT *)(mapi_buf + mapi_buf_index);
    mapi->dop_api->dec_api.bp_buff = &mapi->dop_api->brk->inf.norm_brk_buff[0];
    mapi->dop_api->dec_api.flac_bp_buff = &mapi->dop_api->brk->inf.flac_brk_buff[0];

    mapi->dop_api->io->set_info_cb.priv = 0;
    mapi->dop_api->io->set_info_cb._cb = set_music_sr;

    mapi->dop_api->io->output.priv = 0;
    mapi->dop_api->io->output.output = sup_dec_phy_op;

    mapi->dop_api->io->op_cb.priv = (void *)mapi->dop_api->io;
    mapi->dop_api->io->op_cb.clr = (void *)sup_dec_op_clear;
    mapi->dop_api->io->op_cb.over = (void *)sup_dec_op_over;

    music_player_printf("music_player_creat ok\n");
    return mapi;
}


/*----------------------------------------------------------------------------*/
/**@brief 音乐播放器资源释放函数（会自动停止解码）
   @param hdl--音乐控制句柄
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void music_player_destroy(MUSIC_PLAYER_OBJ **hdl)
{
    if ((hdl == NULL) || (*hdl == NULL)) {
        return ;
    }
    MUSIC_PLAYER_OBJ *mapi = *hdl;
    music_player_stop(mapi);
    if (mapi->dop_api->dec_api.dec_phy_name) {
        if (os_task_del_req(mapi->dop_api->dec_api.dec_phy_name) != OS_TASK_NOT_EXIST) {
            os_taskq_post_event(mapi->dop_api->dec_api.dec_phy_name, 1, SYS_EVENT_DEL_TASK);
            do {
                OSTimeDly(1);
            } while (os_task_del_req(mapi->dop_api->dec_api.dec_phy_name) != OS_TASK_NOT_EXIST);
            music_player_printf("del music dec phy succ 1\n");
        }
    }
    file_operate_ctl(FOP_CLOSE_LOGDEV, mapi->fop_api, 0, 0);
    if (mapi->dop_api->io->mix) {
        sound_mix_chl_del_by_chl((SOUND_MIX_CHL **)&mapi->dop_api->io->mix);
    }
    if (mapi->ps_api) {
        speed_pitch_destroy((PS_OBJ **) & (mapi->ps_api));
    }
    u8 *mapi_buf = (u8 *)(*hdl);
    music_player_free(mapi_buf);
    *hdl = NULL;
}


/*----------------------------------------------------------------------------*/
/**@brief 设置播放器的解码类型函数
   @param
   mapi -- 解码器控制句柄
   parm -- 解码类型
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_decoder_type(MUSIC_PLAYER_OBJ *mapi, u32 parm)
{
    if (mapi == NULL) {
        return;
    }
    mapi->dop_api->dec_api.type_enable |= parm;
}


/*----------------------------------------------------------------------------*/
/**@brief 设置音乐播放器文件选择器的文件后缀匹配函数
   @param
   mapi -- 解码器控制句柄
   ext -- 匹配后缀字符串
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_file_ext(MUSIC_PLAYER_OBJ *mapi, void *ext)
{
    if (mapi == NULL) {
        return ;
    }
    mapi->dop_api->file_type = ext;
}


/*----------------------------------------------------------------------------*/
/**@brief 设置播放器的解码线程名称函数
   @param
   mapi -- 解码器控制句柄
   name -- 线程名称字符串
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
/* void __music_player_set_dec_phy_name(MUSIC_PLAYER_OBJ *mapi, void *name) */
/* { */
/* if (mapi == NULL) { */
/* return ; */
/* } */
/* mapi->dop_api->dec_api.dec_phy_name = name; */
/* } */


/*----------------------------------------------------------------------------*/
/**@brief 设置播放器的解码控制线程名称函数
   @param
   mapi -- 解码器控制句柄
   name -- 控制线程名称
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_dec_father_name(MUSIC_PLAYER_OBJ *mapi, void *name)
{
    if (mapi == NULL) {
        return ;
    }
    mapi->dop_api->io->father_name = name;
}


/*----------------------------------------------------------------------------*/
/**@brief 设置播放器的播放路径
   @param
   mapi -- 解码器控制句柄
   path -- 播放路径
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_path(MUSIC_PLAYER_OBJ *mapi, const char *path)
{
    if (mapi == NULL) {
        return ;
    }
    mapi->fop_api->fop_init->filepath = (u8 *)path;
}


/*----------------------------------------------------------------------------*/
/**@brief 设置播放器的播放簇号
   @param
   mapi -- 解码器控制句柄
   sclust -- 指定簇号
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_filesclust(MUSIC_PLAYER_OBJ *mapi, u32 sclust)
{
    if (mapi == NULL) {
        return ;
    }
    mapi->fop_api->fop_init->filesclust = sclust;
}


/*----------------------------------------------------------------------------*/
/**@brief 设置播放器的文件号
   @param
   mapi -- 解码器控制句柄
   index --指定文件号
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_file_index(MUSIC_PLAYER_OBJ *mapi, u32 index)
{
    if (mapi == NULL) {
        return ;
    }
    mapi->dop_api->file_num = index;
}


/*----------------------------------------------------------------------------*/
/**@brief 设置播放器的eq模式
   @param
   mapi -- 解码器控制句柄
   mode --指定eq模式
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_eq(MUSIC_PLAYER_OBJ *mapi, u8 mode)
{
    if (mapi == NULL) {
        return;
    }
    mapi->dop_api->dec_api.eq = mode;
}


/*----------------------------------------------------------------------------*/
/**@brief 设置播放器的解码循环模式
   @param
   mapi -- 解码器控制句柄
   mode -- 指定播放模式
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_play_mode(MUSIC_PLAYER_OBJ *mapi, u8 mode)
{
    if (mapi == NULL) {
        return ;
    }
    const char *rpt_debug_string[] = {
        "ALL",
        "ONE_DEV",
        "ONE_FILE",
        "RANDOM",
        "FOLDER",
    };
    music_player_printf("repeat mode = %s, %d\n", rpt_debug_string[mode - REPEAT_ALL], mode);
    mapi->fop_api->fop_init->cur_play_mode = mode;
}


/*----------------------------------------------------------------------------*/
/**@brief 设置播放器的播放前的文件选择模式
   @param
   mapi -- 解码器控制句柄
   mode -- 指定文件选择模式
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_file_sel_mode(MUSIC_PLAYER_OBJ *mapi, ENUM_FILE_SELECT_MODE mode)
{
    if (mapi == NULL) {
        return ;
    }
    mapi->fop_api->fop_init->cur_sel_mode = mode;
}

/*----------------------------------------------------------------------------*/
/**@brief 设置播放器的播放前的设备选择模式
   @param
   mapi -- 解码器控制句柄
   mode -- 指定设备选择模式
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_dev_sel_mode(MUSIC_PLAYER_OBJ *mapi, ENUM_DEV_SEL_MODE mode)
{
    if (mapi == NULL) {
        return ;
    }
    mapi->fop_api->fop_init->cur_dev_mode = mode;
}

/*----------------------------------------------------------------------------*/
/**@brief 设置播放器的播放前的逻辑设备盘符
   @param
   mapi -- 解码器控制句柄
   dev_let -- 指定逻辑设备盘符
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_dev_let(MUSIC_PLAYER_OBJ *mapi, u32 dev_let)
{
    if (mapi == NULL) {
        return ;
    }
    mapi->fop_api->fop_init->dev_let = dev_let;
}

/*----------------------------------------------------------------------------*/
/**@brief 设置播放器断点记忆使能
   @param
   mapi -- 解码器控制句柄
   enable -- 1:记忆断点 0：不记忆断点
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_save_breakpoint_enable(MUSIC_PLAYER_OBJ *mapi, u8 enable)
{
    if (mapi == NULL) {
        return ;
    }
    if (enable) {
        mapi->dop_api->dec_api.save_brk = true;
    } else {
        mapi->dop_api->dec_api.save_brk = false;
    }
}

/*----------------------------------------------------------------------------*/
/**@brief 设置播放器断点记忆vm的基地址
   @param
   mapi -- 解码器控制句柄
   vm_base_normal_index -- 普通断点保存基地址
   vm_base_flac_index -- flac断点保存基地址
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_save_breakpoint_vm_base_index(MUSIC_PLAYER_OBJ *mapi, u16 vm_base_normal_index, u16 vm_base_flac_index)
{
    if (mapi == NULL) {
        return ;
    }
    mapi->dop_api->brk->vm_base_index = vm_base_normal_index;
    mapi->dop_api->brk->vm_base_flac_index = vm_base_flac_index;
}

/*----------------------------------------------------------------------------*/
/**@brief 设置播放器播放前回调接口
   @param
   mapi -- 解码器控制句柄
   _cb -- 回调函数接口
   priv -- 回调接口私有参数
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_decode_before_callback(MUSIC_PLAYER_OBJ *mapi, void (*_cb)(void *priv), void *priv)
{
    if (mapi == NULL) {
        return ;
    }
    mapi->dop_api->io->bf_cb._cb = _cb;
    mapi->dop_api->io->bf_cb.priv = priv;
}
/*----------------------------------------------------------------------------*/
/**@brief 设置播放器播放后回调接口
   @param
   mapi -- 解码器控制句柄
   _cb -- 回调函数接口
   priv -- 回调接口私有参数
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_decode_after_callback(MUSIC_PLAYER_OBJ *mapi, void (*_cb)(void *priv), void *priv)
{
    if (mapi == NULL) {
        return ;
    }
    mapi->dop_api->io->af_cb._cb = _cb;
    mapi->dop_api->io->af_cb.priv = priv;
}

/*----------------------------------------------------------------------------*/
/**@brief 设置播放器播放停止回调
   @param
   mapi -- 解码器控制句柄
   _cb -- 回调函数接口
   priv -- 回调接口私有参数
   @return void
   @note	主要是方便客户做一些自主申请的资源释放， 主要针对在decode_before和decode_after
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_decode_stop_callback(MUSIC_PLAYER_OBJ *mapi, void (*_cb)(void *priv), void *priv)
{
    if (mapi == NULL) {
        return ;
    }
    mapi->dop_api->io->stop_cb._cb = _cb;
    mapi->dop_api->io->stop_cb.priv = priv;
}
/*----------------------------------------------------------------------------*/
/**@brief 设置播放器播放解码数据输出回调
   @param
   mapi -- 解码器控制句柄
   output -- 回调函数接口
   priv -- 回调接口私有参数
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_decode_output(MUSIC_PLAYER_OBJ *mapi, void *(*output)(void *priv, void *buff, u32 len), void *priv)
{
    if (mapi == NULL) {
        return ;
    }

    mapi->dop_api->io->output.priv = priv;
    mapi->dop_api->io->output.output = (void *)output;
}


static void *music_player_output_to_speed_pitch(void *priv, void *buf, u32 len)
{
    /* putchar('S'); */
    PS_OBJ *obj = (PS_OBJ *)priv;
    speed_pitch_main(obj, buf, len);
    return priv;
}

static u32 music_player_set_speed_pitch_info(void *priv, dec_inf_t *inf, tbool wait)
{
    MUSIC_PLAYER_OBJ *mapi = (MUSIC_PLAYER_OBJ *)priv;
    speed_pitch_set_samplerate((PS_OBJ *)mapi->ps_api, inf->sr);
    if (mapi->dop_api->io->set_info_cb._cb) {
        printf("\n--func=%s, line=%d\n", __FUNCTION__, __LINE__);
        return mapi->dop_api->io->set_info_cb._cb(mapi->dop_api->io->set_info_cb.priv, inf, wait);
    }
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief 设置播放器变速变调使能
   @param
   mapi -- 解码器控制句柄
   @return true or false
   @note:如果使能了音频叠加， 该接口的调用必须在__music_player_set_output_to_mix之后
*/
/*----------------------------------------------------------------------------*/
tbool __music_player_set_decode_speed_pitch_enable(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return false;
    }

    PS_OBJ *tmp_ps_obj = speed_pitch_creat();
    if (tmp_ps_obj == NULL) {
        mapi->ps_api = NULL;
        return false;
    }

    speed_pitch_set_output(tmp_ps_obj, mapi->dop_api->io->output.output, mapi->dop_api->io->output.priv);
    mapi->dop_api->io->output.priv = (void *)tmp_ps_obj;
    mapi->dop_api->io->output.output = (void *)music_player_output_to_speed_pitch;

    speed_pitch_set_set_info_cbk(tmp_ps_obj, music_player_set_speed_pitch_info, (void *)mapi);
    speed_pitch_set_track_nums(tmp_ps_obj, 2);
    speed_pitch_set_speed(tmp_ps_obj, PS_SPEED_DEFAULT_VAL);
    speed_pitch_set_pitch(tmp_ps_obj, PS_PITCHT_DEFAULT_VAL);
    mapi->ps_api = (void *)tmp_ps_obj;

    return true;
}

/*----------------------------------------------------------------------------*/
/**@brief 设置播放器改变播放速度
   @param
   mapi -- 解码器控制句柄
   val -- 速度值，具体参考speed_pitch_api.h说明
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_speed(MUSIC_PLAYER_OBJ *mapi, u16 val)
{
    if (mapi == NULL || mapi->ps_api == NULL) {
        return ;
    }
    speed_pitch_set_speed((PS_OBJ *)mapi->ps_api, val);
}


/*----------------------------------------------------------------------------*/
/**@brief 设置播放器改变播放语调
   @param
   mapi -- 解码器控制句柄
   val -- 变调值，具体参考speed_pitch_api.h说明
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_pitch(MUSIC_PLAYER_OBJ *mapi, u16 val)
{
    if (mapi == NULL || mapi->ps_api == NULL) {
        return ;
    }
    speed_pitch_set_pitch((PS_OBJ *)mapi->ps_api, val);
}

#if 0
void music_player_test_set_speed(MUSIC_PLAYER_OBJ *mapi)
{
    static u16 speed_val = 80;
    printf("speed_val = %d", speed_val);
    __music_player_set_speed(mapi, speed_val);
    speed_val += 20;
    if (speed_val > 160) {
        speed_val = 40;
    }
}

void music_player_test_set_pitch(MUSIC_PLAYER_OBJ *mapi)
{
    static u16 n = 0;
    static u8 flag = 0;
    u16 pitch_val;
    n += 1000;
    if (n > 20000) {
        n = 0;
        flag = !flag;
    }

    if (flag == 0) {
        pitch_val = 32768L + n;
    } else {
        pitch_val = 32768L - n;
    }

    printf("n = %d, flag = %d", n, flag);
    __music_player_set_pitch(mapi, pitch_val);
}
#endif


/*----------------------------------------------------------------------------*/
/**@brief 设置播放器使能人声消除
   @param
   mapi -- 解码器控制句柄
   en -- 使能开关
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_vocal_enable(MUSIC_PLAYER_OBJ *mapi, u8 en)
{
    if (mapi) {
        if (en) {
            mapi->status |= BIT(MUSIC_OP_STU_VOCAL);
        } else {
            mapi->status &= ~BIT(MUSIC_OP_STU_VOCAL);
        }
    }
}


extern dec_inf_t *get_dec_inf_api(void *priv);
static void music_player_output_cbk(void *priv, void *buf, u32 len)
{
    MUSIC_PLAYER_OBJ *mapi = (MUSIC_PLAYER_OBJ *)priv;
    if (mapi) {
        dec_inf_t *inf = get_dec_inf_api(mapi->dop_api->dec_api.phy_ops);
        if (inf) {
            if ((inf->nch == 2) && ((mapi->status & BIT(MUSIC_OP_STU_VOCAL)) != 0)) {
                putchar('A');
                dac_digital_lr_sub(buf, len);
            }
        }
    }
}


/*----------------------------------------------------------------------------*/
/**@brief 设置播放器数据流控制回调接口
   @param
   mapi -- 解码器控制句柄
   clr -- 清空缓存
   over -- 检查数据是否为空
   priv -- 私有属性
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_op_cbk(MUSIC_PLAYER_OBJ *mapi, void (*clr)(void *priv), tbool(*over)(void *priv), void *priv)
{
    if (mapi == NULL) {
        return ;
    }
    mapi->dop_api->io->op_cb.priv = priv;
    mapi->dop_api->io->op_cb.clr = clr;
    mapi->dop_api->io->op_cb.over = over;
}

/*----------------------------------------------------------------------------*/
/**@brief 设置播放器音频输出到指定叠加通道
   @param
   mapi -- 解码器控制句柄
   mix_obj -- 指定的叠加输出
   key -- 指定叠加通道名称
   mix_buf_len -- 叠加缓存大小
   threshold -- 缓存还剩下多少百分比的数据， 启动一下解码读数
   default_vol -- 输出到叠加通道的默认音量
   max_vol -- 输出到叠加通道的最大音量
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
tbool  __music_player_set_output_to_mix(MUSIC_PLAYER_OBJ *mapi, SOUND_MIX_OBJ *mix_obj, const char *key, u32 mix_buf_len, u8 Threshold, u8 default_vol, u8 max_vol)
{
    if (mapi == NULL || mix_obj == NULL) {
        return false;
    }
    if (mapi->ps_api) {
        music_player_printf("output setting err !!!, please check output setting order !!\n", n, flag);
        return false;
    }

    SOUND_MIX_P mix_p;
    memset((u8 *)&mix_p, 0x0, sizeof(SOUND_MIX_P));
    mix_p.key = key;
    mix_p.out_len = mix_buf_len;
    /* mix_p.set_info = &(mapi->dop_api->io->set_music_info); */
    mix_p.set_info_cb = &mapi->dop_api->io->set_info_cb;
    mix_p.input = &mapi->dop_api->io->output;
    mix_p.op_cb = &mapi->dop_api->io->op_cb;
    mix_p.limit = Threshold;
    mix_p.max_vol = max_vol;
    mix_p.default_vol = default_vol;
    mix_p.chl_cbk.priv = (void *)mapi;
    mix_p.chl_cbk.cbk = (void *)music_player_output_cbk;
    mapi->dop_api->io->mix = (void *)sound_mix_chl_add(mix_obj, &mix_p);

    return (mapi->dop_api->io->mix ? true : false);
}

/*----------------------------------------------------------------------------*/
/**@brief 设置播放器等待mix同步
   @param
   mapi -- 解码器控制句柄
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_mix_wait(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return ;
    }
    __sound_mix_chl_set_wait(mapi->dop_api->io->mix);
}

/*----------------------------------------------------------------------------*/
/**@brief 设置播放器等待mix同步完成
   @param
   mapi -- 解码器控制句柄
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_set_mix_wait_ok(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return ;
    }
    __sound_mix_chl_set_wait_ok(mapi->dop_api->io->mix);
}

/*----------------------------------------------------------------------------*/
/**@brief 播放器获取mix同步状态
   @param
   mapi -- 解码器控制句柄
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
s8 __music_player_get_mix_wait_status(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi) {
        return __sound_mix_chl_check_wait_ready(mapi->dop_api->io->mix);
    } else {
        return -1;
    }
}

/*----------------------------------------------------------------------------*/
/**@brief 设置播放器音频输出到指定叠加通道
   @param
   mapi -- 解码器控制句柄
   vol -- 设置输出到叠加通道的音量值
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void  __music_player_set_output_to_mix_set_vol(MUSIC_PLAYER_OBJ *mapi, u8 vol)
{
    if (mapi == NULL || mapi->dop_api->io->mix == NULL) {
        return;
    }

    __sound_mix_chl_set_vol(mapi->dop_api->io->mix, vol);
}

void __music_player_set_output_to_mix_mute(MUSIC_PLAYER_OBJ *mapi, u8 flag)
{
    if (mapi == NULL || mapi->dop_api->io->mix == NULL) {
        return;
    }
    __sound_mix_chl_set_mute(mapi->dop_api->io->mix, flag);
}

/*----------------------------------------------------------------------------*/
/**@brief 音乐播放器获取当前播放循环模式
   @param
   mapi -- 解码器控制句柄
   @return 歌曲播放总时间
   @note
*/
/*----------------------------------------------------------------------------*/
ENUM_PLAY_MODE __music_player_get_cur_play_mode(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return 0;
    }
    return mapi->fop_api->fop_init->cur_play_mode;
}



/*----------------------------------------------------------------------------*/
/**@brief 获取播放器使能人声消除状态
   @param
   mapi -- 解码器控制句柄
   @return 当前状态
   @note
*/
/*----------------------------------------------------------------------------*/
u8 __music_player_get_vocal_status(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return 0;
    }
    if (((mapi->status & BIT(MUSIC_OP_STU_VOCAL)) != 0)) {
        return 1;
    } else {
        return 0;
    }
}


//test
/* DEC_OP_CB *__music_player_get_op_interface(MUSIC_PLAYER_OBJ *mapi) */
/* { */
/* if (mapi == NULL) { */
/* return 0; */
/* } */
/* return (DEC_OP_CB *)&mapi->dop_api->io->op_cb; */
/* } */
/* _OP_IO *__music_player_get_ouput_interface(MUSIC_PLAYER_OBJ *mapi) */
/* { */
/* if (mapi == NULL) { */
/* return 0; */
/* } */
/* return (_OP_IO *)&mapi->dop_api->io->output; */
/* } */
/*----------------------------------------------------------------------------*/
/**@brief 音乐播放器获取当前歌曲总播放时间函数
   @param
   mapi -- 解码器控制句柄
   @return 歌曲播放总时间
   @note
*/
/*----------------------------------------------------------------------------*/
u32 __music_player_get_total_time(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return 0;
    }
    if (mapi->dop_api->dec_api.phy_ops == NULL) {
        return 0;
    }
    DEC_PHY *phy_ops = mapi->dop_api->dec_api.phy_ops;
    dec_inf_t *dec_inf = phy_ops->dec_ops->get_dec_inf(phy_ops->dc_buf);
    if (dec_inf) {
        return dec_inf->total_time;
    } else {
        return 0;
    }
}

/*----------------------------------------------------------------------------*/
/**@brief 音乐播放器获取当前歌曲的当前播放时间
   @param
   mapi -- 解码器控制句柄
   @return 歌曲当前播放时间
   @note
*/
/*----------------------------------------------------------------------------*/
u32 __music_player_get_play_time(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return 0;
    }
    if (mapi->dop_api->dec_api.phy_ops == NULL) {
        return 0;
    }
    DEC_PHY *phy_ops = mapi->dop_api->dec_api.phy_ops;
    return phy_ops->dec_ops->get_playtime(phy_ops->dc_buf);
}


/*----------------------------------------------------------------------------*/
/**@brief 音乐播放器获取当前歌曲号
   @param
   mapi -- 解码器控制句柄
   isgetreal -- 1：获取文件号在设备中的真正序号， 0：根据播放方式获取当前的序号
   @return 当前播放歌曲号
   @note
*/
/*----------------------------------------------------------------------------*/
u32 __music_player_get_cur_file_index(MUSIC_PLAYER_OBJ *mapi, u8 isGetReal)
{
    u32 file_index = 0;
    if (mapi == NULL) {
        file_index = 0;
    } else {
        if (isGetReal) {
            file_index = mapi->dop_api->file_num;
        } else {
            u32 f_file, e_file;
            switch (mapi->fop_api->fop_init->cur_play_mode) {
            case REPEAT_FOLDER:
                if (fs_folder_file(mapi->fop_api->cur_lgdev_info->lg_hdl->file_hdl, &f_file, &e_file) == true) {
                    file_index = mapi->dop_api->file_num - f_file + 1;
                } else {
                    file_index = 0;
                }
                break;
            case REPEAT_ONE:
                file_index = 1;
                break;
            default:
                file_index = mapi->dop_api->file_num;
                break;
            }
        }
    }
    music_player_printf("cur file index = %d, isGetReal = %d\n", file_index, isGetReal);
    return file_index;
}

/*----------------------------------------------------------------------------*/
/**@brief 音乐播放器获取当前歌曲总播放时间函数
   @param
   mapi -- 解码器控制句柄, isgetreal -- 1:获取设备的总文件数， 0：根据当前播放模式获取文件总数
   @return 歌曲播放总时间
   @note
*/
/*----------------------------------------------------------------------------*/
u32 __music_player_get_file_total(MUSIC_PLAYER_OBJ *mapi, u8 isGetReal)
{
    u32 file_total = 0;
    if (mapi == NULL) {
        file_total = 0;
    } else {
        if (isGetReal) {
            file_total = mapi->fop_api->cur_lgdev_info->total_file_num;
        } else {
            u32 f_file, e_file;
            switch (mapi->fop_api->fop_init->cur_play_mode) {
            case REPEAT_FOLDER:
                if (fs_folder_file(mapi->fop_api->cur_lgdev_info->lg_hdl->file_hdl, &f_file, &e_file) == true) {
                    file_total = (e_file - f_file) + 1;
                } else {
                    file_total = mapi->fop_api->cur_lgdev_info->total_file_num;
                }
                break;

            default:
                file_total = mapi->fop_api->cur_lgdev_info->total_file_num;
                break;
            }
        }
    }

    music_player_printf("music player get total file  = %d, isGetReal = %d\n", file_total, isGetReal);
    return file_total;
}

/*----------------------------------------------------------------------------*/
/**@brief 获取当前音乐播放器名称
   @param
   mapi -- 解码器控制句柄
   @return 歌曲所有的解码器名称
   @note
*/
/*----------------------------------------------------------------------------*/
char *__music_player_get_dec_name(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return NULL;
    }
    if (mapi->dop_api->dec_api.phy_ops == NULL) {
        return NULL;
    }
    DEC_PHY *phy_ops = mapi->dop_api->dec_api.phy_ops;
    return phy_ops->dec_ops->name;
}
/*----------------------------------------------------------------------------*/
/**@brief 获取当前音乐播放器所用的逻辑盘符
   @param
   mapi -- 解码器控制句柄
   @return 歌曲所用的逻辑盘符
   @note
*/
/*----------------------------------------------------------------------------*/
u32 __music_player_get_dev_let(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return 0;
    }
    return mapi->fop_api->fop_init->dev_let;
}
/*----------------------------------------------------------------------------*/
/**@brief 获取当前音乐播放器所用的设备选择模式
   @param
   mapi -- 解码器控制句柄
   @return 歌曲所用的设备选择模式
   @note
*/
/*----------------------------------------------------------------------------*/
ENUM_DEV_SEL_MODE __music_player_get_dev_sel_mode(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return 0;
    }
    return mapi->fop_api->fop_init->cur_dev_mode;
}
/*----------------------------------------------------------------------------*/
/**@brief 获取当前音乐播放器所用的物理设备号
   @param
   mapi -- 解码器控制句柄
   @return 歌曲所用的物理设备号
   @note
*/
/*----------------------------------------------------------------------------*/
u8  __music_player_get_phy_dev(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return 0;
    }
    u8 phy_dev = file_operate_ctl(FOP_GET_PHYDEV_INFO, mapi->fop_api, 0, 0);
    return phy_dev;
}
/*----------------------------------------------------------------------------*/
/**@brief 音乐播放器获取当前歌曲的设备控制句柄，主要与文件操作， 设备判断用
   @param
   mapi -- 解码器控制句柄
   @return 当前歌曲的逻辑设备控制信息
   @note
*/
/*----------------------------------------------------------------------------*/
lg_dev_info *__music_player_get_lg_dev_info(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return NULL;
    }
    return mapi->fop_api->cur_lgdev_info;
}

/*----------------------------------------------------------------------------*/
/**@brief 音乐播放器获取当前歌曲播放状态
   @param
   mapi -- 解码器控制句柄
   @return 歌曲播放状态，暂停/播放/停止
   @note
*/
/*----------------------------------------------------------------------------*/
_DECODE_STATUS __music_player_get_status(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return DECODER_STOP;
    }
    if (mapi->dop_api->dec_api.phy_ops == NULL) {
        return DECODER_STOP;
    }
    return mapi->dop_api->dec_api.phy_ops->status;
}

/*----------------------------------------------------------------------------*/
/**@brief 获取播放器当前速度值
   @param
   mapi -- 解码器控制句柄
   @return 当前速度值
   @note
*/
/*----------------------------------------------------------------------------*/
u16	__music_player_get_cur_speed(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return 0;
    }
    return speed_pitch_get_cur_speed(mapi->ps_api);
}

/*----------------------------------------------------------------------------*/
/**@brief 获取播放器当前变调值
   @param
   mapi -- 解码器控制句柄
   @return 当前变调值
   @note
*/
/*----------------------------------------------------------------------------*/
u16	__music_player_get_cur_pitch(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return 0;
    }
    return speed_pitch_get_cur_pitch(mapi->ps_api);
}

/*----------------------------------------------------------------------------*/
/**@brief 音乐播放器文件选择器操作函数
   @param
   mapi -- 解码器控制句柄
   cmd -- 文件选择命令
   input -- 文件选择控制输入
   output -- 文件选择输出
   @return 文件选择器操作结果
   @note
*/
/*----------------------------------------------------------------------------*/
u32 __music_player_file_operate_ctl(MUSIC_PLAYER_OBJ *mapi, u32 cmd, void *input, void *output)
{
    if (mapi == NULL) {
        return ERR_MUSIC_API_NULL;
    }
    return file_operate_ctl(cmd, mapi->fop_api, input, output);
}

/*----------------------------------------------------------------------------*/
/**@brief 音乐播放器清除当前歌曲断点信息
   @param
   mapi -- 解码器控制句柄
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void __music_player_clear_bp_info(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return ;
    }
    file_operate_ctl(FOP_CLEAR_BPINFO, mapi->fop_api, mapi->dop_api->brk, 0);
}

/*----------------------------------------------------------------------------*/
/**@brief 音乐播放器删除当前播放歌曲
   @param
   mapi -- 解码器控制句柄
   @return 删除成功或者失败
   @note
*/
/*----------------------------------------------------------------------------*/
tbool music_player_delete_playing_file(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return false;
    }
    u32 err;
    music_player_stop(mapi);

    _FIL_HDL *f_h = (_FIL_HDL *)(mapi->fop_api->cur_lgdev_info->lg_hdl->file_hdl);
    FIL *pf = (FIL *)(f_h->hdl);
    err = f_unlink(pf);
    music_player_printf("del err = %x\n", err);
    if (err != 0) {
        return false;
    }

    file_operate_ctl(FOP_GET_TOTALFILE_NUM, mapi->fop_api, mapi->dop_api->file_type, 0);

    return true;
}

/*----------------------------------------------------------------------------*/
/**@brief 音乐播放器快进或者快退
   @param
   mapi -- 解码器控制句柄
   fffr --PLAY_MOD_FB快退 PLAY_MOD_FF快退
   second -- 快进快退秒数
   @return 快进快退执行结果
   @note
*/
/*----------------------------------------------------------------------------*/
tbool music_player_fffr(MUSIC_PLAYER_OBJ *mapi, u8 fffr, u32 second)
{
    if (mapi == NULL || mapi->dop_api->dec_api.phy_ops == NULL) {
        return false;
    }

    DEC_PHY *phy_ops = mapi->dop_api->dec_api.phy_ops;
    DEC_API_IO *api_io = phy_ops->dec_io.priv;
    if (phy_ops->ff_fr.ff_fr_enable == 0) {
        return false;
    }
    if (__music_player_get_status(mapi) == DECODER_PAUSE) {
        music_player_pp(mapi);
    }
    if (api_io->op_cnt == 0) {
        phy_ops->ff_fr.ff_fr = fffr;
        phy_ops->dec_ops->set_step(phy_ops->dc_buf, second);

        dec_inf_t *dec_inf = phy_ops->dec_ops->get_dec_inf(phy_ops->dc_buf);
        api_io->op_cnt = ((dec_inf->sr * 4) / 10) * dec_inf->nch;
    } else {
        /* music_player_printf("op cnt is full!!\n"); */
        return false;
    }
    return true;
}

/*----------------------------------------------------------------------------*/
/**@brief 音乐播放器暂停/播放接口
   @param
   mapi -- 解码器控制句柄
   @return 执行之后播放器的状态，暂停/播放/停止
   @note
*/
/*----------------------------------------------------------------------------*/
_DECODE_STATUS music_player_pp(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return DECODER_STOP;
    }
    return pp_decode(&(mapi->dop_api->dec_api));
}

/*----------------------------------------------------------------------------*/
/**@brief 音乐播放器停止播放，但是保留文件相关信息
   @param
   mapi -- 解码器控制句柄
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void music_player_stop(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return ;
    }

    u32 err = stop_decoder(&(mapi->dop_api->dec_api));
    if (STOP_DEC_GOT_BRKINFO & err) {
        file_operate_ctl(FOP_SAVE_BPINFO, mapi->fop_api, mapi->dop_api->brk, 0);
    } else if (STOP_DEC_GOT_FLACBRKINFO & err) {
        file_operate_ctl(FOP_SAVE_FLACBPINFO, mapi->fop_api, mapi->dop_api->brk, 0);
    }
#if USB_DISK_EN
    //usb->dev_io->ioctrl(0,DEV_SET_REMAIN);
#endif
}

/*----------------------------------------------------------------------------*/
/**@brief 音乐播放器停止播放，并且关闭文件相关操作
   @param
   mapi -- 解码器控制句柄
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void music_player_close(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return ;
    }

    music_player_stop(mapi);
    __music_player_file_operate_ctl(mapi, FOP_CLOSE_LOGDEV, 0, 0);
}
/*----------------------------------------------------------------------------*/
/**@brief 音乐播放器启动函数，相关线程运行, 未播放
   @param
   mapi -- 解码器控制句柄
   @return 启动成功与失败
   @note
*/
/*----------------------------------------------------------------------------*/
tbool music_player_process(MUSIC_PLAYER_OBJ *mapi, void *task_name, u8 prio)
{
    if (mapi == NULL) {
        return false;
    }
    music_player_stop(mapi);
    if (mapi->dop_api->dec_api.dec_phy_name) {
        if (os_task_del_req(mapi->dop_api->dec_api.dec_phy_name) != OS_TASK_NOT_EXIST) {
            os_taskq_post_event(mapi->dop_api->dec_api.dec_phy_name, 1, SYS_EVENT_DEL_TASK);
            do {
                OSTimeDly(1);
            } while (os_task_del_req(mapi->dop_api->dec_api.dec_phy_name) != OS_TASK_NOT_EXIST);
            music_player_printf("del music dec phy succ\n");
        }
    }
    mapi->dop_api->dec_api.dec_phy_name = task_name;
    u32 err = os_task_create_ext(decoder_phy_task, 0, prio, 5, mapi->dop_api->dec_api.dec_phy_name, MUSIC_PHY_TASK_STACK_SIZE);
    /* u32 err = os_task_create_ext(decoder_phy_task, 0, TaskMusicPhyDecPrio, 5, mapi->dop_api->dec_api.dec_phy_name, MUSIC_PHY_TASK_STACK_SIZE); */
    if (err != OS_NO_ERR) {
        music_player_printf("decoder_phy_task creat fail !!!\r");
        return false;
    }
    return true;
}


/*----------------------------------------------------------------------------*/
/**@brief 音乐播放器播放启动函数，开始播放
   @param
   mapi -- 解码器控制句柄
   @return 播放错误列表
   @note
*/
/*----------------------------------------------------------------------------*/
u32 music_player_play(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return ERR_MUSIC_API_NULL;
    }
    mapi->status = 0;
    music_player_printf("cur_dev_mode = %d, dev_let = %c, cur_play_mode = %d, filenum = %d\n",
                        mapi->fop_api->fop_init->cur_dev_mode,
                        mapi->fop_api->fop_init->dev_let,
                        mapi->fop_api->fop_init->cur_sel_mode,
                        mapi->dop_api->file_num);
    return music_play(mapi);

}


/*----------------------------------------------------------------------------*/
/**@brief 音乐播放器播放启动函数，开始播放，该接口支持自定义读写等接口，并且使用自动查找设备内文件，例如自定义加密播放
   @param
   mapi -- 解码器控制句柄
   StartAddr -- 文件播放的起始位置
   medType -- 解码格式
   func -- 回调函数接口
   @return 播放错误列表
   @note
*/
/*----------------------------------------------------------------------------*/
u32 music_player_mulfile_play(MUSIC_PLAYER_OBJ *mapi, u32 StartAddr, u8 medType, mulfile_func_t *func)
{
    u32 err;
    if (mapi == NULL) {
        return ERR_MUSIC_API_NULL;
    }
    mapi->status |= BIT(MUSIC_OP_STU_NO_DEC);
    err = music_play(mapi);
    if (err == 0) {
        err = file_play_start(mapi, StartAddr, medType, func);
    }
    return err;
}

/*----------------------------------------------------------------------------*/
/**@brief 音乐播放器播放启动函数，开始播放，该函数支持自定义读写等接口，所有文件操作有用户定义， 例如网络数据播放的实现
   @param
   mapi -- 解码器控制句柄
   StartAddr -- 文件播放的起始位置
   medType -- 解码格式
   func -- 回调函数接口
   @return 播放错误列表
   @notej
*/
/*----------------------------------------------------------------------------*/
u32 music_player_mulfile_purely_play(MUSIC_PLAYER_OBJ *mapi, u32 StartAddr, u8 medType, mulfile_func_t *func)
{
    return file_play_start(mapi, StartAddr, medType, func);
}


