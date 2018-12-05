#include "linein_digital.h"
#include "linein.h"
#include "sound_mix_api.h"
#include "dec/music_api.h"
#include "sdk_cfg.h"
#include "common/app_cfg.h"




typedef struct __LINEIN_DIGITAL {
    _OP_IO	 output;
    DEC_OP_CB	 op_cb;
    DEC_SET_INFO_CB set_info_cb;
    void *mix;
    volatile u8	vocal;
} LINEIN_DIGITAL;

LINEIN_DIGITAL *linein_dig = NULL;
#define LINEIN_DIGITAL_TASK_NAME		"Linein_D_TASK"

static u32 linein_set_inf(void *priv, dec_inf_t *inf, tbool wait)
{
    dac_set_samplerate(inf->sr, wait);
    return 0;
}

static void *linein_output(void *priv, void *data, u32 len)
{
    printf("O");
    //priv = priv;
//    memcpy(eq_in_buf,data,len);
//    my_eq_run(eq_in_buf,eq_buf,len/2,0);
    cbuf_write_dac(data, len);
    return priv;
}

static void linein_vocal_enable(LINEIN_DIGITAL *obj, u8 en)
{
    if (obj) {
        obj->vocal = en;
    }
}

static u8 linein_get_vocal_status(LINEIN_DIGITAL *obj)
{
    if (obj == NULL) {
        return 0;
    }
    return obj->vocal;
}

static void linein_mix_chl_callback(void *priv, void *buf, u32 len)
{
    LINEIN_DIGITAL *obj = (LINEIN_DIGITAL *)priv;
    if (obj) {
        if (obj->vocal) {
            dac_digital_lr_sub(buf, len);
        }
    }
}

static LINEIN_DIGITAL *linein_digital_init(void)
{
    printf("\n--func=%s, line=%d\n", __FUNCTION__, __LINE__);
    LINEIN_DIGITAL *obj;
    obj = (LINEIN_DIGITAL *)calloc(1, sizeof(LINEIN_DIGITAL));
    printf("\n--func=%s, line=%d\n", __FUNCTION__, __LINE__);
    ASSERT(obj);

    obj->set_info_cb.priv = NULL;
    obj->set_info_cb._cb = linein_set_inf;
    obj->output.priv = NULL;
    obj->output.output = linein_output;
    obj->op_cb.priv = NULL;
    obj->op_cb.clr = NULL;
    obj->op_cb.over = NULL;


    SOUND_MIX_P mix_p;
    memset((u8 *)&mix_p, 0x0, sizeof(SOUND_MIX_P));
    mix_p.key = "LINEIN_MIX";
    mix_p.out_len = 4096L;
    /* mix_p.set_info = &(mapi->dop_api->io->set_music_info); */
    mix_p.set_info_cb = &obj->set_info_cb;
    mix_p.input = &obj->output;
    mix_p.op_cb = &obj->op_cb;
    mix_p.limit = 0;
    mix_p.max_vol = DAC_DIGIT_TRACK_MAX_VOL;
    mix_p.default_vol = DAC_DIGIT_TRACK_DEFAULT_VOL;
    mix_p.chl_cbk.priv = (void *)obj;
    mix_p.chl_cbk.cbk = (void *)linein_mix_chl_callback;
    mix_p.src_p.task_name = LINEIN_DIGITAL_TASK_NAME;
    mix_p.src_p.task_prio = TaskLineinDigPrio;

    obj->mix = (void *)sound_mix_chl_add(sound_mix_api_get_obj(), &mix_p);
    /* ASSERT(obj->mix); */

    if (obj->set_info_cb._cb) {
        dec_inf_t inf;
        memset((u8 *)&inf, 0x0, sizeof(dec_inf_t));
        inf.sr = LINEIN_DEFAULT_DIGITAL_SR;
        obj->set_info_cb._cb(obj->set_info_cb.priv, &inf, 0);
    }
    printf("\n--func=%s, line=%d\n", __FUNCTION__, __LINE__);

    return obj;
}


static void linein_digital_exit(LINEIN_DIGITAL *obj)
{
    if (obj == NULL) {
        return ;
    }

    if (obj->mix) {
        sound_mix_chl_del_by_chl((SOUND_MIX_CHL **)&obj->mix);
    }
    free(obj);
    printf("exit ok");
}

void linein_aux_ladc_2_digtial(u8 *buf, u16 len)
{
#if (LINEIN_DIGTAL_EN)
    if (linein_dig == NULL) {
        return;
    }
    if (linein_dig->output.output) {
        linein_dig->output.output(linein_dig->output.priv, buf, len);
    }
#endif//LINEIN_DIGTAL_EN
}

void linein_digital_enable(u8 en)
{
#if (LINEIN_DIGTAL_EN)
    if (en) {
        if (linein_dig == NULL) {
            LINEIN_DIGITAL *obj = linein_digital_init();
            OS_ENTER_CRITICAL();
            linein_dig = obj;
            OS_EXIT_CRITICAL();
        } else {
            /* printf("linein digital is aready open\n");		 */
        }
    } else {
        linein_digital_exit(linein_dig);
        OS_ENTER_CRITICAL();
        linein_dig = NULL;
        OS_EXIT_CRITICAL();
    }
#endif//LINEIN_DIGTAL_EN
}

void linein_digital_vocal_enable(u8 en)
{
#if (LINEIN_DIGTAL_EN)
    linein_vocal_enable(linein_dig, en);
#endif//LINEIN_DIGTAL_EN
}

u8 linein_digital_get_vocal_status(void)
{
#if (LINEIN_DIGTAL_EN)
    return linein_get_vocal_status(linein_dig);
#else
    return 0;
#endif//LINEIN_DIGTAL_EN
}

void linein_digital_mute(u8 mute)
{
#if (LINEIN_DIGTAL_EN)
    if (linein_dig) {
        __sound_mix_chl_set_mute((SOUND_MIX_CHL *)linein_dig->mix, mute);
    }
#else
    dac_mute(mute, 1);
#endif//LINEIN_DIGTAL_EN
}

tbool linein_digital_mute_status(void)
{
#if (LINEIN_DIGTAL_EN)
    if (linein_dig) {
        return __sound_mix_chl_get_mute_status((SOUND_MIX_CHL *)linein_dig->mix);
    } else {
        return false;
    }
#else
    return (tbool)is_dac_mute();
#endif//LINEIN_DIGTAL_EN
}

