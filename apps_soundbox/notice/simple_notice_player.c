#include "simple_notice_player.h"
#include "rtos/os_api.h"
#include "rtos/os_cfg.h"
#include "dac/dac_api.h"
#include "wav_play.h"

//#define SIMPLE_NOTICE_PLAYER_DEBUG_ENABLE

#ifdef SIMPLE_NOTICE_PLAYER_DEBUG_ENABLE
#define simple_notice_player_printf	printf
#else
#define simple_notice_player_printf(...)
#endif//SIMPLE_NOTICE_PLAYER_DEBUG_ENABLE

///***************************simple notice player***************************************///
///***************************simple notice player***************************************///
///***************************simple notice player***************************************///

/*----------------------------------------------------------------------------*/
/**@brief simple_notice_player 停止播放接口
   @param
     	obj:控制句柄，注意是双重指针
   @return
   @note 保证和simple_notice_player_play_by_path成对出现
*/
/*----------------------------------------------------------------------------*/
void simple_notice_player_stop(SIMPLE_NOTICE_PLAYER_OBJ **obj)
{
    simple_notice_player_printf("simple notice stop ok !! , func = %s\n", __func__);
    music_player_destroy((MUSIC_PLAYER_OBJ **)obj);
}

/*----------------------------------------------------------------------------*/
/**@brief simple_notice_player 暂停/播放接口
   @param
     	obj:控制句柄
   @return
   @note 注意是在播放过程中才生效
*/
/*----------------------------------------------------------------------------*/
_DECODE_STATUS simple_notice_player_pp(SIMPLE_NOTICE_PLAYER_OBJ *obj)
{
    return music_player_pp((MUSIC_PLAYER_OBJ *)obj);
}


/*----------------------------------------------------------------------------*/
/**@brief simple_notice_player 播放接口
   @param
		father_name:控制线程名称
		my_name:解码线程名称
		prio:解码线程优先级，用户自行分配,留意不要和其他线程优先级冲突
		mix_obj:叠加控制句柄，如果不需要叠加，填NULL即可
		mix_key:叠加通道名称，注意不要和其他叠加通道重复
		path:播放路径，这里暂时支持内置flash全路径, 并且只支持MP3、wav两种格式
   @return 提示音控制句柄，播放失败返回NULL
   @note 保证和simple_notice_player_stop成对出现, 播放结束会推出SYS_EVENT_DEC_END消息
   并附带解码线程名称，方便区分是哪个提示音播放结束
*/
/*----------------------------------------------------------------------------*/
SIMPLE_NOTICE_PLAYER_OBJ *simple_notice_player_play_by_path(
    void *father_name, 			/*控制线程名称*/
    void *my_name,				/*提示音线程名称*/
    u8    prio,					/*提示音线程优先级*/
    SOUND_MIX_OBJ *mix_obj,			/*叠加控制句柄，如果不需要叠加，传NULL即可*/
    const char *mix_key,		/*叠加通道key， 主要用于标识叠加通道的字符串， 如果不需要叠加， 传NULL即可*/
    const char *path			/*播放路径*/
)
{
    tbool ret;
    MUSIC_PLAYER_OBJ *obj = music_player_creat();
    if (obj == NULL) {
        return NULL;
    }
    __music_player_set_dec_father_name(obj, father_name);
    __music_player_set_decoder_type(obj, DEC_PHY_MP3 | DEC_PHY_WAV);
    __music_player_set_file_ext(obj, "MP3WAVC");
    __music_player_set_dev_sel_mode(obj, DEV_SEL_SPEC);
    __music_player_set_file_sel_mode(obj, PLAY_FILE_BYPATH);
    __music_player_set_play_mode(obj, REPEAT_ALL);
    __music_player_set_dev_let(obj, 'A');
    __music_player_set_path(obj, path);

    if (mix_obj != NULL && mix_key != NULL) {
        if (__music_player_set_output_to_mix(obj, mix_obj, mix_key, 0x1000, 50, DAC_DIGIT_TRACK_DEFAULT_VOL, DAC_DIGIT_TRACK_MAX_VOL) == false) {
            simple_notice_player_printf("simple_notice_player mix add fail!!, func = %s\n", __func__);
            music_player_destroy(&obj);
            return NULL;
        }
    }

    ret = music_player_process(obj, my_name, prio);
    if (ret == true) {
        u32 m_err = music_player_play(obj);
        if (m_err != SUCC_MUSIC_START_DEC) {
            simple_notice_player_printf("simple notice play fail %d , func = %s\n", m_err, __func__);
            music_player_destroy(&obj);
            return NULL;
        }
    } else {
        simple_notice_player_printf("simple notice task creat fail , func = %s\n", __func__);
        music_player_destroy(&obj);
        return NULL;

    }

    simple_notice_player_printf("simple notice play ok !! , func = %s\n", __func__);
    return (SIMPLE_NOTICE_PLAYER_OBJ *)obj;
}
///***************************simple notice player end***************************************///




///***************************simple wav play***************************************///
///***************************simple wav play***************************************///
///***************************simple wav play***************************************///

struct __SIMPLE_WAV_PLAY_OBJ {
    void 			*father_name;
    void 			*my_name;
    SOUND_MIX_OBJ 	*mix_obj;
    const char 		*mix_key;
    const char 		*path;
    volatile _DECODE_STATUS status;
};

#define SIMPLE_WAV_PLAY_MIX_BUF			(2*1024)
#define SIMPLE_WAV_PLAY_MIX_LIMIT		(50)

static void simple_wav_play_task_deal(void *prag)
{
    SIMPLE_WAV_PLAY_OBJ *obj = (SIMPLE_WAV_PLAY_OBJ *)prag;
    int msg[6] = {0};
    tbool ret;
    WAV_PLAYER *wav_obj = wav_player_creat();
    if (wav_obj == NULL) {

    } else {
        if (__wav_player_set_output_to_mix(wav_obj, obj->mix_obj,
                                           obj->mix_key,
                                           SIMPLE_WAV_PLAY_MIX_BUF,
                                           SIMPLE_WAV_PLAY_MIX_LIMIT,
                                           DAC_DIGIT_TRACK_DEFAULT_VOL,
                                           DAC_DIGIT_TRACK_MAX_VOL) == false) {
            simple_notice_player_printf("notice_player mix add fail!! %d\n", __LINE__);
        }

        ret = wav_player_set_path(wav_obj, obj->path, 1);
        if (ret == false) {
            wav_player_destroy(&wav_obj);
        } else {
            os_taskq_post(obj->my_name, 1, SYS_EVENT_BEGIN_DEC);
        }
    }

    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        if ((wav_obj != NULL) && (wav_player_get_status(wav_obj) == WAV_PLAYER_ST_PLAY)) {
            if (OSTaskQAccept(ARRAY_SIZE(msg), msg) == OS_Q_EMPTY) {
                ret = wav_player_decode(wav_obj);
                if (ret == false) {
                    msg[0] = SYS_EVENT_DEC_END;
                } else {
                    continue;
                }
            }
        } else {
            os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        }

        switch (msg[0]) {
        case SYS_EVENT_DEC_END:
            if (obj->father_name != NULL) {
                os_taskq_post_event(obj->father_name, 2, SYS_EVENT_DEC_END, (int)obj->my_name);
                obj->status = DECODER_STOP;
            }
            break;
        case SYS_EVENT_BEGIN_DEC:
            ret = wav_player_play(wav_obj);
            obj->status = (_DECODE_STATUS)wav_player_get_status(wav_obj);
            break;

        case MSG_MUSIC_PP:
            obj->status = (_DECODE_STATUS)wav_player_pp(wav_obj);
            break;

        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                wav_player_destroy(&wav_obj);
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;

        default:
            break;
        }
    }
}


/*----------------------------------------------------------------------------*/
/**@brief simple_wav_play 停止播放接口
   @param
     	obj:控制句柄，注意是双重指针
   @return
   @note 保证和simple_wav_play_by_path成对出现
*/
/*----------------------------------------------------------------------------*/
void simple_wav_play_stop(SIMPLE_WAV_PLAY_OBJ **obj)
{
    if (obj == NULL || *obj == NULL) {
        return ;
    }
    SIMPLE_WAV_PLAY_OBJ *tmp_obj = *obj;

    if (os_task_del_req(tmp_obj->my_name) != OS_TASK_NOT_EXIST) {
        os_taskq_post_event(tmp_obj->my_name, 1, SYS_EVENT_DEL_TASK);
        do {
            OSTimeDly(1);
        } while (os_task_del_req(tmp_obj->my_name) != OS_TASK_NOT_EXIST);
        /* puts("del notice player  succ\n"); */
    }
    free(tmp_obj);
    *obj = NULL;
}


/*----------------------------------------------------------------------------*/
/**@brief simple_wav_play 暂停/播放接口
   @param
     	obj:控制句柄
   @return
   @note 注意是在播放过程中才生效
*/
/*----------------------------------------------------------------------------*/
_DECODE_STATUS simple_wav_play_pp(SIMPLE_WAV_PLAY_OBJ *obj)
{
    if (obj == NULL) {
        return DECODER_STOP;
    }

    if (obj->status == DECODER_STOP) {
        return DECODER_STOP;
    }

    _DECODE_STATUS wait_status = DECODER_STOP;
    if (obj->status == DECODER_PAUSE) {
        wait_status = DECODER_PLAY;
    } else {
        wait_status = DECODER_PAUSE;
    }

    os_taskq_post(obj->my_name, 1, MSG_MUSIC_PP);
    while (obj->status != wait_status) {
        OSTimeDly(1);
        /* if(obj->status == DECODER_STOP) */
        /* break; */
    }

    return obj->status;
}


/*----------------------------------------------------------------------------*/
/**@brief simple_wav_play 播放接口
   @param
		father_name:控制线程名称
		my_name:解码线程名称
		prio:解码线程优先级，用户自行分配,留意不要和其他线程优先级冲突
		mix_obj:叠加控制句柄，如果不需要叠加，填NULL即可
		mix_key:叠加通道名称，注意不要和其他叠加通道重复
		path:播放路径，这里暂时支持内置flash全路径, 并且只支持pcm无压缩wav格式
   @return 提示音控制句柄，播放失败返回NULL
   @note 保证和simple_wav_play_stop成对出现, 播放结束会推出SYS_EVENT_DEC_END消息
   并附带解码线程名称，方便区分是哪个提示音播放结束
*/
/*----------------------------------------------------------------------------*/
SIMPLE_WAV_PLAY_OBJ *simple_wav_play_by_path(
    void *father_name, 			/*控制线程名称*/
    void *my_name,				/*提示音线程名称*/
    u8    prio,					/*提示音线程优先级*/
    SOUND_MIX_OBJ *mix_obj,			/*叠加控制句柄，如果不需要叠加，传NULL即可*/
    const char *mix_key,		/*叠加通道key， 主要用于标识叠加通道的字符串， 如果不需要叠加， 传NULL即可*/
    const char *path			/*播放路径*/
)
{
    SIMPLE_WAV_PLAY_OBJ *obj = (SIMPLE_WAV_PLAY_OBJ *)calloc(1, sizeof(SIMPLE_WAV_PLAY_OBJ));
    if (obj == NULL) {
        return NULL;
    }

    obj->father_name = father_name;
    obj->my_name = my_name;
    obj->mix_obj = mix_obj;
    obj->mix_key = mix_key;
    obj->path = path;
    obj->status = DECODER_STOP;

    if (OS_NO_ERR == os_task_create_ext(
            simple_wav_play_task_deal,
            (void *)obj,
            prio,
            10,
            my_name,
            2 * 1024)
       ) {
        simple_notice_player_printf("creat simple wav  succ!!\n");
    } else {
        simple_notice_player_printf("creat simple wav  fail!!\n");
        free(obj);
        return NULL;
    }

    return obj;
}

///***************************simple wav play end**********************************///


#if 0
///example
void simple_notice_player_demo_control(void *parg)
{
    int msg[6] = {0};
    _DECODE_STATUS status_test;
    SIMPLE_WAV_PLAY_OBJ *simple_wav_obj = NULL;
    SIMPLE_NOTICE_PLAYER_OBJ *simple_notice_obj = NULL;

    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();
        if (msg[0] != MSG_HALF_SECOND) {
            printf("msg = %x, %d, %s\n", msg[0], __LINE__, __func__);
        }
        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                simple_notice_player_stop(&simple_notice_obj);
                simple_wav_play_stop(&simple_wav_obj);
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;

        case MSG_MUSIC_PP:
            status_test = simple_notice_player_pp(simple_notice_obj);
            if (status_test == MIDI_ST_PLAY) {
                printf("simple_notice_player play>>");
            } else {
                printf("simple_notice_player pause>>");
            }
            break;

        case SYS_EVENT_DEC_END:
            ///无论是simple_notice—还是simple_wav播放结束消息都是SYS_EVENT_DEC_END,通过判断解码线程的名称（my_name）区分
            if (strcmp((const char *)msg[1], "TaskNotice") == 0) {
                printf("simple notice play end = %s\n", (const char *)msg[1]);
                simple_notice_player_stop(&simple_notice_obj);
            } else if (strcmp((const char *)msg[1], "TaskTestWAV") == 0) {
                printf("simple wav play end = %s\n", (const char *)msg[1]);
                simple_wav_play_stop(&simple_wav_obj);
            }
            break;

        case MSG_MUSIC_PREV_FILE:
            if (simple_wav_obj) {
                simple_wav_play_stop(&simple_wav_obj);
            }
            simple_wav_obj = simple_wav_play_by_path(SCRIPT_TASK_NAME,
                             "TaskTestWAV",
                             TaskMusicPrio,
                             sound_mix_api_get_obj(),
                             "WAVKEY",
                             "/test.wav"
                                                    );
            if (simple_wav_obj == NULL) {
                printf("\n--func=%s, line=%d\n", __FUNCTION__, __LINE__);
            }
            break;
        case MSG_MUSIC_NEXT_FILE:
            if (simple_notice_obj) {
                simple_notice_player_stop(&simple_notice_obj);
            }
            simple_notice_obj = simple_notice_player_play_by_path(SCRIPT_TASK_NAME,
                                "TaskNotice",
                                TaskMusicPrio1,
                                sound_mix_api_get_obj(),
                                "NOTICEKEY",
                                "/test_mp.mp3"
                                                                 );
            if (simple_notice_obj == NULL) {
                printf("\n--func=%s, line=%d\n", __FUNCTION__, __LINE__);
            }
            break;
        default:
            break;
        }
    }
}
#endif

