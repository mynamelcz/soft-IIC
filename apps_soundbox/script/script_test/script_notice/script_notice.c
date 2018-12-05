#include "script_notice.h"
#include "common/app_cfg.h"
#include "common/msg.h"
#include "script_notice_key.h"
#include "notice_player.h"
#include "dac/dac_api.h"
#include "cbuf/circular_buf.h"

#define SCRIPT_NOTICE_DEBUG_ENABLE
#ifdef SCRIPT_NOTICE_DEBUG_ENABLE
#define script_notice_printf	printf
#else
#define script_notice_printf(...)
#endif

static tbool script_test_notice_play_msg_callback(int msg[], void *priv)
{
    if (msg[0] != MSG_HALF_SECOND) {
        script_notice_printf("test notice play msg = %d\n", msg[0]);
    }
    ///this is an example
//	if(msg[0] == MSG_MUSIC_PREV_FILE)
//		return true;
    return false;
}

static NOTICE_PlAYER_ERR script_test_notice_play(void *parg, const char *path)
{
    /* NOTICE_PlAYER_ERR err =  notice_player_play_by_path(SCRIPT_TASK_NAME, path, script_test_notice_play_msg_callback, parg); */
    NOTICE_PlAYER_ERR err =  wav_play_by_path(SCRIPT_TASK_NAME, path, script_test_notice_play_msg_callback, parg);
    return err;
}



static void *script_notice_init(void *priv)
{
    dac_channel_on(MUSIC_CHANNEL, FADE_ON);
    script_notice_printf("script notice init ok\n");

    return (void *)NULL;
}

static void script_notice_exit(void **hdl)
{
    script_notice_printf("script notice exit ok\n");

    dac_channel_off(MUSIC_CHANNEL, FADE_ON);
}

static void script_notice_task(void *parg)
{
    int msg[6] = {0};
    u8 test_fail = 0;
    u32 method_cnt = 0;
    NOTICE_PlAYER_ERR n_err;
    u32 test_num_array[2] = {
        5,
        6,
    };
    const char *test_path_array[2] = {
        "/wav/mbg0.wav",
        "/wav/mbg1.wav",
    };

    script_notice_printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>test notice start<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
    method_cnt++;
    script_notice_printf("----------------------------method %d   line %d----------------------------------\n", method_cnt, __LINE__);
    n_err =  wav_play_by_path(SCRIPT_TASK_NAME, "/wav/mbg0.wav", script_test_notice_play_msg_callback, NULL);
    if (n_err != NOTICE_PLAYER_NO_ERR) {
        test_fail = 1;
        if (n_err == NOTICE_PLAYER_TASK_DEL_ERR) {
            test_fail = 2;
        }
        goto __notice_test_exit;
    }

    method_cnt++;
    script_notice_printf("----------------------------method %d   line %d----------------------------------\n", method_cnt, __LINE__);
    n_err =  wav_play_by_num(SCRIPT_TASK_NAME, "/wav/", 5, script_test_notice_play_msg_callback, NULL);
    if (n_err != NOTICE_PLAYER_NO_ERR) {
        test_fail = 1;
        if (n_err == NOTICE_PLAYER_TASK_DEL_ERR) {
            test_fail = 2;
        }
        goto __notice_test_exit;
    }

    method_cnt++;
    script_notice_printf("----------------------------method %d   line %d----------------------------------\n", method_cnt, __LINE__);
    n_err =  notice_player_play_by_path(SCRIPT_TASK_NAME, "/wav/mbg0.wav", script_test_notice_play_msg_callback, NULL);
    if (n_err != NOTICE_PLAYER_NO_ERR) {
        test_fail = 1;
        if (n_err == NOTICE_PLAYER_TASK_DEL_ERR) {
            test_fail = 2;
        }
        goto __notice_test_exit;
    }

    method_cnt++;
    script_notice_printf("----------------------------method %d   line %d----------------------------------\n", method_cnt, __LINE__);
    n_err =  notice_player_play_by_num(SCRIPT_TASK_NAME, "/wav/", 5, script_test_notice_play_msg_callback, NULL);
    if (n_err != NOTICE_PLAYER_NO_ERR) {
        test_fail = 1;
        if (n_err == NOTICE_PLAYER_TASK_DEL_ERR) {
            test_fail = 2;
        }
        goto __notice_test_exit;
    }
    method_cnt++;
    script_notice_printf("----------------------------method %d   line %d----------------------------------\n", method_cnt, __LINE__);
    n_err =  notice_player_play_spec_dev_by_path(SCRIPT_TASK_NAME, 'A', "/wav/mbg0.wav", script_test_notice_play_msg_callback, NULL);
    if (n_err != NOTICE_PLAYER_NO_ERR) {
        test_fail = 1;
        if (n_err == NOTICE_PLAYER_TASK_DEL_ERR) {
            test_fail = 2;
        }
        goto __notice_test_exit;
    }

    method_cnt++;
    script_notice_printf("----------------------------method %d   line %d----------------------------------\n", method_cnt, __LINE__);
    n_err =  notice_player_play_spec_dev_by_num(SCRIPT_TASK_NAME, 'A', "/wav/", 5, script_test_notice_play_msg_callback, NULL);
    if (n_err != NOTICE_PLAYER_NO_ERR) {
        test_fail = 1;
        if (n_err == NOTICE_PLAYER_TASK_DEL_ERR) {
            test_fail = 2;
        }
        goto __notice_test_exit;
    }

    method_cnt++;
    script_notice_printf("----------------------------method %d   line %d----------------------------------\n", method_cnt, __LINE__);
    /* NOTICE_PlAYER_ERR notice_player_play_spec_dev(void *father_name, msg_cb _cb, void *priv, u32 dev_let, u32 rpt_time, u32 delay, NOTICE_PLAYER_METHOD method, int argc, ...) */
    n_err =  notice_player_play_spec_dev(SCRIPT_TASK_NAME, script_test_notice_play_msg_callback, NULL, 'A', 1, 0, NOTICE_PLAYER_METHOD_BY_PATH, 1, "/wav/mbg0.wav");
    if (n_err != NOTICE_PLAYER_NO_ERR) {
        test_fail = 1;
        if (n_err == NOTICE_PLAYER_TASK_DEL_ERR) {
            test_fail = 2;
        }
        goto __notice_test_exit;
    }

    method_cnt++;
    script_notice_printf("----------------------------method %d   line %d----------------------------------\n", method_cnt, __LINE__);
    script_notice_printf("!!!!!!!!attention!!!!!!!!, make sure the play target format is pcm, when use <NOTICE_USE_WAV_PLAYER> this method\n");
    /* NOTICE_PlAYER_ERR notice_player_play_spec_dev(void *father_name, msg_cb _cb, void *priv, u32 dev_let, u32 rpt_time, u32 delay, NOTICE_PLAYER_METHOD method, int argc, ...) */
    n_err =  notice_player_play_spec_dev(SCRIPT_TASK_NAME, script_test_notice_play_msg_callback, NULL, 'A', 1, 0, NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_USE_WAV_PLAYER, 1, "/wav/mbg0.wav");
    if (n_err != NOTICE_PLAYER_NO_ERR) {
        test_fail = 1;
        if (n_err == NOTICE_PLAYER_TASK_DEL_ERR) {
            test_fail = 2;
        }
        goto __notice_test_exit;
    }
    method_cnt++;
    script_notice_printf("----------------------------method %d   line %d----------------------------------\n", method_cnt, __LINE__);
    script_notice_printf("!!!!!!!!attention!!!!!!!!, make sure the play target format is pcm, when use <NOTICE_USE_WAV_PLAYER> this method\n");
    /* NOTICE_PlAYER_ERR notice_player_play_spec_dev(void *father_name, msg_cb _cb, void *priv, u32 dev_let, u32 rpt_time, u32 delay, NOTICE_PLAYER_METHOD method, int argc, ...) */
    n_err =  notice_player_play_spec_dev(SCRIPT_TASK_NAME, script_test_notice_play_msg_callback, NULL, 'A', 1, 0, NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_USE_WAV_PLAYER, 2, "/wav/mbg0.wav", "/wav/mbg1.wav");
    if (n_err != NOTICE_PLAYER_NO_ERR) {
        test_fail = 1;
        if (n_err == NOTICE_PLAYER_TASK_DEL_ERR) {
            test_fail = 2;
        }
        goto __notice_test_exit;
    }
    method_cnt++;
    script_notice_printf("----------------------------method %d   line %d----------------------------------\n", method_cnt, __LINE__);
    script_notice_printf("!!!!!!!!attention!!!!!!!!, make sure the play target format is pcm, when use <NOTICE_USE_WAV_PLAYER> this method\n");

    /* NOTICE_PlAYER_ERR notice_player_play_spec_dev(void *father_name, msg_cb _cb, void *priv, u32 dev_let, u32 rpt_time, u32 delay, NOTICE_PLAYER_METHOD method, int argc, ...) */
    n_err =  notice_player_play_spec_dev(SCRIPT_TASK_NAME, script_test_notice_play_msg_callback, NULL, 'A', 1, 0, NOTICE_PLAYER_METHOD_BY_PATH_ARRAY | NOTICE_USE_WAV_PLAYER, 2, sizeof(test_path_array) / sizeof(test_path_array[0]), test_path_array);
    if (n_err != NOTICE_PLAYER_NO_ERR) {
        test_fail = 1;
        if (n_err == NOTICE_PLAYER_TASK_DEL_ERR) {
            test_fail = 2;
        }
        goto __notice_test_exit;
    }
    method_cnt++;
    script_notice_printf("----------------------------method %d   line %d----------------------------------\n", method_cnt, __LINE__);
    script_notice_printf("!!!!!!!!attention!!!!!!!!, make sure the play target format is pcm, when use <NOTICE_USE_WAV_PLAYER> this method\n");
    script_notice_printf("attention this method param,  when use <NOTICE_PLAYER_METHOD_BY_NUM_ARRAY> this method\n");
    n_err =  notice_player_play_spec_dev(SCRIPT_TASK_NAME, script_test_notice_play_msg_callback, NULL, 'A', 1, 0, NOTICE_PLAYER_METHOD_BY_NUM_ARRAY | NOTICE_USE_WAV_PLAYER, 3, "/wav/", sizeof(test_path_array) / sizeof(test_path_array[0]), test_num_array);
    if (n_err != NOTICE_PLAYER_NO_ERR) {
        test_fail = 1;
        if (n_err == NOTICE_PLAYER_TASK_DEL_ERR) {
            test_fail = 2;
        }
        goto __notice_test_exit;
    }
    method_cnt++;
    script_notice_printf("----------------------------method %d   line %d----------------------------------\n", method_cnt, __LINE__);
    script_notice_printf("!!!!!!!!attention!!!!!!!!, make sure the play target format is pcm,  when use <NOTICE_USE_WAV_PLAYER> this method\n");
    script_notice_printf("attention this method param,  when use <NOTICE_PLAYER_METHOD_BY_NUM> this method\n");
    /* NOTICE_PlAYER_ERR notice_player_play_spec_dev(void *father_name, msg_cb _cb, void *priv, u32 dev_let, u32 rpt_time, u32 delay, NOTICE_PLAYER_METHOD method, int argc, ...) */
    n_err =  notice_player_play_spec_dev(SCRIPT_TASK_NAME, script_test_notice_play_msg_callback, NULL, 'A', 1, 0, NOTICE_PLAYER_METHOD_BY_NUM | NOTICE_USE_WAV_PLAYER, 3, "/wav/"/*must be dirpath*/, 5/*index1*/, 6/*index2*/);
    if (n_err != NOTICE_PLAYER_NO_ERR) {
        test_fail = 1;
        if (n_err == NOTICE_PLAYER_TASK_DEL_ERR) {
            test_fail = 2;
        }
        goto __notice_test_exit;
    }

    method_cnt++;
    script_notice_printf("----------------------------method %d   line %d----------------------------------\n", method_cnt, __LINE__);
    n_err =  notice_player_play_spec_dev(SCRIPT_TASK_NAME, script_test_notice_play_msg_callback, NULL, 'A', 1, 0, NOTICE_PLAYER_METHOD_BY_PATH, 2, "/wav/mbg0.wav", "/wav/mbg1.wav");
    if (n_err != NOTICE_PLAYER_NO_ERR) {
        test_fail = 1;
        if (n_err == NOTICE_PLAYER_TASK_DEL_ERR) {
            test_fail = 2;
        }
        goto __notice_test_exit;
    }
    method_cnt++;
    script_notice_printf("----------------------------method %d   line %d----------------------------------\n", method_cnt, __LINE__);
    n_err =  notice_player_play_spec_dev(SCRIPT_TASK_NAME, script_test_notice_play_msg_callback, NULL, 'A', 1, 0, NOTICE_PLAYER_METHOD_BY_PATH_ARRAY, 2, sizeof(test_path_array) / sizeof(test_path_array[0]), test_path_array);
    if (n_err != NOTICE_PLAYER_NO_ERR) {
        test_fail = 1;
        if (n_err == NOTICE_PLAYER_TASK_DEL_ERR) {
            test_fail = 2;
        }
        goto __notice_test_exit;
    }
    method_cnt++;
    script_notice_printf("----------------------------method %d   line %d----------------------------------\n", method_cnt, __LINE__);
    n_err =  notice_player_play_spec_dev(SCRIPT_TASK_NAME, script_test_notice_play_msg_callback, NULL, 'A', 1, 0, NOTICE_PLAYER_METHOD_BY_NUM_ARRAY, 3, "/wav/", sizeof(test_path_array) / sizeof(test_path_array[0]), test_num_array);
    if (n_err != NOTICE_PLAYER_NO_ERR) {
        test_fail = 1;
        if (n_err == NOTICE_PLAYER_TASK_DEL_ERR) {
            test_fail = 2;
        }
        goto __notice_test_exit;
    }
    method_cnt++;
    script_notice_printf("----------------------------method %d   line %d----------------------------------\n", method_cnt, __LINE__);
    n_err =  notice_player_play_spec_dev(SCRIPT_TASK_NAME, script_test_notice_play_msg_callback, NULL, 'A', 1, 0, NOTICE_PLAYER_METHOD_BY_NUM, 3, "/wav/"/*must be dirpath*/, 5/*index1*/, 6/*index2*/);
    if (n_err != NOTICE_PLAYER_NO_ERR) {
        test_fail = 1;
        if (n_err == NOTICE_PLAYER_TASK_DEL_ERR) {
            test_fail = 2;
        }
        goto __notice_test_exit;
    }
    method_cnt++;
    script_notice_printf("----------------------------method %d   line %d----------------------------------\n", method_cnt, __LINE__);
    script_notice_printf("attention this method param,  when parm3 is NULL, notice play without wait, you  need to wait the msg <SYS_EVENT_PLAY_SEL_END> in your control process\n");
    n_err =  notice_player_play_spec_dev(SCRIPT_TASK_NAME, NULL, NULL, 'A', 1, 0, NOTICE_PLAYER_METHOD_BY_NUM, 3, "/wav/"/*must be dirpath*/, 5/*index1*/, 6/*index2*/);
    if (n_err != NOTICE_PLAYER_NO_ERR_WITHOUT_MSG_DEAL) {
        test_fail = 1;
        goto __notice_test_exit;
    }

__notice_test_exit:
    if (test_fail == 1) {
        script_notice_printf("----------------------------test notice fail----------------------------------\n");
    } else if (test_fail == 2) {
        script_notice_printf("----------------------------test notice change mode exit----------------------------------\n");
    }

    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();
        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                script_notice_printf("script_notice_del \r");
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;
        case SYS_EVENT_PLAY_SEL_END:
            script_notice_printf("method %d play end, !!!!!!!!!!!!!attention the follow deal, stop the notice player\n", method_cnt);
            notice_player_stop();
            script_notice_printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>test notice end<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
            break;

        default:
            break;
        }
    }
}

const SCRIPT script_notice_info = {
    .skip_check = NULL,
    .init = script_notice_init,
    .exit = script_notice_exit,
    .task = script_notice_task,
    .key_register = &script_notice_key,
};


