#include "sdk_cfg.h"
#include "encode/encode.h"
#include "dac_param.h"
#include "toy_rec_module.h"

extern void free_fun(void **ptr);
extern void *malloc_fun(void *ptr, u32 len, char *info);

REC_PARAMETER *toy_rec_parameter = NULL;

REC_PARAMETER *toy_rec_parameter_create(void)
{
    REC_PARAMETER *p_return = NULL;
    p_return = malloc_fun(p_return, sizeof(REC_PARAMETER), 0);
    ASSERT(p_return);
    return p_return;
}

void toy_rec_parameter_destroy(REC_PARAMETER **ds_ptr)
{
    if (*ds_ptr != NULL) {
        free(*ds_ptr);
        *ds_ptr = NULL;
    }
}

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
void toy_rec_parameter_set(u8 rec_mode, u32 head_cut_ms, u32 tail_cut_ms)
{
    if (toy_rec_parameter == NULL) {
        //第一次时才申请
        toy_rec_parameter = toy_rec_parameter_create();
    }
    if (toy_rec_parameter != NULL) {
        toy_rec_parameter->rec_mode = rec_mode;
        toy_rec_parameter->rec_head_cut_ms = head_cut_ms;
        toy_rec_parameter->rec_tail_cut_ms = tail_cut_ms;
    }
}



