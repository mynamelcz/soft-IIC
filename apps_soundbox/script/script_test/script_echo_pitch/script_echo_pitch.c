#include "script_echo_pitch.h"
#include "common/app_cfg.h"
#include "common/msg.h"
#include "script_echo_pitch_key.h"
#include "notice_player.h"
#include "dac/dac_api.h"
#include "cbuf/circular_buf.h"
#include "echo/echo_api.h"
#include "echo/echo_deal.h"

#define SCRIPT_ECHO_PITCH_DEBUG_ENABLE
#ifdef SCRIPT_ECHO_PITCH_DEBUG_ENABLE
#define script_echo_pitch_printf	printf
#else
#define script_echo_pitch_printf(...)
#endif




static void *script_ehco_pitch_init(void *priv)
{
    dac_channel_on(MUSIC_CHANNEL, FADE_ON);
    script_echo_pitch_printf("script echo_pitch init ok\n");

    return (void *)NULL;
}

static void script_ehco_pitch_exit(void **hdl)
{
    script_echo_pitch_printf("script echo_pitch exit ok\n");

    dac_channel_off(MUSIC_CHANNEL, FADE_ON);
}

static void script_ehco_pitch_task(void *parg)
{
    int msg[6] = {0};

    REVERB_API_STRUCT *demo_reverb = NULL;

    script_echo_pitch_printf("----------------------%s test enter ----------------------\n", __func__);
    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();
        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                script_echo_pitch_printf("script_test_del \r");
                echo_exit_api((void **)&demo_reverb);
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;
        default:
            echo_msg_deal_api((void **)&demo_reverb, msg);
            break;
        }
    }
}

const SCRIPT script_echo_pitch_info = {
    .skip_check = NULL,
    .init = script_ehco_pitch_init,
    .exit = script_ehco_pitch_exit,
    .task = script_ehco_pitch_task,
    .key_register = &script_echo_pitch_key,
};


