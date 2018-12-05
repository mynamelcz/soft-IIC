#include "script_wav_mix.h"
#include "common/app_cfg.h"
#include "common/msg.h"
#include "script_wav_mix_key.h"
#include "notice_player.h"
#include "dac/dac_api.h"
#include "cbuf/circular_buf.h"
#include "wav_mix_play.h"

#define SCRIPT_WAV_MIX_DEBUG_ENABLE
#ifdef SCRIPT_WAV_MIX_DEBUG_ENABLE
#define script_wav_mix_printf	printf
#else
#define script_wav_mix_printf(...)
#endif

#define MSG_DEMO_WAV_MIX_CH0_VOL_UP			MSG_DEMO_TEST5
#define MSG_DEMO_WAV_MIX_CH0_VOL_DOWN		MSG_DEMO_TEST6

#define MSG_DEMO_WAV_MIX_CH0_PLAY			MSG_DEMO_TEST0
#define MSG_DEMO_WAV_MIX_CH1_PLAY			MSG_DEMO_TEST1
#define MSG_DEMO_WAV_MIX_CH2_PLAY			MSG_DEMO_TEST2
#define MSG_DEMO_WAV_MIX_CH3_PLAY			MSG_DEMO_TEST3
#define MSG_DEMO_WAV_MIX_CH4_PLAY			MSG_DEMO_TEST4


/* #define MSG_DEMO_WAV_MIX_STOP		MSG_DEMO_TEST0 */
/* #define MSG_DEMO_WAV_MIX_START_CHL0	MSG_DEMO_TEST1 */
/* #define MSG_DEMO_WAV_MIX_START_CHL1	MSG_DEMO_TEST2 */
/* #define MSG_DEMO_WAV_MIX_START	MSG_DEMO_TEST3 */
extern cbuffer_t audio_cb ;
extern u8 dac_read_en;
static tbool wav_mix_wait_stop(void *priv)
{
    //printf("%4d %4d\r",audio_cb.data_len,DAC_BUF_LEN);

    if ((audio_cb.data_len < DAC_BUF_LEN) || (!dac_read_en)) {
        if (audio_cb.data_len > DAC_BUF_LEN) {
            printf("warning output disable when output buff has data\r");
        }
        return true;
    } else {
        return false;
    }
}

static void wav_mix_clear(void *priv)
{
    //priv = priv;
    cbuf_clear(&audio_cb);
}

static void *wav_mix_output(void *priv, void *buf, u16 len)
{
    /* script_test_printf("o"); */
    dac_write((u8 *)buf, len);
    return priv;
}

static u32 set_wav_mix_sr(void *priv, dec_inf_t *inf, tbool wait)
{
    printf("inf->sr %d %d\n", inf->sr, __LINE__);
    dac_set_samplerate(inf->sr, wait);
    return 0;
}



static void *script_wav_mix_init(void *priv)
{
    dac_channel_on(MUSIC_CHANNEL, FADE_ON);
    script_wav_mix_printf("script wav_mix init ok\n");

    return (void *)NULL;
}

static void script_wav_mix_exit(void **hdl)
{
    script_wav_mix_printf("script wav_mix exit ok\n");

    dac_channel_off(MUSIC_CHANNEL, FADE_ON);
}

static void script_wav_mix_task(void *parg)
{
    int msg[6] = {0};
    u8 ch0_pp_flag = 0;
    u8 wav_mix_cur_vol_ch0 = DAC_DIGIT_TRACK_DEFAULT_VOL;
    WAV_MIX_OBJ *obj = wav_mix_creat();
    ASSERT(obj);
    __wav_mix_set_info_cbk(obj, set_wav_mix_sr, NULL);
    __wav_mix_set_data_op_cbk(obj, wav_mix_clear, wav_mix_wait_stop, NULL);
    __wav_mix_set_output_cbk(obj, wav_mix_output, NULL);
    __wav_mix_set_samplerate(obj, 16000);

    /* dac_set_samplerate(16000, 0); */
    /* ret = sound_mix_process(mix_obj, "mix_task", TaskMixPrio, 2 * 1024); */
    tbool ret = wav_mix_process(obj, "mix_task", TaskMixPrio, 2 * 1024);
    if (ret == false) {
        while (1) {
            os_time_dly(100);
            script_wav_mix_printf("------------err-----------------\n");
        }
    }

    wav_mix_chl_add(obj, "chl0", 2, DAC_DIGIT_TRACK_DEFAULT_VOL, DAC_DIGIT_TRACK_MAX_VOL, 0);
    wav_mix_chl_add(obj, "chl1", 1, DAC_DIGIT_TRACK_DEFAULT_VOL, DAC_DIGIT_TRACK_MAX_VOL, 0);
    wav_mix_chl_add(obj, "chl2", 1, DAC_DIGIT_TRACK_DEFAULT_VOL, DAC_DIGIT_TRACK_MAX_VOL, 0);
    wav_mix_chl_add(obj, "chl3", 1, DAC_DIGIT_TRACK_DEFAULT_VOL, DAC_DIGIT_TRACK_MAX_VOL, 0);
    wav_mix_chl_add(obj, "chl4", 1, DAC_DIGIT_TRACK_DEFAULT_VOL, DAC_DIGIT_TRACK_MAX_VOL, 0);

    wav_mix_chl_play(obj, "chl0", 0, 2, "/wav/mbg0.wav", "/wav/mbg1.wav");
    /* wav_mix_chl_play(obj, "chl1", 0, 1, "/wav/drum0.wav"); */
    /* wav_mix_chl_play(obj, "chl2", 0, 1, "/wav/drum1.wav"); */
    /* wav_mix_chl_play(obj, "chl3", 0, 1, "/wav/drum2.wav"); */
    /* wav_mix_chl_play(obj, "chl3", 0, 1, "/wav/drum3.wav"); */

    script_wav_mix_printf("wav mix demo tes ----------------------\n");

    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();
        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                script_wav_mix_printf("script_test_del \r");
                wav_mix_destroy(&obj);
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;

        case MSG_WAV_MIX_END:
            /* script_wav_mix_printf("--%s--\n", (const char *)msg[1]); */
            break;

        case MSG_DEMO_WAV_MIX_CH0_PLAY:
            if (ch0_pp_flag) {
                wav_mix_chl_play(obj, "chl0", 0, 2, "/wav/mbg0.wav", "/wav/mbg1.wav");
            } else {
                wav_mix_chl_stop(obj, "chl0");
            }
            ch0_pp_flag = !ch0_pp_flag;
            break;
        case MSG_DEMO_WAV_MIX_CH1_PLAY:
            wav_mix_chl_play(obj, "chl1", 1, 1, "/wav/drum0.wav");
            break;
        case MSG_DEMO_WAV_MIX_CH2_PLAY:
            wav_mix_chl_play(obj, "chl2", 1, 1, "/wav/drum1.wav");
            break;
        case MSG_DEMO_WAV_MIX_CH3_PLAY:
            wav_mix_chl_play(obj, "chl3", 1, 1, "/wav/drum2.wav");
            break;
        case MSG_DEMO_WAV_MIX_CH4_PLAY:
            wav_mix_chl_play(obj, "chl4", 1, 1, "/wav/drum3.wav");
            break;

        case MSG_MUSIC_PP:
            wav_mix_chl_stop_all(obj);
            ch0_pp_flag = !ch0_pp_flag;
            break;

        case MSG_DEMO_WAV_MIX_CH0_VOL_DOWN:
            if (wav_mix_cur_vol_ch0) {
                wav_mix_cur_vol_ch0--;
            }
            script_wav_mix_printf("ch0 vol down = %d\n", wav_mix_cur_vol_ch0);
            __wav_mix_chl_set_vol(obj, "chl0", wav_mix_cur_vol_ch0);
            break;
        case MSG_DEMO_WAV_MIX_CH0_VOL_UP:
            wav_mix_cur_vol_ch0++;
            if (wav_mix_cur_vol_ch0 >= DAC_DIGIT_TRACK_MAX_VOL) {
                wav_mix_cur_vol_ch0 = DAC_DIGIT_TRACK_MAX_VOL;
            }
            script_wav_mix_printf("ch0 vol up = %d\n", wav_mix_cur_vol_ch0);
            __wav_mix_chl_set_vol(obj, "chl0", wav_mix_cur_vol_ch0);
            break;
        default:
            break;
        }
    }
}

const SCRIPT script_wav_mix_info = {
    .skip_check = NULL,
    .init = script_wav_mix_init,
    .exit = script_wav_mix_exit,
    .task = script_wav_mix_task,
    .key_register = &script_wav_mix_key,
};


