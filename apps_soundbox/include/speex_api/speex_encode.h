#ifndef __SPEEX_ENCODE_H__
#define __SPEEX_ENCODE_H__
#include "typedef.h"
#include "speex/speex_encode_api.h"

typedef struct __SPEEX_ENCODE SPEEX_ENCODE;

void __speex_encode_set_file_io(SPEEX_ENCODE *obj, SPEEX_FILE_IO *_io, void *hdl);
void __speex_encode_set_samplerate(SPEEX_ENCODE *obj, u16 samplerate);
void __speex_encode_set_father_name(SPEEX_ENCODE *obj, const char *father_name);
tbool speex_encode_process(SPEEX_ENCODE *obj, void *task_name, u8 prio);
tbool speex_encode_write_process(SPEEX_ENCODE *obj, void *task_name, u8 prio);
tbool speex_encode_start(SPEEX_ENCODE *obj);
tbool speex_encode_stop(SPEEX_ENCODE *obj);
SPEEX_ENCODE *speex_encode_creat(void);
void speex_encode_destroy(SPEEX_ENCODE **obj);

#endif

