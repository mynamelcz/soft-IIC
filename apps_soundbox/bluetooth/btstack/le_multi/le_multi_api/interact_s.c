#include "interact_s.h"
#include "key_drv/key.h"
#include "common/app_cfg.h"
#include "common/msg.h"
#include "notice_player.h"
#include "dac/dac_api.h"
#include "cbuf/circular_buf.h"
#include "speex_encode.h"
#include "file_operate/file_op.h"
#include "fat/fs_io.h"
#include "fat/tff.h"
#include "speex_encode.h"
#include "speex_decode.h"
#include "le_multi_server.h"
#include "sys_detect.h"
#include "cpu/power_timer_hw.h"

#if ((BLE_MULTI_EN == 1) &&  (BLE_MULTI_GATT_ROLE_CFG == BLE_MULTI_GATT_ROLE_SERVER))
#define INTERACT_S_DEBUG_ENABLE
#ifdef INTERACT_S_DEBUG_ENABLE
#define interact_s_printf	printf
#else
#define interact_s_printf(...)
#endif//INTERACT_S_DEBUG_ENABLE

/* #define BLE_MULTI_CHL_CFG		(BLE_MULTI_ENABLE_CHL0 | BLE_MULTI_ENABLE_CHL1)///使能命令和語音發送通道 */
//#define BLE_MULTI_CHL_CFG		(BLE_MULTI_ENABLE_CHL0)///使能命名發送通道
/* #define BLE_MULTI_CHL_CFG		(BLE_MULTI_ENABLE_CHL1)///使能語音數據發送通道 */


#define SPEEX_ENCODE_TASK_NAME 					"SpeexEnTask"
#define SPEEX_ENCODE_WRITE_TASK_NAME 			"SpeexWrTask"
#define INTERACT_S_TASK_NAME		 			"InteractSTask"

typedef struct __BLE_INTERACT_SERVER {
    bool (*connect)(void);
    bool (*disconnect)(void);
    void (*update_param)(u16 interval_min, u16 interval_max, u16 conn_latency, u16 supervision_timeout);
    bool (*check_init)(void);
    u32(*check_connect)(void);
    void (*regiest)(ble_chl_recieve_cbk_t recv_cbk, ble_md_msg_cbk_t msg_cbk, void *priv);
    int (*send)(u16 chl, u8 *buf, u16 len);
} BLE_INTERACT_SERVER;

struct __INTERACT_S {
    void 		 			*father_name;
    SPEEX_ENCODE 			*encode;
    BLE_INTERACT_SERVER 	*le_io;
} ;


const BLE_INTERACT_SERVER ble_interact_server_io = {
    .connect = le_multi_server_creat_connect,
    .disconnect = le_multi_server_dis_connect,
    .update_param = le_multi_server_regiest_update_connect_param,
    .check_init = le_multi_server_check_init_complete,
    .check_connect = le_multi_server_check_connect_complete,
    .regiest = le_multi_server_regiest_callback,
    .send = le_multi_server_data_send,
};



///<*****server自定义按键命令测试数据， 主要所有的数据一定不能是局部变量或数组
static u8 server_send_cmd_tst_tab[10] = {
    0xaa, 0x55, 0xfe, 0xef, 0x89, 0x98, 0x10, 0x01, 0xbc, 0xcb
};

/*----------------------------------------------------------------------------*/
/**@brief  语音互动等待连接接口
   @param
   @return
   @note server client均可以使用
*/
/*----------------------------------------------------------------------------*/
tbool interact_server_wait_connect(INTERACT_S *obj, u32 con_enable,  tbool(*msg_cb)(void *priv, int msg[]), void *priv)
{
    u8 ret;
    int msg[6];

    if (obj == NULL ||  obj->le_io == NULL ||  obj->le_io->connect == NULL || obj->le_io->check_connect == NULL || obj->le_io->check_init == NULL) {
        return false;
    }

    /* while (obj->le_io->check_init() == false) { */
    /* clear_wdt(); */
    /* OSTimeDly(1); */
    /* } */

    obj->le_io->connect();

    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));

        ret = os_taskq_pend_no_clean(10, ARRAY_SIZE(msg), msg);
        clear_wdt();
        if (ret == OS_TIMEOUT) {
            /* interact_s_printf("waiting ...\n"); */
            if (obj->le_io->check_connect() == con_enable) {
                interact_s_printf("connect ok !!!\n");
                return true;
            }
            continue;
        }
        /* printf("interact s msg = %x \n", msg[0]); */
        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            return false;

        case MSG_INTERACT_CONNECT_OK:
            os_taskq_msg_clean(msg[0], 1);
            if (obj->le_io->check_connect() == con_enable) {
                interact_s_printf("connect ok xxxx !!!\n");
                return true;
            }
            break;
        case MSG_INTERACT_DISCONNECT_OK:
            os_taskq_msg_clean(msg[0], 1);
            interact_s_printf("retry connect again ... fun = %s, line = %d\n", __func__, __LINE__);
            obj->le_io->connect();
            break;
        default:
            if (msg_cb) {
                if (msg_cb(priv, msg) == true) {
                    return false;
                } else {
                    os_taskq_msg_clean(msg[0], 1);
                    continue;
                }
            } else {
                os_taskq_msg_clean(msg[0], 1);
            }
            break;
        }

    }

    interact_s_printf("connect fail !!!\n");
    return false;
}


/*----------------------------------------------------------------------------*/
/**@brief  语音互动server接收按键命令回调接口
   @param
   @return
   @note 提供给server端使用
*/
/*----------------------------------------------------------------------------*/
static void ble_server_reciev_callback(void *priv, u16 channel, u8 *buffer, u16 buffer_size)
{
    INTERACT_S *obj = (INTERACT_S *)priv;
    ASSERT(obj);
    if (channel == 0) {
        interact_s_printf("recive cmd = 0x%x, fun = %s, line = %d\n", buffer[0], __func__, __LINE__);
        os_taskq_post(obj->father_name, 2, MSG_INTERACT_RECIEVE_CMD, buffer[0]);
    } else if (channel == 1) {

    }
}

/*----------------------------------------------------------------------------*/
/**@brief  语音互动server连接状态回调
   @param
   @return
   @note 提供给server端使用
*/
/*----------------------------------------------------------------------------*/
static void ble_server_md_msg_callback(void *priv, u16 msg, u8 *buffer, u16 buffer_size)
{
    INTERACT_S *obj = (INTERACT_S *)priv;
    interact_s_printf("md msg = 0x%x\n", msg);
    switch (msg) {
    case BTSTACK_LE_MSG_CONNECT_COMPLETE:
        os_taskq_post(obj->father_name, 1, MSG_INTERACT_CONNECT_OK); //默认开启
        break;
    case BTSTACK_LE_MSG_DISCONNECT_COMPLETE:
        os_taskq_post(obj->father_name, 1, MSG_INTERACT_DISCONNECT_OK); //默认开启
        break;
    default:
        break;

    }
}




/*----------------------------------------------------------------------------*/
/**@brief  语音互动录音写接口
   @param
   @return
   @note 提供给server端使用
*/
/*----------------------------------------------------------------------------*/
static u16 speex_encode_write(void *hdl, u8 *buf, u16 len)
{
    /* printf("w"); */
    int ret = -1;
    BLE_INTERACT_SERVER *le_io = (BLE_INTERACT_SERVER *)hdl;
    if (le_io == NULL || le_io->send == NULL) {
        return 0;
    }

    ret = le_io->send(1, buf, len);

    if (ret) {
        printf("f");
        return 0;
    }
    return len;
}



/*----------------------------------------------------------------------------*/
/**@brief  语音互动录音接口io配置
   @param
   @return
   @note 提供给server端使用
   @note 这里主要是使用了ble的发送作为录音的写设备接口, 如果是需要写设备，调用相应的fs_write， fs_seek等相关即可欧
*/
/*----------------------------------------------------------------------------*/
static const SPEEX_FILE_IO speex_encode_file_interface  = {
    .seek = NULL,//speex_encode_seek,
    .read = NULL,//speex_encode_read,
    .write = speex_encode_write,//fs_write,//
};


/*----------------------------------------------------------------------------*/
/**@brief  语音互动server录音退出及资源释放
   @param
   @return
   @note 提供给server端使用
*/
/*----------------------------------------------------------------------------*/
void interact_server_destroy(INTERACT_S **hdl)
{
    if ((hdl == NULL) || (*hdl == NULL)) {
        return;
    }

    INTERACT_S *obj = *hdl;

    if (obj->le_io != NULL && obj->le_io->disconnect != NULL) {
        obj->le_io->disconnect();
    }

    speex_encode_destroy(&(obj->encode));

    printf("fun = %s, line = %d !!!\n", __func__, __LINE__);

    free(obj);
    *hdl = NULL;
}


/*----------------------------------------------------------------------------*/
/**@brief  语音互动server录音初始化
   @param
   @return
   @note 提供给server端使用
*/
/*----------------------------------------------------------------------------*/
INTERACT_S *interact_server_creat(void *father_name)
{
    tbool ret = false;
    u32 f_ret;
    INTERACT_S *obj = (INTERACT_S *)calloc(1, sizeof(INTERACT_S));
    ASSERT(obj);

    obj->encode = speex_encode_creat();
    ASSERT(obj->encode);

    obj->father_name = father_name;
    obj->le_io = (BLE_INTERACT_SERVER *)&ble_interact_server_io;

    __speex_encode_set_father_name(obj->encode, obj->father_name);
    __speex_encode_set_samplerate(obj->encode, 16000L);
    __speex_encode_set_file_io(obj->encode, (SPEEX_FILE_IO *)&speex_encode_file_interface, (void *)obj->le_io);

    ret = speex_encode_process(obj->encode, SPEEX_ENCODE_TASK_NAME, TaskEncRunPrio);
    if (ret == true) {
        ret = speex_encode_write_process(obj->encode, SPEEX_ENCODE_WRITE_TASK_NAME, TaskEncWFPrio);
        if (ret == true) {
            interact_s_printf("interact_server encode is aready ok !!\n");
            /* speex_encode_start(obj->encode); */
        } else {
            interact_server_destroy(&obj);
            return NULL;
        }
    } else {
        interact_server_destroy(&obj);
        return NULL;
    }

    if (obj->le_io != NULL && obj->le_io->regiest != NULL) {
        obj->le_io->regiest(ble_server_reciev_callback, ble_server_md_msg_callback, obj);
    }

    while (obj->le_io->check_init() == false) {
        clear_wdt();
        OSTimeDly(1);
    }

    return obj;
}


tbool interact_server_speek_encode_start(INTERACT_S *obj)
{
    if (obj == NULL || obj->encode == NULL) {
        return false;
    }
    return speex_encode_start(obj->encode);
}


tbool interact_server_speek_encode_stop(INTERACT_S *obj)
{
    if (obj == NULL || obj->encode == NULL) {
        return false;
    }
    return speex_encode_stop(obj->encode);
}

///这里的cmd缓存必须是全局数组
tbool interact_server_send_cmd(INTERACT_S *obj, u8 cmd[], u32 cmd_size)
{
    if (obj == NULL || obj->le_io == NULL || obj->le_io->send == NULL) {
        return false;
    }

    obj->le_io->send(0/*0表示命令通道， 1表示音频通道， 这里是发送命令*/, cmd, cmd_size);

    return true;
}

tbool interact_server_reset_le_connect_param(INTERACT_S *obj, u16 interval_min, u16 interval_max)
{
    if (obj == NULL || obj->le_io == NULL || obj->le_io->update_param == NULL) {
        return false;
    }
    obj->le_io->update_param(interval_min, interval_max, 0, 550);
    return true;
}

static volatile u8 update_param_flag = 0;

u8 interact_server_check_update_param_st(void)
{
    return update_param_flag;
}

void interact_server_set_update_param_st(u8 flag)
{
    update_param_flag = flag;
}


static tbool interact_server_wait_update_param_ok(INTERACT_S *obj, u16 interval_min, u16 interval_max, u32 n100ms)
{
    int msg[6];
    u32 cnt = 0;
    update_param_flag = 1;
    tbool ret = interact_server_reset_le_connect_param(obj, interval_min, interval_max);
    if (ret == false) {
        update_param_flag = 0;
        return false;
    }

    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));

        ret = os_taskq_pend_no_clean(10, ARRAY_SIZE(msg), msg);
        clear_wdt();
        if (ret == OS_TIMEOUT) {
            if (update_param_flag == 0) {
                return true;
            }

            cnt ++;
            if (cnt >= n100ms) {
                update_param_flag = 0;
                interact_s_printf("wait_update_param timeout !!\n");
                return true;
            }
        }

        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
        case MSG_INTERACT_DISCONNECT_OK:
            return false;
        case MSG_INTERACT_RECIEVE_CMD:
            os_taskq_msg_clean(msg[0], 1);
            break;
        default:
            os_taskq_msg_clean(msg[0], 1);
            break;

        }
    }

    return false;
}



static volatile u8 speex_encode_flag = 0;
u8 speex_encode_status(void)
{
    return speex_encode_flag;
}

//static volatile u32 tst_send_start_msg_cnt = 0;
//static volatile u8 tst_send_start_msg_flag = 0;
//static volatile u32 tst_send_stop_msg_cnt = 0;
//static volatile u8 tst_send_stop_msg_flag = 0;
//static volatile u8 scan_ready = 0;
//void test_msg_scan(void)
//{
//	if(scan_ready == 0)
//	{
//		tst_send_start_msg_cnt = 0;
//		tst_send_start_msg_flag = 0;
//		tst_send_stop_msg_cnt = 0;
//		tst_send_stop_msg_flag = 0;
//		return;
//	}
//	if(tst_send_start_msg_flag)
//	{
//		tst_send_start_msg_cnt++;
//		if(tst_send_start_msg_cnt > 6)
//		{
//			tst_send_start_msg_cnt = 0;
//			tst_send_start_msg_flag = 0;
//			tst_send_stop_msg_flag = 1;
//			os_taskq_post(INTERACT_S_TASK_NAME, 1, MSG_SPEEX_ENCODE_START);
//		}
//	}
//
//	if(tst_send_stop_msg_flag)
//	{
//		tst_send_stop_msg_cnt++;
//		if(tst_send_stop_msg_cnt > 3)
//		{
//			tst_send_stop_msg_cnt = 0;
//			tst_send_stop_msg_flag = 0;
//			tst_send_start_msg_flag = 1;
//			os_taskq_post(INTERACT_S_TASK_NAME, 1, MSG_SPEEX_ENCODE_STOP);
//		}
//	}
//
//}
//

/*----------------------------------------------------------------------------*/
/**@brief  语音互动server主循环消息处理
   @param
   @return
   @note 提供给server端使用
*/
/*----------------------------------------------------------------------------*/

///以下配置决定传输数据的速度， 也决定功耗的大小， interval越大， 整机功耗就越低， 但是传输速度越慢
#define  INTERACT_IDLE_INTERVAL_MIN			(120)//max 120
#define  INTERACT_IDLE_INTERVAL_MAX			(120)//max 120, 要进入省电模式， 必须不能小于50
#define  INTERACT_WORKING_INTERVAL_MIN		(10)
#define  INTERACT_WORKING_INTERVAL_MAX		(15)
extern const KEY_REG script_interact_key;
static void interact_s_task_deal(void *parg)
{
    u8 test_send_cmd_cnt = 0;
    /* u8 speex_encode_flag; */
    INTERACT_S *obj;
    u8 init_ok = 0;

__try_connect:
    obj = interact_server_creat(INTERACT_S_TASK_NAME);
    ASSERT(obj);

    interact_server_reset_le_connect_param(obj, INTERACT_IDLE_INTERVAL_MIN, INTERACT_IDLE_INTERVAL_MAX);

    if (init_ok == 0) {
        key_msg_reg(INTERACT_S_TASK_NAME, &script_interact_key);
        init_ok = 1;
    }

    interact_server_wait_connect(obj, (BLE_MULTI_ENABLE_CHL0 | BLE_MULTI_ENABLE_CHL1), NULL, NULL);
    /* interact_server_wait_connect(obj, (BLE_MULTI_ENABLE_CHL1), NULL, NULL); */
    /* scan_ready =1; */
    /* tst_send_start_msg_flag = 1; */
    speex_encode_flag = 0;

    int msg[6] = {0};
    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();

        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                interact_s_printf("script_interact_server_del \n");
                interact_server_destroy(&obj);
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;

        case MSG_INTERACT_DISCONNECT_OK:
            interact_s_printf("retry connect again ... fun = %s, line = %d\n", __func__, __LINE__);
            interact_server_destroy(&obj);
            goto __try_connect;
            break;

        case MSG_SPEEX_ENCODE_ERR:
            interact_s_printf("encode err !!! fun = %s, line = %d\n", __func__, __LINE__);
            interact_server_destroy(&obj);
            /* ble_interact_server_io.disconnect(); */
            /* goto __try_connect; */
            break;

        case MSG_INTERACT_SEND_CMD:
            /* ble_interact_server_io.send(0[>0表示命令通道， 1表示音频通道， 这里是发送命令<], &server_send_cmd_tst_tab[test_send_cmd_cnt], 1); */
            interact_s_printf("server send cmd ############## 0x%x\n", server_send_cmd_tst_tab[test_send_cmd_cnt]);
            interact_server_send_cmd(obj, &server_send_cmd_tst_tab[test_send_cmd_cnt], 1);
            test_send_cmd_cnt++;
            if (test_send_cmd_cnt >= (sizeof(server_send_cmd_tst_tab) / sizeof(server_send_cmd_tst_tab[0]))) {
                test_send_cmd_cnt = 0;
            }
            break;

        case MSG_INTERACT_RECIEVE_CMD:
            interact_s_printf("server recieve cmd >>>>>>>>>>>> 0x%x\n", msg[1]);
            break;

        case MSG_SPEEX_ENCODE_START:
            printf("MSG_SPEEX_ENCODE_START\n");
            if (speex_encode_flag) {
                break;
            }

            if (interact_server_speek_encode_start(obj) == true) {
                speex_encode_flag = 1;
                /* interact_server_reset_le_connect_param(obj, INTERACT_WORKING_INTERVAL_MIN, INTERACT_WORKING_INTERVAL_MAX); */
            } else {
                break;
            }

            if (interact_server_wait_update_param_ok(obj, INTERACT_WORKING_INTERVAL_MIN, INTERACT_WORKING_INTERVAL_MAX, 50) == false) {
                break;
            }

            interact_s_printf("fun = %s, line = %d\n", __func__, __LINE__);
            break;

        case MSG_SPEEX_ENCODE_STOP:
            interact_s_printf("MSG_SPEEX_ENCODE_STOP\n");
            if (speex_encode_flag == 0) {
                break;
            }


            if (interact_server_speek_encode_stop(obj) == true) {
                speex_encode_flag = 0;
                /* interact_server_reset_le_connect_param(obj, INTERACT_IDLE_INTERVAL_MIN, INTERACT_IDLE_INTERVAL_MAX); */
            }

            if (interact_server_wait_update_param_ok(obj, INTERACT_IDLE_INTERVAL_MIN, INTERACT_IDLE_INTERVAL_MAX, 5) == false) {
                break;
            }

            break;
        default:
            break;
        }
    }
}



tbool interact_s_task_creat(void)
{
    u8 err = os_task_create(interact_s_task_deal, (void *)0, TaskInteractPrio, 32, INTERACT_S_TASK_NAME);
    if (err) {
        interact_s_printf("interact_s_task_creat  fail, err = %d\r\n", err);
        return false;
    }

    interact_s_printf("interact_s_task_creat  ok, \n");
    return true;
}


#endif

