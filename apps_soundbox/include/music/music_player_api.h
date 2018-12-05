#ifndef __MUSIC_PLAYER_API_H__
#define __MUSIC_PLAYER_API_H__
#include "sdk_cfg.h"
#include "music_player.h"
tbool music_player_err_deal(MUSIC_PLAYER_OBJ *mapi, u32 err, u8 auto_next_file, u8 auto_next_dev);
tbool music_player_mulfile_play_err_deal(MUSIC_PLAYER_OBJ *mapi, u32 err, u8 auto_next_file, u8 auto_next_dev, u32 StartAddr, u8 medType, mulfile_func_t *func);
#endif

