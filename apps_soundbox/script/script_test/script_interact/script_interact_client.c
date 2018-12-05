#include "script_interact.h"
#include "common/app_cfg.h"
#include "common/msg.h"
#include "script_interact_key.h"
#include "notice_player.h"
#include "dac/dac_api.h"
#include "cbuf/circular_buf.h"
#include "speex_encode.h"
#include "file_operate/file_op.h"
#include "fat/fs_io.h"
#include "fat/tff.h"
#include "speex_encode.h"
#include "speex_decode.h"
#include "interact_c.h"

///<******************该模式的运行依赖条件***************************>///
///<******************1、蓝牙跑后台**********************************>///
///<******************2、使能ble*************************************>///
///<******************3、使能ble_multi*******************************>///
//

#if ((BT_BACKMODE == 1) && (BLE_MULTI_EN == 1) &&  (BLE_MULTI_GATT_ROLE_CFG == BLE_MULTI_GATT_ROLE_CLIENT))
#define SCRIPT_INTERACT_CLIENT_DEBUG_ENABLE
#ifdef SCRIPT_INTERACT_CLIENT_DEBUG_ENABLE
#define script_interact_client_printf	printf
#else
#define script_interact_client_printf(...)
#endif


tbool script_interact_client_wait_connect_cbk(void *priv, int msg[])
{
    ///判断msg如果需要打断等待连接建立的， 返回1, 否则返回0
    return false;
}

/*----------------------------------------------------------------------------*/
/**@brief  语音互动client主循环消息处理
   @param
   @return
   @note 提供给client端使用
*/
/*----------------------------------------------------------------------------*/
void script_interact_client_task_deal(void *parg)
{
    int msg[6] = {0};
    INTERACT_C *obj = NULL;

__try_connect:
    obj = interact_client_creat(SCRIPT_TASK_NAME);
    ASSERT(obj);

    interact_c_wait_connect(obj, (void *)script_interact_client_wait_connect_cbk, NULL);

    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();

        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                script_interact_client_printf("script_interact_client_del \n");
                interact_client_destroy(&obj);
                /* speex_decode_api_stop(&dec); */
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;

        case MSG_INTERACT_DISCONNECT_OK:
            script_interact_client_printf("retry connect again ... fun = %s, line = %d\n", __func__, __LINE__);
            interact_client_destroy(&obj);
            goto __try_connect;
            break;


        case MSG_INTERACT_RECIEVE_CMD:
            script_interact_client_printf("client recieve cmd >>>>>>>>>>>> 0x%x\n", msg[1]);
            break;

        default:
            break;
        }
    }
}

static void *script_interact_client_init(void *priv)
{
    script_interact_client_printf("script interact_client init ok\n");

    dac_channel_on(MUSIC_CHANNEL, FADE_ON);
    return (void *)NULL;
}


static void script_interact_client_exit(void **hdl)
{
    dac_channel_off(MUSIC_CHANNEL, FADE_ON);
    script_interact_client_printf("script interact_client exit ok\n");
}

const SCRIPT script_interact_info = {
    .skip_check = NULL,
    .init = script_interact_client_init,
    .exit = script_interact_client_exit,
    .task = script_interact_client_task_deal,
    .key_register = &script_interact_key,
};

#endif//((BT_BACKMODE == 1) && (BLE_MULTI_EN == 1) &&  (BLE_MULTI_GATT_ROLE_CFG == BLE_MULTI_GATT_ROLE_CLIENT))

