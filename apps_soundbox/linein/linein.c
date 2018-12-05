#include "common/app_cfg.h"
#include "rtos/os_api.h"
#include "rtos/os_cfg.h"
#include "common/error.h"
#include "common/msg.h"
#include "rtos/task_manage.h"
#include "dac/dac_api.h"
/* #include "play_sel/play_sel.h" */
#include "key_drv/key.h"
#include "ui/led/led.h"
#include "dac/dac_api.h"
#include "dev_linein.h"
#include "ui/ui_api.h"
#include "dac/ladc.h"
#include "linein_ui.h"
#include "echo/echo_api.h"
#include "echo/echo_deal.h"
#include "record.h"
#include "encode/encode.h"
#include "key_drv/key_voice.h"
#include "dac/dac.h"
#include "dac/dac_api.h"
#include "rcsp/rcsp_interface.h"
#include "dac/eq.h"
#include "notice_player.h"
#include "linein.h"


extern void *malloc_fun(void *ptr, u32 len, char *info);
extern void free_fun(void **ptr);
extern u8 eq_mode;
#if USE_SOFTWARE_EQ
static void aux_eq_task_init();
static void aux_eq_task_stop();
static void aux_eq_run(void *p);
#endif

RECORD_OP_API *aux_rec = NULL;
REVERB_API_STRUCT *aux_reverb = NULL;
#define get_mute_status   is_dac_mute


/*----------------------------------------------------------------------------*/
/**@brief  AUX DAC通道选择，开启
 @param  无
 @return 无
 @note   void aux_dac_channel_on(void)
 */
/*----------------------------------------------------------------------------*/
u8 ladc_aux_ch = 0;
void aux_dac_channel_on(void)
{
    if (LINEIN_CHANNEL == DAC_AMUX0) {
        JL_PORTA->DIR |= (BIT(1) | BIT(2));
        JL_PORTA->DIE &= ~(BIT(1) | BIT(2));
    } else if (LINEIN_CHANNEL == DAC_AMUX1) {
        JL_PORTA->DIR |= (BIT(4) | BIT(3));
        JL_PORTA->DIE &= ~(BIT(4) | BIT(3));
        JL_PORTA->PD  &= ~(BIT(3));//PA3 default pull_down
    } else if (LINEIN_CHANNEL == DAC_AMUX2) {
        JL_PORTB->DIR |= (BIT(11) | BIT(12));
        JL_PORTB->DIE &= ~(BIT(11) | BIT(12));
    }

#if AUX_AD_ENABLE

#if USE_SOFTWARE_EQ
    aux_eq_task_init();
#endif
    dac_channel_on(MUSIC_CHANNEL, FADE_ON);
    set_sys_vol(0, 0, FADE_ON);

#if (SOUND_MIX_GLOBAL_EN == 0 &&  LINEIN_DIGTAL_EN == 0)
#if DAC2IIS_EN
    dac_set_samplerate(44100, 0);
#else
    dac_set_samplerate(LINEIN_DEFAULT_DIGITAL_SR, 0);
#endif

#else//(SOUND_MIX_GLOBAL_EN == 0 &&  LINEIN_DIGTAL_EN == 0)
#if EQ_EN
    eq_enable();
#endif//EQ_EN
#endif//(SOUND_MIX_GLOBAL_EN == 0 &&  LINEIN_DIGTAL_EN == 0)

    if ((LINEIN_CHANNEL == DAC_AMUX0_L_ONLY) || (LINEIN_CHANNEL == DAC_AMUX1_L_ONLY) || \
        (LINEIN_CHANNEL == DAC_AMUX2_L_ONLY) || (LINEIN_CHANNEL == DAC_AMUX_DAC_L)) {
        ladc_aux_ch = ENC_LINE_LEFT_CHANNEL;
    } else if ((LINEIN_CHANNEL == DAC_AMUX0_R_ONLY) || (LINEIN_CHANNEL == DAC_AMUX1_R_ONLY) || \
               (LINEIN_CHANNEL == DAC_AMUX2_R_ONLY) || (LINEIN_CHANNEL == DAC_AMUX_DAC_R)) {
        ladc_aux_ch = ENC_LINE_RIGHT_CHANNEL;
    } else {
        ladc_aux_ch = ENC_LINE_LR_CHANNEL;
    }
#if DAC2IIS_EN
    ladc_reg_init(ladc_aux_ch, LADC_SR44100);
#else
    ladc_reg_init(ladc_aux_ch, LINEIN_DEFAULT_DIGITAL_SR);
#endif
    amux_channel_en(LINEIN_CHANNEL, 1);
    os_time_dly(35);
    set_sys_vol(dac_ctl.sys_vol_l, dac_ctl.sys_vol_r, FADE_ON);
#else
    reg_aux_rec_exit(ladc_close);
    dac_channel_on(LINEIN_CHANNEL, FADE_ON);
    os_time_dly(20);//wait amux channel capacitance charge ok
    set_sys_vol(dac_ctl.sys_vol_l, dac_ctl.sys_vol_r, FADE_ON);
#if (ECHO_EN||PITCH_EN)
    ///开启dac数字通道
    dac_trim_en(0);
    dac_digital_en(1);
#else
    ///关闭dac数字通道
    //dac_trim_en(1);
    //dac_digital_en(0);
#endif//end of ECHO_EN
#endif//end of AUX_AD_ENABLE
}

/*----------------------------------------------------------------------------*/
/**@brief  AUX DAC通道选择，关闭
 @param  无
 @return 无
 @note   void aux_dac_channel_off(void)
 */
/*----------------------------------------------------------------------------*/
void aux_dac_channel_off(void)
{
#if AUX_AD_ENABLE

#if (SOUND_MIX_GLOBAL_EN != 0 ||  LINEIN_DIGTAL_EN != 0)
#if EQ_EN
    eq_disable();
#endif//EQ_EN
#endif//(SOUND_MIX_GLOBAL_EN != 0 ||  LINEIN_DIGTAL_EN != 0)

#if USE_SOFTWARE_EQ
    aux_eq_task_stop();
#endif
    dac_channel_off(MUSIC_CHANNEL, FADE_ON);
    ladc_close(ladc_aux_ch);
    amux_channel_en(LINEIN_CHANNEL, 0);
#else
    dac_channel_off(LINEIN_CHANNEL, FADE_ON);
    dac_trim_en(0);
    /* dac_mute(0, 1); */
#endif
}


#if USE_SOFTWARE_EQ
#define AUX_SOFT_EQ_BUF_SIZE  5*1024
#define AUX_EQ_TASK_NAME "AUX_SOFT_EQ"
typedef struct {
    u8 *data_buf;
    cbuffer_t aux_ladc_cbuf;
    OS_SEM aux_eq_start;
    u8 run_flag;
} aux_soft_eq_var;
aux_soft_eq_var aux_soft_eq;
static void aux_eq_run(void *p)
{
    int msg[3];
    u8 read_buf[512];
    puts("aux_eq_run===\n");
    while (aux_soft_eq.run_flag == 0xaa) {
        os_sem_pend(&aux_soft_eq.aux_eq_start, 0);
        if (cbuf_read(&aux_soft_eq.aux_ladc_cbuf, read_buf, 512) == 512) {
            dac_write(read_buf, 512);
        }
    }
    while (1) {
        memset(msg, 0x0, sizeof(msg));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            puts("DEL AUX EQ TASK\n");
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                if (aux_soft_eq.data_buf) {
                    free(aux_soft_eq.data_buf);
                }
                aux_soft_eq.data_buf = NULL;
                os_task_del_res_name(OS_TASK_SELF);
            }
        default:
            break;
        }
    }
}
void aux_ladc_cbuf_write(u8 *data, u16 length)
{
    if (aux_soft_eq.run_flag != 0xaa) {
        return;
    }
    if (length != cbuf_write(&aux_soft_eq.aux_ladc_cbuf, data, length)) {
        putchar('&');
    }
    os_sem_post(&aux_soft_eq.aux_eq_start);
}
static void aux_eq_task_init()
{
    u32 err;
    aux_soft_eq.data_buf = malloc(AUX_SOFT_EQ_BUF_SIZE);
    ASSERT(aux_soft_eq.data_buf);
    cbuf_init(&aux_soft_eq.aux_ladc_cbuf, aux_soft_eq.data_buf, AUX_SOFT_EQ_BUF_SIZE);
    os_sem_create(&aux_soft_eq.aux_eq_start, 0);
    aux_soft_eq.run_flag = 0xaa;
    err = os_task_create_ext(aux_eq_run,
                             (void *)0,
                             TaskLineinEqPrio,
                             10
#if OS_TIME_SLICE_EN > 0
                             , 1
#endif
                             , AUX_EQ_TASK_NAME
                             , 1024 * 4
                            );

    if (OS_NO_ERR == err) {
        puts("aux eq init ok\n");
    }
}
static void aux_eq_task_stop()
{
    aux_soft_eq.run_flag = 0;
    os_sem_post(&aux_soft_eq.aux_eq_start);
    if (os_task_del_req(AUX_EQ_TASK_NAME) != OS_TASK_NOT_EXIST) {
        os_taskq_post_event(AUX_EQ_TASK_NAME, 1, SYS_EVENT_DEL_TASK);
        do {
            OSTimeDly(1);
        } while (os_task_del_req(AUX_EQ_TASK_NAME) != OS_TASK_NOT_EXIST);
    }
}
#endif
