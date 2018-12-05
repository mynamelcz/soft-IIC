#include "wav_mix_play.h"
#include "wav_play.h"
#include "mem/malloc.h"
#include "common/msg.h"
#include "wav_play.h"
#include "string.h"
#include "rtos/os_api.h"
#include "dac/dac.h"
#include "dac/dac_track.h"
#define WAV_MIX_DEBUG_ENABLE
#ifdef WAV_MIX_DEBUG_ENABLE
#define wav_mix_printf	printf
#else
#define wav_mix_printf(...)
#endif




#define WAV_MIX_SIZEOF_ALIN(var,al)     ((((var)+(al)-1)/(al))*(al))

struct __WAV_MIX_OBJ {
    void 		*task_name;
    FLASHDATA_T  flashdata;
    s16			 mix_buf[WAV_ONCE_READ_POINT_NUM];
    s32			 mix_tmp_buf[WAV_ONCE_READ_POINT_NUM];
    u16			 samplerate;
    _OP_IO		 output;
    DEC_OP_CB	 op_cb;
    DEC_SET_INFO_CB set_info_cb;
    OS_SEM		 mutex;
    struct list_head head;
};


struct __WAV_MIX_CHL {
    struct list_head list;
    void 			*key;
    WAV_MIX_OBJ 	*mix_obj;
    void 			*father_name;
    void 			*path;
    WAV_INFO		*info;
    DAC_DIGIT_VAR	 dac_var;
    u8			 	 file_max;
    u8			 	 file_max_cnt;
    u8			 	 file_cnt;
    u8			 	 rpt_cnt;
    volatile u8		 stop;
    volatile u8		 isBusy;
    OS_MUTEX 	 	 mutex;
};


/*----------------------------------------------------------------------------*/
/**@brief wav叠加器资源创建
   @param void
   @return wav叠加器控制句柄, 失败返回NULL
   @note
*/
/*----------------------------------------------------------------------------*/
WAV_MIX_OBJ *wav_mix_creat(void)
{
    WAV_MIX_OBJ *obj = (WAV_MIX_OBJ *)calloc(1, sizeof(WAV_MIX_OBJ));
    /* ASSERT(obj);	 */
    if (obj == NULL) {
        return NULL;
    }


    if (os_mutex_create(&obj->mutex) != OS_NO_ERR) {
        free(obj);
        return NULL;
    }
    INIT_LIST_HEAD(&obj->head);
    return obj;
}

/*----------------------------------------------------------------------------*/
/**@brief wav叠加器资源释放
   @param void
   @return wav叠加器控制句柄(注意这是一个双重指针!!!!!)
   @note
*/
/*----------------------------------------------------------------------------*/
void wav_mix_destroy(WAV_MIX_OBJ **obj)
{
    if ((obj == NULL) || (*obj == NULL)) {
        return ;
    }
    wav_mix_chl_del_all(*obj);
    if ((*obj)->op_cb.over) {
        while ((*obj)->op_cb.over((*obj)->op_cb.priv)) {
            OSTimeDly(1);
        }
    }

    if (os_task_del_req((*obj)->task_name) != OS_TASK_NOT_EXIST) {
        os_taskq_post_event((*obj)->task_name, 1, SYS_EVENT_DEL_TASK);
        do {
            OSTimeDly(1);
        } while (os_task_del_req((*obj)->task_name) != OS_TASK_NOT_EXIST);
        /* puts("del notice player  succ\n"); */
    }
    free(*obj);
    *obj = NULL;
}


/*----------------------------------------------------------------------------*/
/**@brief wav叠加器总采样率设置
   @param
	obj:wav叠加器总控制 samplerate:采样率
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __wav_mix_set_samplerate(WAV_MIX_OBJ *obj, u16 samplerate)
{
    if (obj == NULL) {
        return ;
    }
    obj->samplerate = samplerate;
}

/*----------------------------------------------------------------------------*/
/**@brief wav叠加器总数据流清除缓存及等待数据输出完回调注册
   @param
	obj:叠加总控制，clr：清空输出缓存回调，over: 等待数据输出完成回调 priv：回调私有参数
   @return
   @note 注意该函数为叠加器创建后到启动叠加器之间必须要调用的函数,并且只能调用一次!!!!!
*/
/*----------------------------------------------------------------------------*/
void __wav_mix_set_data_op_cbk(WAV_MIX_OBJ *obj, void (*clr)(void *priv),  tbool(*over)(void *priv), void *priv)
{
    if (obj == NULL) {
        return ;
    }
    obj->op_cb.clr = clr;
    obj->op_cb.over = over;
    obj->op_cb.priv = priv;
}
/*----------------------------------------------------------------------------*/
/**@brief wav叠加器总数据流采样率设置回调注册
   @param
     	obj:叠加总控制，set_info：采样率设置回调， priv：回调私有参数
   @return
   @note 注意该函数为叠加器创建后到启动叠加器之间必须要调用的函数,并且只能调用一次!!!!!
*/
/*----------------------------------------------------------------------------*/
void __wav_mix_set_info_cbk(WAV_MIX_OBJ *obj, u32(*set_info)(void *priv, dec_inf_t *inf, tbool wait), void *priv)
{
    if (obj == NULL) {
        return ;
    }
    if (set_info == NULL) {
        wav_mix_printf("wav mix set_info cbk is null!!\n");
        return ;
    }
    obj->set_info_cb.priv = priv;
    obj->set_info_cb._cb = (void *)set_info;
}

/*----------------------------------------------------------------------------*/
/**@brief wav叠加器总数据流输出回调注册
   @param
     	obj:叠加总控制，output：输出回调， priv：回调私有参数
   @return
   @note 注意该函数为叠加器创建后到启动叠加器之间必须要调用的函数,并且只能调用一次!!!!!
*/
/*----------------------------------------------------------------------------*/
void __wav_mix_set_output_cbk(WAV_MIX_OBJ *obj, void *(*output)(void *priv, void *buf, u16 len), void *priv)
{
    if (obj == NULL) {
        return ;
    }
    if (output == NULL) {
        wav_mix_printf("wav mix output is null !!\n");
        return;
    }

    obj->output.priv = priv;
    obj->output.output = (void *)output;
}
/*----------------------------------------------------------------------------*/
/**@brief wav叠加器单个通道音量调节接口
   @param
	obj:叠加总控制 key:通道标识  vol：音量值
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void __wav_mix_chl_set_vol(WAV_MIX_OBJ *obj, const char *key, u8 vol)
{
    if (obj == NULL) {
        return ;
    }
    WAV_MIX_CHL *mix_chl = NULL;
    list_for_each_entry(mix_chl, &(obj->head), list) {
        if (strcmp((const char *)key, (const char *)mix_chl->key) == 0) {
            user_dac_digital_set_vol(&(mix_chl->dac_var), vol);
        }
    }
}

/*----------------------------------------------------------------------------*/
/**@brief wav叠加器单个通道播放状态获取
   @param
	obj:叠加总控制 key:通道标识
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
u8 __wav_mix_chl_get_status(WAV_MIX_OBJ *obj, const char *key)
{
    if (obj == NULL) {
        return 0;
    }
    u8 ret = 0;
    WAV_MIX_CHL *mix_chl = NULL;
    list_for_each_entry(mix_chl, &(obj->head), list) {
        if (strcmp((const char *)key, (const char *)mix_chl->key) == 0) {
            ret = ((mix_chl->stop) ? 0 : 1);
        }
    }
    return ret;
}


static u16 wav_mix_file_read(void *hdl, u8 *buf, u32 addr, u16 len)
{
    return phy_flashdata_read((FLASHDATA_T *)hdl, buf, addr, len);
}


static tbool wav_mix_chl_decode(WAV_MIX_CHL *mix_chl)
{
    if (mix_chl == NULL) {
        return false;
    }
    tbool ret;
    WAV_MIX_OBJ *obj = mix_chl->mix_obj;
    if (mix_chl->stop == 1) {
        return false;
    }

    if (obj->samplerate != mix_chl->info[mix_chl->file_cnt].dac_sr) {
        return false;
    }

    mix_chl->isBusy = 1;
    ret = wav_file_decode(&(mix_chl->info[mix_chl->file_cnt]), wav_mix_file_read, (void *) & (obj->flashdata), obj->mix_buf);
    if (ret == false) {
        mix_chl->info[mix_chl->file_cnt].r_len = 0;
        mix_chl->file_cnt++;
        /* wav_mix_printf("mix_chl->file_cnt = %d\n", mix_chl->file_cnt); */
        if (mix_chl->file_cnt >= mix_chl->file_max_cnt) {
            if (mix_chl->rpt_cnt == 0) {
                mix_chl->file_cnt = 0;
            } else {

                os_mutex_pend(&mix_chl->mutex, 0);
                mix_chl->stop = 1;
                os_mutex_post(&mix_chl->mutex);

                if (mix_chl->father_name) {
                    u8 err = os_taskq_post_event(mix_chl->father_name, 2, MSG_WAV_MIX_END, (int)mix_chl->key);
                    if (err != OS_NO_ERR) {
                        wav_mix_printf("wav mix chl end msg send  err !!!!!!\n");
                    }
                }
            }

        }
    } else {
        user_dac_digit_fade(&(mix_chl->dac_var), obj->mix_buf, WAV_ONCE_BUF_LEN);
        dac_mix_buf(obj->mix_tmp_buf, obj->mix_buf, WAV_ONCE_READ_POINT_NUM);
    }

    mix_chl->isBusy = 0;
    return ret;
}


static void wav_mix_process_deal(void *parg)
{
    u8 ret;
    u16 i;
    int msg[3] = {0};
    WAV_MIX_OBJ *obj = (WAV_MIX_OBJ *)parg;
    ASSERT(obj);
    WAV_MIX_CHL *p_cur;

    if (obj->set_info_cb._cb) {
        dec_inf_t inf;
        memset((u8 *)&inf, 0x0, sizeof(dec_inf_t));
        inf.sr = obj->samplerate;
        obj->set_info_cb._cb(obj->set_info_cb.priv, &inf, 1);
    }
    wav_mix_printf("----------------wav mix enter------------------------ \n");

    while (1) {
        ret = OSTaskQAccept(ARRAY_SIZE(msg), msg);
        if (ret == OS_Q_EMPTY) {
            memset((u8 *)obj->mix_tmp_buf, 0, sizeof(obj->mix_tmp_buf));
            list_for_each_entry(p_cur, &(obj->head), list) {
                if (wav_mix_chl_decode(p_cur) == false) {
                    user_dac_digit_reset(&(p_cur->dac_var));
                    continue;
                }
            }

            dac_mix_buf_limit(obj->mix_tmp_buf, obj->mix_buf, WAV_ONCE_READ_POINT_NUM);
            if (obj->output.output) {
                obj->output.output(obj->output.priv, obj->mix_buf, WAV_ONCE_BUF_LEN);
            } else {
                os_time_dly(100);
                wav_mix_printf("wav mix without output err !!!\n");
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



/*----------------------------------------------------------------------------*/
/**@brief wav叠加器叠加处理线程创建及启动
   @param
	obj:叠加总控制 task_name:线程名称 prio:线程优先级 stk_size:堆栈大小
   @return 成功或者失败
   @note
*/
/*----------------------------------------------------------------------------*/
tbool wav_mix_process(WAV_MIX_OBJ *obj, void *task_name, u32 prio, u32 stk_size)
{
    if (obj == NULL) {
        return false;
    }
    if (obj->task_name != NULL) {
        if (os_task_del_req(obj->task_name) != OS_TASK_NOT_EXIST) {
            os_taskq_post_event(obj->task_name, 1, SYS_EVENT_DEL_TASK);
            do {
                OSTimeDly(1);
            } while (os_task_del_req(obj->task_name) != OS_TASK_NOT_EXIST);
            wav_mix_printf("del wav mix task succ\n");
        }
    }

    u32 err = os_task_create_ext(wav_mix_process_deal, (void *)obj, prio, 5, task_name, stk_size);
    if (err != OS_NO_ERR) {
        wav_mix_printf("wav mix creat fail !!!\n");
        return false;
    }

    obj->task_name = task_name;

    return true;
}



static void wav_mix_chl_wait_stop(WAV_MIX_CHL *mix_chl)
{
    if (mix_chl->stop == 0) {

        os_mutex_pend(&mix_chl->mutex, 0);
        mix_chl->stop = 1;
        os_mutex_post(&mix_chl->mutex);

        while (mix_chl->isBusy) {
            os_time_dly(1);
        }
    }
}


/*----------------------------------------------------------------------------*/
/**@brief wav叠加器单个通道停止播放
   @param
	obj:叠加总控制 key:通道标识
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void wav_mix_chl_stop(WAV_MIX_OBJ *obj, const char *key)
{
    WAV_MIX_CHL *mix_chl = NULL;
    list_for_each_entry(mix_chl, &(obj->head), list) {
        if (strcmp((const char *)key, (const char *)mix_chl->key) == 0) {
            /* os_mutex_pend(&mix_chl->mutex, 0); */
            wav_mix_chl_wait_stop(mix_chl);
            /* os_mutex_post(&mix_chl->mutex); */
        }
    }
}



/*----------------------------------------------------------------------------*/
/**@brief wav叠加器所有通道停止播放
   @param
	obj:叠加总控制 key:通道标识
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void wav_mix_chl_stop_all(WAV_MIX_OBJ *obj)
{
    WAV_MIX_CHL *mix_chl = NULL;
    list_for_each_entry(mix_chl, &(obj->head), list) {
        /* if (strcmp((const char *)key, (const char *)mix_chl->key) == 0) { */
        /* os_mutex_pend(&mix_chl->mutex, 0); */

        wav_mix_chl_wait_stop(mix_chl);

        /* os_mutex_post(&mix_chl->mutex); */
        /* }	 */
    }
}

/*----------------------------------------------------------------------------*/
/**@brief wav叠加器单个通道播放启动
   @param
	obj:叠加总控制 key:通道标识, rpt_cnt:0表示无限循环，其他循环rpt_cnt次
	argc：播放音频路径个数 其后可变参数为路径，必须保证是全路径
	例如：argc = 2， 其后参数可以是 /wav/test0.wav, /wav/test1.wav
		wav_mix_chl_play(obj, "ch0", 1, 2, /wav/test0.wav, /wav/test1.wav)
   @return 通道添加成功与失败
   @note
*/
/*----------------------------------------------------------------------------*/
tbool wav_mix_chl_play(WAV_MIX_OBJ *obj, const char *key, u8 rpt_cnt, int argc, ...)
{
    tbool ret = false;
    va_list args;
    va_start(args, argc);

    if (obj == NULL) {
        goto __exit;
    }

    WAV_MIX_CHL *mix_chl = NULL;
    list_for_each_entry(mix_chl, &(obj->head), list) {
        if (strcmp((const char *)key, (const char *)mix_chl->key) == 0) {
            break;
        }
    }

    if (mix_chl == NULL) {
        wav_mix_printf("wav mix chl play no this chl err!!,%d !!!\n", __LINE__);
        goto __exit;
    }

    if (argc > mix_chl->file_max) {
        wav_mix_printf("wav mix chl file cnt over max err!!,   argc = %d, file_max = %d,  %d !!!\n", mix_chl->father_name, __LINE__);
        goto __exit;
    }

    wav_mix_chl_stop(obj, key);
    memset((u8 *)mix_chl->info, 0x0, WAV_MIX_SIZEOF_ALIN(sizeof(WAV_INFO), 4) * mix_chl->file_max);
    mix_chl->file_max_cnt = 0;
    mix_chl->file_cnt = 0;

    const char *path;
    u32 i;
    for (i = 0; i < argc; i++) {
        path = (const char *)va_arg(args, int);

        u32 len = strlen(path);
        const char *pfile = path;
        if (pfile[len - 1] == '/') {
            wav_mix_printf("path is not the whole path err %d !!!\n", __LINE__);
            ret = false;
            goto __exit;
        }
        ret = wav_player_get_file_info(&(mix_chl->info[i]), path, 0/*no support num get file info*/);
        if (ret == false) {
            wav_mix_printf("wav mix chl play get info fail err!!,%d !!!\n", __LINE__);
            goto __exit;
        }
    }

    mix_chl->file_max_cnt = (u8)argc;
    mix_chl->rpt_cnt = rpt_cnt;

    os_mutex_pend(&mix_chl->mutex, 0);
    mix_chl->stop = 0;
    os_mutex_post(&mix_chl->mutex);

    /* wav_mix_printf(" wav mix chl play ok!!!, file_max = %d\n", mix_chl->file_max_cnt); */

__exit:
    va_end(args);
    return ret;
}

/*----------------------------------------------------------------------------*/
/**@brief wav叠加器通道删除
   @param
	obj:叠加总控制 key:通道标识
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void wav_mix_chl_del(WAV_MIX_OBJ *obj, const char *key)
{
    if (obj == NULL) {
        return ;
    }
    tbool ret = false;
    WAV_MIX_CHL *p, *n;
    list_for_each_entry_safe(p, n, &(obj->head), list) {
        if (strcmp((const char *)key, (const char *)p->key) == 0) {
            wav_mix_chl_wait_stop(p);
            os_mutex_pend(&obj->mutex, 0);
            list_del(&p->list);
            free(p);
            os_mutex_post(&obj->mutex);
        }
    }
}

/*----------------------------------------------------------------------------*/
/**@brief wav叠加器删除所有叠加通道
   @param
	obj:叠加总控制
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void wav_mix_chl_del_all(WAV_MIX_OBJ *obj)
{
    if (obj == NULL) {
        return ;
    }
    tbool ret = false;
    WAV_MIX_CHL *p, *n;
    list_for_each_entry_safe(p, n, &(obj->head), list) {
        /* if (strcmp((const char *)key, (const char *)p->key) == 0) { */
        wav_mix_chl_wait_stop(p);
        os_mutex_pend(&obj->mutex, 0);
        list_del(&p->list);
        free(p);
        os_mutex_post(&obj->mutex);
        /* } */
    }
}

/*----------------------------------------------------------------------------*/
/**@brief wav叠加器通道添加
   @param
	obj:叠加总控制 key:通道标识
   @return 成功与失败
   @note
*/
/*----------------------------------------------------------------------------*/
tbool wav_mix_chl_add(WAV_MIX_OBJ *obj, const char *key, u8 file_max, u8 default_vol, u8 max_vol, u8 radd)
{
    if (obj == NULL) {
        return false;
    }

    WAV_MIX_CHL *mix_chl;
    if (radd == 0) {
        list_for_each_entry(mix_chl, &(obj->head), list) {
            if (strcmp((const char *)key, (const char *)mix_chl->key) == 0) {
                wav_mix_printf("wav mix chl is exist !!!\n");
                return false;
            }
        }
    } else {
        wav_mix_chl_del(obj, key);
    }

    u32 ctr_buf_len = WAV_MIX_SIZEOF_ALIN(sizeof(WAV_MIX_CHL), 4) + WAV_MIX_SIZEOF_ALIN(sizeof(WAV_INFO), 4) * file_max;
    u8 *ctr_buf = (u8 *)calloc(1, ctr_buf_len);
    if (ctr_buf == NULL) {
        wav_mix_printf("no space for mix_chl err !!!\n");
        return false;
    }
    mix_chl = (WAV_MIX_CHL *)(ctr_buf);

    ctr_buf += WAV_MIX_SIZEOF_ALIN(sizeof(WAV_MIX_CHL), 4);
    mix_chl->info = (WAV_INFO *)(ctr_buf);


    if (os_mutex_create(&mix_chl->mutex) != OS_NO_ERR) {
        free(mix_chl);
        wav_mix_printf("wav mix chl mutex fail %d !!!\n", __LINE__);
        return false;
    }

    OS_TCB *cur_task_tcb = os_get_task_tcb(OS_TASK_SELF);

    mix_chl->key = (void *)key;
    mix_chl->mix_obj = obj;
    mix_chl->father_name = (void *)cur_task_tcb->name;
    mix_chl->file_max = file_max;
    mix_chl->file_max_cnt = 0;
    mix_chl->rpt_cnt = 1;

    mix_chl->isBusy = 0;

    user_dac_digital_init(&(mix_chl->dac_var), max_vol, default_vol, obj->samplerate);

    os_mutex_pend(&mix_chl->mutex, 0);
    mix_chl->stop = 1;
    os_mutex_post(&mix_chl->mutex);

    wav_mix_printf("wav mix chl add task name  = %s,  %d !!!\n", mix_chl->father_name, __LINE__);

    os_mutex_pend(&obj->mutex, 0);
    list_add(&(mix_chl->list), &(obj->head));
    os_mutex_post(&obj->mutex);

    return true;
}

