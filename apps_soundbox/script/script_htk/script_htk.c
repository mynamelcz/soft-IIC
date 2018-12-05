#include "script_htk.h"
#include "common/msg.h"
#include "script_htk_key.h"
#include "notice_player.h"
#include "dac/dac_api.h"
#include "htk.h"
#include "common/app_cfg.h"
#include "music/music_player.h"
#include "music/music_player_api.h"
#include "sound_mix_api.h"

#define SCRIPT_HTK_DEBUG_ENABLE
#ifdef SCRIPT_HTK_DEBUG_ENABLE
#define script_htk_printf	printf
#else
#define script_htk_printf(...)
#endif

#define HTK_TIMEOUT	30

#define HTK_MUSIC_DEC_TASK_NAME	"HTKDecPhyTask"

#if (SOUND_MIX_GLOBAL_EN)
#define HTK_MIX_KEY					"HTK_MIX"
#define HTK_MIX_BUF_LEN				(4*1024L)
#define HTK_MIX_OUTPUT_DEC_LIMIT	(50)
#endif//SOUND_MIX_GLOBAL_EN


extern volatile u8 new_lg_dev_let;

typedef enum {
    HTK_CMD_SONGS = 0x0,
    HTK_CMD_CHILDREN_SONG,
    HTK_CMD_STORY,
    HTK_CMD_CHINESE_STUDIES,
    HTK_CMD_ENGLISH,

    HTK_CMD_MAX,
} HTK_CMD;

typedef enum {
    HTK_MUSIC_DIR_SONGS = 0x0,
    HTK_MUSIC_DIR_CHILDREN_SONG,
    HTK_MUSIC_DIR_STORY,
    HTK_MUSIC_DIR_CHINESE_STUDIES,
    HTK_MUSIC_DIR_ENGLISH,

    HTK_MUSIC_DIR_MAX,
} HTK_MUSIC_DIR;


const char *htk_music_dir_list[HTK_MUSIC_DIR_MAX] = {
    [HTK_MUSIC_DIR_SONGS] = "/gequ/",
    [HTK_MUSIC_DIR_CHILDREN_SONG] = "/erge/",
    [HTK_MUSIC_DIR_STORY] = "/gushi/",
    [HTK_MUSIC_DIR_CHINESE_STUDIES] = "/guoxue/",
    [HTK_MUSIC_DIR_ENGLISH] = "/yingyu/",

};



typedef struct __SCRIPT_HTK_OP {
    u32 dir_index[HTK_MUSIC_DIR_MAX];
} SCRIPT_HTK_OP;

const char *htk_cmd_debug_list[HTK_CMD_MAX] = {
    "songs",
    "children song",
    "story",
    "chinese studies",
    "english",
};

static tbool script_htk_notice_play_msg_callback(int msg[], void *priv)
{
    if (msg[0] != MSG_HALF_SECOND) {
        script_htk_printf("htk notice play msg = %d\n", msg[0]);
    }
    ///this is an example
//	if(msg[0] == MSG_MUSIC_PREV_FILE)
//		return true;
    return false;
}



static NOTICE_PlAYER_ERR script_htk_notice_play(void *parg, const char *path)
{
    NOTICE_PlAYER_ERR err =  notice_player_play_by_path_to_mix(SCRIPT_TASK_NAME, path, script_htk_notice_play_msg_callback, parg, sound_mix_api_get_obj());
    return err;
}


static tbool script_htk_act_music_play(SCRIPT_HTK_OP *htk_op, HTK_MUSIC_DIR dir)
{
    u32 m_err;
    u32 dir_total_file;
    lg_dev_info *cur_lgdev_info = NULL;
    tbool ret = true;
    NOTICE_PlAYER_ERR n_err;
    u8 exit = 0;
    int msg[6] = {0};
    if ((htk_op == NULL) || (dir >= HTK_MUSIC_DIR_MAX)) {
        return false;
    }

    htk_op->dir_index[dir] ++;
    script_htk_printf("dir %d, file index = %d\n", dir, htk_op->dir_index[dir]);

    MUSIC_PLAYER_OBJ *mapi = music_player_creat();
    MY_ASSERT(mapi);
    __music_player_set_dec_father_name(mapi, SCRIPT_TASK_NAME);
    __music_player_set_decoder_type(mapi, DEC_PHY_MP3);
    __music_player_set_file_ext(mapi, "MP3");
    __music_player_set_dev_sel_mode(mapi, DEV_SEL_SPEC);
    __music_player_set_file_sel_mode(mapi, PLAY_FILE_BYPATH);
    __music_player_set_play_mode(mapi, REPEAT_FOLDER);
    __music_player_set_dev_let(mapi, new_lg_dev_let);
    __music_player_set_path(mapi, htk_music_dir_list[dir]);
    __music_player_set_file_index(mapi, htk_op->dir_index[dir]);

#if (SOUND_MIX_GLOBAL_EN)
    if (true == __music_player_set_output_to_mix(mapi, sound_mix_api_get_obj(), HTK_MIX_KEY, HTK_MIX_BUF_LEN, HTK_MIX_OUTPUT_DEC_LIMIT, DAC_DIGIT_TRACK_DEFAULT_VOL, DAC_DIGIT_TRACK_MAX_VOL)) {
        script_htk_printf("script_music to mix!!\n");
    }
#endif//SOUND_MIX_GLOBAL_EN

    ret = music_player_process(mapi, HTK_MUSIC_DEC_TASK_NAME, TaskMusicPhyDecPrio);
    if (ret == true) {
        m_err = music_player_play(mapi);
        if (m_err != SUCC_MUSIC_START_DEC) {
            ret = false;
            script_htk_printf("script htk music play fail %d !!!\n", m_err);
            n_err = script_htk_notice_play(htk_op, BPF_MUSIC_NO_FILE);
            if (n_err != NOTICE_PLAYER_NO_ERR) {
                script_htk_printf("--%s %d--notice play fail !!\n", __func__, __LINE__);
            }
            goto __exit;
        }
    }

    htk_op->dir_index[dir] = __music_player_get_cur_file_index(mapi, 0);
    script_htk_printf("script htk music play ok !!\n");

    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();
        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            /****attention !!!!  make sure all your malloc resource <<<in this function>>>  free here***/
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                printf("****attention !!!!  make sure all your malloc resource in function %s  free here***/ \n", __func__);
                music_player_destroy(&mapi);
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;
        case SYS_EVENT_DEC_FR_END:
        case SYS_EVENT_DEC_FF_END:
        case SYS_EVENT_DEC_END:
            dir_total_file = __music_player_get_file_total(mapi, 0);
            script_htk_printf("script htk music play end , cur file = %d, total = %d!!\n", htk_op->dir_index[dir], dir_total_file);
            if (htk_op->dir_index[dir] >= dir_total_file) {
                script_htk_printf("htk music dir play is over limit\n");
                htk_op->dir_index[dir] = 0;
            }
            exit = 1;
            break;
        case SYS_EVENT_DEV_OFFLINE:
            printf("htk dev offline \n");
            cur_lgdev_info = __music_player_get_lg_dev_info(mapi);
            if ((cur_lgdev_info != NULL) && (cur_lgdev_info->lg_hdl->phydev_item == (void *)msg[1])) {
                if (__music_player_get_status(mapi) != DECODER_PAUSE) {
                    break;//不是解码暂停状态，有下一个消息SYS_EVENT_DEC_DEVICE_ERR处理
                }
            } else {
                break;
            }
        case SYS_EVENT_DEC_DEVICE_ERR:
            music_player_close(mapi);
            os_taskq_post(MAIN_TASK_NAME, 1, MSG_CHANGE_WORKMODE);
            exit = 1;
            ret = false;
            break;
        default:
            break;
        }
        if (exit) {
            break;
        }
    }
__exit:
    music_player_destroy(&mapi);
    return ret;
}

static tbool htk_action_callback(void *priv, char result[], char nums, u8 isTimeout)
{
    HTK_CMD action = (HTK_CMD)result[0];
    NOTICE_PlAYER_ERR n_err;
    tbool ret = false;
    SCRIPT_HTK_OP *htk_op = (SCRIPT_HTK_OP *)priv;
    if (htk_op == NULL) {
        return false;
    }

    if ((isTimeout == 1) || (nums == 0)) {
        n_err = script_htk_notice_play(htk_op, BPF_HTK_UNKNOW);
        if (n_err != NOTICE_PLAYER_NO_ERR) {
            script_htk_printf("--%s %d--notice play fail !!\n", __func__, __LINE__);
        }
        script_htk_printf("isTimeout or nums == 0\n");
        return true;
    } else {
        if (action < HTK_CMD_MAX) {
            script_htk_printf("htk ack >>>>>>>>: %s\n", htk_cmd_debug_list[action]);
            switch (action) {
            case HTK_CMD_SONGS:
                ret = script_htk_act_music_play(htk_op, HTK_MUSIC_DIR_SONGS);
                break;
            case HTK_CMD_CHILDREN_SONG:
                ret = script_htk_act_music_play(htk_op, HTK_MUSIC_DIR_CHILDREN_SONG);
                break;
            case HTK_CMD_STORY:
                ret = script_htk_act_music_play(htk_op, HTK_MUSIC_DIR_STORY);
                break;
            case HTK_CMD_CHINESE_STUDIES:
                ret = script_htk_act_music_play(htk_op, HTK_MUSIC_DIR_CHINESE_STUDIES);
                break;
            case HTK_CMD_ENGLISH:
                ret = script_htk_act_music_play(htk_op, HTK_MUSIC_DIR_ENGLISH);
                break;

            default:
                ret = false;
                break;

            }
            if (ret == false) {
                script_htk_printf("dir music play fail\n");
            } else {

            }

            n_err = script_htk_notice_play(htk_op, BPF_HTK_NEED_DO_MORE);
            if (n_err != NOTICE_PLAYER_NO_ERR) {
                script_htk_printf("--%s %d--notice play fail !!\n", __func__, __LINE__);
            }

            script_htk_printf("what can i do for you\n");
            return true;
        } else {
            script_htk_printf("----%d---- htk ack is unknowni!!!\n");
            n_err = script_htk_notice_play(htk_op, BPF_HTK_UNKNOW);
            if (n_err != NOTICE_PLAYER_NO_ERR) {
                script_htk_printf("--%s %d--notice play fail !!\n", __func__, __LINE__);
            }
            return true;
        }
    }
    return false;
}

static int htk_msg_callback(void *priv)
{
    u8 ret;
    int msg[6];
    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        ret = os_taskq_accept_no_clean(ARRAY_SIZE(msg), msg);
        clear_wdt();
        if (ret == OS_Q_EMPTY) {
            break;
        } else {
            if (msg[0] == SYS_EVENT_DEL_TASK) {
                return 1;
            } else {
                os_taskq_msg_clean(msg[0], 1);
            }
        }
    }
    return 0;
}


static void *script_htk_init(void *priv)
{
    dac_channel_on(MUSIC_CHANNEL, FADE_ON);
    script_htk_printf("script htk init ok\n");
    SCRIPT_HTK_OP *hdl = (SCRIPT_HTK_OP *)calloc(1, sizeof(SCRIPT_HTK_OP));
    MY_ASSERT(hdl);
    return (void *)hdl;
}

static void script_htk_exit(void **hdl)
{
    if ((hdl != NULL) && (*hdl != NULL)) {
        free(*hdl);
        *hdl = NULL;
    }
    /* notice_player_stop(); */
    dac_channel_off(MUSIC_CHANNEL, FADE_ON);
}

static void script_htk_task(void *parg)
{
    int msg[6] = {0};
    tbool ret;

    NOTICE_PlAYER_ERR n_err;
    SCRIPT_HTK_OP *htk_op = (SCRIPT_HTK_OP *)parg;
    malloc_stats();
    n_err = script_htk_notice_play(NULL, BPF_HTK_MP3);
    if (n_err != NOTICE_PLAYER_TASK_DEL_ERR) {
        ret = htk_player_process((void *)htk_op, htk_action_callback, htk_msg_callback, HTK_TIMEOUT);
        if (ret == false) {
            /* os_taskq_post(MAIN_TASK_NAME, 1, MSG_CHANGE_WORKMODE); */
        }
    }
    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();
        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                script_htk_printf("script_htk_del \r");
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;
        case MSG_HALF_SECOND:
            if (ret == false) {
                script_htk_printf("script htk err !!!\n");
            }
            break;

        default:
            break;
        }
    }
}


const SCRIPT script_htk_info = {
    .skip_check = NULL,
    .init = script_htk_init,
    .exit = script_htk_exit,
    .task = script_htk_task,
    .key_register = &script_htk_key,
};

