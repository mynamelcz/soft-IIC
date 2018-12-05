/*
 *********************************************************************************************************
 *                                            br16
 *                                            btstack
 *                                             CODE
 *
 *                          (c) Copyright 2016-2016, ZHUHAI JIELI
 *                                           All Rights Reserved
 *
 * File : *
 * By   : jamin.li
 * DATE : 2016-04-12 10:17AM    build this file
 *********************************************************************************************************
 */
#include "typedef.h"
//#include "error.h"
#include "sdk_cfg.h"
#include "common/msg.h"
#include "common/app_cfg.h"
#include "bluetooth/le_server_module.h"
#include "bluetooth/le_finger_spinner.h"
#include "rtos/os_api.h"
#include "rtos/task_manage.h"
#include "rcsp/rcsp_interface.h"
#include "rcsp/rcsp_head.h"
#include "rcsp/rcsp_head.h"
#include "bluetooth/ble_wechat.h"

#if MIDI_UPDATE_EN
#include "midi_update.h"
#endif

#if(BLE_BREDR_MODE&BT_BLE_EN)

#if (BLE_MULTI_EN == 0)
#include "bluetooth/le_profile_def.h"

static uint16_t server_send_handle;//ATT 发送handle

uint16_t ble_conn_handle;//设备连接handle
static u8 ble_mutex_flag;// ble与spp 互斥标记,0:表示ble可连接，1：表示ble不可连接

u32 app_data_send(u8 *data, u16 len);
static void app_data_recieve(u8 *data, u16 len);
#if BT_BLE_WECHAT
u8 wechat_buf[512] = {0};
extern u16 _swap16(u16 value);
extern void get_ble_wechat_addr(u8 *addr, u8 mode, u8 *set_addr);

uint8_t challeange[CHALLENAGE_LENGTH] = {0x11, 0x22, 0x33, 0x44}; //
RecvDataPush *epb_unpack_recv_data_push(const uint8_t *buf, int buf_len);

u8 wechat_init_en = 0;
static uint16_t last_index = 0;
u16 curr_wechat_data_len = 0;
u16 wechat_data_len = 0;
u8 wechat_data_buf[512];
void wechat_set_init_handle()
{
    wechat_init_en = 1;
}

void wechat_clr_init_handle()
{
    wechat_init_en = 0;
    last_index = 0;
    curr_wechat_data_len = 0;
    wechat_data_len = 0;
}

u8 wechat_get_init_handle()
{
    return wechat_init_en;
}
#endif

void app_server_le_msg_callback(uint16_t msg, uint8_t *buffer, uint16_t buffer_size)
{
    printf("\n-%s, msg= 0x%x\n", __FUNCTION__, msg);
    switch (msg) {
    case BTSTACK_LE_MSG_CONNECT_COMPLETE:
        ble_conn_handle = buffer[0] + (buffer[1] << 8);
        printf("conn_handle= 0x%04x\n", ble_conn_handle);

#if SUPPORT_APP_RCSP_EN
        rcsp_event_com_start(RCSP_APP_TYPE_BLE);
        rcsp_register_comsend(app_data_send);
#endif // SUPPORT_APP_RCSP_EN

#if BT_BLE_WECHAT
        last_index = 0;
        curr_wechat_data_len = 0;
        wechat_data_len = 0;
#endif
        break;

    case BTSTACK_LE_MSG_DISCONNECT_COMPLETE:
        printf("disconn_handle= 0x%04x\n", buffer[0] + (buffer[1] << 8));
        server_send_handle = 0;

        if (ble_conn_handle != 0) {
            ble_conn_handle = 0;

#if SUPPORT_APP_RCSP_EN
            rcsp_event_com_stop();
#endif // SUPPORT_APP_RCSP_EN

#if MIDI_UPDATE_EN
            midi_update_disconnect();
#endif

            if (ble_mutex_flag == 0) {
                server_advertise_enable();
            }
        }

#if BT_BLE_WECHAT
        wechat_clr_init_handle();
#endif

        break;
    case BTSTACK_LE_MSG_CONNECTIONS_CHANGED:

        break;

    case BTSTACK_LE_MSG_INIT_COMPLETE:
        puts("init server_adv\n");
        server_advertise_enable();
        break;

    default:
        break;
    }

    puts("exit_le_msg_callback\n");
}


#if BT_BLE_WECHAT
u8 *check_csw_data(u8 *buffer, u16 buffer_size)
{
    u8 *ptr;

    if (wechat_data_len) {
        if (curr_wechat_data_len >= buffer_size) {
            curr_wechat_data_len -= buffer_size;
        } else {
            puts("/nerr----!!!/n");
            while (1);
        }

        my_memcpy(&wechat_data_buf[last_index], buffer, buffer_size);

        last_index += buffer_size;
        if (curr_wechat_data_len == 0) {
            ptr = wechat_data_buf;
        } else {
            return NULL;
        }

    } else {
        wechat_data_len	= (buffer[2] << 8 | buffer[3]);
        if (wechat_data_len > 512) {
            puts("\nbuf is not enough\n");
            return NULL;
        }
        curr_wechat_data_len = (buffer[2] << 8 | buffer[3]) - buffer_size;
        last_index = buffer_size;
        if (curr_wechat_data_len > 0) {
            my_memcpy(wechat_data_buf, buffer, buffer_size);
            return NULL;
        }
        ptr = (u8 *)buffer;
    }

    return ptr;
}
#endif

// ATT Client Write Callback for Dynamic Data
// @param attribute_handle to be read
// @param buffer
// @param buffer_size
// @return 0
uint16_t app_server_write_callback(uint16_t attribute_handle, uint8_t *buffer, uint16_t buffer_size)
{

    u16 handle = attribute_handle;
    u16 copy_len;

#if BT_BLE_WECHAT
    u16 all_data_len;
    u8 *packet_ptr = NULL;
#endif

#if 0
    if (buffer_size > 0) {
        printf("\n write_cbk: handle= 0x%04x", handle);
        put_buf(buffer, buffer_size);
    }
#endif

    switch (handle) {

#if BT_BLE_WECHAT
    case ATT_CHARACTERISTIC_0xFEC8_01_CLIENT_CONFIGURATION_HANDLE:
        puts("set_auth_flag\n");
        server_send_handle = handle - 1;
        os_taskq_post_msg("btmsg", 2,  MSG_BT_WECHAT_AUTH_INIT, 1);
        break;

    case ATT_CHARACTERISTIC_0xFEC7_01_VALUE_HANDLE:

        packet_ptr = check_csw_data(buffer, buffer_size);
        if (packet_ptr == NULL) {
            break;
        }

        Protobuf_Head *head = (Protobuf_Head *)packet_ptr;
        u8 head_len = sizeof(Protobuf_Head);
        u16 cmdid = _swap16(head->nCmdId);
        RecvDataPush *recvDataPush;
        SendDataResponse *sendDataResp;
        SwitchViewPush *swichViewPush;
        SwitchBackgroudPush *switchBackgroundPush;
        InitResponse *initResp;
        all_data_len = wechat_data_len;
        last_index = 0;
        curr_wechat_data_len = 0;
        wechat_data_len = 0;


        /* puts("wechat_data_len\n"); */
        /* put_u16hex(all_data_len); */
        /* put_buf(packet_ptr,all_data_len); */


        switch (cmdid) {
        case cmdid_resp_auth:
            puts("auth_resp\n");
            if (!wechat_get_init_handle()) {
                wechat_set_init_handle();
                os_taskq_post_msg("btmsg", 2, MSG_BT_WECHAT_AUTH_INIT, 2);
            }
            break;

        case cmdid_resp_init:
            puts("init_resp\n");
            initResp = epb_unpack_init_response(packet_ptr + head_len, all_data_len - head_len);
            if (!initResp) {
                break;
            }
            if (initResp->base_response != NULL) {
                free(initResp->base_response);
            }
            free(initResp);
            break;

        case cmdid_push_recvData:
            puts("get_data\n");
            recvDataPush = epb_unpack_recv_data_push(packet_ptr + head_len, all_data_len - head_len);
            if (!recvDataPush) {
                break;
            }
            BlueDemoHead *bledemohead = (BlueDemoHead *)recvDataPush->data.data;
            put_u16hex(_swap16(bledemohead->m_cmdid));
            /*
            　　					自行处理数据
                              */

            if (recvDataPush->base_push != NULL) {
                free(recvDataPush->base_push);
            }
            free(recvDataPush);
            break;

        case cmdid_resp_sendData:
            puts("send_data\n");

            sendDataResp = epb_unpack_send_data_response(packet_ptr + head_len, all_data_len - head_len);

            if (!sendDataResp) {
                break;
            }

            bledemohead = (BlueDemoHead *)sendDataResp->data.data;
            if (sendDataResp->base_response != NULL) {
                free(sendDataResp->base_response);
            }
            free(sendDataResp);

            break;

        case cmdid_push_switchView:
            puts("push_view\n");
            swichViewPush = epb_unpack_switch_view_push(packet_ptr + head_len, all_data_len - head_len);
            if (!swichViewPush) {
                break;
            }
            if (swichViewPush->base_push != NULL) {
                free(swichViewPush->base_push);
            }
            free(swichViewPush);
            break;

        case cmdid_push_switchBackgroud:
            puts("push_backgroud\n");
            switchBackgroundPush = epb_unpack_switch_backgroud_push(packet_ptr + head_len, all_data_len - head_len);
            if (! switchBackgroundPush) {
                break;
            }
            if (switchBackgroundPush->base_push != NULL) {
                free(switchBackgroundPush->base_push);
            }
            free(switchBackgroundPush);
            break;

        default:
            break;
        }
        break;


    default:
        break;
    }
#else

    case ATT_CHARACTERISTIC_ae02_01_CLIENT_CONFIGURATION_HANDLE:
        if (buffer[0] != 0) {
            server_send_handle = handle - 1;
        } else {
            server_send_handle = 0;
        }
        break;


    case ATT_CHARACTERISTIC_ae01_01_VALUE_HANDLE:

        /* printf("\n--write, %d\n",buffer_size); */
        /* put_buf(buffer,buffer_size); */
        app_data_recieve(buffer, buffer_size);

        break;

    default:
        break;
    }
#endif
    return 0;
}

//ble send data
u32 app_data_send(u8 *data, u16 len)
{
    int ret_val;

    if (server_send_handle == NULL) {
        return 4;// is disconn
    }

    ret_val = server_notify_indicate_send(server_send_handle, data, len);

    if (ret_val !=  0) {
        puts("\n app_ntfy_fail!!!\n");
        return 4;//disconn
    }
    return 0;
}

extern u8 get_ble_test_key_flag(void);
void ble_server_send_test_key_num(u8 key_num)
{
    if (get_ble_test_key_flag()) {
        if (!ble_conn_handle) {
            return;
        }

        ble_user_send_test_key_num(ble_conn_handle, key_num);
    }
}
//ble recieve data
static void app_data_recieve(u8 *data, u16 len)
{
#if SUPPORT_APP_RCSP_EN
    rcsp_comdata_recieve(data, len);
#elif BLE_FINGER_SPINNER_EN
    blefs_comdata_parse(data, len);
#endif // SUPPORT_APP_RCSP_EN

#if MIDI_UPDATE_EN
    /* printf_buf(data, len); */
    midi_update_prase(data, len);
#endif

}

#if BT_BLE_WECHAT
void set_addr_into_adv(u8 *adv_ind_data, u8 len, u8 *mac_addr)
{
    u8 ind = 0;
    u8 check_ind = 0;

    if (len > 31) {
        return;
    }

    do {
        check_ind = adv_ind_data[ind];
        if (check_ind == 0) {
            return;
        }
        /* printf("check_ind = %d,ind = %d, len = %d\n",check_ind,ind,len); */
        if (adv_ind_data[ind + 1] == 0xff) {
            memcpy(&adv_ind_data[ind + 4], mac_addr, 6);
            return;
        }
        ind = ind + check_ind + 1;
    } while (ind <= (len - 1));
}

u8 test_addr[6] = {0x2e, 0x3a, 0xba, 0x98, 0x22, 0x11};
void adv_reset_deal(void)
{
    u8 ble_mac_addr[6];
    get_ble_wechat_addr(ble_mac_addr, 1, NULL);
    /* get_ble_wechat_addr(ble_mac_addr,0,test_addr); */
    puts("mac_addr\n");
    put_buf(ble_mac_addr, 6);
    set_addr_into_adv(profile_adv_ind_data, sizeof(profile_adv_ind_data), ble_mac_addr);
}
#endif
extern void bt_adv_reset_handle_register(void (*handle)(void));
extern void server_select_scan_rsp_data(u8 data_type);
extern void server_set_scan_rsp_data(u8 *data);
extern void server_set_advertise_data(const u8 *data);
extern void app_advertisements_set_params(uint16_t adv_int_min, uint16_t adv_int_max, uint8_t adv_type,
        uint8_t direct_address_typ, u8 *direct_address, uint8_t channel_map, uint8_t filter_policy);
extern void server_connection_parameter_update(int enable);
extern void app_connect_set_params(uint16_t interval_min, uint16_t interval_max, uint16_t supervision_timeout);
extern void s_att_server_register_conn_update_complete(void (*handle)(uint16_t min, uint16_t max, uint16_t timeout));

/*
 * @brief Set Advertisement Paramters
 * @param adv_int_min
 * @param adv_int_max
 * @param adv_type
 * @param direct_address_type
 * @param direct_address
 * @param channel_map
 * @param filter_policy
 *
 * @note own_address_type is used from gap_random_address_set_mode
 */
void app_set_adv_parm(void)
{
// setup advertisements
    uint16_t adv_int_min = 0x0080;
    uint16_t adv_int_max = 0x00a0;
    uint8_t adv_type = 0;
    u8 null_addr[6];
    memset(null_addr, 0, 6);

    app_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
}

//连接参数请求，只能修改3个参数
void app_set_connect_param(void)
{
// setup connect
    uint16_t conn_interval_min = 15 ;
    uint16_t conn_interval_max = 15;
    uint16_t conn_supervision_timeout = 550;

    app_connect_set_params(conn_interval_min, conn_interval_max, conn_supervision_timeout);
}


void app_server_conn_update_callback(uint16_t min, uint16_t max, uint16_t timeout)
{

    printf("app_min = %d\n", min);
    printf("app_max = %d\n", max);
    printf("timeout= %d\n", timeout);

}

void app_server_init(void)
{
    printf("\n%s\n", __FUNCTION__);
    server_register_profile_data(profile_data);

#if BT_BLE_WECHAT
    bt_adv_reset_handle_register(adv_reset_deal);
#else
    bt_adv_reset_handle_register(NULL);
#endif

    server_set_advertise_data(profile_adv_ind_data);
    server_register_app_callbak(app_server_le_msg_callback, NULL, app_server_write_callback);
#if BT_BLE_WECHAT
    server_select_scan_rsp_data(2);
    server_set_scan_rsp_data(profile_scan_rsp_data);
    app_set_adv_parm();
    server_connection_parameter_update(1);//连接参数使能，0：不修改连接参数，1：修改连接参数
    app_set_connect_param();//注册连接参数
    s_att_server_register_conn_update_complete(app_server_conn_update_callback);//注册连接参数请求回调函数
    ble_mutex_flag = 0;
    wechat_clr_init_handle();
#else
    server_select_scan_rsp_data(0);     //scan_rsp类型选择，0：默认，1：会自动填写ble名字，其他自由填写，2：全部内容自由填写
    server_set_scan_rsp_data(profile_scan_rsp_data);//注册scan_rsp包内容
    app_set_adv_parm();//注册广播参数
    server_connection_parameter_update(1);//连接参数使能，0：不修改连接参数，1：修改连接参数
    app_set_connect_param();//注册连接参数
    s_att_server_register_conn_update_complete(app_server_conn_update_callback);//注册连接参数请求回调函数
    ble_mutex_flag = 0;

#endif
}

/*
spp 和 ble 互斥连接
1、当spp 连接后，ble变为不可连接
2、当ble连上后，若有spp连接上，则断开ble；ble变为不可连接
 */
void ble_enter_mutex(void)
{
    P_FUNCTION


    if (ble_mutex_flag == 1) {
        return;
    }

    puts("in enter mutex\n");
    ble_mutex_flag = 1;
    if (ble_conn_handle != 0) {
        ble_hci_disconnect(ble_conn_handle);
        rcsp_event_com_stop();
    } else {
        server_advertise_disable();
    }
    puts("exit_enter_mu\n");
}

void ble_exit_mutex(void)
{
    P_FUNCTION

    if (ble_mutex_flag == 0) {
        return;
    }

    puts("in_exit mutex\n");
    ble_mutex_flag = 0;
    server_advertise_enable();
    puts("exit_exit_mu\n");
}

void ble_server_disconnect(void)
{
    P_FUNCTION
    if (ble_conn_handle != 0) {
        printf("server discon handle= 0x%x\n ", ble_conn_handle);
        ble_hci_disconnect(ble_conn_handle);
    }
    puts("exit_discnt\n");
}


#if BT_BLE_WECHAT
u16 wechat_auth_init_fun(u8 cmd, u8 *buf)
{
    BaseRequest basReq = {NULL};
    Protobuf_Head pb_head = {0xFE, 1, 0,  _swap16(cmdid_req_auth), 0};
    u8 head_len = sizeof(Protobuf_Head);
    static u16 tx_len = 0;
    static unsigned short seq = 0;
    AuthRequest authReq;
    InitRequest initReq;
    u8 ble_mac_addr[6];
    static uint16_t bleDemoHeadLen = sizeof(BlueDemoHead);
    BlueDemoHead  *bleDemoHead;
    SendDataRequest sendDatReq;

    seq++;

    switch (cmd) {
    case CMD_AUTH:
        get_ble_wechat_addr(ble_mac_addr, 1, NULL);

        authReq.base_request = &basReq;
        authReq.has_md5_device_type_and_device_id = false;
        authReq.md5_device_type_and_device_id.data = NULL;
        authReq.md5_device_type_and_device_id.len = 0;
        authReq.proto_version = PROTO_VERSION;
        authReq.auth_proto = (EmAuthMethod)AUTH_PROTO;
        authReq.auth_method = (EmAuthMethod)AUTH_METHOD;
        authReq.has_aes_sign = false;
        authReq.aes_sign.data = NULL;
        authReq.aes_sign.len = 0;
        authReq.has_mac_address = true;
        authReq.mac_address.data = ble_mac_addr;
        authReq.mac_address.len = 6;
        authReq.has_time_zone = false;
        authReq.time_zone.str = NULL;
        authReq.time_zone.len = 0;
        authReq.has_language = false;
        authReq.language.str = NULL;
        authReq.language.len = 0;
        authReq.has_device_name = false;
        authReq.device_name.str = NULL,
                            authReq.device_name.len = 0,
                                                tx_len = epb_auth_request_pack_size(&authReq) + head_len;

        if (epb_pack_auth_request(&authReq, &buf[head_len], tx_len - head_len) < 0) {
            break;
        }
        pb_head.nCmdId =  _swap16(cmdid_req_auth);
        pb_head.nLength =  _swap16(tx_len);
        pb_head.nSeq =  _swap16(seq);
        memcpy(buf, &pb_head, head_len);
        return tx_len;


    case CMD_INIT:
        initReq.base_request = &basReq;
        initReq.has_resp_field_filter = false;
        initReq.resp_field_filter.data = NULL;
        initReq.resp_field_filter.len = 0;
        initReq.has_challenge = true;
        initReq.challenge.data = challeange;
        initReq.challenge.len = CHALLENAGE_LENGTH;

        tx_len = epb_init_request_pack_size(&initReq) + head_len;
        if (epb_pack_init_request(&initReq, &buf[head_len], tx_len - head_len) < 0) {
            break;
        }
        pb_head.nCmdId =  _swap16(cmdid_req_init);
        pb_head.nLength =  _swap16(tx_len);
        pb_head.nSeq =  _swap16(seq);
        memcpy(buf, &pb_head, head_len);
        return tx_len;

    case CMD_SENDDAT://发送自定义数据
        bleDemoHead = (BlueDemoHead *)malloc(bleDemoHeadLen + sizeof(SEND_TEXT_WECHAT));
        bleDemoHead->m_magicCode[0] = MPBLEDEMO2_MAGICCODE_H;
        bleDemoHead->m_magicCode[1] = MPBLEDEMO2_MAGICCODE_L;
        bleDemoHead->m_version = _swap16(MPBLEDEMO2_VERSION);
        bleDemoHead->m_totalLength = _swap16(bleDemoHeadLen + sizeof(SEND_TEXT_WECHAT));
        bleDemoHead->m_cmdid = _swap16(sendTextReq);
        bleDemoHead->m_seq = _swap16(seq);
        bleDemoHead->m_errorCode = 0;
        memcpy((uint8_t *)bleDemoHead + bleDemoHeadLen, SEND_TEXT_WECHAT, sizeof(SEND_TEXT_WECHAT));
        sendDatReq.base_request = &basReq;
        sendDatReq.data.data = (uint8_t *)bleDemoHead;
        sendDatReq.data.len = bleDemoHeadLen + sizeof(SEND_TEXT_WECHAT);

        //发送到客户服务器
        /* sendDatReq.has_type =  false; */
        /* sendDatReq.type = (EmDeviceDataType)NULL; */

        //发送到微信服务器
        sendDatReq.has_type =  true ;//false;
        sendDatReq.type = (EmDeviceDataType)EDDT_wxDeviceHtmlChatView;////NULL;
        tx_len = epb_send_data_request_pack_size(&sendDatReq) + head_len;
        if (epb_pack_send_data_request(&sendDatReq, &buf[head_len], tx_len - head_len) < 0) {
            break;
        }
        pb_head.nCmdId =  _swap16(cmdid_req_sendData);
        pb_head.nLength =  _swap16(tx_len);
        pb_head.nSeq =  _swap16(seq);
        memcpy(buf, &pb_head, head_len);
        free(bleDemoHead);
        return tx_len;
    default:
        break;
    }
    return 0;
}

void wechat_auth_fun(void)
{
    u16 ble_txlen;

    ble_txlen = wechat_auth_init_fun(CMD_AUTH, wechat_buf);
    app_data_send(wechat_buf, ble_txlen);
}

void wechat_init_fun(void)
{
    u16 ble_txlen;

    ble_txlen = wechat_auth_init_fun(CMD_INIT, wechat_buf);
    app_data_send(wechat_buf, ble_txlen);
}
void wechat_send_fun(void)
{
    u16 ble_txlen;

    ble_txlen = wechat_auth_init_fun(CMD_SENDDAT, wechat_buf);
    app_data_send(wechat_buf, ble_txlen);
}
#endif
#endif//BLE_MULTI_EN
#endif //#if (BLE_BREDR_MODE&BT_BLE_EN)
