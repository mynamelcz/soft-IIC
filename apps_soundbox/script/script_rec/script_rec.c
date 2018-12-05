#include "script_rec.h"
#include "common/msg.h"
#include "script_rec_key.h"
#include "notice/notice_player.h"
#include "dac/dac_api.h"
#include "vm/vm_api.h"
#include "encode/encode_flash.h"
#include "record/record.h"
#include "record/toy_rec_module.h"
#include "music/music_player.h"
#include "file_operate/file_op.h"
#include "fat/fs_io.h"
#include "sdk_cfg.h"
#include "sound_mix_api.h"

#if((0==SUPPORT_APP_RCSP_EN) && (0 == BT_BACKMODE))

#define SCRIPT_REC_DEBUG_ENABLE

#ifdef SCRIPT_REC_DEBUG_ENABLE
#define script_rec_printf	printf
#define script_rec_puts	    puts
#else
#define script_rec_puts	   (...)
#define script_rec_printf (...)
#endif

#if (SOUND_MIX_GLOBAL_EN)
#define REC_PLAY_MIX_KEY					"REC_PLAY_MIX"
#define REC_PLAY_MIX_BUF_LEN				(4*1024L)
#define REC_PLAY_MIX_OUTPUT_DEC_LIMIT		(50)
#endif//SOUND_MIX_GLOBAL_EN


extern volatile u8 new_lg_dev_let;


///录音回放部分start
#define REC_PLAYBACK_PHY_TASK_NAME    "rec_playback_task"

static bool toy_rec_playback_by_path(const char *father_name, const char *path, u8 dev)
{
    lg_dev_info *cur_lgdev_info = NULL;
    MUSIC_PLAYER_OBJ *rec_playback_api = music_player_creat();
    script_rec_printf("path = %s\n", path);
    if (NULL == rec_playback_api) {
        script_rec_puts("the music player create err!!!\n");
        return false;
    }
    __music_player_set_dec_father_name(rec_playback_api, (void *)father_name);
    __music_player_set_decoder_type(rec_playback_api, DEC_PHY_MP3 | DEC_PHY_WAV);
    __music_player_set_file_ext(rec_playback_api, "MP3WAV");
    __music_player_set_dev_sel_mode(rec_playback_api, DEV_SEL_SPEC);
    __music_player_set_path(rec_playback_api, path);
    __music_player_set_file_sel_mode(rec_playback_api, PLAY_FILE_BYPATH);
    __music_player_set_play_mode(rec_playback_api, REPEAT_ALL);
    __music_player_set_dev_let(rec_playback_api, dev);
    /* __music_player_set_save_breakpoint_vm_base_index(mapi, VM_DEV0_BREAKPOINT_REC, VM_DEV0_FLACBREAKPOINT_REC);  */

#if (SOUND_MIX_GLOBAL_EN)
    if (true == __music_player_set_output_to_mix(rec_playback_api, sound_mix_api_get_obj(), REC_PLAY_MIX_KEY, REC_PLAY_MIX_BUF_LEN, REC_PLAY_MIX_OUTPUT_DEC_LIMIT, DAC_DIGIT_TRACK_DEFAULT_VOL, DAC_DIGIT_TRACK_MAX_VOL)) {
        script_rec_puts("script_music to mix!!\n");
    }
#endif//SOUND_MIX_GLOBAL_EN

    if (music_player_process(rec_playback_api, REC_PLAYBACK_PHY_TASK_NAME, TaskMusicPhyDecPrio) == false) {
        script_rec_puts("rec playback false !!!!!\n");
        music_player_destroy(&rec_playback_api);
        return false;
    }

    u32 err = music_player_play(rec_playback_api);
    if (err != SUCC_MUSIC_START_DEC) {
        script_rec_printf("rec playback fail 1 err = %x !!\n", err);
        music_player_destroy(&rec_playback_api);
        return false;
    }


    script_rec_puts("rec play back ok  !!\n");


    u8 exit = 0;
    int msg[6] = {0};
    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
//		 os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        os_taskq_pend_no_clean(0, ARRAY_SIZE(msg), msg);//只查询消息不清理

        if (msg[0] != MSG_HALF_SECOND) {
            script_rec_printf("rec_pb  msg=0x%x \n", msg[0]);
        }
        clear_wdt();
        switch (msg[0]) {
        //这些消息只响应不要清,退出后由外边继续执行
        case SYS_EVENT_DEL_TASK:
        case MSG_REC_START:
        case MSG_REC_PLAYBACK:
            // music_player_destroy(&rec_playback_api);
            //if(os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ)
            //{
            //    script_rec_puts("rec play back task del \r");
            //    os_task_del_res_name(OS_TASK_SELF);
            //}
            exit = 1;
            break;

        //其他消息要清掉
        case SYS_EVENT_DEV_OFFLINE://设备掉线
            cur_lgdev_info = __music_player_get_lg_dev_info(rec_playback_api);
            if ((cur_lgdev_info != NULL) && (cur_lgdev_info->lg_hdl->phydev_item == (void *)msg[1])) {
                script_rec_puts("rec_rpb_DEV_OFLINE_other_dev~\n");
                //其他设备被拔出不管,清消息而不退出播放
                os_taskq_msg_clean(msg[0], 1);
                break;
            }
            script_rec_puts("rec_rpb_DEV_OFLINE_CUR_rec_DEV!!!\n");
        case MSG_REC_STOP://当播放录音时rec_stop用来停止播放录音
        case SYS_EVENT_DEC_FR_END:
        case SYS_EVENT_DEC_FF_END:
        case SYS_EVENT_DEC_END:
        case SYS_EVENT_DEC_DEVICE_ERR:
            music_player_close(rec_playback_api);
            os_taskq_msg_clean(msg[0], 1);
            exit = 1;
            break;
        default:
            os_taskq_msg_clean(msg[0], 1);
            break;
        }
        if (exit) {
            break;
        }
    }
    music_player_destroy(&rec_playback_api);
    return true;
}

//通过查询文件夹的方式确定上次录音  查询文件夹里有多少这种格式的文件
u16 get_last_rec_filenum(char *recpath, char *format, u8 dev)
{
    u32 rec_file_num = 0;
    u32 rec_total_number = 0;
    FILE_OPERATE *rec_dir_api = file_opr_api_create(dev);
    if (NULL == rec_dir_api) {
        script_rec_puts("can not create the file_api\n");
        return 0;
    }

    //fs_get_fileinfo(rec_dir_api->cur_lgdev_info->lg_hdl->fs_hdl, recpath, "MP3", &rec_file_num, &rec_total_number);
    fs_get_fileinfo(rec_dir_api->cur_lgdev_info->lg_hdl->fs_hdl, recpath, format, &rec_file_num, &rec_total_number);

    script_rec_printf("recfilenum= %d,totalnum=%d\n", rec_file_num, rec_total_number);

    file_opr_api_close(rec_dir_api);
    rec_dir_api = NULL;

    return rec_file_num;

}
///录音回放部分end


static tbool script_rec_notice_play_msg_callback(int msg[], void *priv)
{
    if (msg[0] != MSG_HALF_SECOND) {
        script_rec_printf("rec notice play msg = %d\n", msg[0]);
    }
    return false;
}


static u32 script_rec_skip_check(void)
{
#ifdef ENCODE_FLASH_API
    return 1;
#else
    return file_operate_get_total_phydev();
#endif
}

static void *script_rec_init(void *priv)
{
    toy_rec_parameter_set(ENC_MIC_CHANNEL, 50, 300); //设置录音通道、格式、开头结尾要去掉的数据
    script_rec_printf("script rec init ok\n");
    dac_channel_on(DAC_DIGITAL_AVOL, FADE_ON);
    return NULL;
}

static void script_rec_exit(void **hdl)
{
    /* hdl = hdl; */
    script_rec_printf("script rec exit ok\n");

    dac_channel_off(DAC_DIGITAL_AVOL, FADE_ON);
}

static void script_rec_task(void *parg)
{
    int msg[6] = {0};

    char rec_file_name_temp[sizeof(rec_file_name)];
    u16 rec_file_num_temp = 0;
    RECORD_OP_API *toy_rec_mic_api = NULL;

    NOTICE_PlAYER_ERR n_err;
    malloc_stats();
    n_err = notice_player_play_by_path_to_mix(SCRIPT_TASK_NAME, BPF_REC_MP3, script_rec_notice_play_msg_callback, 0, sound_mix_api_get_obj());
    if (n_err != NOTICE_PLAYER_NO_ERR && n_err != NOTICE_PLAYER_NO_ERR_WITHOUT_MSG_DEAL) {
        script_rec_printf("rec notice play err!!\n");
    }
    script_rec_printf("rec notice play ok\n");

    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();
        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                toy_rec_destroy(&toy_rec_mic_api);
                script_rec_printf("script_rec_del \r");
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;

        case MSG_REC_START:
            if (toy_rec_mic_api != NULL) {
                script_rec_puts("rec hdl != NULL\n");
                break;
            }
            script_rec_puts("rec_MSG_REC_START\n");
            toy_rec_mic_api = toy_rec_hdl_create_and_start();
            if (toy_rec_mic_api == NULL) {
                script_rec_puts("rec_cannot_create_hdl_rec_err!!!!\n");
            } else {
                script_rec_puts("rec_start_ok\n");
            }
            break;
        case MSG_REC_PP:
            script_rec_puts("rec_MSG_REC_PP\n");
            toy_rec_pp(&toy_rec_mic_api);
            break;
        case MSG_REC_STOP:
            script_rec_puts("rec_MSG_REC_STOP\n");
            toy_rec_destroy(&toy_rec_mic_api);
            break;
        case MSG_HALF_SECOND:
            // script_rec_puts("rec_MSG_HALF_SECOND\n");
            updata_enc_time(toy_rec_mic_api);//更新录音时间
            break;
        case MSG_ENCODE_ERR:
            script_rec_puts("rec_MSG_ENCODE_ERR\n");
            toy_rec_destroy(&toy_rec_mic_api);
            break;

        case MSG_REC_PLAYBACK:
            script_rec_puts("msg rec playback \n");
#ifdef ENCODE_FLASH_API
            if (file_operate_get_total_phydev() == 0) {
                script_rec_puts("playback the flash recfile\n");
                toy_rec_file_play();
                break;
            }
#endif
            //	rec_file_num_temp = get_last_rec_filenum("/JL_REC",new_lg_dev_let);
            toy_rec_destroy(&toy_rec_mic_api);//如果在录音就停止录音
            rec_file_num_temp = get_last_rec_filenum_from_VM();
            memcpy(rec_file_name_temp, rec_file_name, sizeof(rec_file_name));
            set_rec_name(rec_file_name_temp, rec_file_num_temp);
            toy_rec_playback_by_path(SCRIPT_TASK_NAME, rec_file_name_temp, new_lg_dev_let);//播放最后进入设备的录音
            break;

        case SYS_EVENT_DEV_OFFLINE:
            script_rec_puts("rec_SYS_EVENT_DEV_OFFLINE\n");
            if (toy_rec_mic_api) {
                if (toy_rec_mic_api->fop_api->cur_lgdev_info->lg_hdl->phydev_item == (void *)msg[1]) {
                    script_rec_puts("rec_DEV_OFLINE_CUR_rec_DEV!!!\n");
                    toy_rec_destroy(&toy_rec_mic_api);
#ifndef ENCODE_FLASH_API
                    if (file_operate_get_total_phydev() == 0) {
                        os_taskq_post(MAIN_TASK_NAME, 1, MSG_CHANGE_WORKMODE);
                    }
#endif
                } else {
                    script_rec_puts("rec_DEV_OFLINE_other_dev~\n");
                }
            }
            break;
        default:
            break;
        }
    }
}


const SCRIPT script_rec_info = {
    .skip_check = script_rec_skip_check,
    .init = script_rec_init,
    .exit = script_rec_exit,
    .task = script_rec_task,
    .key_register = &script_rec_key,
};
#endif
