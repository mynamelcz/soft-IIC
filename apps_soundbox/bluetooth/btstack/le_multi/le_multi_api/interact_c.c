#include "interact_c.h"
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
#include "le_multi_client.h"
#include "sys_detect.h"

#if ((BLE_MULTI_EN == 1) &&  (BLE_MULTI_GATT_ROLE_CFG == BLE_MULTI_GATT_ROLE_CLIENT))
#define INTERACT_C_DEBUG_ENABLE
#ifdef INTERACT_C_DEBUG_ENABLE
#define interact_c_printf	printf
#else
#define interact_c_printf(...)
#endif

#define SPEEX_DECODE_TASK_NAME					"SpeexDecTask"
#define INTERACT_C_TASK_NAME		 			"InteractCTask"

#define SPEEX_DECODE_TEMP_BUF_SIZE				(4*1024)
#define SPEEX_DECODE_TEMP_BUF_POST_SEM_LIMIT	(80)//(SPEEX_DECODE_TEMP_BUF_SIZE/32)

typedef struct __BLE_INTERACT_CLIENT {
    bool (*connect)(void);
    bool (*disconnect)(void);
    bool (*check_init)(void);
    bool (*set_scan_param)(u16 interval, u16 window);
    u32(*check_connect)(void);
    void (*regiest)(ble_chl_recieve_cbk_t recv_cbk, ble_md_msg_cbk_t msg_cbk, void *priv);
    int (*send)(u16 chl, u8 *buf, u16 len);
} BLE_INTERACT_CLIENT;


typedef struct __SPEEX_DEC_DATA {
    u8			  *temp_buf;
    cbuffer_t      cbuf;
    OS_SEM         sem;
    u32 		   lost;
} SPEEX_DEC_DATA;

struct __INTERACT_C {
    void 		 		*father_name;
    SPEEX_DECODE 		*decode;
    SPEEX_DEC_DATA 	 	 data;
    BLE_INTERACT_CLIENT	*le_io;
};

const BLE_INTERACT_CLIENT ble_interact_client_io = {
    .connect = le_multi_client_creat_connect,
    .disconnect = le_multi_client_dis_connect,
    .check_init = le_multi_client_check_init_complete,
    .set_scan_param = le_multi_set_scan_param,
    .check_connect = le_multi_client_check_connect_complete,
    .regiest = le_multi_client_regiest_callback,
    .send = le_multi_client_data_send,
};



/*----------------------------------------------------------------------------*/
/**@brief  语音互动等待连接接口
   @param
   @return
   @note server client均可以使用
*/
/*----------------------------------------------------------------------------*/
tbool interact_c_wait_connect(INTERACT_C *obj, tbool(*msg_cb)(void *priv, int msg[]), void *priv)
{
    u8 ret;
    int msg[6];
    if (obj == NULL ||  obj->le_io == NULL ||  obj->le_io->connect == NULL || obj->le_io->check_connect == NULL || obj->le_io->check_init == NULL) {
        return false;
    }
    while (obj->le_io->check_init() == false) {
        clear_wdt();
        OSTimeDly(1);
    }

#if 1
    obj->le_io->connect();
#endif
    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));

        ret = os_taskq_pend_no_clean(10, ARRAY_SIZE(msg), msg);
        clear_wdt();
        if (ret == OS_TIMEOUT) {
            interact_c_printf("waiting ...\n");
            if (obj->le_io->check_connect()) {
                interact_c_printf("connect ok !!!\n");
                return true;
            }
            continue;
        }
        /* interact_c_printf("interact c msg = %x \n", msg[0]); */
        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            return false;

        case MSG_INTERACT_CONNECT_OK:
            os_taskq_msg_clean(msg[0], 1);
            if (obj->le_io->check_connect()) {
                interact_c_printf("connect ok xxxx !!!\n");
                return true;
            }
            break;
        case MSG_INTERACT_DISCONNECT_OK:
            os_taskq_msg_clean(msg[0], 1);
            interact_c_printf("retry connect again ... fun = %s, line = %d\n", __func__, __LINE__);
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

    interact_c_printf("connect fail !!!\n");
    return false;
}


/*----------------------------------------------------------------------------*/
/**@brief  语音互动client端临时缓存结束数据
   @param
   @return
   @note 提供给client端使用
*/
/*----------------------------------------------------------------------------*/
static void speex_decode_data_save(void *priv, u8 *buf, u16 len)
{
    INTERACT_C *obj = (INTERACT_C *)priv;
    SPEEX_DEC_DATA *data = &(obj->data);
    cbuffer_t *cbuffer = &(data->cbuf);
    if ((cbuffer->total_len - cbuffer->data_len) < len) {
        data->lost ++;
    } else {
        cbuf_write(cbuffer, (void *)buf, len);
    }

    if (cbuffer->data_len >= SPEEX_DECODE_TEMP_BUF_POST_SEM_LIMIT) {
        if (speex_decode_get_status(obj->decode) == 0) {
            tbool ret = speex_deoce_start(obj->decode);
            if (ret == true) {
                interact_c_printf("speex decode start ok !!!\n");
            } else {
                interact_c_printf("speex decode start fail !!!\n");
            }

        }
        os_sem_set(&(data->sem), 0);
        os_sem_post(&(data->sem));
    }
}


/*----------------------------------------------------------------------------*/
/**@brief  语音互动client接收数据会回调
   @param
   @return
   @note 提供给client端使用
*/
/*----------------------------------------------------------------------------*/
static void ble_client_reciev_callback(void *priv, u16 channel, u8 *buffer, u16 buffer_size)
{
    INTERACT_C *obj = (INTERACT_C *)priv;
    ASSERT(obj);
    if (channel == 0) {
        interact_c_printf("recive cmd = 0x%x, fun = %s, line = %d\n", buffer[0], __func__, __LINE__);
        os_taskq_post(obj->father_name, 2, MSG_INTERACT_RECIEVE_CMD, buffer[0]);
    } else if (channel == 1) {
        speex_decode_data_save((void *)obj, buffer, buffer_size);
    }
}

/*----------------------------------------------------------------------------*/
/**@brief  语音互动client连接状态回调
   @param
   @return
   @note 提供给client端使用
*/
/*----------------------------------------------------------------------------*/
static void ble_client_md_msg_callback(void *priv, u16 msg, u8 *buffer, u16 buffer_size)
{
    INTERACT_C *obj = (INTERACT_C *)priv;
    ASSERT(obj);
    interact_c_printf("md msg = 0x%x\n", msg);
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
/**@brief  语音互动client数据读取seek接口
   @param
   @return
   @note 提供给client端使用
*/
/*----------------------------------------------------------------------------*/
static bool speex_decode_seek(void *hdl, u8 type, u32 offsize)
{
    return true;
}

/*----------------------------------------------------------------------------*/
/**@brief  语音互动client数据读取read接口, 主要是从临时缓存中获取数据给speex解码器解码
   @param
   @return
   @note 提供给client端使用
*/
/*----------------------------------------------------------------------------*/
static u16 speex_decode_read(void *hdl, u8 *buff, u16 len)
{
    /* printf("fun = %s, line = %d\n", __func__, __LINE__); */
    u8 err;
    SPEEX_DEC_DATA *data = (SPEEX_DEC_DATA *)hdl;
    while (1) {
        if (cbuf_get_data_size(&(data->cbuf)) >= len) {
            cbuf_read(&(data->cbuf), buff, len);
            break;
        } else {
            /* printf("^");		 */
        }

        err =  os_sem_pend(&(data->sem), 0);

        if (err != 0) {
            interact_c_printf("pend fail, err= %x, line = %d, fun = %s\n", err, __LINE__, __func__);
            return 0;
        }
    }
    return len;
}

/*----------------------------------------------------------------------------*/
/**@brief  语音互动client解码接口io配置
   @param
   @return
   @note 提供给client端使用
   @note 如果是需要写设备，调用相应的fs_read， fs_seek等相关即可欧
*/
/*----------------------------------------------------------------------------*/
static const SPEEX_FILE_IO speex_decode_file_interface  = {
    .seek = speex_decode_seek,
    .read = speex_decode_read,
    .write = NULL,//speex_decode_write,//fs_write,//
};


/*----------------------------------------------------------------------------*/
/**@brief  语音互动client解码停止接口
   @param
   @return
   @note 提供给client端使用
*/
/*----------------------------------------------------------------------------*/
void interact_client_destroy(INTERACT_C **hdl)
{
    if ((hdl == NULL) || (*hdl == NULL)) {
        return;
    }

    INTERACT_C *obj = *hdl;

    if (obj->le_io != NULL && obj->le_io->disconnect != NULL) {
        obj->le_io->disconnect();
    }

    os_sem_del(&(obj->data.sem), 1);

    speex_decode_destroy(&(obj->decode));
    free(obj);
    *hdl = NULL;
}


/*----------------------------------------------------------------------------*/
/**@brief  语音互动client解码初始化接口
   @param
   @return
   @note 提供给client端使用
*/
/*----------------------------------------------------------------------------*/
INTERACT_C *interact_client_creat(void *father_name)
{
    tbool ret = false;
    u32 f_ret;

    u32 buf_len = SIZEOF_ALIN(sizeof(INTERACT_C), 4) + SIZEOF_ALIN(SPEEX_DECODE_TEMP_BUF_SIZE, 4);
    u8 *need_buf = calloc(1, buf_len);
    ASSERT(need_buf);

    INTERACT_C *obj = (INTERACT_C *)need_buf;

    need_buf += SIZEOF_ALIN(sizeof(INTERACT_C), 4);
    obj->data.temp_buf = (u8 *)need_buf;

    obj->father_name = father_name;
    obj->le_io = (BLE_INTERACT_CLIENT *)&ble_interact_client_io;

    cbuf_init(&(obj->data.cbuf), obj->data.temp_buf, SPEEX_DECODE_TEMP_BUF_SIZE);

    if (os_sem_create(&(obj->data.sem), 0) != OS_NO_ERR) {
        free(need_buf);
        return NULL;
    }

    dac_set_samplerate(16000L, 0);

    obj->decode = speex_decode_creat();

    __speex_decode_set_file_io(obj->decode, (SPEEX_FILE_IO *)&speex_decode_file_interface, (void *) & (obj->data));

    ret = speex_decode_process(obj->decode, SPEEX_DECODE_TASK_NAME, TaskMusicPhyDecPrio);
    if (ret == true) {
        printf("speex dec aready ok !!!\n");
    } else {

        printf("fun = %s, line = %d !!!\n", __func__, __LINE__);
        interact_client_destroy(&obj);
    }


    if (obj->le_io != NULL && obj->le_io->regiest != NULL) {
        obj->le_io->regiest(ble_client_reciev_callback, ble_client_md_msg_callback, obj);
    }
    return obj;
}


/*----------------------------------------------------------------------------*/
/**@brief  语音互动client主循环消息处理
   @param
   @return
   @note 提供给client端使用
*/
/*----------------------------------------------------------------------------*/
static void interact_c_task_deal(void *parg)
{
    INTERACT_C *obj = interact_client_creat(INTERACT_C_TASK_NAME);

    ASSERT(obj);


__try_connect:
    interact_c_wait_connect(obj, NULL, NULL);

    int msg[6] = {0};
    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();

        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                interact_c_printf("script_interact_client_del \n");
                interact_client_destroy(&obj);
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;

        case MSG_INTERACT_DISCONNECT_OK:
            interact_c_printf("retry connect again ... fun = %s, line = %d\n", __func__, __LINE__);
            goto __try_connect;
            break;


        case MSG_INTERACT_RECIEVE_CMD:
            interact_c_printf("client recieve cmd >>>>>>>>>>>> 0x%x\n", msg[1]);
            break;

        default:
            break;
        }
    }
}

tbool interact_c_task_creat(void)
{
    u8 err = os_task_create(interact_c_task_deal, (void *)0, TaskInteractPrio, 32, INTERACT_C_TASK_NAME);
    if (err) {
        interact_c_printf("interact_c_task_creat  fail, err = %d\r\n", err);
        return false;
    }

    interact_c_printf("interact_c_task_creat  ok, \n");
    return true;
}

#endif

