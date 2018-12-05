#include "script_bt.h"
#include "common/msg.h"
#include "script_bt_key.h"
#include "rtos/task_manage.h"
#include "a2dp_user.h"
#include "esco_user.h"
#include "bluetooth/avctp_user.h"
#include "sound_mix_api.h"

extern void esco_output_before_callback_register(void (*cbk)(BT_ESCO_DATA_STREAM *stream, void *priv), void *priv);
extern void esco_output_after_callback_register(void (*cbk)(BT_ESCO_DATA_STREAM *stream, void *priv), void *priv);
extern void a2dp_media_decode_before_callback_register(void (*cbk)(DEC_API_IO *dec_io, void *priv), void *priv);
extern void a2dp_media_decode_after_callback_register(void (*cbk)(DEC_API_IO *dec_io, void *priv), void *priv);
extern TASK_REGISTER(btstack_task_info);
static void *script_bt_init(void *priv)
{
    printf("script bt init ok\n");
    a2dp_media_decode_before_callback_register(a2dp_decode_before_callback, NULL);//(void *)(sound_mix_api_get_obj()));
    a2dp_media_decode_after_callback_register(a2dp_decode_after_callback, NULL);
    esco_output_before_callback_register(esco_decode_before_callback, NULL);//(void *)(sound_mix_api_get_obj()));
    esco_output_after_callback_register(esco_decode_after_callback, NULL);
    btstack_task_info.init(0);
    return priv;
}

static void script_bt_exit(void **hdl)
{
    printf("script bt exit ok\n");
    btstack_task_info.exit();
}
#if 0
static void script_bt_task(void *parg)
{
    int msg[6] = {0};

    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();
        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;
        default:
            break;
        }
    };

}
#endif

const SCRIPT script_bt_info = {
    .skip_check = NULL,
    .init = script_bt_init,
    .exit = script_bt_exit,
    .task = NULL,//script_bt_task,
    .key_register = NULL,//&script_bt_key,
};

