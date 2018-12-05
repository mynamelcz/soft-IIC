#ifndef __LE_MULTI_SERVER_H__
#define  __LE_MULTI_SERVER_H__
#include "typedef.h"
#include "bluetooth/ble_api.h"

void le_multi_server_init(void);
u32 le_multi_server_check_connect_complete(void);
bool le_multi_server_check_init_complete(void);
bool le_multi_server_creat_connect(void);
bool le_multi_server_dis_connect(void);
void le_multi_server_regiest_update_connect_param(u16 interval_min, u16 interval_max, u16 conn_latency, u16 supervision_timeout);
void le_multi_server_regiest_callback(ble_chl_recieve_cbk_t recieve_cbk, ble_md_msg_cbk_t msg_cbk, void *priv);
int le_multi_server_data_send(u16 chl, u8 *data, u16 len);
void le_multi_server_send_test_key_num(u8 key_num);

#endif// __LE_MULTI_SERVER_H__
