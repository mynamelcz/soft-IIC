#include "script_usb.h"
#include "common/msg.h"
#include "script_usb_key.h"
#include "notice/notice_player.h"
#include "dac/dac.h"
#include "dac/dac_api.h"
#include "usb_device.h"
#include "dev_pc.h"
#include "dev_manage/dev_ctl.h"
#include "sys_detect.h"
#include "rcsp/rcsp_interface.h"
#include "pc_ui.h"
#include "sdk_cfg.h"
#include "sound_mix_api.h"
#ifdef MIO_USB_DEBUG
#include "mio_api/mio_api.h"
#endif


#if USB_PC_EN
#define SCRIPT_HTK_DEBUG_ENABLE
#ifdef SCRIPT_HTK_DEBUG_ENABLE
#define script_usb_printf	printf
#else
#define script_usb_printf(...)
#endif

#undef 	NULL
#define NULL	0

extern u8 usb_spk_vol_value;
extern u8 usb_mic_vol_value;

//extern volatile u32 dac_energy_value;
extern u32 usb_device_unmount(void);
extern DAC_CTL dac_ctl;

static card_reader_status card_status;



static tbool script_usb_notice_play_msg_callback(int msg[], void *priv)
{
    if (msg[0] != MSG_HALF_SECOND) {
        script_usb_printf("usb notice play msg = %d\n", msg[0]);
    }
    ///this is an example
//	if(msg[0] == MSG_MUSIC_PREV_FILE)
//		return true;
    return false;
}



static NOTICE_PlAYER_ERR script_usb_notice_play(const char *path)
{
    NOTICE_PlAYER_ERR n_err;

    n_err = notice_player_play_by_path_to_mix(SCRIPT_TASK_NAME, path, script_usb_notice_play_msg_callback, NULL, sound_mix_api_get_obj());

    return n_err;
}

static u32 script_usb_skip_check(void)
{
    u32 ret = app_usb_slave_online_status();
    script_usb_printf(" usb skip check = %d\n", ret);
    return ret;
}

extern void  dev_fs_close_all(void);
static void *script_usb_init(void *priv)
{
    dev_fs_close_all();


    usb_spk_vol_value = dac_ctl.sys_vol_l;

#if SUPPORT_APP_RCSP_EN
    rcsp_pc_task_sync_vol_init();
#endif



    script_usb_printf("script usb init ok\n");
    dac_channel_on(MUSIC_CHANNEL, FADE_ON);
    return (void *)NULL;
}


static void script_usb_exit(void **hdl)
{
    script_usb_printf("script usb exit ok!!\n");
    /* ui_close_pc(); */
    usb_pc_exit();
    dac_channel_off(MUSIC_CHANNEL, FADE_ON);
}

//pc 模式插入提示音方法
////1、在非mix状态下，usb speaker 插入提示音
//	usb_speaker_output_stop_enable(1);
//	n_err = script_usb_notice_play(BPF_PC_MP3);
//	usb_speaker_output_stop_enable(0);
////2、在mix轻卡下插入提示音，直接调用提示音接口，但是必须保证提示音是pcm wav格式, 否则会卡音

static void script_usb_task(void *parg)
{
    int msg[6] = {0};
    u8 ret;
    u8 pc_ok_flag = 0;
    /* u32 status = 1; */
    NOTICE_PlAYER_ERR n_err = NOTICE_PLAYER_NO_ERR;
    u8 sync_pc_vol_flag = 6;
    u32 pc_mute_status = 0;
    u32 pc_mic_mute_status = 0;
    u32 sys_mute_flag = is_dac_mute();
    usb_mic_vol_value = MAX_SYS_VOL_MIC;
    n_err = script_usb_notice_play(BPF_PC_MP3);
    if (n_err != NOTICE_PLAYER_TASK_DEL_ERR) {
        if (false != usb_pc_init()) {
            pc_ok_flag = 1;
            /* ui_open_pc(NULL, 0); */
        }
    }

    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();
        if (pc_ok_flag == 0) {
            if (msg[0] != SYS_EVENT_DEL_TASK) {
                continue;
            }
        }
#if SUPPORT_APP_RCSP_EN
        rcsp_pc_task_msg_deal_before(msg);
#endif
        switch (msg[0]) {

#ifdef MIO_USB_DEBUG
        case MSG_MIO_START:
            pc_puts("MSG_MIO_START\n");
            music_usb_start();
            break;
        case MSG_MIO_STOP:
            pc_puts("MSG_MIO_STOP\n");
            music_usb_stop();
            break;
        case MSG_MIO_READ:
            pc_puts("MSG_MIO_READ\n");
            music_usb_read();
            break;
#endif

        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
#ifdef MIO_USB_DEBUG
                music_usb_stop();
#endif
                if (pc_mute_status && !sys_mute_flag) {
                    pc_dac_mute(0, FADE_ON);
                }
                script_usb_printf("script_usb_del\r");
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;


        /*************** for AUDIO ******************/
        case MSG_PC_SPK_MUTE:
            pc_puts("MSG_PC_SPK_MUTE\n");
            pc_mute_status = 1;
            pc_dac_mute(1, FADE_ON);
            UI_menu(MENU_REFRESH);
            break;

        case MSG_PC_SPK_UNMUTE:
            pc_puts("MSG_PC_SPK_UNMUTE\n");
            pc_mute_status = 0;
            pc_dac_mute(0, FADE_ON);
            UI_menu(MENU_REFRESH);

        case MSG_PC_SPK_VOL:
//            puts("MSG_PC_SPK_VOL\n");
            usb_spk_vol_value = app_pc_set_speaker_vol(pc_mute_status);
            puts("pc_vol\n");
            if (sync_pc_vol_flag != 1) {
                sync_pc_vol_flag = 1;
            }
            break;

        /*************** for MIC ******************/
        case MSG_PC_MIC_MUTE:
            pc_puts("MSG_PC_MIC_MUTE\n");
            pc_mic_mute_status = 1;
            /* pc_mic_mute(1, FADE_ON); */
            /* UI_menu(MENU_REFRESH); */
            break;

        case MSG_PC_MIC_UNMUTE:
            pc_puts("MSG_PC_SPK_UNMUTE\n");
            pc_mic_mute_status = 0;
        /* pc_mic_mute(0, FADE_ON); */
        /* UI_menu(MENU_REFRESH); */

        case MSG_PC_MIC_VOL:
            //            puts("MSG_PC_MIC_VOL\n");
            usb_mic_vol_value = app_pc_set_mic_vol(pc_mic_mute_status);
            puts("pc_mic_vol\n");
            break;

        /*************** for HID KEY ******************/
        case MSG_PC_MUTE:
            pc_puts("p_m\n");
            app_usb_slave_hid(USB_AUDIO_MUTE);
            /* if(pc_mute_status == 0)
            {
            os_taskq_post_msg(PC_TASK_NAME, 1, MSG_PC_SPK_MUTE);
            }
            else
            {
            os_taskq_post_msg(PC_TASK_NAME, 1, MSG_PC_SPK_MUTE);
            } */
            break;

        case MSG_PC_VOL_DOWN:
            pc_puts("vd\n");
            app_usb_slave_hid(USB_AUDIO_VOLDOWN);
            ///UI_menu(MENU_PC_VOL_DOWN);
            break;

        case MSG_PC_VOL_UP:
            pc_puts("vu\n");
            app_usb_slave_hid(USB_AUDIO_VOLUP);
            ///UI_menu(MENU_PC_VOL_UP);
            break;

        case MSG_PC_PP:
            pc_puts("pp\n");
            app_usb_slave_hid(USB_AUDIO_PP);
            break;

        case MSG_PC_PLAY_NEXT:
            pc_puts("ne\n");
            app_usb_slave_hid(USB_AUDIO_NEXTFILE);
            break;

        case MSG_PC_PLAY_PREV:
            pc_puts("pr\n");
            app_usb_slave_hid(USB_AUDIO_PREFILE);
            break;

        case MSG_LOW_POWER:
            puts("pc_low_power\n");
            ///pc_prompt_play(BPF_LOW_POWER_MP3);
            break;

        /*************** for PC UPDATA ******************/
        case MSG_PC_UPDATA:
            pc_puts("MSG_PC_UPDATA\n");
            dev_updata_mode(usb_device_unmount, PC_UPDATA); //进入PC升级模式
            break;

        case MSG_HALF_SECOND:
//		   pc_puts(" PC_H ");

            /* printf("eng = %d\n",get_dac_energy_value()); */
            if (pc_ok_flag == 0) {
                break;
            }
            if (sync_pc_vol_flag) {
                if (sync_pc_vol_flag > 2) {
                    sync_pc_vol_flag--;
                } else if (sync_pc_vol_flag == 2) {
                    puts("get sync_vol\n");
                    app_usb_slave_hid(USB_AUDIO_VOLUP);
                    sync_pc_vol_flag = 4;
                } else {
                    ;
                }
            }
            break;

        default:
            //pc_puts("pc_default\n");
            break;
        }

#if SUPPORT_APP_RCSP_EN
        rcsp_pc_task_msg_deal_after(msg);
#endif

    }
}


const SCRIPT script_usb_info = {
    .skip_check = script_usb_skip_check,
    .init = script_usb_init,
    .exit = script_usb_exit,
    .task = script_usb_task,
    .key_register = &script_usb_key,
};

#endif
