#include "sound_mix.h"
#include "typedef.h"
#include "cpu/dac_param.h"
#include "cbuf/circular_buf.h"
#include "common/printf.h"
#include "string.h"
#include "mem/malloc.h"
#include "common/msg.h"
#include "rtos/os_api.h"
#include "dac/src_api.h"
#include "uart.h"

//#define SOUND_MIX_DEBUG_ENABLE
#ifdef SOUND_MIX_DEBUG_ENABLE
#define sound_mix_printf	printf
#else
#define sound_mix_printf(...)
#endif


#define SOUND_MIX_USE_ZEBRA_MEMERY_ENABLE	//是否使用zebra ram进行内存分配

#define SIZEOF_ALIN(var,al)     ((((var)+(al)-1)/(al))*(al))

#define SOUND_MIX_TRACK_SINGLE	0
#define SOUND_MIX_TRACK_DUAL	1

#define SOUND_MIX_CHECK_DATA_MIN		(DAC_BUF_LEN*2)
#define SOUND_MIX_ONCE_BUF_LEN			(DAC_BUF_LEN)
#define SOUND_MIX_ONCE_READ_POINT_NUM	(DAC_BUF_LEN/2)

typedef struct __SOUND_MIX_FILTER {
    /* s32	 	 filter_tmp_buf[SOUND_MIX_ONCE_READ_POINT_NUM]; */
    u32	 	 members_cnt;
    u32 	*members;
    void 	*priv;
    void	(*cbk)(void *priv, u8 *fil_buf, u8 *org_buf, u32 len);
} SOUND_MIX_FILTER;

struct __SOUND_MIX_OBJ {
    void 		*task_name;
    /* s16			 mix_buf[SOUND_MIX_ONCE_READ_POINT_NUM]; */
    /* s32			 mix_tmp_buf[SOUND_MIX_ONCE_READ_POINT_NUM]; */
    u16 		 samplerate;
    u8			 with_task;
    _OP_IO		 output;
    DEC_OP_CB	 op_cb;
    DEC_SET_INFO_CB set_info_cb;
    OS_MUTEX	 mutex;
    SOUND_MIX_FILTER *filter;
    struct list_head head;
};



typedef enum __SOUND_MIX_FLAG {
    SOUND_MIX_FLAG_DATA_EMPTY = 0x0,
    SOUND_MIX_FLAG_DATA_ENOUGH,
} SOUND_MIX_FLAG;




struct __SOUND_MIX_CHL {
    struct list_head list;
    const char  	*key;
    SOUND_MIX_OBJ 	*mix_obj;
    void 			*src_obj;
    MIX_CHL_CBK		 chl_cbk;
    SOUND_MIX_SRC_P	 src_p;
    u16				 samplerate;
    u8 			 	 track;
    volatile u8 	 flag;
    volatile u8 	 wait;
    u32 		 	 rlimit;
    DAC_DIGIT_VAR	 dac_var;
    cbuffer_t 	 	 cbuf;
    OS_SEM		 	 sem;
    OS_MUTEX 	 	 mutex;
};


#ifdef SOUND_MIX_USE_ZEBRA_MEMERY_ENABLE

extern void *zebra_malloc(u32 size);
extern void zebra_free(void *p);

void *sound_mix_zebra_calloc(u32 count, u32 size)
{
    void *p;
    sound_mix_printf("sound mix apply size = %d\n", count * size);
    p = zebra_malloc(count * size);
    if (p) {
        /* zero the memory */
        memset(p, 0, count * size);
    }
    return p;
}

void sound_mix_zebra_free(void *p)
{
    zebra_free(p);
}

#define sound_mix_calloc	sound_mix_zebra_calloc
#define sound_mix_free		sound_mix_zebra_free
#else
#define sound_mix_calloc	calloc
#define sound_mix_free		free
#endif//SOUND_MIX_USE_ZEBRA_MEMERY_ENABLE

//
//static u8 pcm_16k_test_data[64] = {
//		0x00, 0x00, 0xFB, 0x30, 0x82, 0x5A, 0x40, 0x76, 0xFF, 0x7F, 0x41, 0x76, 0x82, 0x5A, 0xFB, 0x30,
//		0x00, 0x00, 0x04, 0xCF, 0x7F, 0xA5, 0xC0, 0x89, 0x01, 0x80, 0xBF, 0x89, 0x7F, 0xA5, 0x05, 0xCF,
//		0xFF, 0xFF, 0xFB, 0x30, 0x82, 0x5A, 0x40, 0x76, 0xFF, 0x7F, 0x41, 0x76, 0x82, 0x5A, 0xFB, 0x30,
//		0x00, 0x00, 0x05, 0xCF, 0x7E, 0xA5, 0xBF, 0x89, 0x01, 0x80, 0xC0, 0x89, 0x7E, 0xA5, 0x05, 0xCF
//};
//void test_audio_output(void *outbuf, u32 len)
//{
//	static u32 cnt = 0;
//	u8 *inbuf = pcm_16k_test_data;
//	u8 *outbuf_tmp = (u8*)outbuf;
//	u32 tmp_len = len;
//
//	u32 i = 0;
//
//	while(1)
//	{
//		if(cnt >= 64)
//			cnt = 0;
//		outbuf_tmp[i] = inbuf[cnt];
//		i++;
//		cnt++;
//		if(i>=len)
//			break;
//	}
//}
//

static void sound_mix_mutex_enter(SOUND_MIX_OBJ *obj)
{
    if (obj) {
        if (obj->with_task) {
            os_mutex_pend(&obj->mutex, 0);
        } else {
            OS_ENTER_CRITICAL();
        }
    }
}

static void sound_mix_mutex_exit(SOUND_MIX_OBJ *obj)
{
    if (obj) {
        if (obj->with_task) {
            os_mutex_post(&obj->mutex);
        } else {
            OS_EXIT_CRITICAL();
        }
    }
}

static void *sound_mix_chl_write(void *priv, u8 *buf, u16 len)
{
    SOUND_MIX_CHL *mix_chl = (SOUND_MIX_CHL *)priv;
    /* sound_mix_printf("mix_chl = %x, %d\n", mix_chl, __LINE__); */
    if (mix_chl == NULL) {
        return NULL;
    }

    /* if(strcmp(mix_chl->key, "MUSIC_MIX") == 0) */
    /* { */
    /* putchar('@'); */
    /* } */
    /* sound_mix_printf("w"); */
    u32 wlen = len;
    u32 limit = mix_chl->cbuf.total_len >> 1;

    os_mutex_pend(&mix_chl->mutex, 0);
    while (1) {
        if (len > limit) {
            wlen = 	limit;
        } else {
            wlen = len;
        }

        wlen = cbuf_write(&mix_chl->cbuf, buf, wlen);
        if ((wlen != 0) && (mix_chl->flag == SOUND_MIX_FLAG_DATA_EMPTY)) {
            if (mix_chl->cbuf.data_len >= limit) {
                mix_chl->flag = SOUND_MIX_FLAG_DATA_ENOUGH;
            }
        }
        len -= wlen;
        if (len == 0) {
            break;
        }
        buf += wlen;
        /* sound_mix_printf("a"); */
        os_sem_pend(&mix_chl->sem, 0);
        /* sound_mix_printf("b"); */
    }
    os_mutex_post(&mix_chl->mutex);
    return priv;
}


static void sound_mix_chl_clear(SOUND_MIX_CHL *mix_chl)
{
    os_mutex_pend(&mix_chl->mutex, 0);
    cbuf_clear(&mix_chl->cbuf);
    os_mutex_post(&mix_chl->mutex);
}


static u32 sound_mix_check_chl_data(SOUND_MIX_CHL *mix_chl)
{
    if (mix_chl == NULL) {
        return 0;
    }
    return mix_chl->cbuf.data_len;
}


static void sound_mix_chl_wait_stop(SOUND_MIX_CHL *mix_chl)
{

    /* if(strcmp(mix_chl->key, "MUSIC_MIX") == 0) */
    /* { */
    /* } */
    os_mutex_pend(&mix_chl->mutex, 0);
    if (mix_chl->wait == 0) {
        while (mix_chl->flag) {
            os_time_dly(1);
        }
    }
    cbuf_clear(&mix_chl->cbuf);
    mix_chl->flag = SOUND_MIX_FLAG_DATA_EMPTY;
    mix_chl->samplerate = 0;//reset src
    os_mutex_post(&mix_chl->mutex);
}


static void sound_mix_chl_op_data_clear(void *priv)
{
    SOUND_MIX_CHL *mix_chl = (SOUND_MIX_CHL *)priv;
    sound_mix_chl_wait_stop(mix_chl);
}


static tbool sound_mix_chl_op_data_check_over(void *priv)
{
    SOUND_MIX_CHL *mix_chl = (SOUND_MIX_CHL *)priv;

    if (mix_chl->flag != SOUND_MIX_FLAG_DATA_ENOUGH || mix_chl->wait == 1) {
        return true;
    }


    if (sound_mix_check_chl_data(mix_chl) < SOUND_MIX_CHECK_DATA_MIN) {
        return true;
    }
    return false;
}

static void sound_mix_filter_deal(SOUND_MIX_OBJ *obj, SOUND_MIX_CHL *chl, s16 *mix_buf, s32 *filter_tmp_buf)
{
    if (obj == NULL) {
        return ;
    }

    if (obj->filter) {
        u32 i;
        for (i = 0; i < obj->filter->members_cnt; i++) {
            if (obj->filter->members[i]) {
                if (0 == strcmp(chl->key, (const char *)obj->filter->members[i])) {
                    return ;
                }
            }
        }

        dac_mix_buf(filter_tmp_buf, mix_buf, SOUND_MIX_ONCE_READ_POINT_NUM);
    }
}


tbool sound_mix_process_deal(SOUND_MIX_OBJ *obj, void *buf, u32 len)
{
    if (obj == NULL) {
        return false;
    }

    SOUND_MIX_CHL *p_cur;
    u32 once_point_num = len >> 1;
    s16	*mix_buf = (s16 *)buf;
    s16	filter_mix_buf[once_point_num];
    s32	filter_tmp_buf[once_point_num];
    s32 mix_tmp_buf[once_point_num];

    memset((u8 *)mix_tmp_buf, 0, sizeof(mix_tmp_buf));
    if (obj->filter) {
        memset((u8 *)filter_tmp_buf, 0, sizeof(filter_tmp_buf));
    }

    if (!list_empty(&(obj->head))) {

        list_for_each_entry(p_cur, &(obj->head), list) {
            if ((p_cur->cbuf.total_len - p_cur->cbuf.data_len) >= p_cur->rlimit) {
                /* sound_mix_printf("p");		 */
                os_sem_set(&p_cur->sem, 0);
                os_sem_post(&p_cur->sem);
            }
            if (p_cur->flag == SOUND_MIX_FLAG_DATA_ENOUGH) {
                if (p_cur->wait == 0) {
                    if (p_cur->cbuf.data_len >= len) {
                        cbuf_read(&p_cur->cbuf, (u8 *)mix_buf, len);

                        if (p_cur->chl_cbk.cbk) {
                            p_cur->chl_cbk.cbk(p_cur->chl_cbk.priv, mix_buf, len);
                        }

                        user_dac_digit_fade(&(p_cur->dac_var), mix_buf, len);
                        dac_mix_buf(mix_tmp_buf, mix_buf, once_point_num);
                        sound_mix_filter_deal(obj, p_cur, mix_buf, filter_tmp_buf);
                        /* if(strcmp(p_cur->key, "MUSIC_MIX") == 0) */
                        /* { */
                        /* putchar('M'); */
                        /* } */
                        /* if(strcmp(p_cur->key, "MIDI_MIX") == 0) */
                        /* { */
                        /* putchar('W'); */
                        /* } */
                    } else {
                        user_dac_digit_reset(&(p_cur->dac_var));

                        /* if (strcmp(p_cur->key, "MUSIC_MIX") == 0) { */
                        /* putchar('m'); */
                        /* } */
                        /* if (strcmp(p_cur->key, "MIDI_MIX") == 0) { */
                        /* putchar('w'); */
                        /* } */
                        /* if(strcmp(p_cur->key, "NOTICE_MIX") == 0) */
                        /* { */
                        /* putchar('e'); */
                        /* } */
                        /* if(strcmp(p_cur->key, "A2DP_MIX") == 0) */
                        /* { */
                        /* putchar('p'); */
                        /* } */
                        p_cur->flag = SOUND_MIX_FLAG_DATA_EMPTY;
                        continue;
                    }
                } else {
                    user_dac_digit_reset(&(p_cur->dac_var));
                }
            } else {
                user_dac_digit_reset(&(p_cur->dac_var));
            }
        }
        /* putchar('A'); */
    } else {
        /* putchar('B'); */
        /* memset((u8 *)mix_buf, 0, len); */
    }


    dac_mix_buf_limit(mix_tmp_buf, mix_buf, once_point_num);

    if (obj->filter != NULL && obj->filter->cbk != NULL) {
        dac_mix_buf_limit(filter_tmp_buf, filter_mix_buf, once_point_num);
        obj->filter->cbk(obj->filter->priv, (u8 *)filter_mix_buf, (u8 *)(mix_buf), len);
    }

    /* if (obj->output.output) { */
    /* obj->output.output(obj->output.priv, mix_buf, len); */
    /* } */
    return true;
}



static void sound_mix_task_deal(void *parg)
{
    u8 ret;
    u16 i;
    int msg[3] = {0};
    s16	mix_buf[SOUND_MIX_ONCE_READ_POINT_NUM];
    s16	filter_mix_buf[SOUND_MIX_ONCE_READ_POINT_NUM];
    s32	filter_tmp_buf[SOUND_MIX_ONCE_READ_POINT_NUM];
    s32 mix_tmp_buf[SOUND_MIX_ONCE_READ_POINT_NUM];
    SOUND_MIX_OBJ *obj = (SOUND_MIX_OBJ *)parg;
    ASSERT(obj);

    SOUND_MIX_CHL *p_cur;

    sound_mix_printf("sound mix enter \n");


    memset((u8 *)mix_tmp_buf, 0, sizeof(mix_tmp_buf));
    if (obj->filter) {
        memset((u8 *)filter_tmp_buf, 0, sizeof(filter_tmp_buf));
    }

    while (1) {
        ret = OSTaskQAccept(ARRAY_SIZE(msg), msg);
        if (ret == OS_Q_EMPTY) {

            if (!list_empty(&(obj->head))) {
                memset((u8 *)mix_tmp_buf, 0, sizeof(mix_tmp_buf));
                if (obj->filter) {
                    memset((u8 *)filter_tmp_buf, 0, sizeof(filter_tmp_buf));
                }
            }


            os_mutex_pend(&obj->mutex, 0);
            list_for_each_entry(p_cur, &(obj->head), list) {
                if ((p_cur->cbuf.total_len - p_cur->cbuf.data_len) >= p_cur->rlimit) {
                    /* sound_mix_printf("p");		 */
                    os_sem_set(&p_cur->sem, 0);
                    os_sem_post(&p_cur->sem);
                }
                if (p_cur->flag == SOUND_MIX_FLAG_DATA_ENOUGH) {

                    if (p_cur->wait == 0) {
                        if (p_cur->cbuf.data_len >= SOUND_MIX_ONCE_BUF_LEN) {
                            cbuf_read(&p_cur->cbuf, (u8 *)mix_buf, SOUND_MIX_ONCE_BUF_LEN);

                            if (p_cur->chl_cbk.cbk) {
                                p_cur->chl_cbk.cbk(p_cur->chl_cbk.priv, mix_buf, SOUND_MIX_ONCE_BUF_LEN);
                            }

                            user_dac_digit_fade(&(p_cur->dac_var), mix_buf, SOUND_MIX_ONCE_BUF_LEN);
                            dac_mix_buf(mix_tmp_buf, mix_buf, SOUND_MIX_ONCE_READ_POINT_NUM);
                            sound_mix_filter_deal(obj, p_cur, mix_buf, filter_tmp_buf);
                            /* if(strcmp(p_cur->key, "MUSIC_MIX") == 0) */
                            /* { */
                            /* putchar('o'); */
                            /* } */
                            /* if(strcmp(p_cur->key, "A2DP_MIX") == 0) */
                            /* { */
                            /* putchar('p'); */
                            /* } */
                        } else {
                            user_dac_digit_reset(&(p_cur->dac_var));
                            /* if(strcmp(p_cur->key, "MUSIC_MIX") == 0) */
                            /* { */
                            /* putchar('e'); */
                            /* } */
                            /* os_sem_set(&p_cur->sem, 0); */
                            /* os_sem_post(&p_cur->sem); */
                            p_cur->flag = SOUND_MIX_FLAG_DATA_EMPTY;
                            /* os_mutex_post(&obj->mutex); */
                            continue;
                        }
                    } else {
                        user_dac_digit_reset(&(p_cur->dac_var));
                    }
                } else {
                    user_dac_digit_reset(&(p_cur->dac_var));
                }
            }

            os_mutex_post(&obj->mutex);

            /* sound_mix_printf("r");		 */

            dac_mix_buf_limit(mix_tmp_buf, mix_buf, SOUND_MIX_ONCE_READ_POINT_NUM);

            if (obj->filter != NULL && obj->filter->cbk != NULL) {
                dac_mix_buf_limit(filter_tmp_buf, filter_mix_buf, SOUND_MIX_ONCE_READ_POINT_NUM);
                obj->filter->cbk(obj->filter->priv, (u8 *)filter_mix_buf, (u8 *)(mix_buf), SOUND_MIX_ONCE_BUF_LEN);
            }

            if (obj->output.output) {
                obj->output.output(obj->output.priv, mix_buf, SOUND_MIX_ONCE_BUF_LEN);
            } else {
                os_time_dly(100);
                sound_mix_printf("sound mix without output err !!!\n");
            }
            continue;
        }


        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;
        default:
            break;
        }
    }
}


static void *sound_mix_src_write(void *priv, void *buf, u32 len)
{
    /* sound_mix_printf("n"); */
    SOUND_MIX_CHL *mix_chl = (SOUND_MIX_CHL *)priv;
    if (mix_chl == NULL) {
        return NULL;
    }

    /* if (strcmp(mix_chl->key, "MIDI_MIX") == 0) { */
    /* putchar('#'); */
    /* } */

    src_obj_write_api(mix_chl->src_obj, buf, len);
    return priv;
}


static void *sound_mix_src_write_task(void *priv, void *buf, u32 len)
{
    /* sound_mix_printf("n"); */
    SOUND_MIX_CHL *mix_chl = (SOUND_MIX_CHL *)priv;
    if (mix_chl == NULL) {
        return NULL;
    }

    /* if(strcmp(mix_chl->key, "FM_MIX") == 0) */
    /* { */
    /* putchar('S'); */
    /* } */

    src_obj_write_to_task_api(mix_chl->src_obj, buf, len);
    /* src_obj_write_api(mix_chl->src_obj, buf, len); */
    return priv;
}



static u32 sound_mix_chl_set_info(void *priv, dec_inf_t *inf, tbool wait)
{
    SOUND_MIX_CHL *mix_chl = (SOUND_MIX_CHL *)priv;
    if (mix_chl == NULL) {
        return 0;
    }
    SOUND_MIX_OBJ *mix_obj = mix_chl->mix_obj;

    if (inf->sr != mix_chl->samplerate) {
        if (mix_chl->src_obj) {
            if (mix_chl->src_p.task_name != NULL) {
                src_obj_destroy_with_task_api(&mix_chl->src_obj);
            } else {
                src_obj_destroy_api(&mix_chl->src_obj);
            }
        }

        SRC_OBJ_PARAM  src_p;
        memset((u8 *)&src_p, 0x0, sizeof(SRC_OBJ_PARAM));
        src_p.once_out_len = 128;
        src_p.in_chinc = 1;
        src_p.in_spinc = 2;
        src_p.out_chinc = 1;
        src_p.out_spinc = 2;
        src_p.in_rate = inf->sr;//41100;
        src_p.out_rate = mix_obj->samplerate;//samplerate;
        src_p.nchannel = 2;
        src_p.output.priv = (void *)mix_chl;
        src_p.output._cbk = (void *)sound_mix_chl_write;
        sound_mix_printf("%s mix channel samplerate input = %d, output = %d\n", mix_chl->key, inf->sr, mix_obj->samplerate);
        if (mix_chl->src_p.task_name != NULL) {
            mix_chl->src_obj = src_obj_creat_with_task_api(&src_p, (const char *)mix_chl->src_p.task_name, mix_chl->src_p.task_prio, 2 * 1024);
        } else {
            mix_chl->src_obj = src_obj_creat_api(&src_p);
        }
        ASSERT(mix_chl->src_obj);
        mix_chl->samplerate = inf->sr;

        sound_mix_printf("mix set info ok %d\n", __LINE__);
    }

    return 0;
}


/*----------------------------------------------------------------------------*/
/**@brief 声音叠加器资源创建
   @param void
   @return 声音叠加器控制句柄, 失败返回NULL
   @note
*/
/*----------------------------------------------------------------------------*/
SOUND_MIX_OBJ *sound_mix_creat(void)
{
    SOUND_MIX_OBJ *obj;
    u8 *need_buf;
    u32 need_buf_len;
    need_buf_len = SIZEOF_ALIN(sizeof(SOUND_MIX_OBJ), 4);

    sound_mix_printf("need_buf_len = %d!!\n", need_buf_len);
    need_buf = (u8 *)sound_mix_calloc(1, need_buf_len);
    if (need_buf == NULL) {
        return NULL;
    }

    obj = (SOUND_MIX_OBJ *)need_buf;

    /* if (os_mutex_create(&obj->mutex) != OS_NO_ERR) { */
    /* sound_mix_free(obj); */
    /* return NULL; */
    /* } */
    /* obj->head = LIST_HEAD_INIT(obj->head); */

    src_init_api();
    INIT_LIST_HEAD(&obj->head);

    obj->with_task = 0;

    sound_mix_printf("sound mix obj creat ok!!\n");
    return obj;
}



/*----------------------------------------------------------------------------*/
/**@brief 声音叠加器资源释放
   @param 声音叠加器控制句柄（注意参数类型是一个二重指针!!!!）
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void sound_mix_destroy(SOUND_MIX_OBJ **hdl)
{
    if (hdl == NULL || (*hdl == NULL)) {
        return ;
    }

    SOUND_MIX_OBJ *obj = *hdl;
    if (obj->with_task) {
        if (obj->task_name != NULL) {
            if (os_task_del_req(obj->task_name) != OS_TASK_NOT_EXIST) {
                os_taskq_post_event(obj->task_name, 1, SYS_EVENT_DEL_TASK);
                do {
                    OSTimeDly(1);
                } while (os_task_del_req(obj->task_name) != OS_TASK_NOT_EXIST);
                sound_mix_printf("del sound mix task succ\n");
            }
        }

        if (obj->op_cb.over) {
            while (obj->op_cb.over(obj->op_cb.priv) == false) {
                os_time_dly(5);
            }
        }
    }

    src_exit_api();
    if (obj->filter) {
        sound_mix_free(obj->filter);
    }
    sound_mix_free(obj);
    OS_ENTER_CRITICAL();
    *hdl = NULL;
    OS_EXIT_CRITICAL();
}


/*----------------------------------------------------------------------------*/
/**@brief 声音叠加器终端输出采样率设置
   @param
     	obj:叠加总控制，samplerate：采样率
   @return
   @note 注意该函数为叠加器创建后到启动叠加器之间必须要调用的函数，不允许其他地方调用!!!!!
*/
/*----------------------------------------------------------------------------*/
void __sound_mix_set_samplerate(SOUND_MIX_OBJ *obj, u16 samplerate)
{
    if (obj == NULL) {
        return ;
    }
    obj->samplerate = samplerate;
}


/*----------------------------------------------------------------------------*/
/**@brief 声音叠加器终端输出到线程
   @param
     	obj:叠加总控制
   @return
   @note 注意这个接口一旦被调用，需要线程来处理叠加结果, 否则数据需要在中断中处理
*/
/*----------------------------------------------------------------------------*/
tbool __sound_mix_set_with_task_enable(SOUND_MIX_OBJ *obj)
{
    if (obj == NULL) {
        return false;
    }

    obj->with_task = 1;

    if (os_mutex_create(&obj->mutex) != OS_NO_ERR) {
        sound_mix_printf("sound mix mutex creat err!!\n");
        return false;
    }
    return true;
}



/*----------------------------------------------------------------------------*/
/**@brief 声音叠加器总数据流采样率设置回调注册
   @param
     	obj:叠加总控制，set_info：采样率设置回调， priv：回调私有参数
   @return
   @note 注意该函数为叠加器创建后到启动叠加器之间必须要调用的函数,并且只能调用一次!!!!!
*/
/*----------------------------------------------------------------------------*/
void __sound_mix_set_set_info_cbk(SOUND_MIX_OBJ *obj, u32(*set_info)(void *priv, dec_inf_t *inf, tbool wait), void *priv)
{
    if (obj == NULL) {
        return ;
    }
    if (set_info == NULL) {
        sound_mix_printf("sound mix set_info cbk is null!!\n");
        return ;
    }
    obj->set_info_cb.priv = priv;
    obj->set_info_cb._cb = (void *)set_info;
}


/*----------------------------------------------------------------------------*/
/**@brief 声音叠加器总数据流输出回调注册
   @param
     	obj:叠加总控制，output：输出回调， priv：回调私有参数
   @return
   @note 注意该函数为叠加器创建后到启动叠加器之间必须要调用的函数,并且只能调用一次!!!!!
*/
/*----------------------------------------------------------------------------*/
void __sound_mix_set_output_cbk(SOUND_MIX_OBJ *obj, void *(*output)(void *priv, void *buf, u16 len), void *priv)
{
    if (obj == NULL) {
        return ;
    }
    if (output == NULL) {
        sound_mix_printf("sound mix output cbk is null!!\n");
        return ;
    }

    obj->output.priv = priv;
    obj->output.output = (void *)output;
}


/*----------------------------------------------------------------------------*/
/**@brief 声音叠加器总数据流清除缓存及等待数据输出完回调注册
   @param
	obj:叠加总控制，clr：清空输出缓存回调，over: 等待数据输出完成回调 priv：回调私有参数
   @return
   @note 注意该函数为叠加器创建后到启动叠加器之间必须要调用的函数,并且只能调用一次!!!!!
*/
/*----------------------------------------------------------------------------*/
void __sound_mix_set_data_op_cbk(SOUND_MIX_OBJ *obj, void (*clr)(void *priv),  tbool(*over)(void *priv), void *priv)
{
    if (obj == NULL) {
        return ;
    }
    obj->op_cb.clr = clr;
    obj->op_cb.over = over;
    obj->op_cb.priv = priv;
}


void __sound_mix_set_filter_data_get(SOUND_MIX_OBJ *obj, void (*cbk)(void *priv, u8 *fil_buf, u8 *org_buf, u32 len), void *priv, int cnt, ...)
{
    if (obj == NULL) {
        return;
    }

    if (obj->filter) {
        return ;
    }

    if (cnt == 0) {
        return ;
    }

    va_list args;
    va_start(args, cnt);
    u32 i;

    u8 *need_buf;
    u32 need_buf_len;
    need_buf_len = SIZEOF_ALIN(sizeof(SOUND_MIX_FILTER), 4) + SIZEOF_ALIN((sizeof(u32) * cnt), 4);

    sound_mix_printf("need_buf_len = %d!!\n", need_buf_len);
    need_buf = (u8 *)sound_mix_calloc(1, need_buf_len);
    if (need_buf == NULL) {
        sound_mix_printf("no memory for sound mix filter!!\n");
        va_end(args);
        return ;
    }

    obj->filter = (SOUND_MIX_FILTER *)need_buf;

    need_buf += SIZEOF_ALIN(sizeof(SOUND_MIX_FILTER), 4);
    obj->filter->members = (u32 *)need_buf;

    obj->filter->members_cnt = cnt;
    obj->filter->priv = priv;
    obj->filter->cbk = cbk;

    for (i = 0; i < cnt; i++) {
        obj->filter->members[i] = (u32)va_arg(args, int);
        sound_mix_printf("num[%d] = %s\n", i, (const char *)obj->filter->members[i]);
    }

    va_end(args);
}


/*----------------------------------------------------------------------------*/
/**@brief 声音叠加器单个通道音量调节接口
   @param
	mix_chl:单个通道控制句柄 vol：音量值
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __sound_mix_chl_set_vol(SOUND_MIX_CHL *mix_chl, u8 vol)
{
    if (mix_chl == NULL) {
        return ;
    }
    /* SOUND_MIX_CHL *p = NULL; */
    /* list_for_each_entry(p, &(mix_chl->mix_obj->head), list) { */
    /* if (p == mix_chl) { */
    user_dac_digital_set_vol(&(mix_chl->dac_var), vol);
    /* } */
    /* } */
}

void __sound_mix_chl_set_mute(SOUND_MIX_CHL *mix_chl, u8 flag)
{
    if (mix_chl == NULL) {
        return ;
    }
    user_dac_digital_set_mute(&(mix_chl->dac_var), flag);
    /* user_dac_digital_get_vol(); */
    /* user_dac_digital_set_vol(&(mix_chl->dac_var), vol); */
}

tbool __sound_mix_chl_get_mute_status(SOUND_MIX_CHL *mix_chl)
{
    if (mix_chl == NULL) {
        return false;
    }
    return user_dac_digital_get_mute_status(&(mix_chl->dac_var));
}

u32 __sound_mix_chl_get_data_len(SOUND_MIX_CHL *mix_chl)
{
    if (mix_chl == NULL) {
        return 0;
    }
    return cbuf_get_data_size(&(mix_chl->cbuf));
}

void __sound_mix_chl_set_wait(SOUND_MIX_CHL *mix_chl)
{
    if (mix_chl == NULL) {
        return ;
    }
    mix_chl->wait = 1;
}

s8 __sound_mix_chl_check_wait_ready(SOUND_MIX_CHL *mix_chl)
{
    if (mix_chl) {
        if (mix_chl->flag == SOUND_MIX_FLAG_DATA_ENOUGH) {
            return 1;
        } else {
            return 0;
        }
    }

    return -1;
}

void __sound_mix_chl_set_wait_ok(SOUND_MIX_CHL *mix_chl)
{
    if (mix_chl) {
        mix_chl->wait = 0;
    }
}

/*----------------------------------------------------------------------------*/
/**@brief 声音叠加器处理线程创建及叠加器启动
   @param
	obj:叠加总控制，task_name:叠加处理线程名称 prio：叠加线程优先级，stk_size：堆栈太小
   @return 启动成功与失败
   @note
*/
/*----------------------------------------------------------------------------*/
tbool sound_mix_process(SOUND_MIX_OBJ *obj, void *task_name, u32 prio, u32 stk_size)
{
    if (obj == NULL) {
        return false;
    }

    dec_inf_t inf;
    memset((u8 *)&inf, 0x0, sizeof(dec_inf_t));
    inf.sr = obj->samplerate;

    if (obj->with_task == 0) {
        if (obj->set_info_cb._cb) {
            obj->set_info_cb._cb(obj->set_info_cb.priv, &inf, 1);
        }
        return true;
    } else {
        if (task_name == NULL) {
            sound_mix_printf("task_name is NULL err !!\n");
            return false;
        }
    }

    if (obj->task_name != NULL) {
        if (os_task_del_req(obj->task_name) != OS_TASK_NOT_EXIST) {
            os_taskq_post_event(obj->task_name, 1, SYS_EVENT_DEL_TASK);
            do {
                OSTimeDly(1);
            } while (os_task_del_req(obj->task_name) != OS_TASK_NOT_EXIST);
            sound_mix_printf("del sound mix task succ\n");
        }
    }

    if (obj->set_info_cb._cb) {
        obj->set_info_cb._cb(obj->set_info_cb.priv, &inf, 1);
    }


    u32 err = os_task_create_ext(sound_mix_task_deal, (void *)obj, prio, 5, task_name, stk_size);
    if (err != OS_NO_ERR) {
        sound_mix_printf("sound mix creat fail !!!\n");
        return false;
    }

    obj->task_name = task_name;

    return true;
}


/*----------------------------------------------------------------------------*/
/**@brief 声音叠加器添加叠加通道
   @param
	obj:叠加总控制，param:通道参数配置
   @return 通道叠加通道， 失败返回NULL
   @note
*/
/*----------------------------------------------------------------------------*/
SOUND_MIX_CHL *sound_mix_chl_add(SOUND_MIX_OBJ *obj, SOUND_MIX_P *param)
{
    if (obj == NULL || param == NULL || param->input == NULL || param->op_cb == NULL) {
        return NULL;
    }

    SOUND_MIX_CHL *mix_chl;
    list_for_each_entry(mix_chl, &(obj->head), list) {
        if (strcmp((const char *)param->key, (const char *)mix_chl->key) == 0) {
            sound_mix_printf("this obj has this one chn !!!\n");
            return mix_chl;
        }
    }

    u8 *need_buf = NULL;
    u8 *out_buf;
    u32 need_buf_len;

    need_buf_len = SIZEOF_ALIN(sizeof(SOUND_MIX_CHL), 4) + SIZEOF_ALIN(param->out_len, 4);
    need_buf = (u8 *)sound_mix_calloc(1, need_buf_len);
    if (need_buf == NULL) {
        return NULL;
    }

    mix_chl = (SOUND_MIX_CHL *)(need_buf);

    need_buf += SIZEOF_ALIN(sizeof(SOUND_MIX_CHL), 4);
    out_buf = (u8 *)need_buf;

    cbuf_init(&(mix_chl->cbuf), out_buf, param->out_len);

    mix_chl->wait = param->wait;
    mix_chl->key = param->key;
    mix_chl->mix_obj = obj;
    mix_chl->track = param->track;
    mix_chl->src_p.task_name = param->src_p.task_name;
    mix_chl->src_p.task_prio = param->src_p.task_prio;
    mix_chl->chl_cbk.priv = param->chl_cbk.priv;
    mix_chl->chl_cbk.cbk = param->chl_cbk.cbk;

    user_dac_digital_init(&(mix_chl->dac_var), param->max_vol, param->default_vol, obj->samplerate);

    if (os_mutex_create(&mix_chl->mutex) != OS_NO_ERR) {
        sound_mix_free(mix_chl);
        return NULL;
    }

    if (os_sem_create(&mix_chl->sem, 0) != OS_NO_ERR) {
        sound_mix_free(mix_chl);
        return NULL;
    }

    param->set_info_cb->priv = mix_chl;
    param->set_info_cb->_cb = (void *)sound_mix_chl_set_info;

    param->input->priv = mix_chl;

    if (param->src_p.task_name != NULL) {
        param->input->output = (void *)sound_mix_src_write_task;
    } else {
        param->input->output = (void *)sound_mix_src_write;
    }

    param->op_cb->priv = mix_chl;
    param->op_cb->clr = (void *)sound_mix_chl_op_data_clear;
    param->op_cb->over = (void *)sound_mix_chl_op_data_check_over;


    /* if (param->limit >= 100 || param->limit == 0) { */
    /* param->limit = 50; */
    /* } */

    if (param->limit > 100) {
        param->limit = 100;
    }

    mix_chl->rlimit = param->out_len * param->limit / 100;

    sound_mix_printf("rlimit = %d, out_len = %d, percent = %d\n", mix_chl->rlimit, param->out_len, param->limit);

    sound_mix_printf("mix_chl = %x, %d\n", mix_chl, __LINE__);

    sound_mix_mutex_enter(obj);
    list_add(&(mix_chl->list), &(obj->head));
    sound_mix_mutex_exit(obj);

    return mix_chl;
}



/*----------------------------------------------------------------------------*/
/**@brief 声音叠加器通过key删除某个叠加通道
   @param
	obj:叠加总控制，key:通道标示名
   @return  删除成功与失败
   @note
*/
/*----------------------------------------------------------------------------*/
tbool sound_mix_chl_del_by_key(SOUND_MIX_OBJ *obj, const char *key)
{
    if (obj == NULL) {
        return false;
    }
    tbool ret = false;
    SOUND_MIX_CHL *p, *n;
    list_for_each_entry_safe(p, n, &(obj->head), list) {
        if (strcmp((const char *)key, (const char *)p->key) == 0) {
            if (p->src_obj) {
                if (p->src_p.task_name != NULL) {
                    src_obj_destroy_with_task_api(&p->src_obj);
                } else {
                    src_obj_destroy_api(&p->src_obj);
                }
            }

            sound_mix_chl_wait_stop(p);

            sound_mix_mutex_enter(obj);
            list_del(&p->list);
            sound_mix_mutex_exit(obj);

            sound_mix_free(p);
            ret = true;
        }
    }
    return ret;
}


/*----------------------------------------------------------------------------*/
/**@brief 声音叠加器通过通道句柄删除某个叠加通道
   @param
	obj:叠加总控制，mix_chl:通道句柄（注意这里是一个双重指针）
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void sound_mix_chl_del_by_chl(SOUND_MIX_CHL **mix_chl)
{
    if (mix_chl == NULL || *mix_chl == NULL) {
        return;
    }
    if (sound_mix_chl_del_by_key((*mix_chl)->mix_obj, (*mix_chl)->key) == true) {
        *mix_chl = NULL;
    }
}



