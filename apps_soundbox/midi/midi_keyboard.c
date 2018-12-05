#include "midi_keyboard.h"
#include "dec/MIDI_CTRL_API.h"
#include "mem/malloc.h"
#include "common/app_cfg.h"
#include "common/msg.h"
#include "dac/dac_api.h"
#include "midi_prog_tab.h"
#include "dec/decoder_phy.h"


/**
midi文件， 轨道是规定个数为16， 每一个轨道都会对应16个音色通道，
在midi工具上看到的可能是多个轨道组成的， 但是表象出来是感觉不到
轨道的存在的，只有音色的通道，并且音色不叠加（即使是同一个音色，
比如说会看到两个钢琴通道）
然而，对于每一个轨道的第9号通道， 如果没有指定设置，默认就是敲击乐（128-255）
如果一旦指定， 就是指定乐器指定范围为128-255的其中一种
*/



/*
下表列出的是与音符相对应的命令标记。
八度音阶||                    音符号
  #  ||
     || C   | C#  | D   | D#  | E   | F   | F#  | G   | G#   | A  | A#  | B
-----------------------------------------------------------------------------
  0  ||  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  10 | 11
  1  ||  12 |  13 |  14 |  15 |  16 |  17 |  18 |  19 |  20 |  21 |  22 | 23
  2  ||  24 |  25 |  26 |  27 |  28 |  29 |  30 |  31 |  32 |  33 |  34 | 35
  3  ||  36 |  37 |  38 |  39 |  40 |  41 |  42 |  43 |  44 |  45 |  46 | 47
  4  ||  48 |  49 |  50 |  51 |  52 |  53 |  54 |  55 |  56 |  57 |  58 | 59
  5  ||  60 |  61 |  62 |  63 |  64 |  65 |  66 |  67 |  68 |  69 |  70 | 71
  6  ||  72 |  73 |  74 |  75 |  76 |  77 |  78 |  79 |  80 |  81 |  82 | 83
  7  ||  84 |  85 |  86 |  87 |  88 |  89 |  90 |  91 |  92 |  93 |  94 | 95
  8  ||  96 |  97 |  98 |  99 | 100 | 101 | 102 | 103 |  104| 105 | 106 | 107
  9  || 108 | 109 | 110 |  111| 112 | 113 | 114 | 115 | 116 | 117 | 118 | 119
  10 || 120 | 121 | 122 |  123| 124 | 125 | 126 | 127 |
*/


static const u16 samplerate_tab[9] = {
    48000,
    44100,
    32000,
    24000,
    22050,
    16000,
    12000,
    11025,
    8000
};



#define NOTE_PLAYER_DEBUG_ENABLE
#ifdef NOTE_PLAYER_DEBUG_ENABLE
#define midi_keyboard_printf  printf
#else
#define midi_keyboard_printf(...)
#endif

#define SIZEOF_ALIN(var,al)     ((((var)+(al)-1)/(al))*(al))

#define MIDI_KEYBOARD_TASK            "KeyboardTask"


extern bool midi_player_get_cfg_addr(u8 **addr);




typedef struct __KEYBOARD_INFO {
    void *father_name;

    u8 first_note;
    u8 notice_note_offset;
    u8 note_nums;
    u8 cur_chl_prog;

    u8 main_cur_chl;///当前按键通道
    u8 notice_chl;
    u8 nval; ///力度值0-127， 建议30~50
    u8 samplerate;

    u8 set_flag;//设置参数标志

    s16 player_t;
    MIDI_PLAY_CTRL_TEMPO tempo_info;

    DEC_SET_INFO_CB set_info_cb;
    DEC_OP_CB		op_cb;
    _OP_IO 			output;
    void 		   *mix;

} KEYBOARD_INFO;


typedef struct __KEYBOARD_OPS {
    void 			  *hdl;
    MIDI_CTRL_CONTEXT *_io;
} KEYBOARD_OPS;

struct __MIDI_KEYBOARD {
    KEYBOARD_INFO *info;
    KEYBOARD_OPS  *ops;
};


static tbool midi_keyboard_set_wait_ok(MIDI_KEYBOARD *obj, u32 msg, u32 val)
{
    if (obj == NULL) {
        return false;
    }
    u8 err = 0;

    obj->info->set_flag = 1;

    while (1) {
        err = os_taskq_post(MIDI_KEYBOARD_TASK, 2, msg, val);
        if (err == OS_Q_FULL) {
            OSTimeDly(1);
        } else {
            if (err) {
                obj->info->set_flag = 0;
                return false;
            }
            break;
        }
    }

    while (obj->info->set_flag == 1) {
        OSTimeDly(1);
    }

    return true;
}

/*----------------------------------------------------------------------------*/
/**@brief   midi keyboard 设置控制线程名称
   @param   obj:控制句柄
   @param   father_name: 控制线程名称，即消息处理线程
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_keyboard_set_father_name(MIDI_KEYBOARD *obj, void *father_name)
{
    if (obj == NULL) {
        return ;
    }
    obj->info->father_name = father_name;
}

/*----------------------------------------------------------------------------*/
/**@brief   midi keyboard 设置起始琴键，之后的所有按键按下和松开都会加上该偏移
   @param   obj:控制句柄
   @param   note:偏移值
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_keyboard_set_first_note(MIDI_KEYBOARD *obj, u8 note)
{
    if (obj == NULL) {
        return ;
    }
    obj->info->first_note = note;
}

/*----------------------------------------------------------------------------*/
/**@brief   midi keyboard 设置琴键总数
   @param   obj:控制句柄
   @param   total:总琴键数
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_keyboard_set_note_total(MIDI_KEYBOARD *obj, u8 total)
{
    if (obj == NULL) {
        return ;
    }
    obj->info->note_nums = total;
}

/*----------------------------------------------------------------------------*/
/**@brief   midi keyboard 设置音色切换琴键音提示偏移
   @param   obj:控制句柄
   @param   total:总琴键数
   @return
   @note    主要用于音色切换时选定作为提示声的琴键偏移
*/
/*----------------------------------------------------------------------------*/
void __midi_keyboard_set_notice_note_offset(MIDI_KEYBOARD *obj, u8 note_offset)
{
    if (obj == NULL) {
        return ;
    }
    obj->info->notice_note_offset = note_offset;
}
/*----------------------------------------------------------------------------*/
/**@brief   midi keyboard 设置音色切换琴键音提示的发音通道选定
   @param   obj:控制句柄
   @param   channel:选定的通道， 可以给用户选择的通道有 0~8， 10~15
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_keyboard_set_notice_channel(MIDI_KEYBOARD *obj, u8 channel)
{
    if (obj == NULL) {
        return ;
    }
    obj->info->notice_chl = channel;
}
/*----------------------------------------------------------------------------*/
/**@brief   midi keyboard 设置主音轨，注意区分不要和音色切换音轨冲突
   @param   obj:控制句柄
   @param   channel:选定的通道， 可以给用户选择的通道有 0~8， 10~15
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_keyboard_set_main_channel(MIDI_KEYBOARD *obj, u8 channel)
{
    if (obj == NULL) {
        return ;
    }
    obj->info->main_cur_chl = channel;
}
/*----------------------------------------------------------------------------*/
/**@brief   midi keyboard 设置主音轨按键按下
   @param   obj:控制句柄
   @param   index: 基于起始偏移的按键序号
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_keyboard_set_on(MIDI_KEYBOARD *obj, u8 index)
{
    if (obj == NULL) {
        return ;
    }

    midi_keyboard_set_wait_ok(obj, MSG_MODULE_MIDI_KEYBOARD_ON, index);
}

/*----------------------------------------------------------------------------*/
/**@brief   midi keyboard 设置主音轨按键松开
   @param   obj:控制句柄
   @param   index: 基于起始偏移的按键序号
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_keyboard_set_off(MIDI_KEYBOARD *obj, u8 index)
{
    if (obj == NULL) {
        return ;
    }

    midi_keyboard_set_wait_ok(obj, MSG_MODULE_MIDI_KEYBOARD_OFF, index);
}
/*----------------------------------------------------------------------------*/
/**@brief   midi keyboard 设置切换音色
   @param   obj:控制句柄
   @param   prog:制定乐器，即音色
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_keyboard_set_channel_prog(MIDI_KEYBOARD *obj, u8 prog)
{
    if (obj == NULL) {
        return ;
    }
    obj->info->cur_chl_prog = prog;
    midi_keyboard_set_wait_ok(obj, MSG_MODULE_MIDI_KEYBOARD_SETPROG_CHANGE, 0);
}
/*----------------------------------------------------------------------------*/
/**@brief   midi keyboard 设置切换音色,并按下发声，是配合按键情景使用
   @param   obj:控制句柄
   @param   prog:制定乐器，即音色
   @return
   @note   与void __midi_keyboard_set_channel_prog_off(MIDI_KEYBOARD *obj, u8 prog)成对使用
*/
/*----------------------------------------------------------------------------*/
void __midi_keyboard_set_channel_prog_on(MIDI_KEYBOARD *obj, u8 prog)
{
    if (obj == NULL) {
        return ;
    }
    obj->info->cur_chl_prog = prog;
    midi_keyboard_set_wait_ok(obj, MSG_MODULE_MIDI_KEYBOARD_SETPROG_CHANGE_ON, 0);
}
/*----------------------------------------------------------------------------*/
/**@brief   midi keyboard 设置切换音色松手，是配合按键情景使用
   @param   obj:控制句柄
   @param   prog:制定乐器，即音色
   @return
   @note   与void __midi_keyboard_set_channel_prog_on(MIDI_KEYBOARD *obj, u8 prog)成对使用
*/
/*----------------------------------------------------------------------------*/
void __midi_keyboard_set_channel_prog_off(MIDI_KEYBOARD *obj)
{
    if (obj == NULL) {
        return ;
    }
    midi_keyboard_set_wait_ok(obj, MSG_MODULE_MIDI_KEYBOARD_SETPROG_CHANGE_OFF, 0);
}
/*----------------------------------------------------------------------------*/
/**@brief   midi keyboard 设置主音轨琴键力度，力度值越大，发声强度大
   @param   obj:控制句柄
   @param   val:琴键力度
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_keyboard_set_force_val(MIDI_KEYBOARD *obj, u8 val)
{
    if (obj == NULL) {
        return ;
    }
    obj->info->nval = val;
}
/*----------------------------------------------------------------------------*/
/**@brief   midi keyboard 设置同时支持按下的按键个数
   @param   obj:控制句柄
   @param   total:最大值
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_keyboard_set_noteon_max_nums(MIDI_KEYBOARD *obj, s16 total)
{
    if (obj == NULL) {
        return ;
    }
    obj->info->player_t = total;
}
/*----------------------------------------------------------------------------*/
/**@brief   midi keyboard 设置采样率，对应samplerate_tab
   @param   obj:控制句柄
   @param   samp:对应samplerate_tab坐标
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_keyboard_set_samplerate(MIDI_KEYBOARD *obj, u8 samp)
{
    if (obj == NULL) {
        return ;
    }
    if (samp >= (sizeof(samplerate_tab) / sizeof(samplerate_tab[0]))) {
        return;
    }
    obj->info->samplerate = samp;
}

/*----------------------------------------------------------------------------*/
/**@brief   midi keyboard 设置按键按下音符发声的衰减系数
   @param   obj:控制句柄
   @param   samp:对应samplerate_tab坐标
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __midi_keyboard_set_melody_decay(MIDI_KEYBOARD *obj, u16 val)
{
    if (obj == NULL) {
        return ;
    }

    obj->info->tempo_info.tempo_val = 1024;//设置为固定1024即可
    obj->info->tempo_info.decay_val = val;
    midi_keyboard_set_wait_ok(obj, MSG_MODULE_MIDI_SET_PARAM, CMD_MIDI_CTRL_TEMPO);
}


/*----------------------------------------------------------------------------*/
/**@brief 设置midi keyboard播放器音频输出到指定叠加通道
   @param
   midi -- 解码器控制句柄
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
tbool  __midi_keyboard_set_output_to_mix(MIDI_KEYBOARD *obj, SOUND_MIX_OBJ *mix_obj, const char *key, u32 mix_buf_len, u8 Threshold, u8 default_vol, u8 max_vol)
{
    if (obj == NULL || mix_obj == NULL) {
        return false;
    }

    SOUND_MIX_P mix_p;
    memset((u8 *)&mix_p, 0x0, sizeof(SOUND_MIX_P));
    mix_p.key = key;
    mix_p.out_len = mix_buf_len;
    mix_p.set_info_cb = &obj->info->set_info_cb;
    mix_p.input = &obj->info->output;
    mix_p.op_cb = &obj->info->op_cb;
    mix_p.limit = Threshold;
    mix_p.max_vol = max_vol;
    mix_p.default_vol = default_vol;
    obj->info->mix = (void *)sound_mix_chl_add(mix_obj, &mix_p);

    return (obj->info->mix ? true : false);
}

/*----------------------------------------------------------------------------*/
/**@brief 设置midi keyboard播放器音频输出到指定叠加通道
   @param
   obj -- 解码器控制句柄
   vol -- 设置输出到叠加通道的音量值
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void  __midi_keyboard_set_output_to_mix_set_vol(MIDI_KEYBOARD *obj, u8 vol)
{
    if (obj == NULL || obj->info->mix == NULL) {
        return;
    }

    __sound_mix_chl_set_vol(obj->info->mix, vol);
}
static u32 midi_keyboard_set_sr(void *priv, dec_inf_t *inf, tbool wait)
{
    dac_set_samplerate(inf->sr, wait);
    return 0;
}

static void midi_keyboard_output(void *priv, void *buf, int len)
{
    dac_write(buf, len);
}

/*----------------------------------------------------------------------------*/
/**@brief midi keyboard 控制资源申请
   @param
   @return 控制句柄
   @note
*/
/*----------------------------------------------------------------------------*/
MIDI_KEYBOARD *midi_keyboard_creat(void)
{
    u32 buf_index;

    MIDI_CTRL_CONTEXT *_io = get_midi_ctrl_ops();
    if (_io == NULL) {
        return NULL;
    }

    u32 need_buf_len = SIZEOF_ALIN(sizeof(MIDI_KEYBOARD), 4)
                       + SIZEOF_ALIN(sizeof(KEYBOARD_INFO), 4)
                       + SIZEOF_ALIN(sizeof(KEYBOARD_OPS), 4)
                       + SIZEOF_ALIN(_io->need_workbuf_size(), 4);

    u8 *need_buf = (u8 *)calloc(1, need_buf_len);
    if (need_buf == NULL) {
        return NULL;
    }

    MIDI_KEYBOARD *obj = (MIDI_KEYBOARD *)need_buf;
    need_buf += SIZEOF_ALIN(sizeof(MIDI_KEYBOARD), 4);

    obj->info = (KEYBOARD_INFO *)need_buf;
    need_buf += SIZEOF_ALIN(sizeof(KEYBOARD_INFO), 4);

    obj->ops = (KEYBOARD_OPS *)need_buf;
    need_buf += SIZEOF_ALIN(sizeof(KEYBOARD_OPS), 4);

    obj->ops->hdl = (void *)need_buf;

    obj->ops->_io = _io;

    obj->info->output.priv = NULL;
    obj->info->output.output = (void *)midi_keyboard_output;

    obj->info->set_info_cb.priv = NULL;
    obj->info->set_info_cb._cb = (void *)midi_keyboard_set_sr;

    obj->info->op_cb.priv = NULL;
    obj->info->op_cb.clr = NULL;
    obj->info->op_cb.over = NULL;

///set default info
    __midi_keyboard_set_channel_prog(obj, 0);
    __midi_keyboard_set_main_channel(obj, 0);
    __midi_keyboard_set_notice_channel(obj, 1);
    __midi_keyboard_set_first_note(obj, 48);
    __midi_keyboard_set_note_total(obj, 128);
    __midi_keyboard_set_notice_note_offset(obj, 13);
    __midi_keyboard_set_force_val(obj, 80);
    __midi_keyboard_set_noteon_max_nums(obj, 8);
    __midi_keyboard_set_father_name(obj, NULL);
    __midi_keyboard_set_samplerate(obj, 5);//5);//16K采样

    obj->info->set_flag = 0;

    return obj;
}


/*----------------------------------------------------------------------------*/
/**@brief midi keyboard 控制资源释放
   @param 控制句柄， 注意是双重指针
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void midi_keyboard_destroy(MIDI_KEYBOARD **obj)
{
    if (obj == NULL || (*obj) == NULL) {
        return ;
    }

    MIDI_KEYBOARD *hdl = *obj;
    if (os_task_del_req(MIDI_KEYBOARD_TASK) != OS_TASK_NOT_EXIST) {
        os_taskq_post_event(MIDI_KEYBOARD_TASK, 1, SYS_EVENT_DEL_TASK);
        do {
            OSTimeDly(1);
        } while (os_task_del_req(MIDI_KEYBOARD_TASK) != OS_TASK_NOT_EXIST);
        printf("del MIDI_DEC_TASK_NAME succ\n");
    }
    if (hdl->info->mix) {
        sound_mix_chl_del_by_chl((SOUND_MIX_CHL **)&hdl->info->mix);
    }

    free(*obj);
    *obj = NULL;
}


static void midi_keyboard_task(void *parg)
{
    int msg[3] = {0};
    u8 ret;
    MIDI_KEYBOARD *obj = parg;
    while (1) {

        if (obj != NULL) {
            ret = OSTaskQAccept(ARRAY_SIZE(msg), msg);
            if (ret == OS_Q_EMPTY) {
                obj->ops->_io->run(obj->ops->hdl);
                continue;
            }
        } else {
            os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        }
        switch (msg[0]) {

        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;
        case MSG_MODULE_MIDI_KEYBOARD_ON:
            obj->ops->_io->note_on(obj->ops->hdl,
                                   ((u8)msg[1]) + obj->info->first_note,
                                   obj->info->nval,
                                   obj->info->main_cur_chl);
            obj->info->set_flag = 0;
            break;
        case MSG_MODULE_MIDI_KEYBOARD_OFF:
            obj->ops->_io->note_off(obj->ops->hdl,
                                    ((u8)msg[1]) + obj->info->first_note,
                                    obj->info->main_cur_chl);
            obj->info->set_flag = 0;
            break;

        case MSG_MODULE_MIDI_KEYBOARD_SETPROG_CHANGE_ON:
            obj->ops->_io->set_prog(obj->ops->hdl, prog_list[obj->info->cur_chl_prog], obj->info->main_cur_chl);
            obj->ops->_io->set_prog(obj->ops->hdl, prog_list[obj->info->cur_chl_prog], obj->info->notice_chl);
            obj->ops->_io->note_on(obj->ops->hdl,
                                   obj->info->notice_note_offset + obj->info->first_note,
                                   obj->info->nval,
                                   obj->info->notice_chl);
            obj->info->set_flag = 0;
            break;

        case MSG_MODULE_MIDI_KEYBOARD_SETPROG_CHANGE_OFF:
            obj->ops->_io->note_off(obj->ops->hdl,
                                    obj->info->notice_note_offset + obj->info->first_note,
                                    obj->info->notice_chl);
            obj->info->set_flag = 0;
            break;
        case MSG_MODULE_MIDI_KEYBOARD_SETPROG_CHANGE:
            obj->ops->_io->set_prog(obj->ops->hdl, prog_list[obj->info->cur_chl_prog], obj->info->main_cur_chl);
            obj->info->set_flag = 0;
            break;

        case MSG_MODULE_MIDI_SET_PARAM:
            switch ((u32)msg[1]) {
            case CMD_MIDI_CTRL_TEMPO:
                printf("decay_val = %d\n", obj->info->tempo_info.decay_val);
                obj->ops->_io->ctl_confing(obj->ops->hdl, CMD_MIDI_CTRL_TEMPO, &(obj->info->tempo_info));
                break;
            }
            obj->info->set_flag = 0;
            break;

        default:
            break;
        }
    }
}

/*----------------------------------------------------------------------------*/
/**@brief   midi keyboard 弹奏启动，所有的琴键按下和释放要在此函数调用之后方可以使用
   @param   obj:控制句柄
   @param   father_name: 控制线程名称，即消息处理线程
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
tbool midi_keyboard_play(MIDI_KEYBOARD *obj)
{
    if (obj == NULL) {
        return false;
    }

    u8 err = 0;
    u8 *midi_cache_addr;
    MIDI_CTRL_PARM t_ctrl_param;
    MIDI_CONFIG_PARM t_cfg_param;

    if (midi_player_get_cfg_addr(&midi_cache_addr) == false) {

        return false;
    }

    t_ctrl_param.priv = obj->info->output.priv;
    t_ctrl_param.output = (void *)obj->info->output.output; //decoder_output;      //这个是最后的输出函数接口，
    t_ctrl_param.tempo = 1000;/*tempo*tempo_val/1024决定数据量的多少*/
    t_ctrl_param.track_num = 1;

    t_cfg_param.sample_rate = obj->info->samplerate;
    t_cfg_param.spi_pos = (unsigned char *)midi_cache_addr;
    t_cfg_param.player_t = obj->info->player_t;

    obj->ops->_io->open(obj->ops->hdl,
                        (void *)&t_ctrl_param,
                        (void *)&t_cfg_param);

    obj->ops->_io->set_prog(obj->ops->hdl,
                            prog_list[obj->info->cur_chl_prog],
                            obj->info->main_cur_chl);
    obj->ops->_io->set_prog(obj->ops->hdl,
                            prog_list[obj->info->cur_chl_prog],
                            obj->info->notice_chl);

    u16 samplerate =  samplerate_tab[obj->info->samplerate];

    midi_keyboard_printf("obj->info->cur_chl_prog :%d\r", obj->info->cur_chl_prog);
    midi_keyboard_printf("obj->info->main_cur_chl :%d\r", obj->info->main_cur_chl);
    midi_keyboard_printf("samplerate :%d\r", samplerate);

    if (obj->info->set_info_cb._cb) {
        dec_inf_t inf;
        memset((u8 *)&inf, 0x0, sizeof(dec_inf_t));
        inf.sr = samplerate;
        obj->info->set_info_cb._cb(obj->info->set_info_cb.priv, &inf, 1);
    }

    printf("fun = %s, line = %d\n", __func__, __LINE__);
    err = os_task_create(midi_keyboard_task, (void *)obj, TaskMidiKeyboardPrio, 48, MIDI_KEYBOARD_TASK);
    if (err) {
        midi_keyboard_printf("NotePlayerTask creat  fail, err = %d\r\n", err);
        return false;
    }

    printf("fun = %s, line = %d\n", __func__, __LINE__);
    midi_keyboard_printf("keyboard play ok\r\n");
    return true;
}




