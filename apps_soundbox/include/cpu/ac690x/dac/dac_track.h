#ifndef __DAC_TRACK_H__
#define __DAC_TRACK_H__
#include "typedef.h"
#include "dac/dac_api.h"
#define DAC_DIGIT_TRACK_DEFAULT_VOL			(DIGITAL_VOL_MAX_L)
#define DAC_DIGIT_TRACK_MAX_VOL				(DIGITAL_VOL_MAX_L)

typedef struct __DAC_DIGIT_VAR {
    volatile u8 vol;
    volatile u8 tmp_vol;
    volatile u8 max_vol;
    volatile u8 fade_vol;
    volatile u8 fade_in;
    volatile u8 fade_out;
    /* volatile u8 mute;	 */
    volatile u8 step;
    volatile u8 step_max;
    volatile u8 is_mute;
} DAC_DIGIT_VAR;

void single_to_dual(void *out, void *in, u16 len);
void dual_to_single(void *out, void *in, u16 len);
void single_l_r_2_dual(void *out, void *in_l, void *in_r, u16 in_len);
void dac_mix_buf(s32 *obuf, s16 *ibuf, u16 len);
void dac_mix_buf_limit(s32 *ibuf, s16 *obuf, u16 len);
// void dac_digital_vol_ctrl(void *buffer, u32 len, u8 cur_vol);


void user_dac_digital_init(DAC_DIGIT_VAR *obj, u8 max_vol, u8 default_vol, u16 samplerate);
void user_dac_digit_fade(DAC_DIGIT_VAR *obj, void *buf, u32 len);
void user_dac_digit_reset(DAC_DIGIT_VAR *obj);
void user_dac_digital_set_vol(DAC_DIGIT_VAR *obj, u8 vol);
void user_dac_digital_set_mute(DAC_DIGIT_VAR *obj, u8 flag);
tbool user_dac_digital_get_mute_status(DAC_DIGIT_VAR *obj);
u8 user_dac_digital_get_vol(DAC_DIGIT_VAR *obj);
u8 user_dac_digital_get_max_vol(DAC_DIGIT_VAR *obj);
#endif

