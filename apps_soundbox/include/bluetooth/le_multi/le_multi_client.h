#ifndef __LE_MULTI_CLIENT_H__
#define __LE_MULTI_CLIENT_H__
#include "typedef.h"
#include "bluetooth/ble_api.h"

void le_multi_client_init(void);
void le_multi_client_disconnect_send(void);
u32  le_multi_client_check_connect_complete(void);
bool le_multi_client_creat_connect(void);
bool le_multi_client_dis_connect(void);
bool le_multi_client_check_init_complete(void);
bool le_multi_set_scan_param(u16 interval, u16 window);
void le_multi_client_regiest_callback(ble_chl_recieve_cbk_t recieve_cbk, ble_md_msg_cbk_t msg_cbk, void *priv);
void le_multi_send_test_key_num(u8 key_num);
int  le_multi_client_data_send(u16 chl, u8 *data, u16 len);

#endif// __LE_MULTI_CLIENT_H__
