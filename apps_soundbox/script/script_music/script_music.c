#include "script_music.h"
#include "script_music_key.h"
#include "common/msg.h"
#include "music/music_decrypt.h"
#include "music/music_player.h"
#include "music/music_player_api.h"
#include "notice/notice_player.h"
#include "dac/dac_api.h"
#include "file_operate/file_bs_deal.h"
#include "dac/eq_api.h"
#include "dac/eq.h"
#include "common/app_cfg.h"
#include "file_operate/file_op.h"
#include "fat/fs_io.h"
#include "fat/tff.h"
#include "dec/decoder_phy.h"
#include "clock_interface.h"
#include "clock.h"
#include "vm/vm_api.h"
#include "rcsp/rcsp_interface.h"
#include "sound_mix_api.h"
#include "music_id3.h"
#define SCRIPT_MUSIC_DEBUG_ENABLE
#ifdef SCRIPT_MUSIC_DEBUG_ENABLE
#define script_music_printf	printf
#else
#define script_music_printf(...)
#endif
#ifdef MUSIC_IO_ENABLE
#include "mio/mio_ctrl.h"
#include "mio_api/mio_api.h"
#endif

extern u8 eq_mode;
extern const char dec_file_ext[][3];

static u8 cur_rpt_mode = REPEAT_ALL;




#define DEC_PHY_TASK_NAME	"DecPhyTaskName"

#if (SOUND_MIX_GLOBAL_EN)
#define MUSIC_MIX_KEY					"MUSIC_MIX"
#define MUSIC_MIX_BUF_LEN				(4*1024L)
#define MUSIC_MIX_OUTPUT_DEC_LIMIT		(50)
#endif//SOUND_MIX_GLOBAL_EN



#define MP3_ID3_V1_EN	0
#define MP3_ID3_V2_EN	0

#if (MP3_ID3_V1_EN)
static MP3_ID3_OBJ *id3_v1_obj = NULL;
#endif//MP3_ID3_V1_EN
#ifdef MP3_ID3_V2_EN
static MP3_ID3_OBJ *id3_v2_obj = NULL;
#endif//MP3_ID3_V2_EN


//#define MULFILE_DEBUG_ENABLE

#ifdef MULFILE_DEBUG_ENABLE

static u32 mulfile_test_read(void *hFile, void *buf, u32 size)
{
    /* printf("**read** "); */
    return fs_read(hFile, buf, size);
}

static u32 mulfile_test_write(void *hFile, void *buf, u32 size)
{
    return fs_write(hFile, buf, size);
}

static s32 mulfile_test_seek(void *hFile, u16 cmd, u32 offset)
{
    /* printf("test seek ~~~~~~~~~~~~%x~\n", offset); */
    return fs_seek(hFile, cmd, offset);
}

static u32 mulfile_test_tell(void *hFile)
{
    return fs_tell(hFile);
}

static u32 mulfile_test_get_length(void *hFile)
{
    return fs_length(hFile);
}

const mulfile_func_t mulfile_test_callback = {
    .m_FunRead = mulfile_test_read,
    .m_FunWrite = mulfile_test_write,
    .m_FunSeek = mulfile_test_seek,
    .m_FunTell = mulfile_test_tell,
    .m_Funlength = mulfile_test_get_length,
};

#endif



#if MUSIC_DECRYPT_EN
static u32 music_decrypt_read(void *hFile, void *buf, u32 size)
{
    /* printf("**read** "); */
    u32 r_len;
    u32 faddr;
    faddr = fs_tell(hFile);
    r_len = fs_read(hFile, buf, size);
    if ((r_len != 0) && (r_len != (u16) - 1)) { //attention this
        cryptanalysis_buff(buf, faddr, r_len);
    }

    return r_len;
}
static u32 music_decrypt_write(void *hFile, void *buf, u32 size)
{
    return fs_write(hFile, buf, size);
}
static s32 music_decrypt_seek(void *hFile, u16 cmd, u32 offset)
{
    /* printf("test seek ~~~~~~~~~~~~%x~\n", offset); */
    return fs_seek(hFile, cmd, offset);
}
static u32 music_decrypt_tell(void *hFile)
{
    return fs_tell(hFile);
}
static u32 music_decrypt_get_length(void *hFile)
{
    return fs_length(hFile);
}

const mulfile_func_t music_decrypt_callback = {
    .m_FunRead = music_decrypt_read,
    .m_FunWrite = music_decrypt_write,
    .m_FunSeek = music_decrypt_seek,
    .m_FunTell = music_decrypt_tell,
    .m_Funlength = music_decrypt_get_length,
};

#endif


#ifdef MUSIC_IO_ENABLE

static void music_mio_close(void)
{
    mio_ctrl_t *pmio = (mio_ctrl_t *)g_mio_var;
    if (g_mio_var) {
        g_mio_var = NULL;
        mio_api_close(&pmio);
    }
}

static u32 mio_file_read(void *hFile, void *buf, u32 size)
{
    /* printf("**read** "); */
    u32 ret;
    mio_ctrl_mutex_enter();
#if MUSIC_DECRYPT_EN
    u32 faddr;
    faddr = fs_tell(hFile);
    r_len = fs_read(hFile, buf, size);
    if ((r_len != 0) && (r_len != (u16) - 1)) { //attention this
        cryptanalysis_buff(buf, faddr, r_len);
    }
#else
    ret = fs_read(hFile, buf, size);
#endif
    mio_ctrl_mutex_exit();
    return ret;
}

static u32 mio_file_write(void *hFile, void *buf, u32 size)
{
    u32 ret;
    mio_ctrl_mutex_enter();
    ret = fs_write(hFile, buf, size);
    mio_ctrl_mutex_exit();
    return ret;
}

static s32 mio_file_seek(void *hFile, u16 cmd, u32 offset)
{
    /* printf("test seek ~~~~~~~~~~~~%x~\n", offset); */
    s32 ret;
    mio_ctrl_mutex_enter();
    ret = fs_seek(hFile, cmd, offset);
    mio_ctrl_mutex_exit();
    return ret;
}

static u32 mio_file_tell(void *hFile)
{
    u32 ret;
    mio_ctrl_mutex_enter();
    ret = fs_tell(hFile);
    mio_ctrl_mutex_exit();
    return ret;
}

static u32 mio_file_get_length(void *hFile)
{
    u32 ret;
    mio_ctrl_mutex_enter();
    ret = fs_length(hFile);
    mio_ctrl_mutex_exit();
    return ret;
}

const mulfile_func_t mio_file_callback = {
    .m_FunRead 		= mio_file_read,
    .m_FunWrite 	= mio_file_write,
    .m_FunSeek 		= mio_file_seek,
    .m_FunTell 		= mio_file_tell,
    .m_Funlength 	= mio_file_get_length,
};

#endif


static tbool script_music_play(MUSIC_PLAYER_OBJ *mapi, u8 auto_next)
{
    tbool ret = true;
    u32 err;

#ifndef MULFILE_DEBUG_ENABLE
#ifdef MUSIC_IO_ENABLE
    music_mio_close();
    err = music_player_mulfile_play(mapi, 0, (u8) - 1, (mulfile_func_t *)&mio_file_callback);
    script_music_printf("err = ~~%x, %d\n", err, __LINE__);
    ret = music_player_mulfile_play_err_deal(mapi, err, auto_next, 1, 0, (u8) - 1, (mulfile_func_t *)&mio_file_callback);
    if (ret == false) {
        script_music_printf("music play fail , change work mode %d\n", __LINE__);
        os_taskq_post(MAIN_TASK_NAME, 1, MSG_CHANGE_WORKMODE);
        ret = false;
    }
#elif (MUSIC_DECRYPT_EN == 1)
    err = music_player_mulfile_play(mapi, 0, (u8) - 1, (mulfile_func_t *)&music_decrypt_callback);
    script_music_printf("err = ~~%x, %d\n", err, __LINE__);
    ret = music_player_mulfile_play_err_deal(mapi, err, auto_next, 1, 0, (u8) - 1, (mulfile_func_t *)&music_decrypt_callback);
    if (ret == false) {
        script_music_printf("music play fail , change work mode %d\n", __LINE__);
        os_taskq_post(MAIN_TASK_NAME, 1, MSG_CHANGE_WORKMODE);
        ret = false;
    }
#else
    err = music_player_play(mapi);
    script_music_printf("err = ~~%x, %d\n", err, __LINE__);
    ret = music_player_err_deal(mapi, err, auto_next, 1);
    if (ret == false) {
        script_music_printf("music play fail , change work mode %d\n", __LINE__);
        os_taskq_post(MAIN_TASK_NAME, 1, MSG_CHANGE_WORKMODE);
    }
#endif
#else
    err = music_player_mulfile_play(mapi, 0, (u8) - 1, (mulfile_func_t *)&mulfile_test_callback);
    if (err != SUCC_MUSIC_START_DEC) {
        script_music_printf("music play fail , change work mode %d\n", __LINE__);
        os_taskq_post(MAIN_TASK_NAME, 1, MSG_CHANGE_WORKMODE);
        ret = false;
    }
#endif
    if (ret == false) {
        music_player_close(mapi);
    }
    return ret;
}



/*----------------------------------------------------------------------------*/
/**@brief script_music脚本提示音消息处理回调函数
   @param
   msg -- 播放过程中接收到的消息
   priv -- 播放前传递进来的私有控制句柄
   @return 是否退出提示音播放
   @note
*/
/*----------------------------------------------------------------------------*/
static tbool script_music_notice_play_msg_callback(int msg[], void *priv)
{
    if (msg[0] != MSG_HALF_SECOND) {
        script_music_printf("music notice play msg = %d\n", msg[0]);
    }
    ///this is an example
    if (msg[0] == MSG_MUSIC_PREV_FILE ||
        msg[0] == MSG_MUSIC_NEXT_FILE) {
        return true;
    }
    /* if(msg[0] == MSG_MUSIC_PP) */
    /* { */
    /* _DECODE_STATUS test_st; */
    /* test_st = notice_player_pp();	 */
    /* printf("---------------test_st = %x--------------------\n", test_st); */
    /* } */
    return false;
}


static NOTICE_PlAYER_ERR script_music_notice_play(const char *path)
{
    return notice_player_play_by_path_to_mix(SCRIPT_TASK_NAME, path, script_music_notice_play_msg_callback, 0, sound_mix_api_get_obj());
    ///指定设备播放提示demo
    /* return notice_player_play_spec_dev_by_path(SCRIPT_TASK_NAME, 'B', "/test0_x.mp3", script_music_notice_play_msg_callback, 0); */
    /* return  wav_play_by_path(SCRIPT_TASK_NAME, "/wav/mbg0.wav", script_music_notice_play_msg_callback, NULL); */
}



static void script_music_decode_before_callback(void *priv)
{
    MUSIC_PLAYER_OBJ *mapi = (MUSIC_PLAYER_OBJ *)priv;
    u32 dev_let = __music_player_get_dev_let(mapi);
    if (dev_let != 'A') {
#if LRC_LYRICS_EN
        if (0 == lrc_find(mapi)) {
            music_ui.lrc_flag = TRUE;
        } else {
            printf("lrc_init err\n");
        }
#endif
    }

#if MUSIC_DECRYPT_EN
    lg_dev_info *cur_lgdev_info = __music_player_get_lg_dev_info(mapi);
    get_decode_file_info(cur_lgdev_info->lg_hdl->file_hdl);
#endif


#if SUPPORT_APP_RCSP_EN
    rcsp_music_play_api_before(__music_player_get_dev_sel_mode(mapi), dev_let);
#endif

    vm_check_all(0);
}

static void script_music_decode_after_callback(void *priv)
{
    MUSIC_PLAYER_OBJ *mapi = (MUSIC_PLAYER_OBJ *)priv;
    if (mapi == NULL) {
        return;
    }
    char *dec_name = __music_player_get_dec_name(mapi);

    if (0 == strcmp((const char *)dec_name, "FLA")) { //解码器FLAC
        set_sys_freq(FLAC_SYS_Hz);
    } else if (0 == strcmp((const char *)dec_name, "APE")) { //解码器APE
        set_sys_freq(APE_SYS_Hz);
    } else {
        set_sys_freq(SYS_Hz);
    }


    if (0 == strcmp((const char *)dec_name, "MP3")) { //解码器FLAC
#if (MP3_ID3_V1_EN)
        id3_v1_obj = id3_v1_obj_get(mapi);
#endif//MP3_ID3_V1_EN
#if (MP3_ID3_V2_EN)
        id3_v2_obj = id3_v2_obj_get(mapi);
#endif//MP3_ID3_V2_EN
    }

#ifdef MUSIC_IO_ENABLE
    music_mio_close();
    g_mio_var = mio_api_start(mapi->dop_api->io->f_p, 0);
#endif

#if SUPPORT_APP_RCSP_EN
    rcsp_music_play_api_after(__music_player_get_dev_sel_mode(mapi), __music_player_get_dev_let(mapi), SUCC_MUSIC_START_DEC);
#endif
}

static void script_music_decode_stop_callback(void *priv)
{
    MUSIC_PLAYER_OBJ *mapi = (MUSIC_PLAYER_OBJ *)priv;

    if (mapi == NULL) {
        return;
    }

#if (MP3_ID3_V1_EN)
    id3_obj_post(&id3_v1_obj);
#endif//MP3_ID3_V1_EN

#if (MP3_ID3_V2_EN)
    id3_obj_post(&id3_v2_obj);
#endif//MP3_ID3_V2_EN

    printf("music decode stop ! func=%s, line = %d\n", __FUNCTION__, __LINE__);
}

static u32 script_music_skip_check(void)
{
    return file_operate_get_total_phydev();
}


//static void *music_player_output(void *priv, void *buf, u32 len)
//{
//    printf("o");
//    dac_write(buf, len);
//    return priv;
//}

static void *script_music_init(void *priv)
{
    MUSIC_PLAYER_OBJ *mapi = NULL;
    u32 dev_let = (priv ? (u32)priv : 'B');
    mapi = music_player_creat();
    /* MY_ASSERT(mapi); */
    if (mapi != NULL) {
        __music_player_set_dec_father_name(mapi, SCRIPT_TASK_NAME);
        /* __music_player_set_dec_phy_name(mapi, "DEC_PHY_TASK_NAME"); */
        __music_player_set_decoder_type(mapi, DEC_PHY_MP3 | DEC_PHY_WAV | DEC_PHY_WMA | DEC_PHY_FLAC | DEC_PHY_APE);
        __music_player_set_file_ext(mapi, (void *)dec_file_ext);
        __music_player_set_dev_sel_mode(mapi, DEV_SEL_SPEC);
        __music_player_set_file_sel_mode(mapi, PLAY_BREAK_POINT);
        __music_player_set_play_mode(mapi, REPEAT_ALL);
        __music_player_set_dev_let(mapi, dev_let);
        __music_player_set_decode_before_callback(mapi, script_music_decode_before_callback, mapi);
        __music_player_set_decode_after_callback(mapi, script_music_decode_after_callback, mapi);
        __music_player_set_decode_stop_callback(mapi, script_music_decode_stop_callback, mapi);
        __music_player_set_save_breakpoint_vm_base_index(mapi, VM_DEV0_BREAKPOINT, VM_DEV0_FLACBREAKPOINT);


#if (SOUND_MIX_GLOBAL_EN)
        if (true == __music_player_set_output_to_mix(mapi, sound_mix_api_get_obj(), MUSIC_MIX_KEY, MUSIC_MIX_BUF_LEN, MUSIC_MIX_OUTPUT_DEC_LIMIT, DAC_DIGIT_TRACK_DEFAULT_VOL, DAC_DIGIT_TRACK_MAX_VOL)) {
            script_music_printf("script_music to mix!!\n");
        }
#endif//SOUND_MIX_GLOBAL_EN

#if (SPEED_PITCH_EN)
        __music_player_set_decode_speed_pitch_enable(mapi);
#endif//SPEED_PITCH_EN
        /* __music_player_set_save_breakpoint_vm_base_index(mapi, VM_DEV0_BREAKPOINT_REC, VM_DEV0_FLACBREAKPOINT_REC);  */
        script_music_printf("dev_let = %c\n", (u8)dev_let);
        script_music_printf("DEV_SEL_SPEC = %d, PLAY_FIRST_FILE = %d, REPEAT_ALL = %d\n", DEV_SEL_SPEC, PLAY_FIRST_FILE, REPEAT_ALL);
        script_music_printf("script music inti ok!!\n");
        dac_channel_on(MUSIC_CHANNEL, FADE_ON);

#if MUSIC_DECRYPT_EN
        cipher_init(0x12345678);
#endif

#if EQ_EN
        eq_enable();
        eq_mode = get_eq_default_mode();
#endif // EQ_EN
    } else {
        script_music_printf("music api creat fail\n");
    }

#if SUPPORT_APP_RCSP_EN
    rcsp_music_init(mapi);
#endif

    return (void *)mapi;
}

static void script_music_exit(void **hdl)
{
#if EQ_EN
    eq_disable();
#endif/*EQ_EN*/

#if MUSIC_DECRYPT_EN
    cipher_close();
#endif

#if SUPPORT_APP_RCSP_EN
    rscp_music_exit();
#endif

    __music_player_set_save_breakpoint_enable((MUSIC_PLAYER_OBJ *)*hdl, 1);
    music_player_destroy((MUSIC_PLAYER_OBJ **)hdl);
    script_music_printf("script music exit ok\n");
    dac_channel_off(MUSIC_CHANNEL, FADE_ON);
}


static void script_music_task(void *parg)
{
    int msg[6] = {0};
    tbool ret = true;
    _DECODE_STATUS status;
    lg_dev_info *cur_lgdev_info = NULL;
    NOTICE_PlAYER_ERR n_err = NOTICE_PLAYER_NO_ERR;
    MUSIC_PLAYER_OBJ *mapi = (MUSIC_PLAYER_OBJ *)parg;

    if (mapi != NULL) {
        malloc_stats();
        n_err = script_music_notice_play(BPF_MUSIC_MP3);
        /* if (n_err != NOTICE_PLAYER_NO_ERR && n_err != NOTICE_PLAYER_NO_ERR_WITHOUT_MSG_DEAL) { */
        if (n_err != NOTICE_PLAYER_TASK_DEL_ERR) {
            ret = music_player_process(mapi, DEC_PHY_TASK_NAME, TaskMusicPhyDecPrio);

            if (ret == true) {
                ret = script_music_play(mapi, 1);
            }
        }
    } else {
        ret = false;
        script_music_printf("music play hdl is NULL,  change work mode\n");
        os_taskq_post(MAIN_TASK_NAME, 1, MSG_CHANGE_WORKMODE);
    }

    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();
        if (n_err == NOTICE_PLAYER_TASK_DEL_ERR || ret == false) {
            if (msg[0] != SYS_EVENT_DEL_TASK) {
                continue;
            }
        }

#if SUPPORT_APP_RCSP_EN
        rcsp_music_task_msg_deal_before(msg);
#endif
        switch (msg[0]) {
#ifdef MUSIC_IO_ENABLE
        case MSG_MIO_READ:
            if (mapi && mapi->fop_api->cur_lgdev_info
                && mapi->fop_api->cur_lgdev_info->lg_hdl->file_hdl
                && g_mio_var
               ) {
                mio_ctrl_read_data((mio_ctrl_t *)g_mio_var);
            }
            break;
#endif

        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                script_music_printf("script_music_del \r");
#ifdef MUSIC_IO_ENABLE
                music_mio_close();
#endif
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;
        case SYS_EVENT_DEC_FR_END:
        case SYS_EVENT_DEC_FF_END:
        case SYS_EVENT_DEC_END:
            __music_player_set_file_sel_mode(mapi, PLAY_AUTO_NEXT);
            __music_player_set_dev_sel_mode(mapi, DEV_SEL_CUR);
            ret = script_music_play(mapi, 1);
            break;
        case MSG_MUSIC_PLAY_SN:
            __music_player_set_file_index(mapi, msg[1]);
            __music_player_set_file_sel_mode(mapi, PLAY_SPEC_FILE);
            __music_player_set_dev_sel_mode(mapi, DEV_SEL_CUR);
            ret = script_music_play(mapi, 1);
            break;
        case MSG_MUSIC_SPC_FILE:
            script_music_printf("MSG_MUSIC_SPC_FILE = %x\n", msg[1]);
            __music_player_set_filesclust(mapi, msg[1]);
            __music_player_set_file_sel_mode(mapi, (ENUM_FILE_SELECT_MODE)FOP_OPEN_FILE_BYSCLUCT);
            __music_player_set_dev_sel_mode(mapi, DEV_SEL_CUR);
            ret = script_music_play(mapi, 1);
            break;
        case MSG_MUSIC_NEXT_FILE:
            __music_player_set_file_sel_mode(mapi, PLAY_NEXT_FILE);
            __music_player_set_dev_sel_mode(mapi, DEV_SEL_CUR);
            ret = script_music_play(mapi, 1);
            break;
        case MSG_MUSIC_PREV_FILE:
            __music_player_set_file_sel_mode(mapi, PLAY_PREV_FILE);
            __music_player_set_dev_sel_mode(mapi, DEV_SEL_CUR);
            ret = script_music_play(mapi, 0);
            break;

        case MSG_MUSIC_FF:
            if (music_player_fffr(mapi, PLAY_MOD_FF, 2) == false) {
                /* script_music_printf("ff fail !!\n");	 */
            }
            break;
        case MSG_MUSIC_FR:
            if (music_player_fffr(mapi, PLAY_MOD_FB, 2) == false) {
                /* script_music_printf("fr fail !!\n");	 */
            }
            break;
        case MSG_MUSIC_FFR_DONE:
            break;
        case MSG_MUSIC_PP:
            status = music_player_pp(mapi);
            if (status == DECODER_PAUSE) {
                script_music_printf("pause >>\n");
                /* n_err = script_music_notice_play(BPF_MUSIC_MP3); */
                /* printf("err = 0x%x \n", n_err); */

            } else {
                script_music_printf("play >>\n");
            }
            break;
        case MSG_MUSIC_PLAY:
            if (__music_player_get_status(mapi) == DECODER_PAUSE) {
                music_player_pp(mapi);
            }
            break;
        case MSG_MUSIC_PAUSE:
            if (__music_player_get_status(mapi) == DECODER_PLAY) {
                music_player_pp(mapi);
            }
            break;

        case MSG_MUSIC_U_SD:
            if (file_operate_get_total_phydev() > 1) {
                script_music_printf("music dev change\n");
                __music_player_set_save_breakpoint_enable(mapi, 1);
                __music_player_set_file_index(mapi, 1);
                __music_player_set_file_sel_mode(mapi, PLAY_BREAK_POINT);
                __music_player_set_dev_let(mapi, 0);
                __music_player_set_dev_sel_mode(mapi, DEV_SEL_NEXT);
                ret = script_music_play(mapi, 1);
            }
            break;

        case MSG_MUSIC_EQ:
#if EQ_EN
            if (__music_player_get_status(mapi) == DECODER_STOP) {
                break;
            }
            eq_mode++;
            if (eq_mode > 5) {
                eq_mode = 0;
            }
            script_music_printf("music eq mode = %d\n");
            /* __music_player_set_eq(mapi, eq_mode); */
            eq_mode_sw(eq_mode);
#endif
            break;
        case MSG_MUSIC_RPT:
            cur_rpt_mode++;
            if (cur_rpt_mode >= MAX_PLAY_MODE) {
                cur_rpt_mode = REPEAT_ALL;
            }
            __music_player_set_play_mode(mapi, cur_rpt_mode);
            break;
        case MSG_MUSIC_DEL_FILE:
#ifdef MUSIC_IO_ENABLE
            music_mio_close();
#endif
            ret = music_player_delete_playing_file(mapi);
            if (ret == false) {
                script_music_printf("delete playing file fail\n");
            } else {
                script_music_printf("delete playing file ok\n");
            }
            if (__music_player_get_file_total(mapi, 1) == 0) {
                __music_player_set_file_index(mapi, 1);
                __music_player_set_dev_sel_mode(mapi, DEV_SEL_NEXT);
                __music_player_set_file_sel_mode(mapi, PLAY_FIRST_FILE);
            } else {
                __music_player_set_dev_sel_mode(mapi, DEV_SEL_CUR);
                __music_player_set_file_sel_mode(mapi, PLAY_SPEC_FILE);
            }
            ret = script_music_play(mapi, 1);
            break;

        case SYS_EVENT_DEV_ONLINE:
            script_music_printf("music dev online >>>\n");
            __music_player_set_save_breakpoint_enable(mapi, 1);
            __music_player_set_file_index(mapi, 1);
            __music_player_set_file_sel_mode(mapi, PLAY_BREAK_POINT);
            __music_player_set_dev_let(mapi, msg[1]);
            __music_player_set_dev_sel_mode(mapi, DEV_SEL_SPEC);
            ret = script_music_play(mapi, 1);
            break;
        case SYS_EVENT_DEV_OFFLINE:
            script_music_printf("music dev offline >>>\n");
            cur_lgdev_info = __music_player_get_lg_dev_info(mapi);
            if ((cur_lgdev_info != NULL) && (cur_lgdev_info->lg_hdl->phydev_item == (void *)msg[1])) {
                if (__music_player_get_status(mapi) != DECODER_PAUSE) {
                    break;//解码暂停状态，有下一个消息SYS_EVENT_DEC_DEVICE_ERR处理
                }
            } else {
                break;
            }
        /* break; */
        case SYS_EVENT_DEC_DEVICE_ERR:
            script_music_printf("m SYS_EVENT_DEC_DEVICE_ERR!!\n");
            __music_player_set_save_breakpoint_enable(mapi, 1);
            __music_player_set_file_index(mapi, 1);
            __music_player_set_file_sel_mode(mapi, PLAY_BREAK_POINT);
            __music_player_set_dev_sel_mode(mapi, DEV_SEL_NEXT);
            ret = script_music_play(mapi, 1);
            break;
        case SYS_EVENT_DEV_BEGIN_MOUNT:
            script_music_printf("m SYS_EVENT_DEV_BEGIN_MOUNT!!\n");
            __music_player_set_save_breakpoint_enable(mapi, 1);
#ifdef MUSIC_IO_ENABLE
            music_mio_close();
#endif
            music_player_stop(mapi);
            break;
        case SYS_EVENT_DEV_MOUNT_ERR:
            script_music_printf("m SYS_EVENT_DEV_MOUNT_ERR!!\n");
            __music_player_set_save_breakpoint_enable(mapi, 0);
            __music_player_set_file_sel_mode(mapi, PLAY_BREAK_POINT);
            __music_player_set_dev_sel_mode(mapi, DEV_SEL_NEXT);
            ret = script_music_play(mapi, 1);
            break;
        case MSG_HALF_SECOND:
            script_music_printf("-MH-\n");
            break;
        default:
            break;
        }
        if (ret == false) {
            script_music_printf("script music play err !!!\n");
        }

#if SUPPORT_APP_RCSP_EN
        rcsp_music_task_msg_deal_after(msg);
#endif
    }
}


const SCRIPT script_music_info = {
    .skip_check = script_music_skip_check,
    .init = script_music_init,
    .exit = script_music_exit,
    .task = script_music_task,
    .key_register = &script_music_key,
};

