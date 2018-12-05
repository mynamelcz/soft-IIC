#include "speex_encode.h"
#include "sdk_cfg.h"
#include "common/app_cfg.h"
#include "common/msg.h"
#include "cbuf/circular_buf.h"
#include "cpu/dac_param.h"
#include "dac/dac_track.h"
#include "dac/ladc_api.h"

#define SPEEX_ENCODE_DEBUG_ENABLE
#ifdef SPEEX_ENCODE_DEBUG_ENABLE
#define speex_encode_printf	printf
#else
#define speex_encode_printf(...)
#endif


#define SPEEX_ENCODE_BUF_SIZE			(1024*2)//(512)//
#define SPEEX_ENCODE_GET_DATA_SIZE		(SPEEX_ENCODE_BUF_SIZE/4)//(SPEEX_ENCODE_BUF_SIZE/8)//
#define SPEEX_ENCODE_WRITE_BUF_MIN   	(1024)//(256)//
#define SPEEX_ENCODE_WRITE_BUF_SIZE 	(SPEEX_ENCODE_WRITE_BUF_MIN*8)//(SPEEX_ENCODE_WRITE_BUF_MIN*8)
#define SPEEX_ENCODE_WRITE_FULL_LIMIT	(SPEEX_ENCODE_WRITE_BUF_SIZE - 40)
#define SPEEX_ENCODE_DELAY_LIMIT		(SPEEX_ENCODE_WRITE_BUF_SIZE/2)//(0)

#define SPEEX_ENCODE_SIZEOF_ALIN(var,al)     ((((var)+(al)-1)/(al))*(al))


struct __SPEEX_ENCODE {
    void 			*father_name;
    void 			*en_name;
    void 			*wr_name;
    void 			*ops_hdl;
    speex_enc_ops 	*ops;
    SPEEX_EN_FILE_IO _en_io;
    SPEEX_FILE 		 file;
    cbuffer_t 		 en_cbuf;
    cbuffer_t 		 wr_cbuf;
    OS_SEM		 	 en_sem;
    OS_SEM		 	 wr_sem;
    u32				 wr_delay;
    u32				 pcm_lost;
    u16				 samplerate;
    volatile u8 	 write_st;
    volatile u8 	 encode_st;
    volatile u8 	 is_play;
};

//static u8 pcm_16k_test_data[64] = {
//		0x00, 0x00, 0xFB, 0x30, 0x82, 0x5A, 0x40, 0x76, 0xFF, 0x7F, 0x41, 0x76, 0x82, 0x5A, 0xFB, 0x30,
//		0x00, 0x00, 0x04, 0xCF, 0x7F, 0xA5, 0xC0, 0x89, 0x01, 0x80, 0xBF, 0x89, 0x7F, 0xA5, 0x05, 0xCF,
//		0xFF, 0xFF, 0xFB, 0x30, 0x82, 0x5A, 0x40, 0x76, 0xFF, 0x7F, 0x41, 0x76, 0x82, 0x5A, 0xFB, 0x30,
//		0x00, 0x00, 0x05, 0xCF, 0x7E, 0xA5, 0xBF, 0x89, 0x01, 0x80, 0xC0, 0x89, 0x7E, 0xA5, 0x05, 0xCF
//};
//
//static u32 get_test_data_len = 0;
//static u8 pcm_500hz_data[128] = {
//	0x00, 0x00, 0xF8, 0x18, 0xFC, 0x30, 0x1C, 0x47, 0x82, 0x5A, 0x6D, 0x6A, 0x41, 0x76, 0x89, 0x7D,
//	0xFF, 0x7F, 0x89, 0x7D, 0x41, 0x76, 0x6D, 0x6A, 0x82, 0x5A, 0x1C, 0x47, 0xFC, 0x30, 0xF9, 0x18,
//	0x00, 0x00, 0x08, 0xE7, 0x05, 0xCF, 0xE3, 0xB8, 0x7E, 0xA5, 0x93, 0x95, 0xBF, 0x89, 0x76, 0x82,
//	0x01, 0x80, 0x77, 0x82, 0xBF, 0x89, 0x93, 0x95, 0x7E, 0xA5, 0xE3, 0xB8, 0x05, 0xCF, 0x07, 0xE7,
//	0x00, 0x00, 0xF8, 0x18, 0xFB, 0x30, 0x1D, 0x47, 0x82, 0x5A, 0x6D, 0x6A, 0x41, 0x76, 0x8A, 0x7D,
//	0xFF, 0x7F, 0x89, 0x7D, 0x40, 0x76, 0x6C, 0x6A, 0x82, 0x5A, 0x1C, 0x47, 0xFB, 0x30, 0xF8, 0x18,
//	0x01, 0x00, 0x07, 0xE7, 0x05, 0xCF, 0xE4, 0xB8, 0x7E, 0xA5, 0x94, 0x95, 0xC0, 0x89, 0x76, 0x82,
//	0x01, 0x80, 0x77, 0x82, 0xBF, 0x89, 0x93, 0x95, 0x7E, 0xA5, 0xE3, 0xB8, 0x05, 0xCF, 0x07, 0xE7
//};

/* 编码调用示例 */
/* { */
/* speex_enc_ops *tst_ops=get_speex_enc_obj(); */
/* bufsize=tst_ops->need_buf(); */
/* st=malloc(bufsize); */
/* tst_ops->open(st,&speex_en_io,0);         */
/* while(!tst_ops->run(st))               //读不到数的时候就会返回1了。 */
/* { */
/* } */
/* free(st); */
/* } */

/* u16 (*input_data)(void *priv,s16 *buf,u8 len);           //这里的长度是多少个short */
/* void (*output_data)(void *priv,u8 *buf,u16 len);         //这里的长度是多少个bytes */
SPEEX_ENCODE *g_speex_encode = NULL;


static void speex_encode_ladc_isr_callback(void *ladc_buf, u32 buf_flag, u32 buf_len)
{
    s16 *res;
    s16 *ladc_mic;

    //dual buf
    res = (s16 *)ladc_buf;
    res = res + (buf_flag * DAC_SAMPLE_POINT);

    //ladc_buf offset
    /* ladc_l = res; */
    /* ladc_r = res + DAC_DUAL_BUF_LEN; */


    ladc_mic = res + (2 * DAC_DUAL_BUF_LEN);
    if (g_speex_encode != NULL) {

        if ((cbuf_get_data_size(&(g_speex_encode->wr_cbuf))) > SPEEX_ENCODE_WRITE_FULL_LIMIT) { //判断是否压缩完能写进写缓存
            speex_encode_printf("f");
            return ;
        }

        cbuffer_t *cbuffer = &(g_speex_encode->en_cbuf);

        if ((cbuffer->total_len - cbuffer->data_len) < DAC_DUAL_BUF_LEN) {
            g_speex_encode->pcm_lost ++;
            /* speex_encode_printf("x"); */
        } else {

            cbuf_write(cbuffer, (void *)ladc_mic, DAC_DUAL_BUF_LEN);
            /* cbuf_write(cbuffer, (void *)(pcm_500hz_data + get_test_data_len), DAC_DUAL_BUF_LEN); */
            /* get_test_data_len += DAC_DUAL_BUF_LEN; */
            /* if(get_test_data_len >= sizeof(pcm_500hz_data)) */
            /* { */
            /* get_test_data_len = 0; */
            /* } */
        }

        if (cbuffer->data_len >= SPEEX_ENCODE_GET_DATA_SIZE) {
            os_sem_set(&(g_speex_encode->en_sem), 0);
            os_sem_post(&(g_speex_encode->en_sem));
        }
    }
}


static u16 speex_encode_input(void *priv, s16 *buf, u8 len)
{
    SPEEX_ENCODE *obj = (SPEEX_ENCODE *)priv;
    if (obj == NULL) {
        return 0;
    }
    /* if(len!=160) */
    /* return 0; */
    u32 rlen = len << 1;
    while (1) {
        if ((obj->encode_st == 0) || (obj->is_play == 0)) {
            break;
        }

        if (cbuf_get_data_size(&(obj->en_cbuf)) >= rlen) {
            cbuf_read(&(obj->en_cbuf), (void *)buf, rlen);

            return rlen;
        }

        os_sem_pend(&(obj->en_sem), 0);
    }

    return 0;
}
static void speex_encode_output(void *priv, u8 *buf, u16 len)
{
    SPEEX_ENCODE *obj = (SPEEX_ENCODE *)priv;
    if (obj == NULL) {
        return ;
    }

    if (len <= cbuf_is_write_able(&(obj->wr_cbuf), len)) {
        cbuf_write(&(obj->wr_cbuf), (void *)buf, len);
    } else {
        speex_encode_printf("l");
    }

    if (obj->wr_delay) {
        if (obj->wr_delay > len) {
            obj->wr_delay -= len;
            return ;
            obj->wr_delay = 0;
        }
    }


    os_sem_set(&(obj->wr_sem), 0);
    os_sem_post(&(obj->wr_sem));
}

static int speex_encode_write(SPEEX_ENCODE *obj)
{
    u8 *addr  = NULL;
    u32 rlen = 0;
    addr = cbuf_read_alloc(&(obj->wr_cbuf), &rlen);
    if (addr == NULL || rlen == 0) {
        return 1;
    }

    if (obj->write_st == 0) {
        return 1;
    }

    if (obj->file._io == NULL) {
        speex_encode_printf("file io is null !!!, fun = %s, line = %d\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (obj->file._io->write == NULL) {
        speex_encode_printf("file write io is null !!!\n");
        return -1;
    }

    if (obj->file._io->write(obj->file.hdl, addr, rlen) == 0) {
        speex_encode_printf("file write err !!!\n");
        return -1;
    }
    cbuf_read_updata(&(obj->wr_cbuf), rlen);
    return 0;
}



static void speex_encode_deal(void *parg)
{
    u8 ret;
    u32 err = 0;
    int msg[3] = {0};
    SPEEX_ENCODE *obj = (SPEEX_ENCODE *)parg;
    while (1) {
        if (obj->encode_st) {
            ret = OSTaskQAccept(ARRAY_SIZE(msg), msg);
            if (ret == OS_Q_EMPTY) {
                err = obj->ops->run(obj->ops_hdl);
                if (err == 0) {
                    continue;
                }
            }
        } else {
            /* speex_encode_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__); */
            os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        }

        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            /* speex_encode_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__); */
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;

        case MSG_SPEEX_START:
            /* speex_encode_printf("fun = %s, line = %d\n", __func__, __LINE__); */
            obj->encode_st = 1;
            if (obj->wr_name) {
                os_taskq_post(obj->wr_name, 1, MSG_SPEEX_START);
                while (obj->write_st == 0) {
                    OSTimeDly(1);
                }
            }

            /* speex_encode_printf("fun = %s, line = %d\n", __func__, __LINE__); */
            break;
        case MSG_SPEEX_STOP:
            /* speex_encode_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__); */
            obj->encode_st = 0;
            if (obj->wr_name) {
                os_sem_set(&(obj->wr_sem), 0);
                os_sem_post(&(obj->wr_sem));
                os_taskq_post(obj->wr_name, 1, MSG_SPEEX_STOP);
                while (obj->write_st) {
                    OSTimeDly(1);
                }
            }

            /* speex_encode_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__); */
            break;
        default:
            break;
        }
    }
}




static void speex_encode_write_deal(void *parg)
{
    int err = 0;
    u8 ret;
    u8 write_err_flag = 0;
    int msg[3] = {0};
    SPEEX_ENCODE *obj = (SPEEX_ENCODE *)parg;
    /* obj->write_st = 1; */
    while (1) {
        if ((write_err_flag == 0) && (obj->write_st == 1)) {
            ret = OSTaskQAccept(ARRAY_SIZE(msg), msg);
            if (ret == OS_Q_EMPTY) {
                err = speex_encode_write(obj);
                if (err == 1) {
                    /* printf("p"); */
                    os_sem_pend(&(obj->wr_sem), 0);
                } else if (err == 0) {
                    continue;
                } else {
                    write_err_flag = 1;
                    os_taskq_post(obj->father_name, 1, MSG_SPEEX_ENCODE_ERR);
                }
            }
        } else {
            /* obj->w_busy = 0; */
            os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        }

        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;
        case MSG_SPEEX_START:
            /* speex_encode_printf("fun = %s, line = %d\n", __func__, __LINE__); */
            obj->write_st = 1;
            break;
        case MSG_SPEEX_STOP:
            obj->write_st = 0;
            break;
        default:
            break;
        }
    }
}


tbool speex_encode_process(SPEEX_ENCODE *obj, void *task_name, u8 prio)
{
    if (obj == NULL) {
        return false;
    }

    if (obj->file._io == NULL) {
        return false;
    }

    u32 err;
    err = os_task_create(speex_encode_deal,
                         (void *)obj,
                         prio,
                         10
#if OS_TIME_SLICE_EN > 0
                         , 1
#endif
                         , task_name);

    if (err) {
        speex_encode_printf("speex encode task err\n");
        return false;
    }

    obj->en_name = task_name;
    return true;
}


tbool speex_encode_write_process(SPEEX_ENCODE *obj, void *task_name, u8 prio)
{
    if (obj == NULL) {
        return false;
    }

    if (obj->file._io == NULL) {
        return false;
    }

    u32 err;
    err = os_task_create(speex_encode_write_deal,
                         (void *)obj,
                         prio,
                         10
#if OS_TIME_SLICE_EN > 0
                         , 1
#endif
                         , task_name);

    if (err) {
        speex_encode_printf("speex encode write task err\n");
        return false;
    }

    obj->wr_name = task_name;
    return true;
}


void __speex_encode_set_file_io(SPEEX_ENCODE *obj, SPEEX_FILE_IO *_io, void *hdl)
{
    if (obj == NULL) {
        return ;
    }

    obj->file.hdl = hdl;
    obj->file._io = _io;
}

void __speex_encode_set_samplerate(SPEEX_ENCODE *obj, u16 samplerate)
{
    if (obj == NULL) {
        return ;
    }

    obj->samplerate = samplerate;
}

void __speex_encode_set_father_name(SPEEX_ENCODE *obj, const char *father_name)
{
    if (obj == NULL) {
        return ;
    }

    obj->father_name = (void *)father_name;
}

tbool speex_encode_start(SPEEX_ENCODE *obj)
{
    if (obj == NULL) {
        return false;
    }
    if (obj->encode_st == 1) {
        speex_encode_printf("speex encode is aready start !!\n");
        return false;
    }
    obj->is_play = 1;
    obj->wr_delay = SPEEX_ENCODE_DELAY_LIMIT;
    g_speex_encode = obj;
    if (obj->en_name) {
        ladc_reg_isr_cb_api(speex_encode_ladc_isr_callback);
        ladc_enable(ENC_MIC_CHANNEL, obj->samplerate, VCOMO_EN);
        ladc_mic_gain(60, 0); //设置mic音量,这一句要在ladc_enable(ENC_MIC_CHANNEL,LADC_SR16000, VCOMO_EN);后面
        os_taskq_post(obj->en_name, 1, MSG_SPEEX_START);
        while (obj->encode_st == 0) {
            OSTimeDly(1);
        }
        /* speex_encode_printf("fun = %s, line = %d\n", __func__, __LINE__); */
        return true;
    }

    return false;
}


tbool speex_encode_stop(SPEEX_ENCODE *obj)
{
    if (obj == NULL) {
        return false;
    }
    if (obj->encode_st == 0) {
        speex_encode_printf("speex encode is aready stop!!\n");
        return false;
    }

    obj->is_play = 0;
    if (obj->en_name) {

        os_sem_set(&(obj->en_sem), 0);
        os_sem_post(&(obj->en_sem));

        /* speex_encode_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__); */
        os_taskq_post(obj->en_name, 1, MSG_SPEEX_STOP);
        while (obj->encode_st == 1) {
            OSTimeDly(1);
            /* speex_encode_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__); */
        }

        /* speex_encode_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__); */
        ladc_disable(ENC_MIC_CHANNEL);
        g_speex_encode = NULL;
        return true;
    }

    /* speex_encode_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__); */
    return false;
}



SPEEX_ENCODE *speex_encode_creat(void)
{
    u8 *e_buf = NULL;
    u8 *w_buf = NULL;
    u8 *need_buf;
    u32 need_buf_len;
    u32 speex_enc_need_buf_len;
    speex_enc_ops *tmp_ops = get_speex_enc_obj();
    speex_enc_need_buf_len = tmp_ops->need_buf();

    need_buf_len = SPEEX_ENCODE_SIZEOF_ALIN(sizeof(SPEEX_ENCODE), 4)
                   + SPEEX_ENCODE_SIZEOF_ALIN(speex_enc_need_buf_len, 4)
                   + SPEEX_ENCODE_SIZEOF_ALIN(SPEEX_ENCODE_BUF_SIZE, 4)
                   + SPEEX_ENCODE_SIZEOF_ALIN(SPEEX_ENCODE_WRITE_BUF_SIZE, 4);

    speex_encode_printf("need_buf_len = %d!!\n", need_buf_len);
    need_buf = (u8 *)calloc(1, need_buf_len);
    if (need_buf == NULL) {
        return NULL;
    }
    SPEEX_ENCODE *obj = (SPEEX_ENCODE *)need_buf;

    need_buf += SPEEX_ENCODE_SIZEOF_ALIN(sizeof(SPEEX_ENCODE), 4);
    obj->ops_hdl = (void *)need_buf;

    need_buf += SPEEX_ENCODE_SIZEOF_ALIN(speex_enc_need_buf_len, 4);
    e_buf = (u8 *)need_buf;


    need_buf += SPEEX_ENCODE_SIZEOF_ALIN(SPEEX_ENCODE_BUF_SIZE, 4);
    w_buf = (u8 *)need_buf;

    obj->ops = tmp_ops;

    obj->_en_io.priv =  obj;
    obj->_en_io.input_data = (void *)speex_encode_input;
    obj->_en_io.output_data = (void *)speex_encode_output;
    obj->ops->open(obj->ops_hdl, &(obj->_en_io), 0);

    cbuf_init(&(obj->en_cbuf), e_buf, SPEEX_ENCODE_BUF_SIZE);
    cbuf_init(&(obj->wr_cbuf), w_buf, SPEEX_ENCODE_WRITE_BUF_SIZE);

    if (os_sem_create(&obj->en_sem, 0) != OS_NO_ERR) {
        free(need_buf);
        return NULL;
    }

    if (os_sem_create(&obj->wr_sem, 0) != OS_NO_ERR) {
        free(need_buf);
        return NULL;
    }

    obj->encode_st = 0;
    obj->write_st = 0;
    obj->is_play = 0;

    speex_encode_printf("speex_encode_creat ok!!\n");

    return obj;
}


void speex_encode_destroy(SPEEX_ENCODE **obj)
{
    if ((obj == NULL) || ((*obj) == NULL)) {
        return;
    }
    speex_encode_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
    SPEEX_ENCODE *tmp_obj = *obj;
    speex_encode_stop(tmp_obj);
    speex_encode_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
    if (tmp_obj->en_name) {
        if (os_task_del_req(tmp_obj->en_name) != OS_TASK_NOT_EXIST) {
            os_taskq_post_event(tmp_obj->en_name, 1, SYS_EVENT_DEL_TASK);
            do {
                OSTimeDly(1);
                speex_encode_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
            } while (os_task_del_req(tmp_obj->en_name) != OS_TASK_NOT_EXIST);
        }
    }

    speex_encode_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
    if (tmp_obj->wr_name) {
        if (os_task_del_req(tmp_obj->wr_name) != OS_TASK_NOT_EXIST) {
            os_taskq_post_event(tmp_obj->wr_name, 1, SYS_EVENT_DEL_TASK);
            do {
                OSTimeDly(1);
                speex_encode_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
            } while (os_task_del_req(tmp_obj->wr_name) != OS_TASK_NOT_EXIST);
        }
    }
    speex_encode_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
    free(*obj);
    *obj = NULL;
}


