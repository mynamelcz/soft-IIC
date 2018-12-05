#include "script_ms.h"
#include "common/msg.h"
#include "script_ms_key.h"
#include "notice_player.h"
#include "dac/dac_api.h"
#include "ms/ms_app.h"
#include "sound_mix_api.h"
#include "sdk_cfg.h"

#define SCRIPT_MS_DEBUG_ENABLE
#ifdef SCRIPT_MS_DEBUG_ENABLE
#define script_ms_printf	printf
#define script_ms_puts    puts
#else
#define script_ms_printf(...)
#define script_ms_puts(...)
#endif




static tbool script_ms_notice_play_msg_callback(int msg[], void *priv)
{
    if (msg[0] != MSG_HALF_SECOND) {
        script_ms_printf("ms notice play msg = %d\n", msg[0]);
    }
    return false;
}

static NOTICE_PlAYER_ERR script_ms_notice_play(void *parg, const char *path)
{
    NOTICE_PlAYER_ERR err =  notice_player_play_by_path_to_mix(SCRIPT_TASK_NAME, path, script_ms_notice_play_msg_callback, parg, sound_mix_api_get_obj());
    return err;
}

static void *script_ms_init(void *priv)
{
    dac_channel_on(MUSIC_CHANNEL, FADE_ON);
    script_ms_printf("script ms init ok\n");


    return (void *)NULL;
}

static void script_ms_exit(void **hdl)
{
    script_ms_printf("script ms exit ok\n");

#if (SOUND_MIX_GLOBAL_EN)
    ///退出魔音模式開啟全局疊加功能
    sound_mix_api_init(SOUND_MIX_API_DEFUALT_SAMPLERATE);
#endif//SOUND_MIX_GLOBAL_EN

    dac_channel_off(MUSIC_CHANNEL, FADE_ON);
}

static void script_ms_task(void *parg)
{
    int msg[6] = {0};

    NOTICE_PlAYER_ERR n_err;

    u8 aa = 0;

    n_err = script_ms_notice_play(NULL, BPF_MS_MP3);
    if (n_err != NOTICE_PLAYER_NO_ERR && n_err != NOTICE_PLAYER_NO_ERR_WITHOUT_MSG_DEAL) {
        script_ms_printf("ms notice play err!!\n");
    }

#if (SOUND_MIX_GLOBAL_EN)
    ///進入魔音模式關閉全局疊加功能
    sound_mix_api_close();
#endif//SOUND_MIX_GLOBAL_EN

    os_taskq_post(SCRIPT_TASK_NAME, 1, MSG_MS_START); //默认开启
    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();
        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                ms_task_exit();//结束魔音模块
                script_ms_printf("script_ms_del \r");
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;
        case MSG_MS_START:
            ms_task_exit();//结束魔音模块
            if (aa) {
                aa = 0;
            } else {
                aa = 1;
            }
            puts("MSG_MS_START\n");
            if (aa) {
                ms_task_init(160, 25000, 0);    //运行魔音模块
            } else {
                ms_task_init(160, 25000, 1);    //运行魔音模块
            }
            break;
        case MSG_MS_CLOSE:
            puts("MSG_MS_CLOSE\n");
            ms_task_exit();//结束魔音模块
            break;
        default:
            break;
        }
    }
}


const SCRIPT script_ms_info = {
    .skip_check = NULL,
    .init = script_ms_init,
    .exit = script_ms_exit,
    .task = script_ms_task,
    .key_register = &script_ms_key,
};

