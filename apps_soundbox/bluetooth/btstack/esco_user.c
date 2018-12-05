#include "esco_user.h"
#include "sound_mix_api.h"
#include "string.h"
#include "notice_player.h"
#include "spi_rec.h"
#include "sdk_cfg.h"

#define ESCO_MIX_NAME				"ESCO_MIX"
#define ESCO_MIX_OUTPUT_BUF_LEN		(4*1024)
#define ESCO_MIX_OUTPUT_DEC_LIMIT	(50)//这是一个百分比，表示启动解码时输出buf的数据量百分比


u32 esco_decode_get_output_data_len(void *priv)
{
    SOUND_MIX_CHL *mix_chl = (SOUND_MIX_CHL *)priv;
    return __sound_mix_chl_get_data_len(mix_chl);
}

void esco_decode_before_callback(BT_ESCO_DATA_STREAM *stream, void *priv)
{
    printf("---------------------esco bf----------------------\n");
    if (stream == NULL) {
        return ;
    }
#if SOUND_MIX_GLOBAL_EN
#if SPI_REC_EN
    spirec_api_dac_stop();
#endif//SPI_REC_EN
    notice_player_stop();
    sound_mix_api_close();
    sound_mix_api_init(16000);
    /* SOUND_MIX_OBJ *mix_obj = (SOUND_MIX_OBJ *)priv; */
    SOUND_MIX_OBJ *mix_obj = sound_mix_api_get_obj();//(SOUND_MIX_OBJ *)priv;
    if (mix_obj == NULL) {
        return ;
    }

    printf("esco to mix!!!\n");
    SOUND_MIX_P mix_p;
    memset((u8 *)&mix_p, 0x0, sizeof(SOUND_MIX_P));
    mix_p.key = ESCO_MIX_NAME;
    mix_p.out_len = ESCO_MIX_OUTPUT_BUF_LEN;
    /* mix_p.set_info = &(mapi->dop_api->io->set_music_info); */
    mix_p.set_info_cb = &stream->set_info_cb;
    mix_p.input = &stream->output;
    mix_p.op_cb = &stream->op_cb;
    mix_p.limit = ESCO_MIX_OUTPUT_DEC_LIMIT;
    mix_p.max_vol = DAC_DIGIT_TRACK_MAX_VOL;
    mix_p.default_vol = DAC_DIGIT_TRACK_DEFAULT_VOL;
    stream->mix = (void *)sound_mix_chl_add(mix_obj, &mix_p);
    ASSERT(stream->mix);
    stream->getlen.cbk = esco_decode_get_output_data_len;
    stream->getlen.priv = stream->mix;
    stream->obuf_size = ESCO_MIX_OUTPUT_BUF_LEN;
#endif//SOUND_MIX_GLOBAL_EN
}


void esco_decode_after_callback(BT_ESCO_DATA_STREAM *stream, void *priv)
{
    printf("-------------------- esco af------------------------\n");
    if (stream == NULL) {
        return ;
    }

#if SOUND_MIX_GLOBAL_EN
    if (stream->mix) {
        sound_mix_chl_del_by_chl((SOUND_MIX_CHL **)&stream->mix);
    }

#if SPI_REC_EN
    spirec_api_dac_stop();
#endif//SPI_REC_EN
    notice_player_stop();
    sound_mix_api_close();
    sound_mix_api_init(SOUND_MIX_API_DEFUALT_SAMPLERATE);
#endif//SOUND_MIX_GLOBAL_EN
}

