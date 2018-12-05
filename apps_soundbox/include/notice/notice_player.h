#ifndef __NOTICE_PLAYER_H__
#define __NOTICE_PLAYER_H__
#include "sdk_cfg.h"
#include "music/music_player.h"
#include "notice_cfg.h"

#define NOTICE_PLAYER_TASK_NAME	"NOTICE_PLAYER_TASK"

#define NOTICE_USE_WAV_PLAYER		(1L<<16)
#define NOTICE_STOP_WITH_BACK_END_MSG	(1L<<17)

typedef enum __NOTICE_PLAYER_METHOD {
    NOTICE_PLAYER_METHOD_BY_PATH = 0x0,
    NOTICE_PLAYER_METHOD_BY_NUM,
    NOTICE_PLAYER_METHOD_BY_PATH_ARRAY,
    NOTICE_PLAYER_METHOD_BY_NUM_ARRAY,

    NOTICE_PLAYER_METHOD_BY_FLASH_REC_ADDR,
    NOTICE_PLAYER_METHOD_BY_SPI_REC_NUM,
} NOTICE_PLAYER_METHOD;

typedef enum {
    NOTICE_PLAYER_NO_ERR = 0x0,
    NOTICE_PLAYER_NO_ERR_WITHOUT_MSG_DEAL,
    NOTICE_PLAYER_PLAY_ERR,
    NOTICE_PLAYER_MSG_BREAK_ERR,
    NOTICE_PLAYER_IS_PLAYING_ERR,
    NOTICE_PLAYER_TASK_DEL_ERR,
} NOTICE_PlAYER_ERR;


typedef tbool(*msg_cb)(int msg[], void *priv);

NOTICE_PlAYER_ERR notice_player_play(void *father_name, msg_cb _cb, void *priv, u32 rpt_time, u32 delay, NOTICE_PLAYER_METHOD method, int argc, ...);
NOTICE_PlAYER_ERR notice_player_play_by_num(void *father_name, const char *dir_path, u32 num, msg_cb _cb, void *priv);
NOTICE_PlAYER_ERR notice_player_play_by_path(void *father_name, const char *path, msg_cb _cb, void *priv);
NOTICE_PlAYER_ERR notice_player_play_by_flash_rec_addr(void *father_name, u32 flash_addr, msg_cb _cb, void *priv);

NOTICE_PlAYER_ERR notice_player_play_spec_dev(void *father_name, msg_cb _cb, void *priv, u32 dev_let, u32 rpt_time, u32 delay, NOTICE_PLAYER_METHOD method, int argc, ...);
NOTICE_PlAYER_ERR notice_player_play_spec_dev_by_path(void *father_name, u32 dev_let, const char *path, msg_cb _cb, void *priv);
NOTICE_PlAYER_ERR notice_player_play_spec_dev_by_num(void *father_name, u32 dev_let, const char *dir_path, u32 num, msg_cb _cb, void *priv);

NOTICE_PlAYER_ERR notice_player_play_to_mix(void *father_name, msg_cb _cb, void *priv, u32 rpt_time, u32 delay, SOUND_MIX_OBJ *mix_obj, NOTICE_PLAYER_METHOD method, int argc, ...);
NOTICE_PlAYER_ERR notice_player_play_by_path_to_mix(void *father_name, const char *path, msg_cb _cb, void *priv, SOUND_MIX_OBJ *mix_obj);
NOTICE_PlAYER_ERR notice_player_play_by_num_to_mix(void *father_name, const char *dir_path, u32 num, msg_cb _cb, void *priv, SOUND_MIX_OBJ *mix_obj);
NOTICE_PlAYER_ERR notice_player_play_by_flash_rec_addr_to_mix(void *father_name, u32 flash_addr, msg_cb _cb, void *priv, SOUND_MIX_OBJ *mix_obj);
NOTICE_PlAYER_ERR notice_player_play_spec_dev_to_mix(void *father_name, msg_cb _cb, void *priv, u32 dev_let, u32 rpt_time, u32 delay, SOUND_MIX_OBJ *mix_obj, NOTICE_PLAYER_METHOD method, int argc, ...);
NOTICE_PlAYER_ERR notice_player_play_spec_dev_by_path_to_mix(void *father_name, u32 dev_let, const char *path, msg_cb _cb, void *priv, SOUND_MIX_OBJ *mix_obj);
NOTICE_PlAYER_ERR notice_player_play_spec_dev_by_num_to_mix(void *father_name, u32 dev_let, const char *dir_path, u32 num, msg_cb _cb, void *priv, SOUND_MIX_OBJ *mix_obj);
NOTICE_PlAYER_ERR wav_play_by_path_to_mix(void *father_name, const char *path, msg_cb _cb, void *priv, SOUND_MIX_OBJ *mix_obj);
NOTICE_PlAYER_ERR wav_play_by_num_to_mix(void *father_name, const char *dir_path, u32 num, msg_cb _cb, void *priv, SOUND_MIX_OBJ *mix_obj);


NOTICE_PlAYER_ERR wav_play_by_num(void *father_name, const char *dir_path, u32 num, msg_cb _cb, void *priv);
NOTICE_PlAYER_ERR wav_play_by_path(void *father_name, const char *path, msg_cb _cb, void *priv);
u8 notice_player_get_status(void);
void notice_player_stop(void);
_DECODE_STATUS notice_player_pp(void);

void *tone_number_get_name(u32 number);

#endif


