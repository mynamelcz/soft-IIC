#ifndef __RECORD_H__
#define __RECORD_H__

#include "includes.h"
#include "encode/encode.h"

// #define ENC_DEBUG

#ifdef ENC_DEBUG
#define enc_puts     puts
#define enc_printf   printf
#else
#define enc_puts(...)
#define enc_printf(...)
#endif

#define REC_TASK_NAME        "RECTask"
#define ENC_RUN_TASK_NAME       "EncRunTask"
#define ENC_WFILE_TASK_NAME     "EncWFileTask"



extern const char rec_folder_name[8];                   //录音文件夹  //仅支持一层目录
extern  const char rec_file_name[21];      //MP3录音文件名（含路径）


/*----------------------------------------------------------------------------*/
/** @brief:创建一个录音句柄
 @param: 无
 @return:返回一个录音操作句柄
 @author:zyq
 @note:RECORD_OP_API * toy_rec_hdl_create_and_start(void);
*/
/*----------------------------------------------------------------------------*/
RECORD_OP_API *toy_rec_hdl_create_and_start(void);


/*----------------------------------------------------------------------------*/
/** @brief:录音暂停
  @param: 已创建的录音句柄
  @return:无
  @author:zyq
  @note:void toy_rec_pp(RECORD_OP_API** rec_api);
 */
/*----------------------------------------------------------------------------*/
void toy_rec_pp(RECORD_OP_API **rec_api);



/*----------------------------------------------------------------------------*/
/** @brief:录音结束
  @param: 已创建的录音句柄
  @return:无
  @author:zyq
  @note:void toy_rec_destroy(RECORD_OP_API **rec_api);
 */
/*----------------------------------------------------------------------------*/
void toy_rec_destroy(RECORD_OP_API **rec_api);


void toy_rec_file_play(void);

void set_rec_name(char *name, u8 cnt);
u16 get_last_rec_filenum_from_VM(void);
void encode_flash_zone(u32 addr, u32 len);
extern const struct task_info rec_task_info;

extern void bt_rec_buf_write(s16 *buf, u32 len);
extern void rec_get_dac_data(RECORD_OP_API *rec_api, s16 *buf, u32 len); //录音获取pcm数据函数

#endif/*__RECORD_H__*/
