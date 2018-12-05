#include "midi_player.h"
#include "dec/MIDI_CTRL_API.h"
#include "dec/decoder_phy.h"
#include "dac/dac_api.h"
#include "midi_prog_tab.h"
#include "cbuf/circular_buf.h"
#include "dec/sup_dec_op.h"
#include "file_operate/file_op.h"
#include "common/msg.h"
#include "fat/fs_io.h"
#include "fat/tff.h"
#include "common/app_cfg.h"
#include "file_operate/logic_dev_list.h"

/* #define MIDI_DEBUG_ENABLE */
#ifdef MIDI_DEBUG_ENABLE
#define midi_printf printf
#else
#define midi_printf(...)
#endif//MIDI_DEBUG_ENABLE


#define SIZEOF_ALIN(var,al)     ((((var)+(al)-1)/(al))*(al))

#define MIDI_CFG_FILE_NAME	"midi_cfg.bin"
#define SPI_CACHE_READ_ADDR(addr)  (u32)(0x1000000+(((u32)addr)))


extern u32 lg_dev_mount(void *parm, u8 first_let, u32 total_lgdev);
extern u32 file_operate_ini(void *parm, u32 *chg_flag);
extern u32 file_operate_ini_again(void *parm, u32 *chg_flag);

extern lg_dev_list *lg_dev_find(u32 drv);
extern tbool syd_drive_open(void **p_fs_hdl, void *p_hdev, u32 drive_base);
extern tbool syd_drive_close(void **p_fs_hdl);
extern bool syd_get_file_byindex(void *p_fs_hdl, void **p_f_hdl, u32 file_number, char *lfn);
extern bool syd_file_close(void *p_fs_hdl, void **p_f_hdl);
extern u16 syd_read(void *p_f_hdl, u8 _xdata *buff, u16 len);
extern bool syd_get_file_bypath(void *p_fs_hdl, void **p_f_hdl, u8 *path, char *lfn);
extern tbool syd_get_file_addr(u32 *addr, void **p_f_hdl);

extern cbuffer_t audio_cb ;
extern u8 dac_read_en;


const u16 tempo_list[] = {
    [TEMPO_FAST_9] = 512,
    [TEMPO_FAST_8] = 563,
    [TEMPO_FAST_7] = 614,
    [TEMPO_FAST_6] = 665,
    [TEMPO_FAST_5] = 716,
    [TEMPO_FAST_4] = 767,
    [TEMPO_FAST_3] = 818,
    [TEMPO_FAST_2] = 869,
    [TEMPO_FAST_1] = 920,
    [TEMPO_FAST_0] = 971,
    [TEMPO_NORMAL] = 1024,
    [TEMPO_SLOW_9] = 1126,
    [TEMPO_SLOW_8] = 1228,
    [TEMPO_SLOW_7] = 1330,
    [TEMPO_SLOW_6] = 1432,
    [TEMPO_SLOW_5] = 1534,
    [TEMPO_SLOW_4] = 1636,
    [TEMPO_SLOW_3] = 1738,
    [TEMPO_SLOW_2] = 1840,
    [TEMPO_SLOW_1] = 1942,
    [TEMPO_SLOW_0] = 2048,
};

typedef struct __MIDI_DECODE_OPS {
    void 			  *hdl;
    audio_decoder_ops *ops;
} MIDI_DECODE_OPS;



typedef struct __MIDI_DECCODE_FILE {
    void 			     *hdl;
    MIDI_DECCODE_FILE_IO *_io;
} MIDI_DECCODE_FILE;


typedef struct __MIDI_DATA_STREAM {
    DEC_SET_INFO_CB set_info_cb;
    DEC_OP_CB		op_cb;
    _OP_IO 			output;
} MIDI_DATA_STREAM;


struct __MIDI_PLAYER {
    void              *father_name;
    void              *dec_name;
    FILE_OPERATE      *f_op;
    MIDI_DECCODE_FILE  file;
    MIDI_INIT_STRUCT   ma_init;
    MIDI_DECODE_OPS    dec_ops;
    IF_DECODER_IO      dec_io;
    MIDI_DATA_STREAM   stream_io;
    u32                file_num;
    u8				   read_err;
    volatile u8 	   set_flag;
    volatile MIDI_ST   status;
    void 			  *mix;
};


static const MIDI_DECCODE_FILE_IO midi_file_io = {
    .seek = (void *)fs_seek,
    .read = (void *)fs_read,
    .get_size = (void *)fs_length,
};



bool midi_player_get_cfg_addr(u8 **addr)
{
    bool ret = false;
    lg_dev_list *pnode;
    void *fs_hdl = NULL;  //文件系统句柄
    void *file_hdl = NULL;//文件句柄

    pnode = lg_dev_find('A');
#ifndef TOYS_EN
    if (!syd_drive_open(&fs_hdl, pnode->cur_dev.lg_hdl->phydev_item, 0)) //打开文件系统
#else /*TOYS_EN*/
    if (!syd_drive_open(&fs_hdl, pnode->p_cur_dev->lg_hdl->phydev_item, 0)) //打开文件系统
#endif /*TOYS_EN*/
    {
        midi_printf("syd_drive_open err!\n");
        return false;
    }
    ret = syd_get_file_bypath(fs_hdl, &file_hdl, (u8 *)MIDI_CFG_FILE_NAME, 0);
    if (ret == false) {
        midi_printf("syd_get_file_byindex err!\n");
    } else {
        u32 file_addr;
        tbool ret1;
        ret1 = syd_get_file_addr((u32 *)&file_addr, &file_hdl);
        if (ret1 == true) {
            ret = true;
            *addr = (u8 *)SPI_CACHE_READ_ADDR(file_addr);
            midi_printf("syd get file ok !!, addr =  %x\n", *addr);
        } else {
            ret = false;
            midi_printf("obj get file addr err!!\n");
        }
    }


    syd_file_close(fs_hdl, &file_hdl); //完成操作后，关闭文件
    syd_drive_close(&fs_hdl);  //完成操作后，关闭文件系统
    return ret;
}




static tbool midi_player_data_over(void *priv)
{
    //printf("%4d %4d\r",audio_cb.data_len,DAC_BUF_LEN);

    if ((audio_cb.data_len < DAC_BUF_LEN) || (!dac_read_en)) {
        if (audio_cb.data_len > DAC_BUF_LEN) {
            printf("warning output disable when output buff has data\r");
        }
        return true;
    } else {
        return false;
    }
}

static void midi_player_data_clear(void *priv)
{
    //priv = priv;
    cbuf_clear(&audio_cb);
}


static void *midi_player_output(void *priv, void *buf, u32 len)
{
    dac_write((u8 *)buf, len);
    return priv;
}


static s32 midi_player_decoder_input(void *priv, u32 faddr, u8 *buf, u32 len, u8 type)
{
    MIDI_PLAYER *obj = (MIDI_PLAYER *)priv;
    MIDI_DECCODE_FILE *file = &(obj->file);
    if (file->_io == NULL) {
        midi_printf("midi file io is null, fun = %s, line = %d\n", __FUNCTION__, __LINE__);
        return 0;
    }
    u16 read_len0 = 0;
    if (0 == type) {
        if (file->_io->seek) {
            file->_io->seek(file->hdl, SEEK_SET, faddr);
        }
        if (file->_io->read) {
            read_len0 = file->_io->read(file->hdl, buf, len);
        }
    } else {
        //dec_phy_printf("decoder_input xx : %08x\r\n",(u32)buf);
    }
    if ((u16) - 1 == read_len0) {
        obj->read_err = 1;
        return 0;
    } else {
        return read_len0;
    }
}




static s32 midi_player_decoder_check_buf(void *priv, u32 faddr, u8 *buf)
{
    s32 read_len = 0;
    u16 len = 0;
    MIDI_PLAYER *obj = (MIDI_PLAYER *)priv;
    MIDI_DECCODE_FILE *file = &(obj->file);
    if (file->_io == NULL) {
        midi_printf("midi file io is null, fun = %s, line = %d\n", __FUNCTION__, __LINE__);
        return 0;
    }
    if (file->_io->seek) {
        file->_io->seek(file->hdl, SEEK_SET, faddr);
    }
    if (file->_io->read) {
        len = file->_io->read(file->hdl, buf, 512);
    }
    if ((u16) - 1 == len) {
        obj->read_err = 1;
        return 0;
    }
    read_len = len;
    return read_len;
}


static void midi_player_decoder_output(void *priv, void *buf, u32 len)
{

    MIDI_PLAYER *obj = (MIDI_PLAYER *)priv;
    if (obj->stream_io.output.output) {
        obj->stream_io.output.output(obj->stream_io.output.priv, buf, len);
    }
}

static u32 midi_player_get_lslen(void *priv)
{
    u32 ret;
    ret = -1;
    return ret;
}

static u32 midi_player_store_rev_data(void *priv, u32 addr, int len)
{
    return len;
}



static tbool midi_player_set_wait_ok(MIDI_PLAYER *obj, u32 msg, u32 val)
{
    if (obj == NULL) {
        return false;
    }
    u8 err = 0;
    obj->set_flag = 1;
    /* OS_TCB *cur_task_tcb = 	OSGetTaskTcb(OS_TASK_SELF); */

    /* if(strcmp(cur_task_tcb->name, obj->dec_name) == 0) */
    /* { */
    /* err = os_taskq_post(obj->dec_name, 2, msg, val); */
    /* if(err) */
    /* { */
    /* return false;		 */
    /* } */
    /* } */
    /* else */
    {
        while (1) {
            err = os_taskq_post(obj->dec_name, 2, msg, val);
            if (err == OS_Q_FULL) {
                OSTimeDly(1);
            } else {
                if (err) {
                    obj->set_flag = 0;
                    return false;
                }
                break;
            }
        }

        while (obj->set_flag == 1) {
            OSTimeDly(1);
        }
    }

    return true;
}

/*----------------------------------------------------------------------------*/
/**@brief   midi播放器资源创建
   @param   void
   @return  midi控制句柄
   @note
*/
/*----------------------------------------------------------------------------*/
MIDI_PLAYER *midi_player_creat(void)
{
    audio_decoder_ops *ops = get_midi_ops();
    if (ops == NULL) {
        return NULL;
    }

    u32 need_buf_len = ops->need_dcbuf_size();

    u32 buf_len = (SIZEOF_ALIN(sizeof(MIDI_PLAYER), 4)
                   + SIZEOF_ALIN(sizeof(FILE_OPERATE), 4)
                   + SIZEOF_ALIN(sizeof(FILE_OPERATE_INIT), 4)
                   + SIZEOF_ALIN(need_buf_len, 4));

    u8 *need_buf = (u8 *)calloc(1, buf_len);

    if (need_buf == NULL) {
        return NULL;
    }

    MIDI_PLAYER *obj = (MIDI_PLAYER *)need_buf;
    need_buf += SIZEOF_ALIN(sizeof(MIDI_PLAYER), 4);


    obj->f_op = (FILE_OPERATE *)need_buf;
    need_buf += SIZEOF_ALIN(sizeof(FILE_OPERATE), 4);

    obj->f_op->fop_init = (FILE_OPERATE_INIT *)need_buf;
    need_buf += SIZEOF_ALIN(sizeof(FILE_OPERATE_INIT), 4);

    obj->dec_ops.hdl = (void *)need_buf;
    obj->dec_ops.ops = ops;

    obj->file.hdl = NULL;
    obj->file._io = (MIDI_DECCODE_FILE_IO *)&midi_file_io;


    obj->stream_io.set_info_cb.priv = NULL;
    obj->stream_io.set_info_cb._cb = set_music_sr;
    obj->stream_io.op_cb.priv = NULL;
    obj->stream_io.op_cb.clr = (void *)midi_player_data_clear;
    obj->stream_io.op_cb.over = (void *)midi_player_data_over;
    obj->stream_io.output.priv = NULL;
    obj->stream_io.output.output = midi_player_output;

    obj->dec_io.priv = (void *)obj;
    obj->dec_io.get_lslen = (void *)midi_player_get_lslen;
    obj->dec_io.store_rev_data = (void *)midi_player_store_rev_data;
    obj->dec_io.input = (void *)midi_player_decoder_input;
    obj->dec_io.check_buf = (void *)midi_player_decoder_check_buf;
    obj->dec_io.output = (void *)midi_player_decoder_output;

    u8 *midi_cache_addr;
    if (midi_player_get_cfg_addr(&midi_cache_addr) == false) {
        midi_player_destroy(&obj);
        return NULL;
    }
    obj->ma_init.init_info.spi_pos = (unsigned char *)midi_cache_addr;

    ///default config
    u16 midi_default_vol_tab[CTRL_CHANNEL_NUM];
    u16 i;
    for (i = 0; i < CTRL_CHANNEL_NUM; i++) {
        midi_default_vol_tab[i] = 4096;//4096是原始音量， 8192外部音量放大一倍
    }

    obj->ma_init.switch_info = 0;

    __midi_player_set_vol_tab(obj, midi_default_vol_tab);
    __midi_player_set_mode(obj, CMD_MIDI_CTRL_MODE_2);
    __midi_player_set_noteon_max_nums(obj, 8);
    __midi_player_set_samplerate(obj, 5);
    __midi_player_set_tempo(obj, TEMPO_NORMAL);
    __midi_player_set_main_chl_prog(obj, 0, 0);
    __midi_player_set_mainTrack_channel(obj, 1);
    __midi_player_set_mark_trigger(obj, NULL, NULL);
    __midi_player_set_beat_trigger(obj, NULL, NULL);
    __midi_player_set_timeDiv_trigger(obj, NULL, NULL);
    __midi_player_set_melody_trigger(obj, NULL, NULL);
    __midi_player_set_switch_disable(obj, 0xFFFFFFFF);///清除所有使能
    __midi_player_set_switch_enable(obj, MELODY_PLAY_ENABLE);
    __midi_player_set_play_mode(obj, REPEAT_FOLDER);

    __midi_player_set_melody_decay(obj, 1024); //具体看函数说明
    __midi_player_set_melody_delay(obj, 0); //具体看函数说明

    __midi_player_set_melody_mute_threshold(obj, (u32)(1 << 26));

    midi_printf("midi_creat ok!!\n");

    return obj;
}




/*----------------------------------------------------------------------------*/
/**@brief   midi播放器资源释放
   @param   midi 控制句柄， 注意是二重指针
   @return  void
   @note
*/
/*----------------------------------------------------------------------------*/
void midi_player_destroy(MIDI_PLAYER **obj)
{
    if (obj == NULL || (*obj) == NULL) {
        return ;
    }
    MIDI_PLAYER *hdl = *obj;
    midi_player_stop(hdl);
    if (os_task_del_req(hdl->dec_name) != OS_TASK_NOT_EXIST) {
        os_taskq_post_event(hdl->dec_name, 1, SYS_EVENT_DEL_TASK);
        do {
            OSTimeDly(1);
        } while (os_task_del_req(hdl->dec_name) != OS_TASK_NOT_EXIST);
        /* midi_printf("del MIDI_DEC_TASK_NAME succ\n"); */
    }
    file_operate_ctl(FOP_CLOSE_LOGDEV, hdl->f_op, 0, 0);

    if (hdl->mix) {
        sound_mix_chl_del_by_chl((SOUND_MIX_CHL **)&hdl->mix);
    }

    free(*obj);
    *obj = NULL;
}

/*----------------------------------------------------------------------------*/
/**@brief   midi播放器设置控制线程名称
   @param   obj:播放器控制句柄
   @param   father_name:控制线程名称
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_father_name(MIDI_PLAYER *obj, void *father_name)
{
    if (obj) {
        obj->father_name = father_name;
    }
}
/*----------------------------------------------------------------------------*/
/**@brief   midi播放器设置逻辑设备盘符
   @param   obj:播放器控制句柄
   @param   dev_let:逻辑设备盘符
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_dev_let(MIDI_PLAYER *obj, u32 dev_let)
{
    if (obj) {
        obj->f_op->fop_init->dev_let = dev_let;
    }
}
/*----------------------------------------------------------------------------*/
/**@brief   midi播放器设置文件播放路径
   @param   obj:播放器控制句柄
   @param   path:文件播放路径
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_path(MIDI_PLAYER *obj, const char *path)
{
    if (obj) {
        obj->f_op->fop_init->filepath = (u8 *)path;
    }
}
/*----------------------------------------------------------------------------*/
/**@brief   midi播放器设置文件播放序号
   @param   obj:播放器控制句柄
   @param   filenum:文件序号
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_filenum(MIDI_PLAYER *obj, u32 filenum)
{
    if (obj) {
        obj->file_num = filenum;
    }
}
/*----------------------------------------------------------------------------*/
/**@brief   midi播放器设置循环模式
   @param   obj:播放器控制句柄
   @param   mode:循环模式
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_play_mode(MIDI_PLAYER *obj, ENUM_PLAY_MODE mode)
{
    if (obj) {
        obj->f_op->fop_init->cur_play_mode = mode;
    }
}
/*----------------------------------------------------------------------------*/
/**@brief   midi播放器设置文件选择模式
   @param   obj:播放器控制句柄
   @param   mode:文件选择模式
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_sel_mode(MIDI_PLAYER *obj, ENUM_FILE_SELECT_MODE mode)
{
    if (obj) {
        obj->f_op->fop_init->cur_sel_mode = mode;
    }
}
/*----------------------------------------------------------------------------*/
/**@brief   midi播放器设置设备选择模式
   @param   obj:播放器控制句柄
   @param   mode:设备选择模式
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_dev_mode(MIDI_PLAYER *obj, ENUM_DEV_SEL_MODE mode)
{
    if (obj) {
        obj->f_op->fop_init->cur_dev_mode = mode;
    }
}
/*----------------------------------------------------------------------------*/
/**@brief   midi播放器最大通道数
   @param   obj:播放器控制句柄
   @param   total:最大通道数， 即同时播放音符的个数
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_noteon_max_nums(MIDI_PLAYER *obj, s16 total)
{
    if (obj) {
        obj->ma_init.init_info.player_t = total;
    }
}
/*----------------------------------------------------------------------------*/
/**@b:ief   midi播放器采样率设置
   @param   obj:播放器控制句柄
   @param   采样率
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_samplerate(MIDI_PLAYER *obj, u8 samp)
{
    if (obj) {
        obj->ma_init.init_info.sample_rate = samp;
    }
}
/*----------------------------------------------------------------------------*/
/**@brief   midi播放器变速设置
   @param   obj:播放器控制句柄
   @param   prog_index:tempo_list对应的坐标
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_tempo(MIDI_PLAYER *obj, u8 tempo_index)
{
    if (obj) {
        if (tempo_index < sizeof(tempo_list) / sizeof(tempo_list[0])) {
            obj->ma_init.tempo_info.tempo_val = tempo_list[tempo_index];
            midi_player_set_wait_ok(obj, MSG_MODULE_MIDI_SET_PARAM, CMD_MIDI_CTRL_TEMPO);
        }
    }
}

/*----------------------------------------------------------------------------*/
/**@brief   midi播放器直接配置参数变速设置
   @param   obj:播放器控制句柄
   @param   val:变速变速tmpo值
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_tempo_directly(MIDI_PLAYER *obj, u16 val)
{
    if (obj) {
        obj->ma_init.tempo_info.tempo_val = val;
    }
}

/*----------------------------------------------------------------------------*/
/**@brief   midi播放器设置音符衰减速度
   @param   obj:播放器控制句柄
   @param   低11bit为调节衰减参数的, 值越小，衰减的越快， 1024为默认值，范围为：0~1024
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_melody_decay(MIDI_PLAYER *obj, u16 val)
{
    if (obj) {
        obj->ma_init.tempo_info.decay_val = ((obj->ma_init.tempo_info.decay_val & ~(0x7ff)) | (val & 0x7ff));
        midi_printf("decay_val = %x, line = %d\n", obj->ma_init.tempo_info.decay_val, __LINE__);
        midi_player_set_wait_ok(obj, MSG_MODULE_MIDI_SET_PARAM, CMD_MIDI_CTRL_TEMPO);
    }
}


/*----------------------------------------------------------------------------*/
/**@brief   midi播放器设置音符播放延时
   @param   obj:播放器控制句柄
   @param   val:延时系数, 节尾音长度的, 值越大拉得越长, 0为默认值, 31延长1s，范围为：0~31，
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_melody_delay(MIDI_PLAYER *obj, u16 val)
{
    if (obj) {
        obj->ma_init.tempo_info.decay_val = ((obj->ma_init.tempo_info.decay_val & ~(0x1f << 11)) | ((val & 0x1f) << 11));
        midi_printf("decay_val = %x, line = %d\n", obj->ma_init.tempo_info.decay_val, __LINE__);
        midi_player_set_wait_ok(obj, MSG_MODULE_MIDI_SET_PARAM, CMD_MIDI_CTRL_TEMPO);
    }
}

/*----------------------------------------------------------------------------*/
/**@brief   midi播放器音符衰减结束阀值设置，主要用于音符播放结束获取
   @param   obj:播放器控制句柄
   @param   val:，阀值 建议值为1L<<29
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_melody_mute_threshold(MIDI_PLAYER *obj, u32 val)
{
    if (obj) {
        obj->ma_init.tempo_info.mute_threshold = val;
        midi_player_set_wait_ok(obj, MSG_MODULE_MIDI_SET_PARAM, CMD_MIDI_CTRL_TEMPO);
    }
}

/*----------------------------------------------------------------------------*/
/**@brief   midi播放器音色设置
   @param   obj:播放器控制句柄
   @param   prog_index:prog_list对应音色的坐标
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_main_chl_prog(MIDI_PLAYER *obj, u8 prog_index, u8 mode)
{
    if (obj) {

        if (prog_index >= (sizeof(prog_list) / sizeof(prog_list[0]))) {
            return ;
        }

        u8 prog = prog_list[prog_index];
        u16 prog_ex_vol = prog_ex_vol_list[prog_index];

        obj->ma_init.prog_info.prog = prog;
        obj->ma_init.prog_info.replace_mode = mode;
        obj->ma_init.prog_info.ex_vol = prog_ex_vol;
        midi_player_set_wait_ok(obj, MSG_MODULE_MIDI_SET_PARAM, CMD_MIDI_SET_CHN_PROG);
    }
}
/*----------------------------------------------------------------------------*/
/**@brief   midi播放器主通道选择设置
   @param   obj:播放器控制句柄
   @param   chl:通道
   @return
   @note 设置主轨道， 当设置的主轨道， 超出了总轨道数16， 会自动选取主轨道， 自动选取的主轨道是第一个发声的轨道
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_mainTrack_channel(MIDI_PLAYER *obj, u8 chl)
{
    if (obj) {
        obj->ma_init.mainTrack_info.chn = chl;
    }
}
/*----------------------------------------------------------------------------*/
/**@brief   midi播放器mark回调设置
   @param   obj:播放器控制句柄
   @param   mark_trigger:回调接口回调函数
   @param   priv:回调接口回调函数的私有参数
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_mark_trigger(MIDI_PLAYER *obj, u32(*mark_trigger)(void *priv, u8 *val, u8 len), void *priv)
{
    if (obj) {
        obj->ma_init.mark_info.priv = priv;
        obj->ma_init.mark_info.mark_trigger = mark_trigger;
    }
}
/*----------------------------------------------------------------------------*/
/**@brief   midi播放器拍回调设置
   @param   obj:播放器控制句柄
   @param   beat_trigger:回调接口回调函数
   @param   priv:回调接口回调函数的私有参数
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_beat_trigger(MIDI_PLAYER *obj, u32(*beat_trigger)(void *priv, u8 val1, u8 val2), void *priv)
{
    if (obj) {
        obj->ma_init.beat_info.priv = priv;
        obj->ma_init.beat_info.beat_trigger = beat_trigger;
    }
}
/*----------------------------------------------------------------------------*/
/**@brief   midi播放器音符回调设置
   @param   obj:播放器控制句柄
   @param   melody_trigger:回调接口回调函数
   @param   priv:回调接口回调函数的私有参数
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_melody_trigger(MIDI_PLAYER *obj, u32(*melody_trigger)(void *priv, u8 key, u8 vel), void *priv)
{
    if (obj) {
        obj->ma_init.moledy_info.priv = priv;
        obj->ma_init.moledy_info.melody_trigger = melody_trigger;
    }
}
/*----------------------------------------------------------------------------*/
/**@brief   midi播放器小节回调设置
   @param   obj:播放器控制句柄
   @param   timeDiv_trigger:小节回调接口回调函数
   @param   priv:小节回调接口回调函数的私有参数
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_timeDiv_trigger(MIDI_PLAYER *obj, u32(*timeDiv_trigger)(void *priv), void *priv)
{
    if (obj) {
        obj->ma_init.tmDiv_info.priv = priv;
        obj->ma_init.tmDiv_info.timeDiv_trigger = timeDiv_trigger;
    }
}
/*----------------------------------------------------------------------------*/
/**@brief   midi播放器控制模式设置接口
   @param   obj:播放器控制句柄
   @param   mode:控制模式
   @return
   @note	用户可以在任意有需要变换播放模式的地方调用来改变播放模式
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_mode(MIDI_PLAYER *obj, u8 mode)
{
    if (obj) {
        obj->ma_init.mode_info.mode = mode;
        midi_player_set_wait_ok(obj, MSG_MODULE_MIDI_SET_PARAM, CMD_MIDI_CTRL_MODE);

    }
}
/*----------------------------------------------------------------------------*/
/**@brief   midi播放器打开某个控制开关接口
   @param   obj:播放器控制句柄
   @param   sw:某个控制
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_switch_enable(MIDI_PLAYER *obj, u32 sw)
{
    if (obj) {
        obj->ma_init.switch_info |= sw;
    }
}


/*----------------------------------------------------------------------------*/
/**@brief   midi播放器关闭某个控制开关接口
   @param   obj:播放器控制句柄
   @param   sw:某个控制
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_switch_disable(MIDI_PLAYER *obj, u32 sw)
{
    if (obj) {
        obj->ma_init.switch_info &= ~(sw);
    }
}

/*----------------------------------------------------------------------------*/
/**@brief   midi播放器资源小节回跳接口
   @param   obj:播放器控制句柄
   @param   seek_n:例如-1表示回跳一个小节
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_seek_back_n(MIDI_PLAYER *obj, s8 seek_n)
{
    if (obj) {
        MIDI_SEEK_BACK_STRUCT   seek_s;
        seek_s.seek_back_n = seek_n;
        obj->dec_ops.ops->dec_confing(obj->dec_ops.hdl, CMD_MIDI_SEEK_BACK_N, &seek_s);
        //os_taskq_post(MIDI_DEC_TASK_NAME, 3, MSG_MODULE_MIDI_SET_PARAM, CMD_MIDI_SEEK_BACK_N, (u32)seek_n);
    }
}

/*----------------------------------------------------------------------------*/
/**@brief   midi播放器设置对应通道的音量值
   @param   obj:播放器控制句柄
   @param   tab:16通道的音量配置数组指针
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_vol_tab(MIDI_PLAYER *obj, u16 tab[CTRL_CHANNEL_NUM])
{
    if (obj) {
        memcpy((u8 *)obj->ma_init.vol_info.cc_vol, (u8 *)tab, CTRL_CHANNEL_NUM * sizeof(u16));
        midi_player_set_wait_ok(obj, MSG_MODULE_MIDI_SET_PARAM, CMD_MIDI_SET_EX_VOL);
    }
}

/*----------------------------------------------------------------------------*/
/**@brief   midi播放器在onekeyonenote过程中往下执行动作设置， 被调用一次，往下播放一个音符
   @param   obj:播放器控制句柄
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_go_on(MIDI_PLAYER *obj)
{
    if (obj) {
        midi_player_set_wait_ok(obj, MSG_MODULE_MIDI_SET_PARAM, CMD_MIDI_GOON);
    }
}
/*----------------------------------------------------------------------------*/
/**@brief 设置midi播放器音频输出到指定叠加通道
   @param
   obj -- 解码器控制句柄
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
tbool  __midi_player_set_output_to_mix(MIDI_PLAYER *obj, SOUND_MIX_OBJ *mix_obj, const char *key, u32 mix_buf_len, u8 Threshold, u8 default_vol, u8 max_vol)
{
    if (obj == NULL || mix_obj == NULL) {
        return false;
    }

    SOUND_MIX_P mix_p;
    memset((u8 *)&mix_p, 0x0, sizeof(SOUND_MIX_P));
    mix_p.key = key;
    mix_p.out_len = mix_buf_len;
    /* mix_p.set_info = &(mapi->dop_api->io->set_music_info); */
    mix_p.set_info_cb = &obj->stream_io.set_info_cb;
    mix_p.input = &obj->stream_io.output;
    mix_p.op_cb = &obj->stream_io.op_cb;
    mix_p.limit = Threshold;
    mix_p.max_vol = max_vol;
    mix_p.default_vol = default_vol;
    obj->mix = (void *)sound_mix_chl_add(mix_obj, &mix_p);

    return (obj->mix ? true : false);
}


/*----------------------------------------------------------------------------*/
/**@brief   midi播放器设置输出到mix等待同步使能
   @param   obj:播放器控制句柄
   @return
   @note 直达__midi_player_set_mix_wait_ok设置等待同步完成才会输出音频
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_mix_wait(MIDI_PLAYER *obj)
{
    if (obj == NULL) {
        return ;
    }
    __sound_mix_chl_set_wait(obj->mix);
}

/*----------------------------------------------------------------------------*/
/**@brief   midi播放器设置输出到mix等待同步完成
   @param   obj:播放器控制句柄
   @return
   @note:改操作需要关中断来完成
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_mix_wait_ok(MIDI_PLAYER *obj)
{
    if (obj == NULL) {
        return ;
    }
    __sound_mix_chl_set_wait_ok(obj->mix);
}

/*----------------------------------------------------------------------------*/
/**@brief   midi播放器获取同步状态是否就绪
   @param   obj:播放器控制句柄
   @return  -1:获取状态失败，0:未同步成功 1：同步成功
   @note
*/
/*----------------------------------------------------------------------------*/
s8 __midi_player_get_mix_wait_status(MIDI_PLAYER *obj)
{
    if (obj) {
        return __sound_mix_chl_check_wait_ready(obj->mix);
    } else {
        return -1;
    }
}

/*----------------------------------------------------------------------------*/
/**@brief 设置midi播放器音频输出到指定叠加通道
   @param
   obj -- 解码器控制句柄
   vol -- 设置输出到叠加通道的音量值
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void  __midi_player_set_output_to_mix_set_vol(MIDI_PLAYER *obj, u8 vol)
{
    if (obj == NULL || obj->mix == NULL) {
        return;
    }

    __sound_mix_chl_set_vol(obj->mix, vol);
}

void __midi_player_set_output_to_mix_mute(MIDI_PLAYER *obj, u8 flag)
{
    if (obj == NULL || obj->mix == NULL) {
        return;
    }
    __sound_mix_chl_set_mute(obj->mix, flag);
}

/*----------------------------------------------------------------------------*/
/**@brief   midi播放器设置文件操作接口
   @param   obj:播放器控制句柄 _io:file操作接口
   @return 	void
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_player_set_file_io(MIDI_PLAYER *obj, MIDI_DECCODE_FILE_IO *_io)
{
    if (obj) {
        obj->file._io = _io;
    }
}


/*----------------------------------------------------------------------------*/
/**@brief   midi播放器获取控制使能状态
   @param   obj:播放器控制句柄
   @return 	控制使能状态
   @note
*/
/*----------------------------------------------------------------------------*/
u32 __midi_player_get_switch_enable(MIDI_PLAYER *obj)
{
    if (obj) {
        return obj->ma_init.switch_info;
    }
    return 0;
}
/*----------------------------------------------------------------------------*/
/**@brief   midi播放器获取控制使能状态
   @param   obj:播放器控制句柄
   @return 	当前midi播放的模式
   @note
*/
/*----------------------------------------------------------------------------*/
u32 __midi_player_get_mode(MIDI_PLAYER *obj)
{
    if (obj) {
        return obj->ma_init.mode_info.mode;
    }
    return CMD_MIDI_CTRL_MODE_ERR;
}

/*----------------------------------------------------------------------------*/
/**@brief   midi播放器播放状态获取接口
   @param   obj:播放器控制句柄
   @return 	播放状态
   @note
*/
/*----------------------------------------------------------------------------*/
MIDI_ST  __midi_player_get_status(MIDI_PLAYER *obj)
{
    if (obj) {
        return obj->status;
    }
    return MIDI_ST_STOP;
}

static void midi_dec_setting(MIDI_PLAYER *obj, u32 cmd)
{
    if (obj == NULL) {
        return;
    }
    switch (cmd) {
    case CMD_MIDI_SEEK_BACK_N:
        break;

    case CMD_MIDI_GOON:
        obj->dec_ops.ops->dec_confing(obj->dec_ops.hdl, CMD_MIDI_GOON, NULL);
        break;

    case CMD_MIDI_SET_CHN_PROG:
        obj->dec_ops.ops->dec_confing(obj->dec_ops.hdl, cmd, &(obj->ma_init.prog_info));
        break;

    case CMD_MIDI_CTRL_TEMPO:
        obj->dec_ops.ops->dec_confing(obj->dec_ops.hdl, cmd, &(obj->ma_init.tempo_info));
        break;

    case CMD_MIDI_CTRL_MODE:
        obj->dec_ops.ops->dec_confing(obj->dec_ops.hdl, cmd, &(obj->ma_init.mode_info));
        break;

    case CMD_MIDI_SET_SWITCH:
        break;

    case CMD_MIDI_SET_EX_VOL:
        obj->dec_ops.ops->dec_confing(obj->dec_ops.hdl, cmd, &(obj->ma_init.vol_info));
        break;

    default:
        break;
    }
}


static void midi_player_task_deal(void *parg)
{
    int msg[6] = {0};
    u8 ret;
    u32 err;
    int err_msg;
    MIDI_PLAYER *obj = (MIDI_PLAYER *)parg;

    while (1) {
        if (obj->status == MIDI_ST_PLAY) {
            ret = OSTaskQAccept(ARRAY_SIZE(msg), msg);
            if (ret == OS_Q_EMPTY) {
                obj->dec_ops.ops->dec_confing(obj->dec_ops.hdl, CMD_MIDI_SET_SWITCH, &(obj->ma_init.switch_info));

                err = obj->dec_ops.ops->run(obj->dec_ops.hdl, 0);

                if (!err) {
                    continue;
                } else {
                    obj->status = MIDI_ST_STOP;
                    if (err == MAD_ERROR_FILE_END) {
                        midi_printf("obj dec end %x!!\r", err);
                        err_msg = MSG_MODULE_MIDI_DEC_END;
                    } else {
                        if (obj->read_err) {
                            obj->read_err = 0;
                            err_msg = SYS_EVENT_DEC_DEVICE_ERR;
                        } else {
                            err_msg = MSG_MODULE_MIDI_DEC_ERR;
                        }
                    }

                    if (obj->father_name) {
                        os_taskq_post_event((char *)obj->father_name, 2, err_msg, (int)obj->dec_name);
                    }
                }
            }
        } else {
            os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        }
        /* midi_printf("obj phy msg = %x\n", msg[0]); */
        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                midi_printf(" midi_task del \r");
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;

        case MSG_MODULE_MIDI_DEC_BEGIN:
            obj->status = MIDI_ST_PLAY;
            break;

        case MSG_MODULE_MIDI_DEC_STOP:
            obj->status = MIDI_ST_STOP;
            break;

        case MSG_MODULE_MIDI_DEC_PP:
            if (obj->status == MIDI_ST_PLAY) {
                obj->status = MIDI_ST_PAUSE;
            } else if (obj->status == MIDI_ST_PAUSE) {
                obj->status = MIDI_ST_PLAY;
            }
            break;

        case MSG_MODULE_MIDI_SET_PARAM:
            midi_dec_setting(obj, (u32)msg[1]);
            obj->set_flag = 0;
            break;

        default:
            break;
        }
    }
}


/*----------------------------------------------------------------------------*/
/**@brief   midi播放器执行线程启动接口
   @param   obj:播放器控制句柄
   @return 	成功与否
   @note
*/
/*----------------------------------------------------------------------------*/
tbool midi_player_process(MIDI_PLAYER *obj, void *task_name, u8 prio)
{
    if (obj == NULL) {
        return false;
    }
    if (obj->dec_name) {
        if (os_task_del_req(obj->dec_name) != OS_TASK_NOT_EXIST) {
            os_taskq_post_event(obj->dec_name, 1, SYS_EVENT_DEL_TASK);
            do {
                OSTimeDly(1);
            } while (os_task_del_req(obj->dec_name) != OS_TASK_NOT_EXIST);
            midi_printf("del MIDI_DEC_TASK_NAME succ\n");
        }
    }

    u32 err = os_task_create_ext(midi_player_task_deal, (void *)obj, prio, 24, task_name, 1024 * 4);
    if (err != OS_NO_ERR) {
        midi_printf("decoder_phy_task creat fail !!!err = %x, line = %d\n", err, __LINE__);
        return false;
    }
    obj->dec_name = task_name;
    return true;
}
/*----------------------------------------------------------------------------*/
/**@brief   midi播放器启动播放
   @param   obj:播放器控制句柄
   @return 	播放成功与否
   @note
*/
/*----------------------------------------------------------------------------*/
tbool midi_player_play(MIDI_PLAYER *obj)
{
    if (obj == NULL) {
        return false;
    }

    midi_player_stop(obj);

    _FORMAT_STATUS err = FORMAT_ERR;
    ///file open
    u32 ret;
    u32 phy_dev_chg;
    FILE_OPERATE *cur_dev_fop = obj->f_op;

    midi_printf("\n midi_process start %x\n", cur_dev_fop);

    if (cur_dev_fop->fop_init->cur_dev_mode != DEV_SEL_CUR) {
        cur_dev_fop->fop_init->filenum = &(obj->file_num);
        ret = file_operate_ini(cur_dev_fop, &phy_dev_chg);
        if (ret != FILE_OP_NO_ERR) {
            midi_printf("\n file_operate_ini err \n");
            return false;
        }

        if (cur_dev_fop->cur_lgdev_info->fat_type == 0) {
            u32 num;
            printf("fat_type == 0\n");
            ret = lg_dev_mount(cur_dev_fop->cur_lgdev_info->lg_hdl->phydev_item,
                               cur_dev_fop->cur_lgdev_info->dev_let, 1);

            if (ret) {
                midi_printf("\n lg_dev_mount err \n");
                return false;
            }

            midi_printf("lg_dev_mount ok\r");

            ret = file_operate_ini_again(cur_dev_fop, &phy_dev_chg);
            if (ret) {
                midi_printf("\n lg_dev_mount err \n");
                return false;
            }

            num = file_operate_ctl(FOP_GET_TOTALFILE_NUM, cur_dev_fop, (void *)"MID", NULL);
            if (num == 0) {
                midi_printf("no obj file err\n");
                return false;
            }
            cur_dev_fop->fop_init->cur_dev_mode = DEV_SEL_CUR;
            midi_printf("file_operate_ini ok\n");
        }

    }

    if (cur_dev_fop->fop_init->cur_sel_mode == PLAY_FILE_BYPATH) {
        midi_printf("obj path:%s\n", cur_dev_fop->fop_init->filepath);
    }

    if (FILE_OP_NO_ERR != file_operate_ctl(cur_dev_fop->fop_init->cur_sel_mode,
                                           cur_dev_fop,
                                           &obj->file_num,
                                           0)) {
        midi_printf("file_operate_ctl err~~~\n");
        return false;
    }
    cur_dev_fop->fop_init->cur_dev_mode = DEV_SEL_CUR;
    midi_printf("obj->file_num:%d\r", obj->file_num);
    ///io init
    obj->file.hdl = cur_dev_fop->cur_lgdev_info->lg_hdl->file_hdl;

    MIDI_INIT_STRUCT  *test_init = &(obj->ma_init);
    midi_printf("player_t:%d\n", test_init->init_info.player_t);
    midi_printf("sample_rate:%d\n", test_init->init_info.sample_rate);
    midi_printf("tempo_val:%d\n", test_init->tempo_info.tempo_val);
    midi_printf("prog:%d\n", test_init->prog_info.prog);
    midi_printf("chn:%d\n", test_init->mainTrack_info.chn);
    midi_printf("mode:%d\n", test_init->mode_info.mode);
    midi_printf("switch_info:%d\n", test_init->switch_info);

    obj->dec_ops.ops->open(obj->dec_ops.hdl, (const struct if_decoder_io *) & (obj->dec_io), 0);


    obj->dec_ops.ops->dec_confing(obj->dec_ops.hdl, CMD_INIT_CONFIG, &(obj->ma_init));

    ///format_check
    err = obj->dec_ops.ops->format_check(obj->dec_ops.hdl);

    if (obj->file._io) {
        if (obj->file._io->seek) {
            obj->file._io->seek(obj->file.hdl, SEEK_SET, 0);
        }
    }

    if (err != FORMAT_OK) {
        midi_printf("format_check err!!!\n");
        return false;
    }

    if (NULL != obj->stream_io.op_cb.clr) {
        obj->stream_io.op_cb.clr(obj->stream_io.op_cb.priv);
    }

    ///set samplerate
    dec_inf_t *inf = obj->dec_ops.ops->get_dec_inf(obj->dec_ops.hdl);
    if (obj->stream_io.set_info_cb._cb) {
        obj->stream_io.set_info_cb._cb(obj->stream_io.set_info_cb.priv, inf, 1);
    }

    os_taskq_post(obj->dec_name, 1, MSG_MODULE_MIDI_DEC_BEGIN);
    while (obj->status != MIDI_ST_PLAY) {
        os_time_dly(1);
    }
    midi_printf("obj player play ok\n");
    return true;
}
/*----------------------------------------------------------------------------*/
/**@brief   midi播放器暂停/播放接口
   @param   obj:播放器控制句柄
   @return 	播放器播放状态
   @note
*/
/*----------------------------------------------------------------------------*/
MIDI_ST midi_player_pp(MIDI_PLAYER *obj)
{
    MIDI_ST status = MIDI_ST_STOP;
    if (obj) {
        if (obj->status == MIDI_ST_PLAY) {
            os_taskq_post(obj->dec_name, 1, MSG_MODULE_MIDI_DEC_PP);
            while (obj->status == MIDI_ST_PLAY) {
                os_time_dly(1);
            }
        } else if (obj->status == MIDI_ST_PAUSE) {
            os_taskq_post(obj->dec_name, 1, MSG_MODULE_MIDI_DEC_PP);
            while (obj->status == MIDI_ST_PAUSE) {
                os_time_dly(1);
            }
        }
        status = obj->status;
    }
    return status;
}
/*----------------------------------------------------------------------------*/
/**@brief   midi播放器停止播放接口
   @param   obj:播放器控制句柄
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void midi_player_stop(MIDI_PLAYER *obj)
{
    if (obj == NULL) {
        return ;
    }
    if (obj->status != MIDI_ST_STOP) {
        os_taskq_post(obj->dec_name, 1, MSG_MODULE_MIDI_DEC_STOP);
        while (obj->status != MIDI_ST_STOP) {
            os_time_dly(1);
        }

        if (obj->stream_io.op_cb.over) {
            while (!obj->stream_io.op_cb.over(obj->stream_io.op_cb.priv)) {
                OSTimeDly(5);
            }
        }
    }
}

