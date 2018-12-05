#ifndef __SOUND_MIX_API_H__
#define __SOUND_MIX_API_H__
#include "typedef.h"
#include "sound_mix.h"

tbool sound_mix_api_init(u16 targar_sr);
void sound_mix_api_close(void);
SOUND_MIX_OBJ *sound_mix_api_get_obj(void);
tbool sound_mix_api_process(void *buf, u32 len);

#endif// __SOUND_MIX_API_H__
