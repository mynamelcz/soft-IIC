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
#include "interact_s.h"



///<******************该模式的运行依赖条件***************************>///
///<******************1、蓝牙跑后台**********************************>///
///<******************2、使能ble*************************************>///
///<******************3、使能ble_multi*******************************>///
//


#if ((BT_BACKMODE == 1) && (BLE_MULTI_EN == 1) &&  (BLE_MULTI_GATT_ROLE_CFG == BLE_MULTI_GATT_ROLE_SERVER))
#define SCRIPT_INTERACT_SERVER_DEBUG_ENABLE
#ifdef SCRIPT_INTERACT_SERVER_DEBUG_ENABLE
#define script_interact_server_printf	printf
#else
#define script_interact_server_printf(...)
#endif

///<*****server自定义按键命令测试数据， 主要所有的数据一定不能是局部变量或数组
static u8 send_cmd_tst_tab[10] = {
    0xaa, 0x55, 0xfe, 0xef, 0x89, 0x98, 0x10, 0x01, 0xbc, 0xcb
};


tbool script_interact_server_wait_connect_cbk(void *priv, int msg[])
{
    ///判断msg如果需要打断等待连接建立的， 返回1, 否则返回0
    return false;
}

static void scritpt_interact_server_task_deal(void *parg)
{
    u8 test_send_cmd_cnt = 0;
    INTERACT_S *obj;
__try_connect:
    obj = interact_server_creat(SCRIPT_TASK_NAME);
    ASSERT(obj);

    interact_server_wait_connect(obj, BLE_MULTI_ENABLE_CHL1, script_interact_server_wait_connect_cbk, NULL);

    int msg[6] = {0};
    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();

        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                script_interact_server_printf("script_interact_server_del \n");
                interact_server_destroy(&obj);
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;

        case MSG_INTERACT_DISCONNECT_OK:
            script_interact_server_printf("retry connect again ... fun = %s, line = %d\n", __func__, __LINE__);
            interact_server_destroy(&obj);
            goto __try_connect;
            break;

        case MSG_SPEEX_ENCODE_ERR:
            script_interact_server_printf("encode err !!! fun = %s, line = %d\n", __func__, __LINE__);
            interact_server_destroy(&obj);
            /* ble_interact_server_io.disconnect(); */
            /* goto __try_connect; */
            break;

        case MSG_INTERACT_SEND_CMD:
            /* ble_interact_server_io.send(0[>0表示命令通道， 1表示音频通道， 这里是发送命令<], &send_cmd_tst_tab[test_send_cmd_cnt], 1); */
            script_interact_server_printf("server send cmd ############## 0x%x\n", send_cmd_tst_tab[test_send_cmd_cnt]);
            interact_server_send_cmd(obj, &send_cmd_tst_tab[test_send_cmd_cnt], 1);
            test_send_cmd_cnt++;
            if (test_send_cmd_cnt >= (sizeof(send_cmd_tst_tab) / sizeof(send_cmd_tst_tab[0]))) {
                test_send_cmd_cnt = 0;
            }
            break;

        case MSG_INTERACT_RECIEVE_CMD:
            script_interact_server_printf("server recieve cmd >>>>>>>>>>>> 0x%x\n", msg[1]);
            break;

        case MSG_SPEEX_ENCODE_START:
            script_interact_server_printf("MSG_SPEEX_ENCODE_START\n");
            if (interact_server_speek_encode_start(obj) == true) {
                /* interact_server_reset_le_connect_param(obj, 10, 15); */
            }
            script_interact_server_printf("fun = %s, line = %d\n", __func__, __LINE__);
            break;

        case MSG_SPEEX_ENCODE_STOP:
            script_interact_server_printf("MSG_SPEEX_ENCODE_STOP\n");
            if (interact_server_speek_encode_stop(obj) == true) {
                /* interact_server_reset_le_connect_param(obj, 50, 50); */
            }
            break;
        default:
            break;
        }
    }
}

static void *script_interact_server_init(void *priv)
{
    script_interact_server_printf("script interact_server init ok\n");

    /* dac_channel_on(MUSIC_CHANNEL, FADE_ON); */
    return (void *)NULL;
}


static void script_interact_server_exit(void **hdl)
{
    /* dac_channel_off(MUSIC_CHANNEL, FADE_ON); */
    script_interact_server_printf("script interact_server exit ok\n");
}


const SCRIPT script_interact_info = {
    .skip_check = NULL,
    .init = script_interact_server_init,
    .exit = script_interact_server_exit,
    .task = scritpt_interact_server_task_deal,
    .key_register = &script_interact_key,
};

#endif

