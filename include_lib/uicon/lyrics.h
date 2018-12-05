#ifndef _LYRICS_H_
#define _LYRICS_H_

#include "typedef.h"

typedef struct __LRC_CFG {
    u16 once_read_len;///一次读取长度配置
    u16 once_disp_len;///一次显示缓存长度配置
    u16 label_temp_buf_len;///时间标签缓存总长度配置
    u8   lrc_text_id;         ///text id
    u8   read_next_lrc_flag;    ///是否预读下一条歌词lrc
    u8	 enable_save_lable_to_flash;//使能保存时间标签到flash，主要是解决长歌词文件不支持问题
    void (*roll_speed_ctrl_cb)(u8 lrc_len, u32 time_gap, u8 *roll_speed);///速度控制配置
    void (*clr_lrc_disp_cb)(void);///清屏回调
} LRC_CFG;


extern void lrc_destroy(void);
extern int lrc_param_init(LRC_CFG *cfg);
extern bool lrc_analysis(void *lrc_handle);
extern int lrc_show(u16 dbtime_s, u8 btime_100ms);
extern void lrc_label_save_to_flash_init(u32 addr, u32 len, u32 base_addr);

#endif
