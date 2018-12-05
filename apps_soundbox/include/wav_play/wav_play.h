#ifndef __WAV_PLAY_H__
#define __WAV_PLAY_H__
#include "sdk_cfg.h"
#include "dec/music_api.h"
#include "sound_mix.h"
#include "cpu/dac_param.h"

#define WAV_CHECK_DATA_MIN		(DAC_BUF_LEN*2)
#define WAV_ONCE_BUF_LEN		(DAC_BUF_LEN*4)//(DAC_BUF_LEN)//
#define WAV_ONCE_READ_POINT_NUM	(WAV_ONCE_BUF_LEN/2)

typedef enum __WAV_PLAYER_ST {
    WAV_PLAYER_ST_STOP = 0x0,
    WAV_PLAYER_ST_PAUSE,
    WAV_PLAYER_ST_PLAY = 0x55aa,
} WAV_PLAYER_ST;

enum {
    SINGLE_TRACK = 1,
    DUAL_TRACK,
};

typedef struct __FLASHDATA_T {
    /* void *hdl; */
    u8 buf[512];
    u32 sector;
} FLASHDATA_T;

typedef struct __WAV_INFO {
    u32 addr;
    u32 r_len;
    u32 total_len;
    u16 dac_sr;
    u16 track;
} WAV_INFO;

typedef struct __WAV_PLAYER WAV_PLAYER;

WAV_PLAYER *wav_player_creat(void);
void wav_player_destroy(WAV_PLAYER **hdl);

void __wav_player_set_info(WAV_PLAYER *w_play, WAV_INFO *info);
void __wav_player_set_read_cbk(WAV_PLAYER *w_play, u16(*read)(void *priv, u8 *buf, u32 addr, u16 len), void *priv);
void __wav_player_set_output_cbk(WAV_PLAYER *w_play, void *(*output)(void *priv, void *buf, u32 len), void *priv);
tbool __wav_player_set_output_to_mix(WAV_PLAYER *w_play, SOUND_MIX_OBJ *mix_obj, const char *key, u32 mix_buf_len, u8 Threshold, u8 default_vol, u8 max_vol);

tbool wav_player_decode(WAV_PLAYER *w_play);
tbool wav_player_set_path(WAV_PLAYER *w_play, const char *path, u32 num);
tbool wav_player_play(WAV_PLAYER *w_play);
// void wav_player_stop(WAV_PLAYER *w_play);
// WAV_PLAYER_ST wav_player_pp(WAV_PLAYER *w_play);
WAV_PLAYER_ST wav_player_get_status(WAV_PLAYER *w_play);
WAV_PLAYER_ST wav_player_pp(WAV_PLAYER *w_play);

///非wav_player 特有接口, 可以给其他接口使用
tbool wav_player_get_file_info(WAV_INFO *info, const char *path, u32 num);
u16 phy_flashdata_read(FLASHDATA_T *flashdata, u8 *buf, u32 addr, u16 len);
tbool wav_file_decode(WAV_INFO *info, u16(*read)(void *priv, u8 *buf, u32 addr, u16 len), void *priv, s16 outbuf[WAV_ONCE_READ_POINT_NUM]);
#endif
