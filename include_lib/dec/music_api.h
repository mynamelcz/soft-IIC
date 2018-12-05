#ifndef _MUSIC_API_H_
#define _MUSIC_API_H_

#include "file_operate/file_op.h"
#include "decoder_phy.h"
#include "file_operate/fs_io_h.h"

/*
typedef enum __FORMAT_STATUS
{
}_FORMAT_STATUS;
*/

typedef enum __MUSIC_OP_STU {
    MUSIC_OP_STU_NONE = 0x0,
    MUSIC_OP_STU_NO_DEC,
    MUSIC_OP_STU_AB_REPEAT,
    MUSIC_OP_STU_VOCAL,
} MUSIC_OP_STU;


typedef struct __MUSIC_API {
    void *dec_phy_name;
    void *file_type;
    u32 file_num;
    DEC_API dec_api;
    DEC_API_IO *io;
    FS_BRK_POINT *brk;
} _MUSIC_API;

typedef struct _MUSIC_OP_API_ {
    FILE_OPERATE *fop_api;
    _MUSIC_API   *dop_api;
    void		 *ps_api;
    u32 		  status;
    void 		 *status_p;
} MUSIC_OP_API;


typedef void *(*GET_ID3_FUN)(u32 len);


// u32 music_play(MUSIC_OP_API *m_op_api, ENUM_DEV_SEL_MODE dev_sel, u32 dev_let, ENUM_FILE_SELECT_MODE file_sel, void *pra);
u32 music_play(MUSIC_OP_API *m_op_api);
void music_stop_decoder(MUSIC_OP_API *parm);
void music_decoder_info(void *before_fun, void *after_fun);

void reg_get_id3v2_buf(GET_ID3_FUN fun);
// MP3_ID3 *get_id3v2_info(void);

#ifdef MULTI_FILE_EN
void file_play_stop(MUSIC_OP_API *m_op_api);
u32  file_play_start(MUSIC_OP_API *m_op_api, u32 StartAddr, u8 medType, mulfile_func_t *func);
#endif

#endif
