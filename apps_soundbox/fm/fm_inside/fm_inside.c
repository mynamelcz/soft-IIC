/*--------------------------------------------------------------------------*//**@file     fm_inside.c
   @brief    内部收音底层驱动
   @details
   @author
   @date   2011-3-30
   @note
*/
/*----------------------------------------------------------------------------*/
#include "sdk_cfg.h"
#include "clock_api.h"
#include "fm/fm_api.h"
#include "fm_inside.h"
#include "fm/fm_radio.h"
#include "fm_inside_api.h"
#include "dac/src_api.h"
#include "dac/dac_api.h"
#include "dac/dac.h"
#include "iis/dac2iis.h"
#include "notice_player.h"
#include "common/app_cfg.h"
#include "dec/decoder_phy.h"

/* #include "play_sel/play_sel.h" */
#if	FM_INSIDE


#if (SOUND_MIX_GLOBAL_EN)
#include "sound_mix_api.h"

#define FM_MIX_NAME					"FM_MIX"
#define FM_MIX_OUTPUT_BUF_LEN		(4*1024)
#define FM_MIX_OUTPUT_DEC_LIMIT		(50)

#endif

typedef struct __FM_DATA_STREAM {
    DEC_SET_INFO_CB set_info_cb;
    DEC_OP_CB		op_cb;
    _OP_IO 			output;
    void 		   *mix;
} FM_DATA_STREAM;

static FM_DATA_STREAM fm_data_s;

struct fm_config_dat *fm_set_p = NULL;

static void *fm_src_obj = NULL;

extern RECORD_OP_API *rec_fm_api;
/*----------------------------------------------------------------------------*/
/**@brief  FM变采样输出函数
   @param  buf：FM音频数据地址
   @param  len：FM音频数据长度
   @return 无
   @note   被变采样函数调用
*/
/*----------------------------------------------------------------------------*/
#if 0
static const u8 fm_sine_buf_32K[] = {
    0x00, 0x00, 0xae, 0x11, 0xad, 0x22, 0x58, 0x32,
    0x13, 0x40, 0x58, 0x4b, 0xb8, 0x53, 0xe0, 0x58,
    0x9e, 0x5a, 0xe0, 0x58, 0xb8, 0x53, 0x58, 0x4b,
    0x13, 0x40, 0x58, 0x32, 0xad, 0x22, 0xae, 0x11,
    0x00, 0x00, 0x52, 0xee, 0x53, 0xdd, 0xa8, 0xcd,
    0xed, 0xbf, 0xa8, 0xb4, 0x48, 0xac, 0x20, 0xa7,
    0x62, 0xa5, 0x20, 0xa7, 0x48, 0xac, 0xa8, 0xb4,
    0xed, 0xbf, 0xa8, 0xcd, 0x53, 0xdd, 0x52, 0xee

    /* 0x01, 0x00, 0x02, 0x00, 0xae, 0x11, 0xae, 0x11, 0xad, 0x22, 0xad, 0x22, 0x58, 0x32, 0x58, 0x32, 0x13, */
    /* 0x40, 0x13, 0x40, 0x58, 0x4b, 0x58, 0x4b, 0xb8, 0x53, 0xb8, 0x53, 0xe0, 0x58, 0xe0, 0x58, 0x9e, 0x5a, */
    /* 0x9e, 0x5a, 0xe0, 0x58, 0xe0, 0x58, 0xb8, 0x53, 0xb8, 0x53, 0x58, 0x4b, 0x58, 0x4b, 0x13, 0x40, 0x13, */
    /* 0x40, 0x58, 0x32, 0x58, 0x32, 0xad, 0x22, 0xad, 0x22, 0xae, 0x11, 0xae, 0x11, 0x00, 0x00, 0x00, 0x00, */
    /* 0x52, 0xee, 0x52, 0xee, 0x53, 0xdd, 0x53, 0xdd, 0xa8, 0xcd, 0xa8, 0xcd, 0xed, 0xbf, 0xed, 0xbf, 0xa8, */
    /* 0xb4, 0xa8, 0xb4, 0x48, 0xac, 0x48, 0xac, 0x20, 0xa7, 0x20, 0xa7, 0x62, 0xa5, 0x62, 0xa5, 0x20, 0xa7, */
    /* 0x20, 0xa7, 0x48, 0xac, 0x48, 0xac, 0xa8, 0xb4, 0xa8, 0xb4, 0xed, 0xbf, 0xed, 0xbf, 0xa8, 0xcd, 0xa8, */
    /* 0xcd, 0x53, 0xdd, 0x53, 0xdd, 0x52, 0xee, 0x52, 0xee */
};
#endif

void fm_rec_dac(s16 *buf, u32 len)
{
#if (REC_SOURCE == 0)//0: rec source = fm
    u8 fm_rec_buf[0x40];
    s16 *dp = (s16 *)fm_rec_buf;
    u32 rec_point;
    if ((rec_fm_api != NULL) && (rec_fm_api->rec_ctl != NULL)) {
        if (rec_fm_api->rec_ctl->enable == ENC_STAR) {
            /* rec_fm_api->rec_ctl->ladc.save_ladc_buf(&rec_fm_api->rec_ctl->ladc,fm_sine_buf_32K,0,64); */
            /* rec_fm_api->rec_ctl->ladc.save_ladc_buf(&rec_fm_api->rec_ctl->ladc,fm_sine_buf_32K,1,64); */

            for (rec_point = 0; rec_point < 0x20; rec_point++) {
                dp[rec_point] = buf[rec_point * 2]; //all left channel data
            }
            rec_fm_api->rec_ctl->ladc.save_ladc_buf(&rec_fm_api->rec_ctl->ladc, dp, 0, 0x40);

            for (rec_point = 0; rec_point < 0x20; rec_point++) {
                dp[rec_point] = buf[rec_point * 2 + 1]; //all right channel data
            }
            rec_fm_api->rec_ctl->ladc.save_ladc_buf(&rec_fm_api->rec_ctl->ladc, dp, 1, 0x40);
        }
    }

#endif

}
#define FM_FADEIN_VAL	100
static u16 fm_fadein_cnt = 0;
static void fm_dac_out(void *priv, s16 *buf, u32 len)///len是byte为单位
{
    if (fm_fadein_cnt) {
        fm_fadein_cnt--;
        /* printf(" %d ",fm_fadein_cnt); */
        return;
    }

    //stop fm output when prompt playing
    /* if (play_sel_busy()) { */
    /* return; */
    /* } */
    if (notice_player_get_status()) {
        return;
    }

    if (fm_mode_var && fm_mode_var->fm_mute) { //MUTE
        memset(buf, 0x00, len);
    }

    /* putchar('F'); */
    cbuf_write_dac(buf, len);

    fm_rec_dac(buf, len);
}

/*----------------------------------------------------------------------------*/
/**@brief  FM driver 数据输出
   @param  buf：FM音频数据地址
   @param  len：FM音频数据长度
   @return 无
   @note
*/
/*----------------------------------------------------------------------------*/
static void fm_drv_data_out(void *priv, void *buf, u32 len)
{
    /* putchar('f'); */
    src_obj_write_to_task_api(priv, buf, len);
    /* src_write_data_api(buf, len); */
}

/*----------------------------------------------------------------------------*/
/**@brief  FM src 变采样使能配置
   @param  flag -- 开关
   @param
   @return 无
   @note
*/
/*----------------------------------------------------------------------------*/
#define FM_SRC_TASK_NAME 		"FMSrcTASK"
#define FM_SRC_TASK_STK_SIZE	(1*1024)
static void src_md_fm_ctl(int flag, _OP_IO *input)
{
    SRC_OBJ_PARAM src_p;
    void *tmp_src_obj = NULL;

    if (flag == 1) {
        memset((u8 *)&src_p, 0x0, sizeof(SRC_OBJ_PARAM));
        src_p.once_out_len = 128;
        src_p.in_chinc = 1;
        src_p.in_spinc = 2;
        src_p.out_chinc = 1;
        src_p.out_spinc = 2;
        src_p.in_rate = FM_SOURCE_SAMPLERATE;//41667;
        src_p.out_rate = FM_DAC_OUT_SAMPLERATE;//samplerate;
        src_p.nchannel = 2;
        src_p.output.priv = input->priv;
        src_p.output._cbk = (void *)input->output;
        src_init_api();
        tmp_src_obj = src_obj_creat_with_task_api(&src_p, FM_SRC_TASK_NAME, TaskFMsrcPrio, FM_SRC_TASK_STK_SIZE);
        ASSERT(tmp_src_obj);
        input->priv = tmp_src_obj;
        input->output = (void *)fm_drv_data_out;
        /* OS_ENTER_CRITICAL(); */
        /* fm_src_obj = tmp_src_obj; */
        /* OS_EXIT_CRITICAL(); */
        /* puts("fm_src_start\n"); */
        //src_p.in_chinc = 1;
        //src_p.in_spinc = 2;
        //src_p.out_chinc = 1;
        //src_p.out_spinc = 2;
        //src_p.in_rate = 41667;
        //src_p.out_rate = FM_DAC_OUT_SAMPLERATE;
        //src_p.nchannel = 2;
        //src_p.output.priv = NULL;
        //src_p.output._cbk = (void *)fm_dac_out;
        ///* src_p.output_cbk = (void *)fm_dac_out; */
        //src_init_api(&src_p);
    } else {
        /* puts("fm_src_stop\n"); */
        if (input->priv) {
            src_obj_destroy_with_task_api(&(input->priv));
        }
        src_exit_api();
        fm_src_obj = NULL;
    }

}

///--------------------------FM_INSIDE_API------------------------
static u32 fm_inside_set_info(void *priv, dec_inf_t *inf, tbool wait)
{
    dac_set_samplerate(inf->sr, wait);
    return 0;
}

void fm_inside_init(void)
{
    bool mute_flag;

    dec_inf_t inf;
    memset((u8 *)&inf, 0x0, sizeof(dec_inf_t));

#if (SOUND_MIX_GLOBAL_EN)
#else
    mute_flag = is_dac_mute();
    if (mute_flag == 0) {
        dac_mute(1, 1);
    }
#endif

#if DAC2IIS_EN
    dac2iis_src_open_ctrl(0);
#endif

    memset((u8 *)&fm_data_s, 0x0, sizeof(FM_DATA_STREAM));
    fm_data_s.set_info_cb.priv = NULL;
    fm_data_s.set_info_cb._cb = fm_inside_set_info;
    fm_data_s.op_cb.priv = NULL;
    fm_data_s.op_cb.clr = NULL;
    fm_data_s.op_cb.over = NULL;
    fm_data_s.output.priv = NULL;
    fm_data_s.output.output = (void *)fm_dac_out;

#if (SOUND_MIX_GLOBAL_EN)
    SOUND_MIX_P mix_p;
    memset((u8 *)&mix_p, 0x0, sizeof(SOUND_MIX_P));
    mix_p.key = FM_MIX_NAME;
    mix_p.out_len = FM_MIX_OUTPUT_BUF_LEN;
    /* mix_p.set_info = &(mapi->dop_api->io->set_music_info); */
    mix_p.set_info_cb = &fm_data_s.set_info_cb;
    mix_p.input = &fm_data_s.output;
    mix_p.op_cb = &fm_data_s.op_cb;
    mix_p.limit = FM_MIX_OUTPUT_DEC_LIMIT;
    mix_p.max_vol = DAC_DIGIT_TRACK_MAX_VOL;
    mix_p.default_vol = DAC_DIGIT_TRACK_DEFAULT_VOL;
    mix_p.src_p.task_name = FM_SRC_TASK_NAME;
    mix_p.src_p.task_prio = TaskFMsrcPrio;
    fm_data_s.mix = (void *)sound_mix_chl_add(sound_mix_api_get_obj(), &mix_p);

    inf.sr = FM_SOURCE_SAMPLERATE;
#else
    src_md_fm_ctl(1, &(fm_data_s.output));
    inf.sr = FM_DAC_OUT_SAMPLERATE;
#endif

    set_fm_dac_out_fun(fm_data_s.output.output, fm_data_s.output.priv);

    if (fm_data_s.set_info_cb._cb) {
        fm_data_s.set_info_cb._cb(fm_data_s.set_info_cb.priv, &inf, 1);
    }

    /* set_fm_dac_out_fun(fm_drv_data_out, NULL); */
    /* src_md_fm_ctl(1); */
    /* dac_set_samplerate(FM_DAC_OUT_SAMPLERATE, 1); */


#if (SOUND_MIX_GLOBAL_EN)
    __sound_mix_chl_set_mute((SOUND_MIX_CHL *)fm_data_s.mix, 1);
#endif

    dac_channel_on(FM_INSI_CHANNEL, FADE_ON);

    fm_inside_module_open();

    //FM搜台参数1设置：分别对应：				//nrs	//nc1	//nc2	//ncc
    fm_inside_io_ctrl(SET_FM_INSIDE_SCAN_ARG1, 	4, 		55, 	45, 	40);

    //FM搜台参数2设置：分别对应：				//noise_scan	//noise_pga	//cnr	//(reserve)	//agc_max	//(reserve)
    fm_inside_io_ctrl(SET_FM_INSIDE_SCAN_ARG2, 	0, 				8, 			2,		10, 			9, 			0);

    //系统时钟获取，用于内部FM模块
    fm_inside_io_ctrl(SET_FM_INSIDE_OSC_CLK, OSC_Hz / 1000000);

    //FM内部晶振选择，0：BT_OSC	,1：RTC_OSC
    fm_inside_io_ctrl(SET_FM_INSIDE_OSC_SEL, 0);

    //FM搜台信息打印
#ifdef __DEBUG
    fm_inside_io_ctrl(SET_FM_INSIDE_PRINTF, 1);
#endif


    fm_inside_on(0);

    fm_set_stereo(0);
    /* fm_set_abw(48); */

    fm_set_mod(0);


#if (SOUND_MIX_GLOBAL_EN)
    __sound_mix_chl_set_mute((SOUND_MIX_CHL *)fm_data_s.mix, 0);
#else
    dac_mute(mute_flag, 1); //recover mute status
#endif
    fm_fadein_cnt = FM_FADEIN_VAL;
}

bool fm_inside_set_fre(u16 fre)
{
    u8 udat8;
    u32 freq;
    freq = fre * FM_STEP;
    udat8 = set_fm_inside_freq(freq);
    printf("fre = %u  %d\n", fre, udat8);
    return udat8;
}

bool fm_inside_read_id(void)
{
    return read_fm_inside_id();
}

void fm_inside_powerdown(void)
{
    fm_inside_off();
#if (SOUND_MIX_GLOBAL_EN)
    if (fm_data_s.mix) {
        sound_mix_chl_del_by_chl((SOUND_MIX_CHL **) & (fm_data_s.mix));
    }
#else
    src_md_fm_ctl(0, &(fm_data_s.output));
#endif
    dac_channel_off(FM_INSI_CHANNEL, FADE_ON);
#if DAC2IIS_EN
    dac2iis_src_open_ctrl(1);
#endif
}

void fm_inside_mute(u8 flag)
{
#if (SOUND_MIX_GLOBAL_EN)
    __sound_mix_chl_set_mute((SOUND_MIX_CHL *)fm_data_s.mix, flag);
#else
    /* dac_mute(flag,1); */
#endif
    /* fm_mode_var->fm_mute = flag; */
}


#endif // FM_INSIDE
