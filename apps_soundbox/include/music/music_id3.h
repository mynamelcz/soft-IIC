#ifndef __MUSIC_ID3_H__
#define  __MUSIC_ID3_H__
#include "typedef.h"
#include "music_player.h"

typedef struct __MP3_ID3_OBJ MP3_ID3_OBJ;

void id3_obj_post(MP3_ID3_OBJ **obj);
MP3_ID3_OBJ *id3_v1_obj_get(MUSIC_PLAYER_OBJ *mapi);
MP3_ID3_OBJ *id3_v2_obj_get(MUSIC_PLAYER_OBJ *mapi);

#endif// __MUSIC_ID3_H__

