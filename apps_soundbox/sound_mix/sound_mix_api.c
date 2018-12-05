#include "sound_mix_api.h"
#include "cbuf/circular_buf.h"
#include "dac/dac_api.h"
#include "sdk_cfg.h"
#include "common/app_cfg.h"
#include "echo/echo_api.h"

#if SPI_REC_EN
#include "spi_rec.h"
#endif

#define SOUND_MIX_API_DEBUG_ENABLE
#ifdef SOUND_MIX_API_DEBUG_ENABLE
#define sound_mix_api_printf	printf
#else
#define sound_mix_api_printf(...)
#endif


#define SOUND_MIX_API_TASK_NAME				"SoundMixTask"

static SOUND_MIX_OBJ *g_sound_mix = NULL;


extern cbuffer_t audio_cb ;
extern u8 dac_read_en;
static tbool sound_mix_api_stop(void *priv)
{
    //printf("%4d %4d\r",audio_cb.data_len,DAC_BUF_LEN);

    if ((audio_cb.data_len < DAC_BUF_LEN) || (!dac_read_en)) {
        if (audio_cb.data_len > DAC_BUF_LEN) {
            sound_mix_api_printf("warning output disable when output buff has data\r");
        }
        return true;
    } else {
        return false;
    }
}

static void sound_mix_api_clear(void *priv)
{
    //priv = priv;
    cbuf_clear(&audio_cb);
}


static void *sound_mix_api_output(void *priv, void *buf, u32 len)
{
    /* script_test_printf("o"); */
    dac_write((u8 *)buf, len);
    return priv;
}

static u32 set_mix_sr(void *priv, dec_inf_t *inf, tbool wait)
{
    sound_mix_api_printf("inf->sr %d %d\n", inf->sr, __LINE__);
    dac_set_samplerate(inf->sr, wait);
    return 0;
}

static void sound_mix_get_filter_date_callback(void *priv, u8 *fil_buf, u8 *org_buf, u32 len)
{
    /* putchar('f');	 */
    if (echo_check_reverb_api_ready() == true) {
        u32 echo_len;

        echo_len = cbuf_get_data_len_echo();

        /* if (echo_len * 2 >= len) { */
        if (echo_len >= (len >> 1)) {
            /* putchar('@'); */
            s16 ech_comp_buf[DAC_BUF_LEN / 4]; //1chl 32point 64byte
            cbuf_read_echo(ech_comp_buf, len >> 1); //echo_mic_1chl : datalen = len/2
            echo_digital_vol_ctrl(ech_comp_buf, len >> 1); //digital vol
            echo_dac_mixture(org_buf, ech_comp_buf, len);
            echo_dac_mixture(fil_buf, ech_comp_buf, len);
        } else {
//            putchar('+');
        }

#if (SPI_REC_EN)
        rd_da((s16 *)fil_buf, 0);
        spirec_save_dac(fil_buf);
#endif//SPI_REC_EN

        echo_cbuf_write_detect();
    }
}

tbool sound_mix_api_init(u16 targar_sr)
{
    tbool ret;
    SOUND_MIX_OBJ *mix_obj = sound_mix_creat();
    if (mix_obj == NULL) {
        return false;
    }

    /* if(__sound_mix_set_with_task_enable(mix_obj) == false) */
    /* { */
    /* sound_mix_destroy(&mix_obj); */
    /* return false;		 */
    /* } */
    //__sound_mix_set_output_cbk(mix_obj, (void *)sound_mix_api_output, NULL);
    //__sound_mix_set_data_op_cbk(mix_obj, sound_mix_api_clear, sound_mix_api_stop, NULL);
    __sound_mix_set_set_info_cbk(mix_obj, set_mix_sr, NULL);
    __sound_mix_set_samplerate(mix_obj, targar_sr);
    ///以下接口主要是用于获取叠加结果数据，包括没有过滤的数据和过滤的数据结果， 该接口是可变参数， NOTICE_MIX表示要过滤提示叠加
    __sound_mix_set_filter_data_get(mix_obj, sound_mix_get_filter_date_callback, NULL, 1, "NOTICE_MIX");

    ret = sound_mix_process(mix_obj, NULL, 0, 0);
    if (ret == false) {
        sound_mix_destroy(&mix_obj);
        return false;
    }

    OS_ENTER_CRITICAL();
    g_sound_mix = mix_obj;
    OS_EXIT_CRITICAL();

    sound_mix_api_printf("sound mix api init ok!!!\n");
    return true;
}

void sound_mix_api_close(void)
{
    if (g_sound_mix) {
        sound_mix_destroy(&g_sound_mix);
    }
}


SOUND_MIX_OBJ *sound_mix_api_get_obj(void)
{
    return g_sound_mix;
}

tbool sound_mix_api_process(void *buf, u32 len)
{
    return sound_mix_process_deal(g_sound_mix, buf, len);
}


