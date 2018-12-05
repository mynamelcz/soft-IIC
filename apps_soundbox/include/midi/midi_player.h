#ifndef __MIDI_PLAYER_H__
#define __MIDI_PLAYER_H__
#include "dec/MIDI_DEC_API.h"
#include "file_operate/file_op.h"
#include "sound_mix.h"

enum {
    TEMPO_SLOW_0 = 0x0,
    TEMPO_SLOW_1,
    TEMPO_SLOW_2,
    TEMPO_SLOW_3,
    TEMPO_SLOW_4,
    TEMPO_SLOW_5,
    TEMPO_SLOW_6,
    TEMPO_SLOW_7,
    TEMPO_SLOW_8,
    TEMPO_SLOW_9,
    TEMPO_NORMAL,
    TEMPO_FAST_0,
    TEMPO_FAST_1,
    TEMPO_FAST_2,
    TEMPO_FAST_3,
    TEMPO_FAST_4,
    TEMPO_FAST_5,
    TEMPO_FAST_6,
    TEMPO_FAST_7,
    TEMPO_FAST_8,
    TEMPO_FAST_9,
    TEMPO_MAX,
};

typedef enum __MIDI_ST {
    MIDI_ST_STOP = 0,
    MIDI_ST_PLAY,
    MIDI_ST_PAUSE,
} MIDI_ST;

typedef struct __MIDI_DECCODE_FILE_IO {
    u32(*seek)(void *hdl, u8 type, u32 offsize);
    u16(*read)(void *hdl, u8 *buf, u16 len);
    u32(*get_size)(void *hdl);
} MIDI_DECCODE_FILE_IO;


typedef struct __MIDI_PLAYER  MIDI_PLAYER;

void __midi_player_set_father_name(MIDI_PLAYER *obj, void *father_name);
void __midi_player_set_dev_let(MIDI_PLAYER *obj, u32 dev_let);
void __midi_player_set_path(MIDI_PLAYER *obj, const char *path);
void __midi_player_set_filenum(MIDI_PLAYER *obj, u32 filenum);
void __midi_player_set_play_mode(MIDI_PLAYER *obj, ENUM_PLAY_MODE mode);
void __midi_player_set_sel_mode(MIDI_PLAYER *obj, ENUM_FILE_SELECT_MODE mode);
void __midi_player_set_dev_mode(MIDI_PLAYER *obj, ENUM_DEV_SEL_MODE mode);
void __midi_player_set_noteon_max_nums(MIDI_PLAYER *obj, s16 total);
void __midi_player_set_samplerate(MIDI_PLAYER *obj, u8 samp);
void __midi_player_set_tempo(MIDI_PLAYER *obj, u8 tempo);
void __midi_player_set_tempo_directly(MIDI_PLAYER *obj, u16 val);
void __midi_player_set_melody_decay(MIDI_PLAYER *obj, u16 val);
void __midi_player_set_melody_delay(MIDI_PLAYER *obj, u16 val);
void __midi_player_set_melody_mute_threshold(MIDI_PLAYER *obj, u32 val);
void __midi_player_set_main_chl_prog(MIDI_PLAYER *obj, u8 prog_index, u8 mode);
void __midi_player_set_mainTrack_channel(MIDI_PLAYER *obj, u8 chl);
void __midi_player_set_mark_trigger(MIDI_PLAYER *obj, u32(*mark_trigger)(void *priv, u8 *val, u8 len), void *priv);
void __midi_player_set_beat_trigger(MIDI_PLAYER *obj, u32(*beat_trigger)(void *priv, u8 val1, u8 val2), void *priv);
void __midi_player_set_melody_trigger(MIDI_PLAYER *obj, u32(*melody_trigger)(void *priv, u8 key, u8 vel), void *priv);
void __midi_player_set_timeDiv_trigger(MIDI_PLAYER *obj, u32(*timeDiv_trigger)(void *priv), void *priv);
void __midi_player_set_mode(MIDI_PLAYER *obj, u8 mode);
void __midi_player_set_switch_enable(MIDI_PLAYER *obj, u32 sw);
void __midi_player_set_switch_disable(MIDI_PLAYER *obj, u32 sw);
void __midi_player_set_vol_tab(MIDI_PLAYER *obj, u16 tab[CTRL_CHANNEL_NUM]);
void __midi_player_set_seek_back_n(MIDI_PLAYER *obj, s8 seek_n);
void __midi_player_set_go_on(MIDI_PLAYER *obj);
tbool  __midi_player_set_output_to_mix(MIDI_PLAYER *obj, SOUND_MIX_OBJ *mix_obj, const char *key, u32 mix_buf_len, u8 Threshold, u8 default_vol, u8 max_vol);
void  __midi_player_set_output_to_mix_set_vol(MIDI_PLAYER *obj, u8 vol);
void __midi_player_set_output_to_mix_mute(MIDI_PLAYER *obj, u8 flag);
void __midi_player_set_mix_wait(MIDI_PLAYER *obj);
void __midi_player_set_mix_wait_ok(MIDI_PLAYER *obj);
s8 __midi_player_get_mix_wait_status(MIDI_PLAYER *obj);
void __midi_player_set_file_io(MIDI_PLAYER *obj, MIDI_DECCODE_FILE_IO *_io);
u32  __midi_player_get_switch_enable(MIDI_PLAYER *obj);
u32  __midi_player_get_mode(MIDI_PLAYER *obj);
MIDI_ST  __midi_player_get_status(MIDI_PLAYER *obj);

MIDI_PLAYER *midi_player_creat(void);
void midi_player_destroy(MIDI_PLAYER **hdl);

tbool midi_player_process(MIDI_PLAYER *obj, void *task_name, u8 prio);
tbool midi_player_play(MIDI_PLAYER *obj);
void midi_player_stop(MIDI_PLAYER *obj);
MIDI_ST midi_player_pp(MIDI_PLAYER *obj);

#endif//__MIDI_PLAYER_H__

