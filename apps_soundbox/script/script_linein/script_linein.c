#include "script_linein.h"
#include "linein_digital.h"
#include "common/msg.h"
#include "script_linein_key.h"
#include "notice/notice_player.h"
#include "dac/dac_api.h"
#include "linein.h"
#include "dev_linein.h"
#include "sound_mix_api.h"
#include "spi_rec.h"

#define SCRIPT_LINEIN_DEBUG_ENABLE
#ifdef SCRIPT_LINEIN_DEBUG_ENABLE
#define script_linein_printf	printf
#else
#define script_linein_printf(...)
#endif


/* static u8 spi_rec_flag = 0; */

static tbool script_linein_notice_play_msg_callback(int msg[], void *priv)
{
    if (msg[0] != MSG_HALF_SECOND) {
        script_linein_printf("linein notice play msg = %d\n", msg[0]);
    }
    ///this is an example
//	if(msg[0] == MSG_MUSIC_PREV_FILE)
//		return true;
    return false;
}

static u32 script_linein_skip_check(void)
{
    return aux_is_online();
}
static u8 script_linein_mute_flag;
static void *script_linein_init(void *priv)
{
    script_linein_printf("script linein init ok\n");
    if (is_dac_mute()) {
        script_linein_mute_flag = 1;
    } else {
        script_linein_mute_flag = 0;
    }
#if (SOUND_MIX_GLOBAL_EN)
    sound_mix_api_close();
#endif//SOUND_MIX_GLOBAL_EN
#if (LINEIN_DIGTAL_EN)
    sound_mix_api_init(LINEIN_DEFAULT_DIGITAL_SR);
#endif//LINEIN_DIGTAL_EN
    return (void *)NULL;
}

static void script_linein_exit(void **hdl)
{
    aux_dac_channel_off();
    if (script_linein_mute_flag) {
        script_linein_mute_flag = 0;
        dac_mute(1, 1);
    }
    script_linein_printf("script linein exit ok\n");

#if (LINEIN_DIGTAL_EN)
    sound_mix_api_close();
#endif//LINEIN_DIGTAL_EN
#if (SOUND_MIX_GLOBAL_EN)
    sound_mix_api_init(SOUND_MIX_API_DEFUALT_SAMPLERATE);
#endif//SOUND_MIX_GLOBAL_EN
    /* dac_channel_off(MUSIC_CHANNEL, FADE_ON); */
}


static void script_linein_task(void *parg)
{
    int msg[6] = {0};

    /* REVERB_API_STRUCT *linein_reverb = NULL; */
    NOTICE_PlAYER_ERR n_err;
    malloc_stats();

    dac_channel_on(MUSIC_CHANNEL, FADE_ON);
    n_err = notice_player_play_by_path_to_mix(SCRIPT_TASK_NAME, BPF_LINEIN_MP3, script_linein_notice_play_msg_callback, 0, sound_mix_api_get_obj());
    if (n_err != NOTICE_PLAYER_NO_ERR && n_err != NOTICE_PLAYER_NO_ERR_WITHOUT_MSG_DEAL) {
        script_linein_printf("linein notice play err!!\n");
    }
    script_linein_printf("linein notice play ok\n");
    /* dac_channel_off(MUSIC_CHANNEL, FADE_ON); */

    linein_digital_enable(1);

    aux_dac_channel_on();

    /* os_taskq_post(SCRIPT_TASK_NAME, 1, MSG_ECHO_START); */

    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();

#if SUPPORT_APP_RCSP_EN
        rcsp_linein_task_msg_deal_before(msg);
#endif

        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                script_linein_printf("script_linein_del \r");
                linein_digital_enable(0);
                /* echo_exit_api((void **)&linein_reverb); */
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;

        case MSG_VOCAL_SWITCH:
            if (linein_digital_get_vocal_status()) {
                linein_digital_vocal_enable(0);
            } else {
                linein_digital_vocal_enable(1);
            }
            break;

        case MSG_AUX_MUTE:
            if (linein_digital_mute_status()) {
                script_linein_printf("MSG_AUX_UNMUTE\n");
                linein_digital_mute(0);
                /* send_key_voice(500); */
                /* led_fre_set(15,0); */
            } else {
                script_linein_printf("MSG_AUX_MUTE\n");
                os_time_dly(30);
                linein_digital_mute(1);
                /* led_fre_set(0,0); */
            }
            /* UI_menu(MENU_REFRESH); */
            break;


//#if (SOUND_MIX_GLOBAL_EN != 0 ||  LINEIN_DIGTAL_EN != 0)
//#if EQ_EN
//        case MSG_MUSIC_EQ:
//			{
//				//this is an example
//				extern void eq_mode_sw(int mode);
//				static u8 test_eq_mode = 0;
//				test_eq_mode++;
//				if (test_eq_mode > 5) {
//					test_eq_mode = 0;
//				}
//				printf("linein eq mode = %d\n", test_eq_mode);
//				/* __music_player_set_eq(mapi, eq_mode); */
//				eq_mode_sw(test_eq_mode);			}
//			break;
//#endif//EQ_EN
//#endif//(SOUND_MIX_GLOBAL_EN != 0 ||  LINEIN_DIGTAL_EN != 0)
#if 0
        case MSG_KTV_SPI_REC:
            if (spi_rec_flag == 0) {
                printf("rec start ^^^^^^^^^^^\n");
                notice_player_stop();
                linein_digital_enable(1);
                spirec_api_dac();
            } else {
                printf("rec play ~~~~~~~~~~~~~\n");
                spirec_api_dac_stop();
                linein_digital_enable(0);
                spirec_api_play_to_mix(SCRIPT_TASK_NAME, spirec_filenum, sound_mix_api_get_obj(), NULL, NULL);
            }
            spi_rec_flag = !spi_rec_flag;
            break;


        case MSG_KTV_NOTICE_ADD_PLAY_TEST:
            puts("add notice\n");
            wav_play_by_path_to_mix(SCRIPT_TASK_NAME, "/test.wav", NULL, NULL, sound_mix_api_get_obj());
            break;

        ///提示播放结束处理， 录音播放结束也是走这个分支
        case SYS_EVENT_PLAY_SEL_END:
            /* printf("add mix notice end !!\n"); */
            notice_player_stop();
            linein_digital_enable(1);
            break;
#endif
        default:
            /* echo_msg_deal_api((void **)&linein_reverb, msg); */
            break;
        }
#if SUPPORT_APP_RCSP_EN
        rcsp_linein_task_msg_deal_after(msg);
#endif
    }
}


const SCRIPT script_linein_info = {
    .skip_check = script_linein_skip_check,
    .init = script_linein_init,
    .exit = script_linein_exit,
    .task = script_linein_task,
    .key_register = &script_linein_key,
};

