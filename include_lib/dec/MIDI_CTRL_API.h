#ifndef MIDI_CTRL_API_h__
#define MIDI_CTRL_API_h__

#include "MIDI_DEC_API.h"

#include "typedef.h"


typedef struct _EVENT_CONTEXT_ {
    u8 msg;
    u8 val1;
    u8 val2;
    u8 trk_num;
    u16 delta;
} EVENT_CONTEXT;

typedef struct _MIDI_CTRL_PARM_ {
    char track_num;                //0-16
    unsigned int tempo;             //tempo
    void *priv;
    void (*output)(void *priv, void *data, int len);
} MIDI_CTRL_PARM;

typedef struct _MIDI_CTRL_CONTEXT_ {
    u32(*need_workbuf_size)() ;		                                   ///<获取需要的buffer
    u32(*open)(void *work_buf, void *dec_parm, void *parm);               ///跟解码一样的配置
    u32(*run)(void *work_buf);                                            ///播放
    u32(*set_prog)(void *work_buf, u8 prog, u8 chn);                      ///设置乐器
    u32(*note_on)(void *work_buf, u8 nkey, u8 nvel, u8 chn);              ///指定播放单个音符,nkey跟nvel的有效值是0-127
    u32(*note_off)(void *work_buf, u8 nkey, u8 chn);                      ///指定播放单个音符,nkey跟nvel的有效值是0-127
    u32(*pitch_bend)(void *work_buf, u16 pitch_val, u8 chn);
    u32(*ctl_confing)(void *work_buf, u32 cmd, void *parm);
} MIDI_CTRL_CONTEXT;


#define MIDI_CTRLC_NOTEOFF  0x80                        //按键松开
#define MIDI_CTRLC_NOTEON   0x90                        //按键按下
#define MIDI_CTRLC_CTLCHG   0xB0
#define MIDI_CTRLC_PRGCHG   0xC0                        //改变乐器
#define MIDI_CTRLC_PWCHG    0xE0

#define MIDI_CTRLC_VOL     0x07
#define MIDI_CTRLC_EXPR    0x0B
#define MIDI_CTRLC_SFT_ON  0x43



extern MIDI_CTRL_CONTEXT *get_midi_ctrl_ops();


#if 0

//调用示例

MIDI_CONFIG_PARM midi_t_parm;
MIDI_CTRL_PARM midi_ctrl_parmt;

midi_t_parm.sample_rate = 5;
midi_t_parm.spi_pos = spi_memory;
midi_t_parm.player_t = 8;                    //跟解码一致

midi_ctrl_parmt.output = mp_output;          //这个是最后的输出函数接口，
midi_ctrl_parmt.tempo = 1000;
midi_ctrl_parmt.track_num = 1;

{
    int play_flag;
    MIDI_CTRL_CONTEXT *test_ops = get_midi_ctrl_ops();
    EVENT_CONTEXT event_t;
    int vel_val = 127;
    int i = 0;


    buflen = test_ops->need_workbuf_size();
    bufptr = malloc(buflen);
    test_ops->open(bufptr, &midi_ctrl_parmt, &midi_t_parm);
    test_ops->ctl_confing(bufptr, CMD_MIDI_CTRL_TEMPO, 1024);
    test_ops->set_prog(bufptr, 0, 0);
    while (!test_ops->run((bufptr))) {
        test_ci++;
        if (test_ci % 200 == 0) {
            test_ops->note_on(bufptr, test_ci + 0x40, vel_val);
        }

        if (test_ci > 10000) {
            break;
        }

    }
}


#endif

#endif // MIDI_CTRL_API_h__

