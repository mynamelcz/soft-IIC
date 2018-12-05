#include "script_sound_mix.h"
#include "common/app_cfg.h"
#include "common/msg.h"
#include "script_sound_mix_key.h"
#include "notice_player.h"
#include "dac/dac_api.h"
#include "cbuf/circular_buf.h"
#include "sound_mix.h"
#include "clock.h"
#include "midi_player.h"
#define SCRIPT_SOUND_MIX_DEBUG_ENABLE
#ifdef SCRIPT_SOUND_MIX_DEBUG_ENABLE
#define script_sound_mix_printf	printf
#else
#define script_sound_mix_printf(...)
#endif


#define MSG_DEMO_SOUND_MIX_CH0_VOL_UP		MSG_DEMO_TEST5
#define MSG_DEMO_SOUND_MIX_CH0_VOL_DOWN		MSG_DEMO_TEST6

#define MSG_DEMO_SOUND_MIX_CH0_PLAY			MSG_DEMO_TEST0
#define MSG_DEMO_SOUND_MIX_CH1_PLAY			MSG_DEMO_TEST1

#define MSG_DEMO_SOUND_MIX_CH0_NEXT			MSG_DEMO_TEST2
#define MSG_DEMO_SOUND_MIX_CH1_NEXT			MSG_DEMO_TEST3

#define MSG_DEMO_SOUND_MIX_TEST_NOTICE		MSG_DEMO_TEST4
#define MSG_DEMO_SOUND_MIX_TEST_NOTICE_ADD	MSG_DEMO_TEST7


#define MSG_DEMO_SOUND_MIX_TEST_MIDI		MSG_DEMO_TEST8
#define MSG_DEMO_SOUND_MIX_TEST_MIDI_ADD	MSG_DEMO_TEST9

#define MSG_DEMO_SOUND_MIX_TEST_RESET		MSG_DEMO_TEST10
/* #define MSG_DEMO_WAV_MIX_CH3_PLAY			MSG_DEMO_TEST3 */
/* #define MSG_DEMO_WAV_MIX_CH4_PLAY			MSG_DEMO_TEST4 */


#define SOUND_MIX_CHL_MAX		2//ram资源有限， 目前只能开启最多两个通道
#define SCRIPT_TASK_NEW_PRIO	3

enum {
    SOUND_MIX_TEST_MODE_MUSIC = 0X0,
    SOUND_MIX_TEST_MODE_MUSIC_NOTICE,
    SOUND_MIX_TEST_MODE_MIDI_NITICE,
};

typedef struct __MIX_TEST_PARM {
    const char *player_name;//解码线程名称
    const char *chl_name;//叠加通道名称
    MUSIC_PLAYER_OBJ *player_obj;//解码控制句柄
    u8 	player_task_prio;//解码线程的优先级
    u8	run_dec_threshold;//叠加输入控制， 在叠加输入缓存数据可写入空间为中空间百分之run_dec_threshold时， 启动解码，获取更多的数据填入该缓存
    u32 output_buf_size;//叠加输入缓存大小
} MIX_TEST_PARM; //该结构体为测试参考, 做叠加时不一定按照这个来


extern volatile u8 new_lg_dev_let;

static u32 sound_mix_midi_play_melody_trigger(void *priv, u8 key, u8 vel)
{
    script_sound_mix_printf("melody = %x, vel = %x\n", key, vel);
    return 0;
}

static tbool script_sound_mix_notice_play_msg_callback(int msg[], void *priv)
{
    if (msg[0] != MSG_HALF_SECOND) {
        script_sound_mix_printf("test notice play msg = %d\n", msg[0]);
    }
    ///this is an example
//	if(msg[0] == MSG_MUSIC_PREV_FILE)
//		return true;
    return false;
}

static NOTICE_PlAYER_ERR script_sound_mix_notice_play(void *parg, const char *path, SOUND_MIX_OBJ *mix_obj)
{
    /* NOTICE_PlAYER_ERR err =  notice_player_play_by_path_to_mix(SCRIPT_TASK_NAME, path, script_sound_mix_notice_play_msg_callback, parg, mix_obj); */
    NOTICE_PlAYER_ERR err =  wav_play_by_path_to_mix(SCRIPT_TASK_NAME, path, script_sound_mix_notice_play_msg_callback, parg, mix_obj);
    return err;
}



extern cbuffer_t audio_cb ;
extern u8 dac_read_en;
static tbool mix_wait_stop(void *priv)
{
    //printf("%4d %4d\r",audio_cb.data_len,DAC_BUF_LEN);

    if ((audio_cb.data_len < DAC_BUF_LEN) || (!dac_read_en)) {
        if (audio_cb.data_len > DAC_BUF_LEN) {
            printf("warning output disable when output buff has data\r");
        }
        return true;
    } else {
        return false;
    }
}

static void mix_clear(void *priv)
{
    //priv = priv;
    cbuf_clear(&audio_cb);
}


static void *mix_output(void *priv, void *buf, u32 len)
{
    /* script_test_printf("o"); */
    dac_write((u8 *)buf, len);
    return priv;
}

static u32 set_mix_sr(void *priv, dec_inf_t *inf, tbool wait)
{
    printf("inf->sr %d %d\n", inf->sr, __LINE__);
    dac_set_samplerate(inf->sr, wait);
    return 0;
}

static MUSIC_PLAYER_OBJ *test_mix_music_creat(SOUND_MIX_OBJ *obj, void *task_name, u8 prio)
{
    if (obj == NULL) {
        return NULL;
    }
    MUSIC_PLAYER_OBJ *music_obj = music_player_creat();
    MY_ASSERT(music_obj);
    __music_player_set_dec_father_name(music_obj, SCRIPT_TASK_NAME);
    __music_player_set_decoder_type(music_obj, DEC_PHY_MP3 | DEC_PHY_WAV);
    __music_player_set_file_ext(music_obj, "MP3WAV");
    __music_player_set_dev_sel_mode(music_obj, DEV_SEL_SPEC);
    __music_player_set_file_sel_mode(music_obj, PLAY_FIRST_FILE);
    __music_player_set_play_mode(music_obj, REPEAT_ALL);
    /* __music_player_set_dev_let(music_obj, 'B'); */
    __music_player_set_dev_let(music_obj, new_lg_dev_let);

    tbool ret = music_player_process(music_obj, task_name, prio);
    if (ret == false) {
        music_player_destroy(&music_obj);
        return NULL;
    }
    return music_obj;
}

static void *script_sound_mix_init(void *priv)
{
    dac_channel_on(MUSIC_CHANNEL, FADE_ON);
    script_sound_mix_printf("script sound_mix init ok\n");

    return (void *)NULL;
}

static void script_sound_mix_exit(void **hdl)
{
    script_sound_mix_printf("script sound_mix exit ok\n");

    dac_channel_off(MUSIC_CHANNEL, FADE_ON);
}

static void script_sound_mix_task(void *parg)
{
    int msg[6] = {0};
    tbool ret;
    u32 err;
    u8 sound_mix_test_mode = SOUND_MIX_TEST_MODE_MUSIC;
    NOTICE_PlAYER_ERR n_err;
    MIDI_PLAYER *midi = NULL;
    u8 sound_mix_cur_vol_ch0 = DAC_DIGIT_TRACK_DEFAULT_VOL;
    u8 i;
    MIX_TEST_PARM mix_item[SOUND_MIX_CHL_MAX];

//改变系统运行的一些参数
    OSTaskChangePrio(SCRIPT_TASK_NAME, SCRIPT_TASK_NEW_PRIO);
    set_sys_freq(MIX_SYS_Hz);///叠加时需要提高系统时钟


//set param
    memset((u8 *)mix_item, 0x0, sizeof(MIX_TEST_PARM)*SOUND_MIX_CHL_MAX);

    mix_item[0].player_name = "musictask";
    mix_item[0].chl_name = "mix";
    mix_item[0].player_obj = NULL;
    mix_item[0].player_task_prio = TaskMusicPrio;
    mix_item[0].run_dec_threshold = 50;
    mix_item[0].output_buf_size = 0x1200;

    mix_item[1].player_name = "musictask1";
    mix_item[1].chl_name = "mix1";
    mix_item[1].player_obj = NULL;
    mix_item[1].player_task_prio = TaskMusicPrio1;
    mix_item[1].run_dec_threshold = 50;
    mix_item[1].output_buf_size = 0x1200;

    SOUND_MIX_OBJ *mix_obj = sound_mix_creat();
    ASSERT(mix_obj);

    if (__sound_mix_set_with_task_enable(mix_obj) == false) {
        printf("set_with_task_enable fail !!\n");
        ASSERT(0);
    }

    __sound_mix_set_output_cbk(mix_obj, (void *)mix_output, NULL);
    __sound_mix_set_data_op_cbk(mix_obj, mix_clear, mix_wait_stop, NULL);
    __sound_mix_set_set_info_cbk(mix_obj, set_mix_sr, NULL);
    __sound_mix_set_samplerate(mix_obj, 48000);


    ret = sound_mix_process(mix_obj, "mix_task", TaskMixPrio, 2 * 1024);
    if (ret == false) {
        while (1) {
            script_sound_mix_printf("mix process fail %d\n", __LINE__);
            OSTimeDly(100);
        }
    }

    os_taskq_post(SCRIPT_TASK_NAME, 1, MSG_MUSIC_PLAY);

    script_sound_mix_printf("-----------------------------enter sound mix test -----------------------------\n");

    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();
        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                for (i = 0; i < SOUND_MIX_CHL_MAX; i++) {
                    music_player_destroy(&(mix_item[i].player_obj));
                }
                midi_player_destroy(&midi);
                notice_player_stop();
                sound_mix_destroy(&mix_obj);
                set_sys_freq(SYS_Hz);
                script_sound_mix_printf("script_test_del \r");
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;
        case MSG_DEMO_SOUND_MIX_TEST_RESET:
            for (i = 0; i < SOUND_MIX_CHL_MAX; i++) {
                music_player_destroy(&(mix_item[i].player_obj));
            }
            midi_player_destroy(&midi);
        case MSG_MUSIC_PLAY:
            for (i = 0; i < SOUND_MIX_CHL_MAX; i++) {
                mix_item[i].player_obj = test_mix_music_creat(mix_obj, (void *)mix_item[i].player_name, mix_item[i].player_task_prio);
                if (mix_item[i].player_obj) {
                    __music_player_set_output_to_mix(mix_item[i].player_obj, mix_obj, mix_item[i].chl_name, mix_item[i].output_buf_size, mix_item[i].run_dec_threshold, DAC_DIGIT_TRACK_DEFAULT_VOL, DAC_DIGIT_TRACK_MAX_VOL);
                    /* __music_player_set_file_index(mix_item[i].player_obj, 1); */
                    err = music_player_play(mix_item[i].player_obj);
                    if (err != SUCC_MUSIC_START_DEC) {
                        script_sound_mix_printf("music %d play fail %x\n", i, err);
                        continue;
                    }
                }
            }

            sound_mix_test_mode = SOUND_MIX_TEST_MODE_MUSIC;
            break;

        case SYS_EVENT_DEC_FR_END:
        case SYS_EVENT_DEC_FF_END:
        case SYS_EVENT_DEC_END:
            for (i = 0; i < SOUND_MIX_CHL_MAX; i++) {
                if (0 == strcmp(mix_item[i].player_name, (const char *)msg[1])) {
                    __music_player_set_file_sel_mode(mix_item[i].player_obj, PLAY_AUTO_NEXT);
                    __music_player_set_dev_sel_mode(mix_item[i].player_obj, DEV_SEL_CUR);
                    err = music_player_play(mix_item[i].player_obj);
                    if (err != SUCC_MUSIC_START_DEC) {
                        script_sound_mix_printf("music %d play auto next fail %x\n", i, err);
                        break;
                    }
                }
            }
            break;

        case MSG_DEMO_SOUND_MIX_CH0_VOL_UP:
            sound_mix_cur_vol_ch0++;
            if (sound_mix_cur_vol_ch0 >= DAC_DIGIT_TRACK_MAX_VOL) {
                sound_mix_cur_vol_ch0 = DAC_DIGIT_TRACK_MAX_VOL;
            }
            script_sound_mix_printf("up %d\n", sound_mix_cur_vol_ch0);
            __music_player_set_output_to_mix_set_vol(mix_item[0].player_obj, sound_mix_cur_vol_ch0);
            break;
        case MSG_DEMO_SOUND_MIX_CH0_VOL_DOWN:
            if (sound_mix_cur_vol_ch0) {
                sound_mix_cur_vol_ch0--;
            }
            script_sound_mix_printf("down %d\n", sound_mix_cur_vol_ch0);
            __music_player_set_output_to_mix_set_vol(mix_item[0].player_obj, sound_mix_cur_vol_ch0);
            break;

        case MSG_DEMO_SOUND_MIX_CH0_PLAY:
            music_player_pp(mix_item[0].player_obj);
            break;
        case MSG_DEMO_SOUND_MIX_CH1_PLAY:
            music_player_pp(mix_item[1].player_obj);
            break;

        case MSG_DEMO_SOUND_MIX_CH0_NEXT:
            __music_player_set_file_sel_mode(mix_item[0].player_obj, PLAY_NEXT_FILE);
            __music_player_set_dev_sel_mode(mix_item[0].player_obj, DEV_SEL_CUR);
            err = music_player_play(mix_item[0].player_obj);
            if (err != SUCC_MUSIC_START_DEC) {
                script_sound_mix_printf("music 1 play next fail %x\n", err);
                break;
            }
            break;
        case MSG_DEMO_SOUND_MIX_CH1_NEXT:
            __music_player_set_file_sel_mode(mix_item[1].player_obj, PLAY_NEXT_FILE);
            __music_player_set_dev_sel_mode(mix_item[1].player_obj, DEV_SEL_CUR);
            err = music_player_play(mix_item[1].player_obj);
            if (err != SUCC_MUSIC_START_DEC) {
                script_sound_mix_printf("music 1 play next fail %x\n", err);
                break;
            }
            break;

        case SYS_EVENT_PLAY_SEL_END:
            notice_player_stop();
            __music_player_set_output_to_mix_set_vol(mix_item[0].player_obj, sound_mix_cur_vol_ch0);
            __midi_player_set_output_to_mix_set_vol(midi, DAC_DIGIT_TRACK_DEFAULT_VOL);
            break;

        case MSG_DEMO_SOUND_MIX_TEST_NOTICE:
            for (i = 0; i < SOUND_MIX_CHL_MAX; i++) {
                music_player_destroy(&(mix_item[i].player_obj));
            }
            midi_player_destroy(&midi);
            script_sound_mix_printf("-----------------------------enter sound mix test notice add-----------------------------\n");

            mix_item[0].player_obj = test_mix_music_creat(mix_obj, (void *)mix_item[0].player_name, mix_item[0].player_task_prio);
            if (mix_item[0].player_obj) {
                printf("000\n");
                __music_player_set_output_to_mix(mix_item[0].player_obj, mix_obj, mix_item[0].chl_name, mix_item[0].output_buf_size, mix_item[0].run_dec_threshold, DAC_DIGIT_TRACK_DEFAULT_VOL, DAC_DIGIT_TRACK_MAX_VOL);
                err = music_player_play(mix_item[0].player_obj);
                if (err != SUCC_MUSIC_START_DEC) {
                    script_sound_mix_printf("music %d play fail %x, %d\n", i, err, __LINE__);
                    break;
                }
            }
            sound_mix_test_mode = SOUND_MIX_TEST_MODE_MUSIC_NOTICE;
            printf("001\n");
            break;

        case MSG_DEMO_SOUND_MIX_TEST_NOTICE_ADD:
            if (sound_mix_test_mode == SOUND_MIX_TEST_MODE_MUSIC) {
                break;
            }

            __music_player_set_output_to_mix_set_vol(mix_item[0].player_obj, ((sound_mix_cur_vol_ch0 > 5) ? (sound_mix_cur_vol_ch0 - 5) : sound_mix_cur_vol_ch0));

            __midi_player_set_output_to_mix_set_vol(midi, DAC_DIGIT_TRACK_DEFAULT_VOL - 5);
            n_err =  wav_play_by_path_to_mix(SCRIPT_TASK_NAME, "/wav/mbg0.wav", NULL, NULL, mix_obj);
            /* n_err =  notice_player_play_by_path_to_mix(SCRIPT_TASK_NAME, "/wav/mbg0.wav", NULL, NULL, mix_obj); */
            /* notice_player_play_by_path_to_mix */
            /* n_err = script_sound_mix_notice_play(NULL, "/wav/mbg0.wav", mix_obj); */
            if (n_err != NOTICE_PLAYER_NO_ERR && n_err != NOTICE_PLAYER_NO_ERR_WITHOUT_MSG_DEAL) {
                script_sound_mix_printf("test notice play err!!\n");
            }

            break;

        case MSG_MODULE_MIDI_DEC_ERR:
        case MSG_MODULE_MIDI_DEC_END:
            __midi_player_set_dev_mode(midi, DEV_SEL_CUR);
            __midi_player_set_sel_mode(midi, PLAY_AUTO_NEXT);
            ret = midi_player_play(midi);
            if (ret == false) {
                midi_player_destroy(&midi);
                script_sound_mix_printf("midi play fail  %d\n",  __LINE__);
                break;
            }
            break;
        case MSG_DEMO_SOUND_MIX_TEST_MIDI:
            for (i = 0; i < SOUND_MIX_CHL_MAX; i++) {
                music_player_destroy(&(mix_item[i].player_obj));
            }
            midi_player_destroy(&midi);

            midi = midi_player_creat();
            MY_ASSERT(midi);

            __midi_player_set_father_name(midi, SCRIPT_TASK_NAME);
            __midi_player_set_mode(midi, CMD_MIDI_CTRL_MODE_2);
            __midi_player_set_switch_enable(midi, MELODY_PLAY_ENABLE);

            __midi_player_set_melody_trigger(midi, sound_mix_midi_play_melody_trigger, NULL);
            __midi_player_set_switch_enable(midi, MELODY_ENABLE);
            __midi_player_set_dev_let(midi, 'A');
            __midi_player_set_path(midi, "/midi/songa01.mid");
            __midi_player_set_dev_mode(midi, DEV_SEL_SPEC);
            __midi_player_set_sel_mode(midi, PLAY_FILE_BYPATH);

            ret = midi_player_process(midi, "MidiMixTest", TaskMidiAudioPrio);
            if (ret == false) {
                script_sound_mix_printf("midi process fail %d\n", __LINE__);
                while (1);
            }

            ret = __midi_player_set_output_to_mix(midi, mix_obj, "midi_mix", 0x1000, 50, DAC_DIGIT_TRACK_DEFAULT_VOL, DAC_DIGIT_TRACK_MAX_VOL);
            if (ret == false) {
                script_sound_mix_printf("midi mix fail %d\n", __LINE__);
                while (1);
            }


            ret = midi_player_play(midi);
            if (ret == false) {
                midi_player_destroy(&midi);
                script_sound_mix_printf("midi play fail  %d\n",  __LINE__);
                break;
            }

            sound_mix_test_mode = SOUND_MIX_TEST_MODE_MIDI_NITICE;
            break;

        /* case MSG_DEMO_SOUND_MIX_TEST_MIDI_ADD: */
        /* break; */



        case MSG_HALF_SECOND:
            if (sound_mix_test_mode == SOUND_MIX_TEST_MODE_MIDI_NITICE) {
                break;
            }
            script_sound_mix_printf("dec_time = %d\n", __music_player_get_play_time(mix_item[0].player_obj));
            if (sound_mix_test_mode == SOUND_MIX_TEST_MODE_MUSIC) {
                script_sound_mix_printf("-------------------------------dec_time = %d\n", __music_player_get_play_time(mix_item[1].player_obj));
            }
            break;
        default:
            break;
        }
    }
}

const SCRIPT script_sound_mix_info = {
    .skip_check = NULL,
    .init = script_sound_mix_init,
    .exit = script_sound_mix_exit,
    .task = script_sound_mix_task,
    .key_register = &script_sound_mix_key,
};


