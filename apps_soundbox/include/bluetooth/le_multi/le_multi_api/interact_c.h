#ifndef __INTERACT_C_H__
#define __INTERACT_C_H__
#include "typedef.h"

typedef struct __INTERACT_C 	INTERACT_C;

tbool interact_c_wait_connect(INTERACT_C *obj, tbool(*msg_cb)(void *priv, int msg[]), void *priv);
void interact_client_destroy(INTERACT_C **hdl);
INTERACT_C *interact_client_creat(void *father_name);
tbool interact_c_task_creat(void);

#endif//__INTERACT_C_H__;
