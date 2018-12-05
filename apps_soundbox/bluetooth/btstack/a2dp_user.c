#include "a2dp_user.h"
#include "sound_mix_api.h"
#include "string.h"
#include "sdk_cfg.h"

#define A2DP_MIX_NAME				"A2DP_MIX"
#define A2DP_MIX_OUTPUT_BUF_LEN		(4*1024)
#define A2DP_MIX_OUTPUT_DEC_LIMIT	(50)//这是一个百分比，表示启动解码时输出buf的数据量百分比


void a2dp_decode_before_callback(DEC_API_IO *dec_io, void *priv)
{
    printf("--------------------- a2dp bf----------------------\n");
    if (dec_io == NULL) {
        return ;
    }

#if SOUND_MIX_GLOBAL_EN
    SOUND_MIX_OBJ *mix_obj = sound_mix_api_get_obj();//(SOUND_MIX_OBJ *)priv;
    if (mix_obj == NULL) {
        return ;
    }

    printf("a2dp to mix!!!\n");
    SOUND_MIX_P mix_p;
    memset((u8 *)&mix_p, 0x0, sizeof(SOUND_MIX_P));
    mix_p.key = A2DP_MIX_NAME;
    mix_p.out_len = A2DP_MIX_OUTPUT_BUF_LEN;
    /* mix_p.set_info = &(mapi->dop_api->io->set_music_info); */
    mix_p.set_info_cb = &dec_io->set_info_cb;
    mix_p.input = &dec_io->output;
    mix_p.op_cb = &dec_io->op_cb;
    mix_p.limit = A2DP_MIX_OUTPUT_DEC_LIMIT;
    mix_p.max_vol = DAC_DIGIT_TRACK_MAX_VOL;
    mix_p.default_vol = DAC_DIGIT_TRACK_DEFAULT_VOL;
    dec_io->mix = (void *)sound_mix_chl_add(mix_obj, &mix_p);
#endif//SOUND_MIX_GLOBAL_EN
}


void a2dp_decode_after_callback(DEC_API_IO *dec_io, void *priv)
{
    printf("--------------------a2dp af------------------------\n");
    if (dec_io == NULL) {
        return ;
    }

#if SOUND_MIX_GLOBAL_EN
    if (dec_io->mix) {
        sound_mix_chl_del_by_chl((SOUND_MIX_CHL **)&dec_io->mix);
    }
#endif//SOUND_MIX_GLOBAL_EN
}

