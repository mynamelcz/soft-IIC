#include "script_midi.h"
#include "common/msg.h"
#include "common/app_cfg.h"
#include "script_midi_key.h"
#include "notice/notice_player.h"
#include "dac/dac_api.h"
#include "midi_player.h"
#include "fat/fs_io.h"
#include "sound_mix_api.h"
#include "sdk_cfg.h"

#define SCRIPT_MIDI_DEBUG_ENABLE
#ifdef SCRIPT_MIDI_DEBUG_ENABLE
#define script_midi_printf	printf
#else
#define script_midi_printf(...)
#endif


#define MIDI_DEC_TASK_NAME      		"MidiAudioTask"

#if (SOUND_MIX_GLOBAL_EN)
#define MIDI_MIX_CHL_NAME				"MIDI_MIX"
#define MIDI_MIX_BUF_LEN				(4*1024L)
#define MIDI_MIX_OUTPUT_DEC_LIMIT		(50)
#endif//SOUND_MIX_GLOBAL_EN

/* #define MIDI_ENABLE_ONE_KEY_ONE_NOTE */
/* #define MIDI_ENABLE_USE_USER_FILE_IO		//用户自定义文件操作接口， 用户可以在读接口进行解密相关操作 */

/* const char *midi_song_list[] = { */
/* "/midi/songa01.mid", */
/* "/midi/songa02.mid", */
/* }; */

#ifdef MIDI_ENABLE_USE_USER_FILE_IO
static bool midi_user_file_seek(void *p_f_hdl, u8 type, u32 offsize)
{
    script_midi_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
    return fs_seek(p_f_hdl, type, offsize);
}

static u16 midi_user_file_read(void *p_f_hdl, u8 *buff, u16 len)
{
    script_midi_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
    return fs_read(p_f_hdl, buff, len);
}

static u32 midi_user_file_get_size(void *p_f_hdl)
{
    script_midi_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
    return fs_length(p_f_hdl);
}

static const MIDI_DECCODE_FILE_IO midi_user_file_io = {
    .seek = (void *)midi_user_file_seek,
    .read = (void *)midi_user_file_read,
    .get_size = (void *)midi_user_file_get_size,
};
#endif

static tbool script_midi_notice_play_msg_callback(int msg[], void *priv)
{
    if (msg[0] != MSG_HALF_SECOND) {
        script_midi_printf("midi notice play msg = %d\n", msg[0]);
    }
    ///this is an example
//	if(msg[0] == MSG_MUSIC_PREV_FILE)
//		return true;
    return false;
}


static u32 midi_play_melody_trigger(void *priv, u8 key, u8 vel)
{
    if (key == 0xff) {
        script_midi_printf(" end -------melody = %x, vel = %x\n", key, vel);
        return 0;
    }
    script_midi_printf(" start -------melody = %x, vel = %x\n", key, vel);
    return 0;
}

static void *script_midi_init(void *priv)
{
    tbool ret;
    MIDI_PLAYER *midi = midi_player_creat();
    MY_ASSERT(midi);

    __midi_player_set_father_name(midi, SCRIPT_TASK_NAME);
#ifdef MIDI_ENABLE_ONE_KEY_ONE_NOTE//midi one key one note配置
    __midi_player_set_mode(midi, CMD_MIDI_CTRL_MODE_1);
#else//midi播放配置
    __midi_player_set_mode(midi, CMD_MIDI_CTRL_MODE_2);
#endif

#ifdef MIDI_ENABLE_USE_USER_FILE_IO
    __midi_player_set_file_io(midi, (MIDI_DECCODE_FILE_IO *)&midi_user_file_io);
#endif
    __midi_player_set_melody_trigger(midi, midi_play_melody_trigger, NULL);

    __midi_player_set_switch_enable(midi, MELODY_ENABLE);
#if MIDI_UPDATE_EN
    ///如果方案使用midi通过app更新功能， 需要固定一种完整的音色，例如大钢琴，在所有midi播放的时候，只能以大钢琴该音色播放
    __midi_player_set_main_chl_prog(midi, 0/*具体选择何种音色(乐器)， 用户根据实际设定*/, 0);
    __midi_player_set_switch_enable(midi, SET_PROG_ENABLE);
#endif
    __midi_player_set_dev_let(midi, 'A');
    __midi_player_set_filenum(midi, 1);
    __midi_player_set_path(midi, "/midi/");
    __midi_player_set_dev_mode(midi, DEV_SEL_SPEC);
    __midi_player_set_play_mode(midi, REPEAT_FOLDER);
    __midi_player_set_sel_mode(midi, PLAY_FILE_BYPATH);

    __midi_player_set_melody_decay(midi, 1024); //具体看函数说明
    __midi_player_set_melody_delay(midi, 1); //具体看函数说明
    __midi_player_set_melody_mute_threshold(midi, (u32)(1 << 26));
    __midi_player_set_tempo_directly(midi, 1042);

#if (SOUND_MIX_GLOBAL_EN)
    ret = __midi_player_set_output_to_mix(midi, sound_mix_api_get_obj(), MIDI_MIX_CHL_NAME, MIDI_MIX_BUF_LEN, MIDI_MIX_OUTPUT_DEC_LIMIT, DAC_DIGIT_TRACK_DEFAULT_VOL, DAC_DIGIT_TRACK_MAX_VOL);
    if (ret == false) {
        printf("midi mix fail %d\n", __LINE__);
    } else {
        script_midi_printf("midi mix ok!!\n");
    }
#endif//SOUND_MIX_GLOBAL_EN

    dac_channel_on(MUSIC_CHANNEL, FADE_ON);
    script_midi_printf("script midi init ok\n");
    return (void *)midi;
}

static void script_midi_exit(void **hdl)
{
    midi_player_destroy((MIDI_PLAYER **)hdl);
    dac_channel_off(MUSIC_CHANNEL, FADE_ON);
    script_midi_printf("script midi exit ok\n");
}

static void script_midi_task(void *parg)
{
    int msg[6] = {0};
    tbool ret;
    MIDI_ST status = MIDI_ST_STOP;
    NOTICE_PlAYER_ERR n_err;
    malloc_stats();
    n_err = notice_player_play_by_path_to_mix(SCRIPT_TASK_NAME, BPF_MIDI_MP3, script_midi_notice_play_msg_callback, NULL, (sound_mix_api_get_obj()));
    if (n_err != NOTICE_PLAYER_NO_ERR && n_err != NOTICE_PLAYER_NO_ERR_WITHOUT_MSG_DEAL) {
        script_midi_printf("midi notice play err!!\n");
    }
    script_midi_printf("midi notice play ok\n");

    MIDI_PLAYER *midi = (MIDI_PLAYER *)parg;
    ret = midi_player_process(midi, MIDI_DEC_TASK_NAME, TaskMidiAudioPrio);
    if (ret == true) {
        os_taskq_post(SCRIPT_TASK_NAME, 1, MSG_MUSIC_PLAY);
    }

    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();
        if (msg[0] != MSG_HALF_SECOND) {
            script_midi_printf("msg = %x, %d, %s\n", msg[0], __LINE__, __func__);
        }
        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                script_midi_printf("script_midi_del \r");
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;

        case SYS_EVENT_DEC_DEVICE_ERR:
            break;
        case MSG_MODULE_MIDI_DEC_ERR:
        case MSG_MODULE_MIDI_DEC_END:
            __midi_player_set_dev_mode(midi, DEV_SEL_CUR);
            __midi_player_set_sel_mode(midi, PLAY_AUTO_NEXT);
        case MSG_MUSIC_PLAY:
            /* __midi_player_set_main_chl_prog(midi, 3, 1); */
            ret = midi_player_play(midi);
            /* __midi_player_set_main_chl_prog(midi, 3, 1); */
            break;
        case MSG_MUSIC_PP:
            status = midi_player_pp(midi);
            if (status == MIDI_ST_PLAY) {
                script_midi_printf("midi play>>");
            } else {
                script_midi_printf("midi pause>>");
            }
            break;
        case MSG_MUSIC_PREV_FILE:
            __midi_player_set_dev_mode(midi, DEV_SEL_CUR);
            __midi_player_set_sel_mode(midi, PLAY_PREV_FILE);
            ret = midi_player_play(midi);
            break;
        case MSG_MUSIC_NEXT_FILE:
            __midi_player_set_dev_mode(midi, DEV_SEL_CUR);
            __midi_player_set_sel_mode(midi, PLAY_NEXT_FILE);
            ret = midi_player_play(midi);
            break;
        case MSG_MIDI_OKON:
#ifdef MIDI_ENABLE_ONE_KEY_ONE_NOTE//midi one key one note配置
            __midi_player_set_go_on(midi);
#endif
            break;
        default:
            break;
        }
        if (ret == false) {
#if MIDI_UPDATE_EN
            //midi播放失败， 有可能是midi文件更新不成功，开发者可以在这里添加提示音， 提示使用者， 需要通过app下载midi文件
            script_midi_printf("midi play err , play notice , please download midi file by jlmusicplant app!!!\n");
#else
            script_midi_printf("midi play err !!!\n");
#endif
        }
    }
}

const SCRIPT script_midi_info = {
    .skip_check = NULL,
    .init = script_midi_init,
    .exit = script_midi_exit,
    .task = script_midi_task,
    .key_register = &script_midi_key,
};

