#include "script_fm.h"
#include "common/msg.h"
#include "script_fm_key.h"
#include "notice/notice_player.h"
#include "dac/dac_api.h"
#include "fm/fm_radio.h"
#include "fm/fm_api.h"
#include "fm_inside.h"
#include "ui/led/led.h"
#include "dac/eq_api.h"
#include "dac/eq.h"
#include "fm_ui.h"
#include "echo/echo_api.h"
#include "dac/dac_api.h"
#include "dac/dac.h"
#include "rcsp/rcsp_interface.h"
#include "sound_mix_api.h"

#define SCRIPT_FM_DEBUG_ENABLE
#ifdef SCRIPT_FM_DEBUG_ENABLE
#define script_fm_printf	printf
#else
#define script_fm_printf(...)
#endif


extern FM_MODE_VAR *fm_mode_var;       ///>FM状态结构体
extern RECORD_OP_API *rec_fm_api;
extern void *fm_reverb;
extern u8 eq_mode;

extern bool fm_test_set_freq(u16 freq);
extern void *tone_number_get_name(u32 number);


static const u16 fm_test_freq_arr[] = {
    875,
    878,
    888,
    898,
    905,
    915,
    918,
    928,
    939,
    951,
    967,
    971,
    980,
    991,
    1007,
    1012,
    1030,
    1038,
    1043,
    1049,
    1071
};







/* typedef struct __SCRIPT_FM_CTRL { */
/* int test; */
/* } SCRIPT_FM_CTRL; */

static tbool script_fm_notice_play_msg_callback(int msg[], void *priv)
{
    if (msg[0] != MSG_HALF_SECOND) {
        script_fm_printf("fm notice play msg = %d\n", msg[0]);
    }
    ///this is an example
//	if(msg[0] == MSG_MUSIC_PREV_FILE)
//		return true;
    return false;
}


static tbool script_fm_notice_play(const char *path)
{
    NOTICE_PlAYER_ERR n_err;

    dac_channel_on(MUSIC_CHANNEL, FADE_ON);
    n_err = notice_player_play_by_path_to_mix(SCRIPT_TASK_NAME, path, script_fm_notice_play_msg_callback, NULL, sound_mix_api_get_obj());
    dac_channel_off(MUSIC_CHANNEL, FADE_ON);
    dac_channel_on(FM_INSI_CHANNEL, FADE_ON);
#if (SOUND_MIX_GLOBAL_EN == 0)
    dac_set_samplerate(FM_DAC_OUT_SAMPLERATE, 1);
#endif
    fm_module_mute(0);
    if (n_err == NOTICE_PLAYER_NO_ERR) {
        return true;
    }
    return false;
}


static void script_fm_frq_play(u16 freq)
{
    void *a, *b, *c, *d;
    NOTICE_PlAYER_ERR n_err;
    fm_module_mute(1);
    clear_dac_buf(1);

    a = tone_number_get_name(freq % 10000 / 1000);
    b = tone_number_get_name(freq % 1000 / 100);
    c = tone_number_get_name(freq % 100 / 10);
    d = tone_number_get_name(freq % 10);

    printf("a:%s	b:%s	c:%s	d:%s\n", a, b, c, d);
    /* if (dac_ctl.toggle == 0) { */
    /* puts("\n\n###########DAC_ON##############\n\n"); */
    /* dac_on_control(); */
    /* } */
    /* dac_channel_on(MUSIC_CHANNEL, FADE_ON); */
    /* set_sys_vol(dac_ctl.sys_vol_l, dac_ctl.sys_vol_r, FADE_ON); */
    /* audio_sfr_dbg(0); */
    if (freq % 10000 / 1000) {
        /* tone_play_by_name(FM_TASK_NAME, 4, a, b, c, d); */
        n_err = notice_player_play_to_mix(SCRIPT_TASK_NAME, script_fm_notice_play_msg_callback, NULL, 0, 0, sound_mix_api_get_obj(), NOTICE_PLAYER_METHOD_BY_PATH, 4, a, b, c, d);
    } else {
        /* tone_play_by_name(FM_TASK_NAME, 3, b, c, d); */
        n_err = notice_player_play_to_mix(SCRIPT_TASK_NAME, script_fm_notice_play_msg_callback, NULL, 0, 0, sound_mix_api_get_obj(), NOTICE_PLAYER_METHOD_BY_PATH, 3, b, c, d);
    }
    if (n_err != NOTICE_PLAYER_NO_ERR) {
        script_fm_printf("fm notice player fail !!\n");
    }
    dac_channel_off(MUSIC_CHANNEL, FADE_ON);
    dac_channel_on(FM_INSI_CHANNEL, FADE_ON);
#if (SOUND_MIX_GLOBAL_EN == 0)
    dac_set_samplerate(FM_DAC_OUT_SAMPLERATE, 1);
#endif
    fm_module_mute(0);
}


static void script_fm_test_freq_fun(void)
{
    static u8 i = 0;

    u8 max = sizeof(fm_test_freq_arr) / sizeof(fm_test_freq_arr[0]);

    if (i >= max) {
        i = 0;
    }

    script_fm_frq_play(fm_test_freq_arr[i]);
    fm_test_set_freq(fm_test_freq_arr[i]);
    i++;
}

static void *script_fm_init(void *priv)
{
#if EQ_EN
    eq_enable();
    eq_mode = get_eq_default_mode();
#endif/*EQ_EN*/
    led_fre_set(15, 0);
    /* fm_arg_open(); */
    script_fm_printf("script fm init ok\n");

    return (void *)NULL;
}

static void script_fm_exit(void **hdl)
{
#if SWITCH_PWR_CONFIG
    extern void fm_ldo_level(u8 level);
    if (get_pwr_config_flag()) {
        fm_ldo_level(FM_LDO_REDUCE_LEVEL);
    }
#endif

#if EQ_EN
    eq_disable();
#endif/*EQ_EN*/
    notice_player_stop();
    SET_UI_UNLOCK();
    //                UI_menu(MENU_WAIT);
    ui_close_fm();
    exit_rec_api(&rec_fm_api); //停止录音和释放资源
    echo_exit_api(&fm_reverb);
    fm_radio_powerdown();
    /* fm_mode_var->scan_mode = FM_UNACTIVE; */

#if (SOUND_MIX_GLOBAL_EN == 0)
    dac_set_samplerate(24000, 1);
#endif
    script_fm_printf("script fm exit ok\n");
}

static void script_fm_task(void *parg)
{
    int msg[6] = {0};
    u32 res = 0;
    u8 fm_init_ok = 0;
    u8 scan_counter = 0;

    NOTICE_PlAYER_ERR n_err = NOTICE_PLAYER_NO_ERR;
    n_err = script_fm_notice_play(BPF_RADIO_MP3);
    if (n_err != NOTICE_PLAYER_TASK_DEL_ERR) {
        fm_arg_open();
        fm_init_ok = fm_radio_init();
        if (fm_init_ok) {
            fm_mode_var->fm_rec_op = &rec_fm_api;
            ui_open_fm(&fm_mode_var, sizeof(FM_MODE_VAR **));
        } else {
            script_fm_printf("fm radio init fail!!!\n");
        }
    }

    /* script_fm_test_freq_fun(); */
    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();
        if ((fm_init_ok == 0) || (n_err == NOTICE_PLAYER_TASK_DEL_ERR)) {
            if (msg[0] != SYS_EVENT_DEL_TASK) {
                continue;
            }
        }
        if (!fm_msg_filter(msg[0])) { //根据FM状态，过滤消息
            continue;
        }
#if FM_REC_EN
        if (!fm_rec_msg_filter(rec_fm_api, msg[0])) { //根据FM录音状态，过滤消息
            continue;
        }
#endif
#if SUPPORT_APP_RCSP_EN
        rcsp_fm_task_msg_deal_before(msg);
#endif
        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                script_fm_printf("script_fm_del \n");
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;
        case  MSG_FM_SCAN_ALL_INIT:
            fm_printf("MSG_FM_SCAN_ALL_INIT\n");
            led_fre_set(5, 0);
            if (fm_mode_var->scan_mode == FM_SCAN_STOP) {
                fm_module_mute(1);
                os_time_dly(50);
                fm_info->dat[FM_CHAN] = 0;
                fm_info->dat[FM_FRE] = 0;
                clear_all_fm_point();
                fm_mode_var->wTotalChannel = 0;
                fm_mode_var->wFreChannel = 0;
                fm_mode_var->wFreq = MIN_FRE - 1;//自动搜索从最低的频点开始
                scan_counter = MAX_CHANNL;
                fm_mode_var->scan_mode = FM_SCAN_ALL;
            } else {
                scan_counter = 1;//再搜索一个频点就停止
            }

        case  MSG_FM_SCAN_ALL:
            script_fm_printf("MSG_FM_SCAN_ALL\n");
            if (fm_radio_scan(fm_mode_var->scan_mode)) {
                if (fm_mode_var->scan_mode == FM_SCAN_ALL) {
#if SUPPORT_APP_RCSP_EN
                    rcsp_fm_scan_channel_deal(msg);
#endif
                    /// Wait one second
                    res = 10;
                    while (res) {
                        os_time_dly(10);
                        res--;
                    }
                } else {
                    fm_mode_var->scan_mode = FM_SCAN_STOP;
                    led_fre_set(15, 0);
                    SET_UI_UNLOCK();
                    UI_menu(MENU_REFRESH);
                    break;
                }
            }
            scan_counter--;
            if (scan_counter == 0) {
                if (fm_mode_var->scan_mode == FM_SCAN_ALL) {               //全频点搜索结束，播放第一个台
                    os_taskq_post_msg(SCRIPT_TASK_NAME, 1, MSG_FM_NEXT_STATION);
                    fm_mode_var->scan_mode = FM_SCAN_STOP;
                    fm_printf("FM_SCAN_OVER \n");
                } else {                        //半自动搜索，播放当前频点
                    fm_mode_var->scan_mode = FM_SCAN_STOP;
                    fm_module_mute(0);

                    UI_menu(MENU_REFRESH);
                }
                led_fre_set(15, 0);
                SET_UI_UNLOCK();
            } else {   //搜索过程
                os_time_dly(1);
                if (fm_mode_var->scan_mode != FM_SCAN_STOP) {
                    os_sched_lock();
                    res = os_taskq_post_msg(SCRIPT_TASK_NAME, 1, MSG_FM_SCAN_ALL);
                    if (res == OS_Q_FULL) {
                        os_taskq_flush_name(SCRIPT_TASK_NAME);
                        os_taskq_post_msg(SCRIPT_TASK_NAME, 1, MSG_FM_SCAN_ALL);
                    }
                    os_sched_unlock();
                }
            }


            break;

        case MSG_FM_SCAN_ALL_DOWN:
            fm_printf("MSG_FM_SCAN_ALL_DOWN\n");
            /* fm_mode_var->scan_mode = FM_SCAN_NEXT; */
            fm_mode_var->scan_mode = FM_SCAN_PREV;
            os_taskq_post(SCRIPT_TASK_NAME, 1, MSG_FM_SCAN_ALL);
            break;

        case MSG_FM_SCAN_ALL_UP:
            fm_printf("MSG_FM_SCAN_ALL_UP\n");
            /* fm_mode_var->scan_mode = FM_SCAN_PREV; */
            fm_mode_var->scan_mode = FM_SCAN_NEXT;
            os_taskq_post(SCRIPT_TASK_NAME, 1, MSG_FM_SCAN_ALL);
            break;

        case  MSG_FM_PREV_STEP:
            fm_printf("MSG_FM_PREV_STEP\n");
            res = fm_module_set_fre(FM_FRE_DEC);
            if (res) {
                save_fm_point(fm_mode_var->wFreq - MIN_FRE);
                fm_mode_var->wTotalChannel = get_total_mem_channel();
            }
            fm_mode_var->wFreChannel = get_channel_via_fre(fm_mode_var->wFreq - MIN_FRE);
            fm_info->dat[FM_FRE] = fm_mode_var->wFreq - MIN_FRE;
            fm_info->dat[FM_CHAN] = fm_mode_var->wFreChannel;
            fm_save_info();
            fm_module_mute(0);
            if (!res) {
                fm_mode_var->wFreChannel = 0;
            }
            UI_menu(MENU_FM_MAIN);
            break;

        case MSG_FM_NEXT_STEP:
            fm_printf("MSG_FM_NEXT_STEP\n");
            res = fm_module_set_fre(FM_FRE_INC);
            if (res) {
                save_fm_point(fm_mode_var->wFreq - MIN_FRE);
                fm_mode_var->wTotalChannel = get_total_mem_channel();
            }
            fm_mode_var->wFreChannel = get_channel_via_fre(fm_mode_var->wFreq - MIN_FRE);
            fm_info->dat[FM_FRE] = fm_mode_var->wFreq - MIN_FRE;
            fm_info->dat[FM_CHAN] = fm_mode_var->wFreChannel;
            fm_save_info();
            fm_module_mute(0);
            if (!res) {
                fm_mode_var->wFreChannel = 0;
            }
            UI_menu(MENU_FM_MAIN);
            break;

        case MSG_FM_PREV_STATION:
            fm_printf("MSG_FM_PREV_STATION\n");

            if (fm_mode_var->wTotalChannel == 0) {
                break;
            }

            fm_mode_var->wFreChannel -= 2;

        case MSG_FM_NEXT_STATION:
            fm_printf("MSG_FM_NEXT_STATION\n");

            if (fm_mode_var->wTotalChannel == 0) {
                break;
            }

            fm_mode_var->wFreChannel++;
            set_fm_channel();
            break;

        case MSG_FM_SELECT_CHANNEL:
            fm_mode_var->wFreChannel = msg[1];
            set_fm_channel();
            break;

        case MSG_FM_SELECT_FREQ:
            fm_mode_var->wFreq = msg[1];
            fm_mode_var->wFreChannel = get_channel_via_fre(fm_mode_var->wFreq - MIN_FRE);
            fm_module_set_fre(FM_CUR_FRE);
            fm_module_mute(0);
            break;

        case MSG_FM_PP:
            /* script_fm_test_freq_fun(); //for fm_freq_debug */
            /* break; */
            /* fm_msg_printf(); //for fm_scan_debug */
            /* break; */
#if UI_ENABLE
            if (UI_var.bCurMenu == MENU_INPUT_NUMBER) { //暂停和播放
                os_taskq_post(SCRIPT_TASK_NAME, 1, MSG_INPUT_TIMEOUT);
                break;
            }
#endif
            {
                if (fm_mode_var->fm_mute == 0) {
                    fm_module_mute(1);
                } else {
                    fm_module_mute(0);
                }
            }

            break;

        case MSG_FM_DEL_CHANNEL:
            del_fm_channel((u8)msg[1]);
            break;

        case MSG_FM_SAVE_CHANNEL:
            save_fm_channel((u16)msg[1]);
            break;

        case MSG_MUSIC_PP:
#if UI_ENABLE
            if (UI_var.bCurMenu == MENU_INPUT_NUMBER) { //暂停和播放
                os_taskq_post(SCRIPT_TASK_NAME, 1, MSG_INPUT_TIMEOUT);
                break;
            }
#endif
            break;

        case MSG_INPUT_TIMEOUT:
            /*由红外界面返回*/
            if (input_number <= MAX_CHANNL) {						//输入的是台号
                msg[0] = get_fre_via_channle(input_number);
                if (msg[0] != 0xff) {
                    fm_mode_var->wFreq = msg[0] + MIN_FRE;
                    fm_mode_var->wFreChannel = input_number;
                    if (fm_mode_var->fm_mute == 0) {
                        fm_module_set_fre(FM_CUR_FRE);
                        fm_module_mute(0);
                    } else {
                        fm_module_set_fre(FM_CUR_FRE);
                        fm_module_mute(1);
                    }
                    UI_menu(MENU_FM_DISP_FRE);
                }
            } else if ((input_number >= MIN_FRE) && (input_number <= MAX_FRE)) { //输入的是频点
                fm_mode_var->wFreq = input_number;
                fm_mode_var->wFreChannel = get_channel_via_fre(fm_mode_var->wFreq - MIN_FRE);
                if (fm_mode_var->fm_mute == 0) {
                    fm_module_set_fre(FM_CUR_FRE);
                    fm_module_mute(0);
                } else {
                    fm_module_set_fre(FM_CUR_FRE);
                    fm_module_mute(1);
                }

            }
            input_number = 0;
            input_number_cnt = 0;
            UI_menu(MENU_FM_DISP_FRE);
            break;

        case MSG_PROMPT_PLAY:
        case MSG_LOW_POWER:
            puts("fm_low_power\n");
            script_fm_notice_play("test.mp3");
            /* fm_prompt_play(BPF_LOW_POWER_MP3); */
            break;

#if LCD_SUPPORT_MENU
        case MSG_ENTER_MENULIST:
            UI_menu_arg(MENU_LIST_DISPLAY, UI_MENU_LIST_ITEM);
            break;
#endif

        case MSG_HALF_SECOND:
            /* get_fm_stereo(); */
#if FM_REC_EN
            if (updata_enc_time(rec_fm_api)) {
                UI_menu(MENU_HALF_SEC_REFRESH);
            }
#endif

            break;

#if EQ_EN
        case MSG_MUSIC_EQ: {
            eq_mode++;
            if (eq_mode > 5) {
                eq_mode = 0;
            }
            eq_mode_sw(eq_mode);
            printf("MSG_FM_EQ %d, \n ", eq_mode);
        }
        break;
#endif/*EQ_EN*/
        default:
            break;
        }
#if SUPPORT_APP_RCSP_EN
        rcsp_fm_task_msg_deal_after(msg);
#endif
    }
}


const SCRIPT script_fm_info = {
    .skip_check = NULL,
    .init = script_fm_init,
    .exit = script_fm_exit,
    .task = script_fm_task,
    .key_register = &script_fm_key,
};

