#ifndef __SIMPLE_NOTICE_PLAYER_H__
#define __SIMPLE_NOTICE_PLAYER_H__
#include "typedef.h"
#include "sdk_cfg.h"
#include "sound_mix.h"
#include "music/music_player.h"


///simple notice player
typedef MUSIC_PLAYER_OBJ SIMPLE_NOTICE_PLAYER_OBJ;

void simple_notice_player_stop(SIMPLE_NOTICE_PLAYER_OBJ **obj);
_DECODE_STATUS simple_notice_player_pp(SIMPLE_NOTICE_PLAYER_OBJ *obj);
SIMPLE_NOTICE_PLAYER_OBJ *simple_notice_player_play_by_path(
    void *father_name, 			/*控制线程名称*/
    void *my_name,				/*提示音线程名称*/
    u8    prio,					/*提示音线程优先级*/
    SOUND_MIX_OBJ *mix_obj,			/*叠加控制句柄，如果不需要叠加，传NULL即可*/
    const char *mix_key,		/*叠加通道key， 主要用于标识叠加通道的字符串， 如果不需要叠加， 传NULL即可*/
    const char *path			/*播放路径*/
);




///simple wav play
typedef struct __SIMPLE_WAV_PLAY_OBJ  SIMPLE_WAV_PLAY_OBJ;

void simple_wav_play_stop(SIMPLE_WAV_PLAY_OBJ **obj);
_DECODE_STATUS simple_wav_play_pp(SIMPLE_WAV_PLAY_OBJ *obj);
SIMPLE_WAV_PLAY_OBJ *simple_wav_play_by_path(
    void *father_name, 			/*控制线程名称*/
    void *my_name,				/*提示音线程名称*/
    u8    prio,					/*提示音线程优先级*/
    SOUND_MIX_OBJ *mix_obj,			/*叠加控制句柄，如果不需要叠加，传NULL即可*/
    const char *mix_key,		/*叠加通道key， 主要用于标识叠加通道的字符串， 如果不需要叠加， 传NULL即可*/
    const char *path			/*播放路径*/
);


#endif// __SIMPLE_NOTICE_PLAYER_H__
