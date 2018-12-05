#ifndef __SRC_PARAM_H__
#define __SRC_PARAM_H__
#include "typedef.h"

typedef enum {
    SRC_IOCTRL_CMD_SET_TASK_INFO = 0x0, //if src writ without task, need creat task
    SRC_IOCTRL_CMD_GET_TASK_INFO, //if src writ without task, by get name to exit task
} SRC_IOCTRL_CMD;

typedef struct __SRC_OUTPUT {
    void *priv;
    void (*_cbk)(void *priv, void *buf, u16 len); ///转换后数据输出回调，中断调用
} SRC_OUTPUT;

typedef struct {
    u8 	nchannel;    ///一次转换的通道个数，(1 ~ SRC_MAX_CHANNEL)
    u8 	reserver[1];
    u16 once_out_len;///输出必选是4byte对齐, 并且是(4*通道数）的倍数
    u16 in_rate;    ///输入采样率
    u16 out_rate;   ///输出采样率
    u16 in_chinc;   ///输入方向,多通道转换时，每通道数据的地址增量
    u16 in_spinc;   ///输入方向,同一通道后一数据相对前一数据的地址增量
    u16 out_chinc;  ///输出方向,多通道转换时，每通道数据的地址增量
    u16 out_spinc;  ///输出方向,同一通道后一数据相对前一数据的地址增量
    SRC_OUTPUT output;
} SRC_OBJ_PARAM;

typedef struct __SRC_OBJ SRC_OBJ;

#endif
