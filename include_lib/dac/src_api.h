/*
*********************************************************************************************************
*                                             BC51
*
*                                             CODE
*
*                          (c) Copyright 2015-2016, ZHUHAI JIELI
*                                           All Rights Reserved
*
* File : *
* By   : jamin.li
* DATE : 11/11/2015 build this file
*********************************************************************************************************
*/
#ifndef _SRC_H_
#define _SRC_H_

#include "src_param.h"

void src_init_api(void);
void src_exit_api(void);
void *src_obj_creat_api(SRC_OBJ_PARAM *parm);
void src_obj_destroy_api(void **obj);
void src_obj_write_api(void *obj, void *buf, u16 len);
//int src_ioctrl_api(u32 cmd, void *obj, void *parm);

///the follow io will creat one task to deal src process
void *src_obj_creat_with_task_api(SRC_OBJ_PARAM *parm, const char *task_name, u8 prio, u32 stk_size);
void src_obj_destroy_with_task_api(void **obj);///make sure your src_obj_write_to_task_api is not run any more
void src_obj_write_to_task_api(void *obj, void *buf, u16 len);

#endif
