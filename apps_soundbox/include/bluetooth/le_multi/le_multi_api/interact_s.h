#ifndef __INTERACT_S_H__
#define __INTERACT_S_H__
#include "typedef.h"

#define BLE_MULTI_ENABLE_CHL0	BIT(0)///使能通道0, 命令发送通道
#define BLE_MULTI_ENABLE_CHL1	BIT(1)///使能通道1, 语音发送通道

typedef struct __INTERACT_S 	INTERACT_S;

void interact_server_destroy(INTERACT_S **hdl);
INTERACT_S *interact_server_creat(void *father_name);
tbool interact_server_speek_encode_start(INTERACT_S *obj);
tbool interact_server_speek_encode_stop(INTERACT_S *obj);
tbool interact_server_send_cmd(INTERACT_S *obj, u8 cmd[], u32 cmd_size);
tbool  interact_server_reset_le_connect_param(INTERACT_S *obj, u16 interval_min, u16 interval_max);
tbool interact_server_wait_connect(INTERACT_S *obj, u32 con_enable,  tbool(*msg_cb)(void *priv, int msg[]), void *priv);
tbool interact_s_task_creat(void);

#endif//  __INTERACT_S_H__
