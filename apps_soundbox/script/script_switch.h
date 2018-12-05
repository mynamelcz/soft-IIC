#ifndef __SCRIPT_SWITCH_H__
#define __SCRIPT_SWITCH_H__
#include "sdk_cfg.h"
#include "sys_detect.h"
#include "script_list.h"

#define SCRIPT_TASK_NAME	"ScriptTask"
#define SCRIPT_MSG_MAX_SIZE		48




void script_switch(SCRIPT_ID_TYPE target, void *priv);
SCRIPT_ID_TYPE script_get_id(void);
SCRIPT_ID_TYPE script_get_last_id(void);

#define MY_ASSERT(a, ...)\
	do{\
		if(!(a))\
		{\
			printf("MY_ASSERT-FAILD: "#a"\n"__VA_ARGS__);\
			while(1);\
		}\
	}while(0);

#endif
