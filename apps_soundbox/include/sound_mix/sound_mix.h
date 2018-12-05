#ifndef __SOUND_MIX_H__
#define __SOUND_MIX_H__
#include "typedef.h"
#include "dec/music_api.h"
#include "dac/dac_track.h"

typedef struct __SOUND_MIX_OBJ SOUND_MIX_OBJ;
typedef struct __SOUND_MIX_CHL SOUND_MIX_CHL;

typedef struct __SOUND_MIX_SRC_P {
    void *task_name;
    u8    task_prio;
} SOUND_MIX_SRC_P;

typedef struct __MIX_CHL_CBK {
    void *priv;
    void (*cbk)(void *priv, void *buf, u32 len);
} MIX_CHL_CBK;

typedef struct __SOUND_MIX_P {
    const char *key;
    u32 out_len;
    u8	track;
    u8	limit;
    u8 	default_vol;
    u8  max_vol;
    u8  wait;
    DEC_SET_INFO_CB *set_info_cb;
    _OP_IO *input;///注意赋值
    DEC_OP_CB *op_cb;///注意赋值
    SOUND_MIX_SRC_P src_p;
    MIX_CHL_CBK chl_cbk;
} SOUND_MIX_P;

SOUND_MIX_OBJ *sound_mix_creat(void);
void sound_mix_destroy(SOUND_MIX_OBJ **hdl);
void __sound_mix_set_samplerate(SOUND_MIX_OBJ *obj, u16 samplerate);
tbool __sound_mix_set_with_task_enable(SOUND_MIX_OBJ *obj);
void __sound_mix_set_once_rlen(SOUND_MIX_OBJ *obj, u32 rlen);
void __sound_mix_set_set_info_cbk(SOUND_MIX_OBJ *obj, u32(*set_info)(void *priv, dec_inf_t *inf, tbool wait), void *priv);
void __sound_mix_set_output_cbk(SOUND_MIX_OBJ *obj, void *(*output)(void *priv, void *buf, u16 len), void *priv);
void __sound_mix_set_data_op_cbk(SOUND_MIX_OBJ *obj, void (*clr)(void *priv),  tbool(*over)(void *priv), void *priv);
void __sound_mix_set_filter_data_get(SOUND_MIX_OBJ *obj, void (*cbk)(void *priv, u8 *fil_buf, u8 *org_buf, u32 len), void *priv, int cnt, ...);
void __sound_mix_chl_set_vol(SOUND_MIX_CHL *mix_chl, u8 vol);
void __sound_mix_chl_set_mute(SOUND_MIX_CHL *mix_chl, u8 flag);
tbool __sound_mix_chl_get_mute_status(SOUND_MIX_CHL *mix_chl);
void __sound_mix_chl_set_wait(SOUND_MIX_CHL *mix_chl);
void __sound_mix_chl_set_wait_ok(SOUND_MIX_CHL *mix_chl);
s8 __sound_mix_chl_check_wait_ready(SOUND_MIX_CHL *mix_chl);
u32 __sound_mix_chl_get_data_len(SOUND_MIX_CHL *mix_chl);
tbool sound_mix_process(SOUND_MIX_OBJ *obj, void *task_name, u32 prio, u32 stk_size);
SOUND_MIX_CHL *sound_mix_chl_add(SOUND_MIX_OBJ *obj, SOUND_MIX_P *param);
tbool sound_mix_chl_del_by_key(SOUND_MIX_OBJ *obj, const char *key);
void sound_mix_chl_del_by_chl(SOUND_MIX_CHL **mix_chl);
tbool sound_mix_process_deal(SOUND_MIX_OBJ *obj, void *buf, u32 len);
#endif
