#include "speex_decode.h"
#include "sdk_cfg.h"
#include "common/app_cfg.h"
#include "common/msg.h"
/* #include "cbuf/circular_buf.h" */
#include "cpu/dac_param.h"
#include "dac/dac_track.h"
#include "dac/ladc_api.h"

#define SPEEX_DECODE_DEBUG_ENABLE
#ifdef SPEEX_DECODE_DEBUG_ENABLE
#define speex_decode_printf	printf
#else
#define speex_decode_printf(...)
#endif

#define SPEEX_DECODE_SIZEOF_ALIN(var,al)     ((((var)+(al)-1)/(al))*(al))

struct __SPEEX_DECODE {
    void 				 *dec_name;
    void 				 *ops_hdl;
    audio_decoder_ops    *ops;
    struct if_decoder_io  _dec_io;
    SPEEX_FILE 		 	  file;
    volatile u8			  status;
};

#if 0
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

#if 1
static u32 speex_decode_get_lslen(void *priv)
{
    u32 ret;
    ret = -1;
    return ret;
}

static u32 speex_decode_store_rev_data(void *priv, u32 addr, int len)
{
    return len;
}

static s32 speex_decod_check_buf(void *priv, u32 faddr, u8 *buf)
{
    SPEEX_FILE *file = (SPEEX_FILE *)priv;
    s32 read_len = 0;
    u16 len;
    if (file->_io == NULL) {
        return 0;
    }
    file->_io->seek(file->hdl, 0x1, faddr);
    len = file->_io->read(file->hdl, buf, file->read_len);
    if ((u16) - 1 == len) {
        return 0;
    }

    read_len = len;
    return read_len;
}

static s32 speex_decode_input(void *priv, u32 faddr, u8 *buf, u32 len, u8 type)
{
    SPEEX_FILE *file = (SPEEX_FILE *)priv;
    u16 read_len0 = 0;

    if (file->_io == NULL) {
        return 0;
    }

    if (0 == type) {
        file->_io->seek(file->hdl, 0x1, faddr);
        read_len0 = file->_io->read(file->hdl, buf, len);
    } else {
        file->read_len = len;
    }

    if ((u16) - 1 == read_len0) {
        return 0;
    } else {
        return read_len0;
    }
}

static void speex_decode_output(void *priv, void *buf, u32 len)
{
    dac_write((u8 *)buf, len);
#if 0
    DEC_API_IO *api_io = priv;
    if (api_io->op_cnt > len) {
        api_io->op_cnt -= len;
    } else {
        api_io->op_cnt = 0;
    }
    api_io->output.output(api_io->output.priv, buf, len);
#endif
}


static void speex_decode_deal(void *parg)
{
    u8 ret;
    u32 err = 0;
    int msg[3] = {0};
    SPEEX_DECODE *obj = (SPEEX_DECODE *)parg;
    while (1) {
        if (obj->status == 1) {
            ret = OSTaskQAccept(ARRAY_SIZE(msg), msg);
            if (ret == OS_Q_EMPTY) {
                err = obj->ops->run(obj->ops_hdl, 0);
                if (err == 0) {
                    continue;
                } else {
                    obj->status = 0;
                }
            }
        } else {
            os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        }

        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;

        case MSG_SPEEX_START:
            obj->status = 1;
            break;
        case MSG_SPEEX_STOP:
            obj->status = 0;
            break;
        default:
            break;
        }
    }
}


tbool speex_decode_process(SPEEX_DECODE *obj, void *task_name, u8 prio)
{
    printf("fun = %s, line = %d !!!\n", __func__, __LINE__);
    if (obj == NULL) {
        return false;
    }

    printf("fun = %s, line = %d !!!\n", __func__, __LINE__);
    if (obj->file._io == NULL) {
        return false;
    }

    printf("fun = %s, line = %d !!!\n", __func__, __LINE__);
#if 1
    obj->_dec_io.priv = &(obj->file);
    obj->_dec_io.input = (void *)speex_decode_input;
    obj->_dec_io.check_buf = (void *)speex_decod_check_buf;
    obj->_dec_io.output = (void *)speex_decode_output;
    obj->_dec_io.get_lslen = (void *)speex_decode_get_lslen;
    obj->_dec_io.store_rev_data = (void *)speex_decode_store_rev_data;
#endif

    obj->ops->open(obj->ops_hdl, &obj->_dec_io, NULL);

    u32 err;
    err = os_task_create(speex_decode_deal,
                         (void *)obj,
                         prio,
                         10
#if OS_TIME_SLICE_EN > 0
                         , 1
#endif
                         , task_name);

    if (err) {
        speex_decode_printf("speex encode task err\n");
        return false;
    }

    obj->dec_name = task_name;
    return true;
}

void __speex_decode_set_file_io(SPEEX_DECODE *obj, SPEEX_FILE_IO *_io, void *hdl)
{
    if (obj == NULL) {
        return;
    }

    obj->file.hdl = hdl;
    obj->file._io = _io;
}

SPEEX_DECODE *speex_decode_creat(void)
{
    u8 *need_buf;
    u32 need_buf_len;
    u32 speex_dec_need_buf_len;
    audio_decoder_ops  *tmp_ops = get_speex_ops();
    speex_dec_need_buf_len = tmp_ops->need_dcbuf_size();
    need_buf_len = SPEEX_DECODE_SIZEOF_ALIN(sizeof(SPEEX_DECODE), 4)
                   + SPEEX_DECODE_SIZEOF_ALIN(speex_dec_need_buf_len, 4);

    speex_decode_printf("need_buf_len = %d!!\n", need_buf_len);
    need_buf = (u8 *)calloc(1, need_buf_len);
    if (need_buf == NULL) {
        return NULL;
    }
    SPEEX_DECODE *obj = (SPEEX_DECODE *)need_buf;

    need_buf += SPEEX_DECODE_SIZEOF_ALIN(sizeof(SPEEX_DECODE), 4);
    obj->ops_hdl = (void *)need_buf;

    obj->ops = tmp_ops;

    return obj;

}


void speex_decode_destroy(SPEEX_DECODE **obj)
{
    if (obj == NULL || (*obj) == NULL) {
        return;
    }
    SPEEX_DECODE *tmp_obj = (*obj);
    if (tmp_obj->dec_name) {
        if (os_task_del_req(tmp_obj->dec_name) != OS_TASK_NOT_EXIST) {
            os_taskq_post_event(tmp_obj->dec_name, 1, SYS_EVENT_DEL_TASK);
            do {
                OSTimeDly(1);
            } while (os_task_del_req(tmp_obj->dec_name) != OS_TASK_NOT_EXIST);
        }
    }
    free(*obj);
    *obj = NULL;
}



tbool speex_deoce_start(SPEEX_DECODE *obj)
{
    if (obj == NULL) {
        return false;
    }
    if (obj->status == 1) {
        speex_decode_printf("speex decode is aready start !!\n");
        return true;
    }
    if (obj->dec_name) {
        os_taskq_post(obj->dec_name, 1, MSG_SPEEX_START);
        while (obj->status == 0) {
            OSTimeDly(1);
        }

        speex_decode_printf("fun = %s, line = %d\n", __func__, __LINE__);
        return true;
    }

    return false;
}


tbool speex_decode_stop(SPEEX_DECODE *obj)
{
    if (obj == NULL) {
        return false;
    }
    if (obj->status == 0) {
        speex_decode_printf("speex encode is aready stop!!\n");
        return true;
    }

    if (obj->dec_name) {
        os_taskq_post(obj->dec_name, 1, MSG_SPEEX_STOP);
        while (obj->status == 1) {
            OSTimeDly(1);
        }

        return true;
    }

    return false;
}

u8 speex_decode_get_status(SPEEX_DECODE *obj)
{
    if (obj == NULL) {
        return 0;
    }

    return obj->status;
}


#endif
