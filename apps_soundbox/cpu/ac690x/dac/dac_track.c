#include "dac/dac_track.h"
#include "dac/dac.h"
#include "rtos/os_api.h"

///不同采样率数据调节音量淡入淡出步进
const u8 user_dac_fade_max_step[9] = {
    /*8000*/  4,
    /*11025*/ 4,
    /*12000*/ 6,
    /*16000*/ 8,
    /*22050*/ 10,
    /*24000*/ 12,
    /*32000*/ 16,
    /*44100*/ 20,
    /*48000*/ 64,
};

void single_to_dual(void *out, void *in, u16 len)
{
    s16 *outbuf = out;
    s16 *inbuf = in;
    len >>= 1;
    while (len--) {
        *outbuf++ = *inbuf;
        *outbuf++ = *inbuf;
        inbuf++;
    }
}


void dual_to_single(void *out, void *in, u16 len)
{
    s16 *outbuf = out;
    s16 *inbuf = in;
    len >>= 2;
    while (len--) {
        *outbuf++ = (inbuf[0] + inbuf[1]) >> 1;
        inbuf += 2;
    }
}


void single_l_r_2_dual(void *out, void *in_l, void *in_r, u16 in_len)
{
    s16 *outbuf = out;
    s16 *inbuf_l = in_l;
    s16 *inbuf_r = in_r;
    in_len >>= 1;
    while (in_len--) {
        *outbuf++ = *inbuf_l++;
        *outbuf++ = *inbuf_r++;
    }
}



void dac_mix_buf(s32 *obuf, s16 *ibuf, u16 len)
{
    u16 i;
    for (i = 0; i < len; i++) {
        obuf[i] += ibuf[i];
    }
}

void dac_mix_buf_limit(s32 *ibuf, s16 *obuf, u16 len)
{
    u16 i;
    for (i = 0; i < len; i++) {
        if (ibuf[i] > 32767) {
            ibuf[i] = 32767;
        } else if (ibuf[i] < -32768) {
            ibuf[i] = -32768;
        }
        obuf[i] = (s16)ibuf[i];
    }
}


void user_dac_digital_init(DAC_DIGIT_VAR *obj, u8 max_vol, u8 default_vol, u16 samplerate)
{
    if (obj == NULL) {
        return ;
    }

    u8 cnt = 0;
    if (default_vol >= max_vol) {
        default_vol = max_vol;
    }

    obj->max_vol = max_vol;
    obj->vol = default_vol;
    obj->tmp_vol = obj->vol;
    obj->fade_vol = default_vol;
    obj->fade_in = 0;
    obj->fade_out = 0;
    obj->is_mute = 0;
    /* obj->mute = 0;		 */
    switch (samplerate) {
    case 48000:
        cnt++;
    case 44100:
        cnt++;
    case 32000:
        cnt++;
    case 24000:
        cnt++;
    case 22050:
        cnt++;
    case 16000:
        cnt++;
    case 12000:
        cnt++;
    case 11025:
        cnt++;
    case 8000:
        break;
    default:
        cnt = 0;
        break;
    }

    obj->step = 0;
    obj->step_max = user_dac_fade_max_step[cnt];
    /* printf("step_max = %d-------------------\n", obj->step_max); */
}

static u8 user_dac_digit_fade_step(DAC_DIGIT_VAR *obj)
{
    if (obj == NULL) {
        return 1;
    }

    obj->step ++ ;
    if (obj->fade_in) {
        if (obj->step < obj->step_max) {
            return 0;
        } else {
            obj->step = 0;
        }
        if (obj->vol < obj->fade_vol) {
            obj->vol++;
        } else {
            obj->fade_in = 0;
        }
    } else if (obj->fade_out) {
        if (obj->step < obj->step_max) {
            return 0;
        } else {
            obj->step = 0;
        }
        if (obj->vol > obj->fade_vol) {
            obj->vol--;
        } else {
            obj->fade_out = 0;
        }
    }
    /* else if (obj->mute) */
    /* { */
    /* } */
    /* else */
    /* { */
    /* return 1; */
    /* } */
    return 0;
}

void user_dac_digit_fade(DAC_DIGIT_VAR *obj, void *buf, u32 len)
{
    s32 valuetemp;
    u32 i;
    u16 curtabvol;
    s16 *inbuf = (s16 *)buf;

    if (obj == NULL) {
        return ;
    }

    len >>= 1;
    for (i = 0; i < len; i++) {
        valuetemp = inbuf[i];

        if (user_dac_digit_fade_step(obj)) {
            return;
        }
        curtabvol = digital_vol_tab[obj->vol];

        valuetemp = (valuetemp * curtabvol) >> 14 ;
        if (valuetemp < -32768) {
            valuetemp = -32768;
        } else if (valuetemp > 32767) {
            valuetemp = 32767;
        }
        inbuf[i] = (s16)valuetemp;
    }
}

void user_dac_digit_fade_in(DAC_DIGIT_VAR *obj)
{
    /* obj->mute = 0; */
    obj->step = 0;
    obj->fade_in = 1;
    while (obj->fade_in) {
        os_time_dly(1);
    }
}

void user_dac_digit_fade_out(DAC_DIGIT_VAR *obj)
{
    /* obj->mute = 1; */
    obj->step = 0;
    obj->fade_out = 1;
    while (obj->fade_out) {
        os_time_dly(1);
    }
}

void user_dac_digit_set_step(DAC_DIGIT_VAR *obj, u8 step)
{
    if (!step) {
        step = 1;
    }
    obj->step_max = step;
}

void user_dac_digit_reset(DAC_DIGIT_VAR *obj)
{
    obj->fade_in = 0;
    obj->fade_out = 0;
    obj->vol = obj->fade_vol;
}

void user_dac_digital_set_vol(DAC_DIGIT_VAR *obj, u8 vol)
{
    if (obj == NULL) {
        return ;
    }
    if (vol >= obj->max_vol) {
        vol = obj->max_vol;
    }

    if (obj->is_mute == 0) {
        obj->fade_vol = vol;
        if (obj->fade_vol > obj->vol) {
            user_dac_digit_fade_in(obj);
        } else {
            user_dac_digit_fade_out(obj);
        }
        /* obj->tmp_vol = obj->vol; */
    } else {
        obj->tmp_vol = vol;
    }
}

void user_dac_digital_set_mute(DAC_DIGIT_VAR *obj, u8 flag)
{
    if (obj == NULL) {
        return ;
    }

    if (flag) {
        if (obj->is_mute == 0) {
            obj->tmp_vol = obj->vol;
            user_dac_digital_set_vol(obj, 0);
            obj->is_mute = 1;
            printf("chl mute !!, tmp_vol = %d\n", obj->tmp_vol);
        }
    } else {
        if (obj->is_mute) {
            printf("chl unmute !!, tmp_vol = %d\n", obj->tmp_vol);
            obj->is_mute = 0;
            user_dac_digital_set_vol(obj, obj->tmp_vol);
        }
    }
}

tbool user_dac_digital_get_mute_status(DAC_DIGIT_VAR *obj)
{
    if (obj == NULL) {
        return false;
    }

    return (obj->is_mute ? true : false);
}

u8 user_dac_digital_get_vol(DAC_DIGIT_VAR *obj)
{
    if (obj == NULL) {
        return 0;
    }

    return obj->vol;
}

u8 user_dac_digital_get_max_vol(DAC_DIGIT_VAR *obj)
{
    if (obj == NULL) {
        return 0;
    }
    return obj->max_vol;
}

