#ifndef __HTK_H__
#define __HTK_H__
#include "sdk_cfg.h"

typedef tbool(*act_cb_fun)(void *priv, char result[], char nums, u8 isTimeout);
typedef int (*msg_cb_fun)(void *priv);


tbool htk_player_process(void *priv, act_cb_fun act_cb, msg_cb_fun msg_cb, u16 timeout);

#endif//__HTK_H__

