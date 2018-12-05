#ifndef __WAV_MIX_PLAY_H__
#define __WAV_MIX_PLAY_H__
#include "typedef.h"
#include "dec/music_api.h"

// #define WAV_MIX_DEFAULT_VOL			(DIGITAL_VOL_MAX_L)
// #define WAV_MIX_MAX_VOL				(DIGITAL_VOL_MAX_L)

typedef struct __WAV_MIX_OBJ WAV_MIX_OBJ;
typedef struct __WAV_MIX_CHL WAV_MIX_CHL;
// typedef struct __WAV_MIX_CHL* WAV_MIX_KEY;

WAV_MIX_OBJ *wav_mix_creat(void);
void wav_mix_destroy(WAV_MIX_OBJ **obj);

void __wav_mix_set_samplerate(WAV_MIX_OBJ *obj, u16 samplerate);
void __wav_mix_set_output_cbk(WAV_MIX_OBJ *obj, void *(*output)(void *priv, void *buf, u16 len), void *priv);
void __wav_mix_set_data_op_cbk(WAV_MIX_OBJ *obj, void (*clr)(void *priv),  tbool(*over)(void *priv), void *priv);
void __wav_mix_set_info_cbk(WAV_MIX_OBJ *obj, u32(*set_info)(void *priv, dec_inf_t *inf, tbool wait), void *priv);
void __wav_mix_chl_set_vol(WAV_MIX_OBJ *obj, const char *key, u8 vol);
u8 __wav_mix_chl_get_status(WAV_MIX_OBJ *obj, const char *key);

tbool wav_mix_process(WAV_MIX_OBJ *obj, void *task_name, u32 prio, u32 stk_size);
tbool wav_mix_chl_play(WAV_MIX_OBJ *obj, const char *key, u8 rpt_cnt, int argc, ...);
void wav_mix_chl_stop(WAV_MIX_OBJ *obj, const char *key);
void wav_mix_chl_stop_all(WAV_MIX_OBJ *obj);

void wav_mix_chl_del(WAV_MIX_OBJ *obj, const char *key);
void wav_mix_chl_del_all(WAV_MIX_OBJ *obj);
tbool wav_mix_chl_add(WAV_MIX_OBJ *obj, const char *key, u8 file_max, u8 default_vol, u8 max_vol, u8 radd);
#endif

