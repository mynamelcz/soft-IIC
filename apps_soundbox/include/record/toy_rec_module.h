#include "common/includes.h"

typedef struct _REC_PARAMETER {
    u32 rec_head_cut_ms; //开头砍掉的数据长度
    u32 rec_tail_cut_ms; //结尾砍掉的数据长度
    u8 rec_mode;         //录音类型（比如蓝牙录音、收音录音、mic录音、line_in录音）
} REC_PARAMETER;



extern REC_PARAMETER *toy_rec_parameter;
/*----------------------------------------------------------------------------*/
/*
 @brief: 录音参数设置
 @param: rec_mode:录音模式：mic录音、bt录音、fm录音、line_in录音

 @param: head_cut_ms:开头要砍去的数据 单位：ms
 @param: tail_cut_ms:结尾要砍去的数据 单位：ms
 @author:zyq
 @note:void toy_rec_parameter_set(u8 rec_mode,u32 head_cut_ms,u32 tail_cut_ms);
*/
/*----------------------------------------------------------------------------*/
void toy_rec_parameter_set(u8 rec_mode, u32 head_cut_ms, u32 tail_cut_ms);

