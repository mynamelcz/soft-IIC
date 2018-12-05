#ifndef _SPEEX_ENCODE_API_H_
#define _SPEEX_ENCODE_API_H_
#include "dec/if_decoder_ctrl.h"


typedef struct _SPEEX_EN_FILE_IO_ {
    void *priv;
    u16(*input_data)(void *priv, s16 *buf, u8 len);          //这里的长度是多少个short
    void (*output_data)(void *priv, u8 *buf, u16 len);       //这里的长度是多少个bytes
} SPEEX_EN_FILE_IO;


typedef struct __SPEEX_ENC_OPS {
    u32(*need_buf)();
    void (*open)(u8 *ptr, SPEEX_EN_FILE_IO *audioIO, u8 complexity);
    u32(*run)(u8 *ptr);
} speex_enc_ops;


extern speex_enc_ops *get_speex_enc_obj();         //编码
extern audio_decoder_ops *get_speex_ops();         //解码
// extern speex_dec_ops *get_speex_dec_ops();         //解码


typedef struct __SPEEX_FILE_IO {
    /* void *hdl;	 */
    /* void *(*open)(u32 dev_let, char *path); */
    bool (*seek)(void *hdl, u8 type, u32 offsize);
    u16(*read)(void *hdl, u8 *buf, u16 len);
    u16(*write)(void *hdl, u8 *buf, u16 len);
} SPEEX_FILE_IO;

typedef struct __SPEEX_FILE {
    void *hdl;
    SPEEX_FILE_IO *_io;
    u32 read_len;
} SPEEX_FILE;

#endif


#if 0

//编码调用示例
{
    speex_enc_ops *tst_ops = get_speex_enc_obj();
    bufsize = tst_ops->need_buf();
    st = malloc(bufsize);
    tst_ops->open(st, &speex_en_io, 0);
    while (!tst_ops->run(st)) {            //读不到数的时候就会返回1了。
    }
    free(st);
}


//解码流程示例，跟其它格式解码接口统一
{
    audio_decoder_ops  *testdec_ops = get_speex_ops();
    bufsize = testdec_ops->need_dcbuf_size(&tmp);
    dec = malloc(bufsize);
    testdec_ops->open(dec, &speex_dec_io, NULL);

    while (!testdec_ops->run(dec, 0)) {
    }
    free(dec);

}



#endif
