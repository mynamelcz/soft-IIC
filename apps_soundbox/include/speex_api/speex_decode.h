#ifndef __SPEEX_DECODE_H__
#define __SPEEX_DECODE_H__
#include "typedef.h"
#include "speex/speex_encode_api.h"
typedef struct __SPEEX_DECODE SPEEX_DECODE;

u8 speex_decode_get_status(SPEEX_DECODE *obj);
tbool speex_deoce_start(SPEEX_DECODE *obj);
tbool speex_decode_stop(SPEEX_DECODE *obj);

tbool speex_decode_process(SPEEX_DECODE *obj, void *task_name, u8 prio);
void __speex_decode_set_file_io(SPEEX_DECODE *obj, SPEEX_FILE_IO *_io, void *hdl);
SPEEX_DECODE *speex_decode_creat(void);
void speex_decode_destroy(SPEEX_DECODE **obj);

#endif
