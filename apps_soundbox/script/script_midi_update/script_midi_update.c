#include "script_midi_update.h"
#include "midi_update.h"
#include "common/app_cfg.h"
#include "common/msg.h"
#include "script_midi_update_key.h"
#include "notice_player.h"
#include "dac/dac_api.h"
#include "cbuf/circular_buf.h"
#include "bluetooth/avctp_user.h"
/* #include "bluetooth/le_server_module.h" */

///**************************this is a empty script midi_update*******************************///

#if SCRIPT_MIDI_UPDATE_EN

#define SCRIPT_MIDI_UPDATE_DEBUG_ENABLE
#ifdef SCRIPT_MIDI_UPDATE_DEBUG_ENABLE
#define script_midi_update_printf	printf
#else
#define script_midi_update_printf(...)
#endif

extern void ble_server_disconnect(void);
extern void server_advertise_disable(void);
extern void server_advertise_enable(void);
extern void bt_disconnect_page_scan_disable(u8 flag);

static u32 script_midi_update_skip_check(void)
{
    return midi_update_check();
}

static void *script_midi_update_init(void *priv)
{
    script_midi_update_printf("script midi_update init ok\n");
    script_midi_update_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
    bt_disconnect_page_scan_disable(1);
    user_send_cmd_prepare(USER_CTRL_DISCONNECTION_HCI, 0, NULL);
    user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_DISABLE, 0, NULL);
    user_send_cmd_prepare(USER_CTRL_WRITE_CONN_DISABLE, 0, NULL);

    return (void *)NULL;
}

static void script_midi_update_exit(void **hdl)
{
    user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_ENABLE, 0, NULL);
    user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
    bt_disconnect_page_scan_disable(0);
    script_midi_update_printf("script midi_update exit ok\n");
}

static void script_midi_update_task(void *parg)
{
    int msg[6] = {0};
    u8 *update_buf = NULL;
    u32 update_len = 0;
    SCRIPT_ID_TYPE last_script;

    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();
        switch (msg[0]) {
        case MSG_MODULE_MIDI_UPDATE_TO_FLASH:
            update_buf = (u8 *)msg[1];
            update_len = (u32)msg[2];

            /* ble_server_disconnect(); */
            /* OSTimeDly(2); */
            /* server_advertise_disable(); */
            /* OSTimeDly(2); */

            if (midi_update_to_flash(update_buf, update_len) == true) {
                script_midi_update_printf("script midi_update succ\n");
            } else {
                script_midi_update_printf("script midi_update fail\n");
            }
            midi_update_stop();
        /* server_advertise_enable(); */

        case MSG_MODULE_MIDI_UPDATE_DISCONNECT:
            last_script = script_get_last_id();
            if ((last_script >= SCRIPT_ID_MAX) || (last_script == SCRIPT_ID_MIDI_UPDATE)) {
                last_script = 0;
            }
            script_midi_update_printf("return to last script, last script = %d\n", last_script);
            os_taskq_post(MAIN_TASK_NAME, 2, SYS_EVENT_TASK_RUN_REQ, last_script);
            break;

        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                script_midi_update_printf("script_midi_update_del \r");
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;
        default:
            break;
        }
    }
}

const SCRIPT script_midi_update_info = {
    .skip_check = script_midi_update_skip_check,
    .init = script_midi_update_init,
    .exit = script_midi_update_exit,
    .task = script_midi_update_task,
    .key_register = &script_midi_update_key,
};

#endif
