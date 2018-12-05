#ifndef __MUSIC_PLAYER_H__
#define __MUSIC_PLAYER_H__
#include "sdk_cfg.h"
#include "dec/music_api.h"
#include "sound_mix.h"
typedef MUSIC_OP_API MUSIC_PLAYER_OBJ;


MUSIC_PLAYER_OBJ *music_player_creat(void);
void music_player_destroy(MUSIC_PLAYER_OBJ **hdl);
void __music_player_set_decoder_type(MUSIC_PLAYER_OBJ *mapi, u32 parm);
void __music_player_set_file_ext(MUSIC_PLAYER_OBJ *mapi, void *ext);
// void __music_player_set_dec_phy_name(MUSIC_PLAYER_OBJ *mapi, void *name);
void __music_player_set_dec_father_name(MUSIC_PLAYER_OBJ *mapi, void *name);
void __music_player_set_path(MUSIC_PLAYER_OBJ *mapi, const char *path);
void __music_player_set_filesclust(MUSIC_PLAYER_OBJ *mapi, u32 sclust);
void __music_player_set_file_index(MUSIC_PLAYER_OBJ *mapi, u32 index);
void __music_player_set_eq(MUSIC_PLAYER_OBJ *mapi, u8 mode);
void __music_player_set_play_mode(MUSIC_PLAYER_OBJ *mapi, u8 mode);
void __music_player_set_file_sel_mode(MUSIC_PLAYER_OBJ *mapi, ENUM_FILE_SELECT_MODE mode);
void __music_player_set_dev_sel_mode(MUSIC_PLAYER_OBJ *mapi, ENUM_DEV_SEL_MODE mode);
void __music_player_set_dev_let(MUSIC_PLAYER_OBJ *mapi, u32 dev_let);
void __music_player_set_save_breakpoint_enable(MUSIC_PLAYER_OBJ *mapi, u8 enable);
void __music_player_set_save_breakpoint_vm_base_index(MUSIC_PLAYER_OBJ *mapi, u16 vm_base_normal_index, u16 vm_base_flac_index);
void __music_player_set_decode_before_callback(MUSIC_PLAYER_OBJ *mapi, void (*_cb)(void *priv), void *priv);
void __music_player_set_decode_after_callback(MUSIC_PLAYER_OBJ *mapi, void (*_cb)(void *priv), void *priv);
void __music_player_set_decode_stop_callback(MUSIC_PLAYER_OBJ *mapi, void (*_cb)(void *priv), void *priv);
void __music_player_set_decode_output(MUSIC_PLAYER_OBJ *mapi, void *(*output)(void *priv, void *buff, u32 len), void *priv);
tbool __music_player_set_decode_speed_pitch_enable(MUSIC_PLAYER_OBJ *mapi);
void __music_player_set_speed(MUSIC_PLAYER_OBJ *mapi, u16 val);
void __music_player_set_pitch(MUSIC_PLAYER_OBJ *mapi, u16 val);
void __music_player_set_vocal_enable(MUSIC_PLAYER_OBJ *mapi, u8 en);
void __music_player_set_op_cbk(MUSIC_PLAYER_OBJ *obj, void (*clr)(void *priv), tbool(*over)(void *priv), void *priv);
tbool  __music_player_set_output_to_mix(MUSIC_PLAYER_OBJ *mapi, SOUND_MIX_OBJ *mix_obj, const char *key, u32 mix_buf_len, u8 Threshold, u8 default_vol, u8 max_vol);
void  __music_player_set_output_to_mix_set_vol(MUSIC_PLAYER_OBJ *mapi, u8 vol);
void __music_player_set_output_to_mix_mute(MUSIC_PLAYER_OBJ *mapi, u8 flag);
void __music_player_set_mix_wait(MUSIC_PLAYER_OBJ *mapi);
void __music_player_set_mix_wait_ok(MUSIC_PLAYER_OBJ *mapi);
s8 __music_player_get_mix_wait_status(MUSIC_PLAYER_OBJ *mapi);
u32 __music_player_get_total_time(MUSIC_PLAYER_OBJ *mapi);
u32 __music_player_get_play_time(MUSIC_PLAYER_OBJ *mapi);
u32 __music_player_get_cur_file_index(MUSIC_PLAYER_OBJ *mapi, u8 isGetReal);
u32 __music_player_get_file_total(MUSIC_PLAYER_OBJ *mapi, u8 isGetReal);
u16	__music_player_get_cur_speed(MUSIC_PLAYER_OBJ *mapi);
u16	__music_player_get_cur_pitch(MUSIC_PLAYER_OBJ *mapi);
u8 __music_player_get_vocal_status(MUSIC_PLAYER_OBJ *mapi);
ENUM_PLAY_MODE __music_player_get_cur_play_mode(MUSIC_PLAYER_OBJ *mapi);
char *__music_player_get_dec_name(MUSIC_PLAYER_OBJ *mapi);
ENUM_DEV_SEL_MODE __music_player_get_dev_sel_mode(MUSIC_PLAYER_OBJ *mapi);
u32 __music_player_get_dev_let(MUSIC_PLAYER_OBJ *mapi);
u8  __music_player_get_phy_dev(MUSIC_PLAYER_OBJ *mapi);
lg_dev_info *__music_player_get_lg_dev_info(MUSIC_PLAYER_OBJ *mapi);
_DECODE_STATUS __music_player_get_status(MUSIC_PLAYER_OBJ *mapi);
u32 __music_player_file_operate_ctl(MUSIC_PLAYER_OBJ *mapi, u32 cmd, void *input, void *output);
void __music_player_clear_bp_info(MUSIC_PLAYER_OBJ *mapi);
tbool music_player_delete_playing_file(MUSIC_PLAYER_OBJ *mapi);
tbool music_player_fffr(MUSIC_PLAYER_OBJ *mapi, u8 fffr, u32 second);
_DECODE_STATUS music_player_pp(MUSIC_PLAYER_OBJ *mapi);
void music_player_stop(MUSIC_PLAYER_OBJ *mapi);
void music_player_close(MUSIC_PLAYER_OBJ *mapi);
tbool music_player_process(MUSIC_PLAYER_OBJ *mapi, void *task_name, u8 prio);
u32 music_player_play(MUSIC_PLAYER_OBJ *mapi);
u32 music_player_mulfile_play(MUSIC_PLAYER_OBJ *mapi, u32 StartAddr, u8 medType, mulfile_func_t *func);
u32 music_player_mulfile_purely_play(MUSIC_PLAYER_OBJ *mapi, u32 StartAddr, u8 medType, mulfile_func_t *func);
#endif

