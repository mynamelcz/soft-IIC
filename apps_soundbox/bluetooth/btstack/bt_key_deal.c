#include "bt_key_deal.h"
#include "rtos/os_api.h"
#include "common/msg.h"
#include "common/app_cfg.h"
#include "bluetooth/avctp_user.h"
#include "rtos/task_manage.h"
#include "dec/decoder_phy.h"
#include "ui/led/led.h"
#include "dac/dac_api.h"
#include "dac/dac.h"
#include "ui/ui_api.h"
#include "key_drv/key.h"
#include "bt_ui.h"
#include "dac/eq_api.h"
#include "sys_detect.h"
#include "dac/eq.h"
#include "encode/encode.h"
#include "echo/echo_api.h"
#include "echo/echo_deal.h"
#include "vm/vm.h"
#include "vm/vm_api.h"
#include "rcsp/rcsp_interface.h"
#include "bluetooth/nfc_api.h"
#include "key_drv/key_voice.h"
#include "notice_player.h"
#include "script_bt_key.h"
#include "script_list.h"
#include "sound_mix_api.h"
#include "timer.h"

#include "bluetooth/le_server_module.h"
#include "spi_rec/spi_rec.h"
u8 spi_rec_flag = 0;


#define BT_REC_Hz  120000000L

RECORD_OP_API *rec_bt_api = NULL;
REVERB_API_STRUCT *bt_reverb = NULL;
BT_DIS_VAR bt_ui_var;
u8 fast_test = 0;

extern volatile u8 scl_cont;
extern volatile u8 sda_cont;

struct user_ctrl {
    u8 is_phone_number_come;
    u8 play_phone_number_flag;
    u8 phone_prompt_tone_playing_flag;
    u8 phone_num_len;
    u8 income_phone_num[30];
    u8 user_cmd_param_buf[30];
    u8 bt_eq_mode;
    void *tone_by_file_name;
    u8 auto_connection_counter;
    u16 auto_shutdown_cnt;
    u8 esco_close_echo_flag;
    u8 wait_app_updata_flag;
    u8 discover_control_cnt;
    u8 auto_connection_stereo;
};

//#ifdef HAVE_MALLOC
//_PLAY_SEL_PARA *psel_p = NULL;
//#else
//_PLAY_SEL_PARA __psel_p;
//#define psel_p (&__psel_p)
//#endif

static struct user_ctrl __user_val sec_used(.ram1_data);
#define user_val (&__user_val)

extern tbool mutex_resource_apply(char *resource, int prio, void (*apply_response)(), void (*release_request)());
extern void set_poweroff_wakeup_io();
extern void set_poweroff_wakeup_io_handle_register(void (*handle)(), void (*sleep_io_handle)(), void (*sleep_handle)());
extern u8 a2dp_get_status(void);
extern bool get_sco_connect_status();
extern void set_esco_sr(u16 sr);

#if BT_BLE_WECHAT
extern void wechat_auth_fun(void);
extern void wechat_init_fun(void);
extern void wechat_send_fun(void);
#endif

extern char rec_file_name_last[];
extern char rec_let_last;

OS_MUTEX tone_manage_semp ; //= OS_MUTEX_INIT();

void bt_update_close_echo()
{
    echo_exit_api((void **)&bt_reverb);
    spirec_api_dac_stop();
    //exit_rec_api(&rec_bt_api);//exit rec when esco change

}
u8 get_esco_st(u8 sw)//esco call back fun, get esco status, 0 off, 1 on
{
#if BT_KTV_EN
    int msg_echo_start = MSG_ECHO_START;
    int msg_echo_stop = MSG_ECHO_STOP;

    spirec_api_dac_stop();
    /* exit_rec_api(&rec_bt_api);//exit rec when esco change */
    ladc_pga_gain(2, 0);//2:mic_channel, 0:gain

    if (sw) {
        /* puts("***iphone ktv start\n"); */
    } else {
        puts("***iphone ktv stop\n");
        echo_msg_deal_api((void **)&bt_reverb, &msg_echo_stop);
        os_taskq_post_event("btmsg", 1, MSG_ECHO_START);
        /* echo_msg_deal_api((void**)&bt_reverb, &msg_echo_start); */
    }
    return 1;

#else
    /* exit_rec_api(&rec_bt_api);//exit rec when esco change */
    spirec_api_dac_stop();

    if (sw) { //esco on
        echo_exit_api((void **)&bt_reverb);//echo not support when esco on
    }
    return 0;
#endif//end of BT_KTV_EN
}

u8 get_bt_eq_mode(void)
{
    return user_val->bt_eq_mode;
}

void set_bt_eq_mode(int new_mode)
{
#if EQ_EN
    if (new_mode > 5) {
        new_mode = 0;
    }
    user_val->bt_eq_mode = new_mode;
    eq_mode_sw(new_mode);
#endif
}

void create_bt_msg_task(char *task_name)
{
    u32 err;
    err = os_task_create(TaskBtMsgStack, (void *)0, TaskBTMSGPrio, 30, task_name);
    if (OS_NO_ERR != err) {
        puts("create bt msg fail\n");
        while (1);
    }
}



/*获取到来电电话号码，用于报号功能*/
void hook_hfp_incoming_phone_number(char *number, u16 length)
{

    if (user_val->is_phone_number_come == 0) {
        puts("phone_number\n");
        put_buf((u8 *)number, length);
        user_val->is_phone_number_come = 1;
        user_val->phone_num_len = length;
        memcpy(user_val->income_phone_num, number, length);
#if BT_PHONE_NUMBER
        if (!is_bt_in_background_mode()) {
            update_bt_current_status(NULL, BT_STATUS_PHONE_NUMBER, 0);
        }
#endif
    }
    //put_buf(user_val->income_phone_num,user_val->phone_num_len);
}



static u8 g_bt_tone_statue;
static void bt_tone_mutex_play(void)
{
    printf("bt_tone_mutex_play\n");
    NOTICE_PlAYER_ERR n_err = NOTICE_PLAYER_NO_ERR;
#if(BT_MODE!=NORMAL_MODE)
    return;
#endif
    /* exit_rec_api(&rec_bt_api);//exit rec when src change */

    switch (g_bt_tone_statue) {
    case BT_STATUS_POWER_ON:
#if BT_HID_INDEPENDENT_MODE || USER_SUPPORT_PROFILE_HID
        if (__get_hid_independent_flag()) {
            //HID独立提示音
            n_err = notice_player_play_to_mix("resourse",
                                              NULL,
                                              NULL,
                                              1,
                                              10,
                                              sound_mix_api_get_obj(),
                                              NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_STOP_WITH_BACK_END_MSG,
                                              1,
                                              BPF_INIT_HID_MP3);
        } else
#endif
        {
            n_err = notice_player_play_to_mix("resourse",
                                              NULL,
                                              NULL,
                                              1,
                                              10,
                                              sound_mix_api_get_obj(),
                                              NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_STOP_WITH_BACK_END_MSG,
                                              1,
                                              BPF_BT_MP3);
        }
        break;

    case BT_STATUS_POWER_OFF:
        n_err = notice_player_play_to_mix("resourse",
                                          NULL,
                                          NULL,
                                          1,
                                          10,
                                          sound_mix_api_get_obj(),
                                          NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_STOP_WITH_BACK_END_MSG,
                                          1,
                                          BPF_POWER_OFF_MP3);
        break;

    case BT_STATUS_RESUME:
        if (get_curr_channel_state() != 0) {
            n_err = notice_player_play_to_mix("resourse",
                                              NULL,
                                              NULL,
                                              1,
                                              10,
                                              sound_mix_api_get_obj(),
                                              NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_STOP_WITH_BACK_END_MSG,
                                              2,
                                              BPF_BT_MP3,
                                              BPF_CONNECT_MP3);
        } else {
            n_err = notice_player_play_to_mix("resourse",
                                              NULL,
                                              NULL,
                                              1,
                                              10,
                                              sound_mix_api_get_obj(),
                                              NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_STOP_WITH_BACK_END_MSG,
                                              2,
                                              BPF_BT_MP3,
                                              BPF_DISCONNECT_MP3);
        }
        break;
    case BT_STATUS_FIRST_CONNECTED:
    case BT_STATUS_SECOND_CONNECTED:
        //bt_power_max(0xFF);
#if BT_HID_INDEPENDENT_MODE || USER_SUPPORT_PROFILE_HID
        if (__get_hid_independent_flag()) {
            //HID独立提示音
            n_err = notice_player_play_to_mix("resourse",
                                              NULL,
                                              NULL,
                                              1,
                                              10,
                                              sound_mix_api_get_obj(),
                                              NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_STOP_WITH_BACK_END_MSG,
                                              1,
                                              BPF_CONN_HID_MP3);
        } else
#endif
        {
#if BT_TWS
            if (BT_CURRENT_CONN_PHONE == get_bt_current_conn_type()) {
                puts("------BT_CURRENT_CONN_PHONE\n");
                n_err = notice_player_play_to_mix("resourse",
                                                  NULL,
                                                  NULL,
                                                  1,
                                                  10,
                                                  sound_mix_api_get_obj(),
                                                  NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_STOP_WITH_BACK_END_MSG,
                                                  1,
                                                  BPF_CONNECT_ONLY_MP3);
            } else if (BT_CURRENT_CONN_STEREO_MASTER_PHONE == get_bt_current_conn_type()) {
                puts("--------BT_CURRENT_CONN_STEREO_MASTER_PHONE\n");
                n_err = notice_player_play_to_mix("resourse",
                                                  NULL,
                                                  NULL,
                                                  1,
                                                  10,
                                                  sound_mix_api_get_obj(),
                                                  NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_STOP_WITH_BACK_END_MSG,
                                                  1,
                                                  BPF_CONNECT_LEFT_MP3);
            } else if ((BT_CURRENT_CONN_STEREO_MASTER == get_bt_current_conn_type()) || (BT_CURRENT_CONN_STEREO_PHONE_MASTER == get_bt_current_conn_type())) {
                puts("--------BT_CURRENT_CONN_STEREO_MASTER\n");
                n_err = notice_player_play_to_mix("resourse",
                                                  NULL,
                                                  NULL,
                                                  1,
                                                  10,
                                                  sound_mix_api_get_obj(),
                                                  NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_STOP_WITH_BACK_END_MSG,
                                                  1,
                                                  BPF_CONNECT_LEFT_MP3);
            } else if (BT_CURRENT_CONN_STEREO_SALVE == get_bt_current_conn_type()) {
                puts("--------BT_CURRENT_CONN_STEREO_SALVE\n");
                n_err = notice_player_play_to_mix("resourse",
                                                  NULL,
                                                  NULL,
                                                  1,
                                                  10,
                                                  NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_STOP_WITH_BACK_END_MSG,
                                                  1,
                                                  BPF_CONNECT_RIGHT_MP3);
            } else {
                puts("--------BPF_CONNECT_ONLY_MP3\n");
                n_err = notice_player_play_to_mix("resourse",
                                                  NULL,
                                                  NULL,
                                                  1,
                                                  10,
                                                  sound_mix_api_get_obj(),
                                                  NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_STOP_WITH_BACK_END_MSG,
                                                  1,
                                                  BPF_CONNECT_ONLY_MP3);
            }
#else
            n_err = notice_player_play_to_mix("resourse",
                                              NULL,
                                              NULL,
                                              1,
                                              10,
                                              sound_mix_api_get_obj(),
                                              NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_STOP_WITH_BACK_END_MSG,
                                              1,
                                              BPF_CONNECT_MP3);
#endif
        }
        break;
    case BT_STATUS_FIRST_DISCONNECT:
    case BT_STATUS_SECOND_DISCONNECT:
#if BT_HID_INDEPENDENT_MODE || USER_SUPPORT_PROFILE_HID
        if (__get_hid_independent_flag()) {
            //HID独立提示音
            n_err = notice_player_play_to_mix("resourse",
                                              NULL,
                                              NULL,
                                              1,
                                              10,
                                              sound_mix_api_get_obj(),
                                              NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_STOP_WITH_BACK_END_MSG,
                                              1,
                                              BPF_DISCON_HID_MP3);
        } else
#endif
        {
#if BT_TWS
            if (BT_CURRENT_CONN_PHONE == get_bt_current_conn_type()) {
                puts("----disconn BT_CURRENT_CONN_STEREO_MASTER\n");
                n_err = notice_player_play_to_mix("resourse",
                                                  NULL,
                                                  NULL,
                                                  1,
                                                  10,
                                                  sound_mix_api_get_obj(),
                                                  NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_STOP_WITH_BACK_END_MSG,
                                                  1,
                                                  BPF_DISCONNECT_MP3);
            } else {
                puts("-----disconn BPF_DISCONNECT_MP3\n");
                n_err = notice_player_play_to_mix("resourse",
                                                  NULL,
                                                  NULL,
                                                  1,
                                                  10,
                                                  sound_mix_api_get_obj(),
                                                  NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_STOP_WITH_BACK_END_MSG,
                                                  1,
                                                  BPF_DISCONNECT_MP3);
            }
#else
            n_err = notice_player_play_to_mix("resourse",
                                              NULL,
                                              NULL,
                                              1,
                                              10,
                                              sound_mix_api_get_obj(),
                                              NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_STOP_WITH_BACK_END_MSG,
                                              1,
                                              BPF_DISCONNECT_MP3);
#endif
        }
        break;
    case BT_STATUS_TONE_BY_FILE_NAME:
        n_err = notice_player_play_to_mix("resourse",
                                          NULL,
                                          NULL,
                                          1,
                                          10,
                                          sound_mix_api_get_obj(),
                                          NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_STOP_WITH_BACK_END_MSG,
                                          1,
                                          user_val->tone_by_file_name);
        break;
    case BT_STATUS_PHONE_INCOME:
        puts("new tone play income\n");
        n_err = notice_player_play_to_mix("resourse",
                                          NULL,
                                          NULL,
                                          0,
                                          10,
                                          sound_mix_api_get_obj(),
                                          NOTICE_PLAYER_METHOD_BY_PATH | NOTICE_STOP_WITH_BACK_END_MSG,
                                          1,
                                          BPF_RING_MP3);
        break;
#if BT_PHONE_NUMBER
    case BT_STATUS_PHONE_NUMBER:
        puts("new tone play number !!\n");
        if (user_val->is_phone_number_come) { //有电话号码就播放
            u32 file_name[30] = {0};
            int i, cnt;
            for (i = 0, cnt = 0; i < user_val->phone_num_len; i++) {
                if ((user_val->income_phone_num[i] >= 0x30) && (user_val->income_phone_num[i] <= 0x39)) {
                    file_name[cnt] = (u32)tone_number_get_name(user_val->income_phone_num[i] - 0x30);
                    cnt++;
                }
            }
            n_err = notice_player_play_to_mix("resourse",
                                              NULL,
                                              NULL,
                                              1,
                                              20,
                                              sound_mix_api_get_obj(),
                                              NOTICE_PLAYER_METHOD_BY_PATH_ARRAY | NOTICE_STOP_WITH_BACK_END_MSG,
                                              2,
                                              cnt,
                                              file_name);

            break;//break要放在if里面，不播放直接free了
        }
#endif
    default:
        os_sem_post(&tone_manage_semp);
        return;
    }
    puts("tone_mutex_play\n");

    os_sem_post(&tone_manage_semp);
}
static void bt_tone_mutex_stop(void)
{
#if(BT_MODE!=NORMAL_MODE)
    return;
#endif
    notice_player_stop();
    dac_channel_on(BT_CHANNEL, FADE_ON);
    puts("tone_mutex_stop\n");
    if (user_val->play_phone_number_flag == 1) {
        //来电报号提示音播完后要播要循环播提示声
        user_val->play_phone_number_flag = 0;
        update_bt_current_status(NULL, BT_STATUS_PHONE_INCOME, 0);
    }
}

void user_ctrl_prompt_tone_play(u8 status, void *ptr)
{
    /* u8 *buf_addr = NULL; */
    /* u32 need_buf; */
#if(BT_MODE!=NORMAL_MODE)
    return;
#endif
    if ((bt_power_is_poweroff_post() && (status != BT_STATUS_FIRST_DISCONNECT))
        || is_bt_in_background_mode()) { //background not play tone
        return;
    }
    puts("play_tone\n");
    if (get_remote_test_flag() && (status == BT_STATUS_FIRST_CONNECTED)) { //测试盒测试不播放提示音
        return;
    }

    if ((going_to_pwr_off == 1) && (status != BT_STATUS_POWER_OFF)) { //软关机的时候只播关机提示音
        puts("-----------powering off---------\n");
        return;
    }

    os_sem_pend(&tone_manage_semp, 0);
    if (notice_player_get_status() != 0) {
        printf("notice player is busy !!\n");
        notice_player_stop();
    }

    g_bt_tone_statue = status;

    switch (status) {
    case BT_STATUS_POWER_ON:
        break;
    case BT_STATUS_POWER_OFF:
        break;
    case BT_STATUS_RESUME:
        break;
    case BT_STATUS_FIRST_CONNECTED:
    case BT_STATUS_SECOND_CONNECTED:
        bt_power_max(0xFF);
        break;
    case BT_STATUS_FIRST_DISCONNECT:
    case BT_STATUS_SECOND_DISCONNECT:
        break;
    case BT_STATUS_TONE_BY_FILE_NAME:
        break;
    case BT_STATUS_PHONE_INCOME:
        user_val->play_phone_number_flag = 0;
        user_val->phone_prompt_tone_playing_flag = 1;
        break;
#if BT_PHONE_NUMBER
    case BT_STATUS_PHONE_NUMBER:
        if (user_val->is_phone_number_come) { //有电话号码就播放
            user_val->phone_prompt_tone_playing_flag = 1;
            break;
        }
#endif
    default:
        os_sem_post(&tone_manage_semp);
        return;
    }

    mutex_resource_apply("tone", 4, bt_tone_mutex_play, bt_tone_mutex_stop);
    os_sem_pend(&tone_manage_semp, 0);
    os_sem_post(&tone_manage_semp);
}

/*封装一层方便直接使用文件号播放*/
static void bt_prompt_play_by_name(void *file_name, void *let)
{
#ifdef BT_PROMPT_EN         //定义在avctp_user.h
    puts("bt_prompt_play\n");
    user_val->tone_by_file_name = file_name;
    user_ctrl_prompt_tone_play(BT_STATUS_TONE_BY_FILE_NAME, let);
#endif

}
u8 get_stereo_salve(u8 get_type)
{
    u8 stereo_info;
    if (0 == get_type) { //开机直接进入回链状态
        return BD_CONN_MODE;
    } else { //从vm中获取对箱上次是做主机还是做从机
        if (-VM_READ_NO_INDEX == vm_read_api(VM_BT_STEREO_INFO, &stereo_info)) {
            return BD_CONN_MODE;
        }
        printf("-----is_stereo_salve=0x%x\n", stereo_info);
        if (0xaa == stereo_info) { //从机
            return BD_PAGE_MODE;
        }
    }
    return BD_CONN_MODE;
}
void bt_stereo_poweroff_togerther()
{
#if BT_TWS
#if BT_TWS_POWEROFF_TOGETHER
    if (is_check_stereo_slave()) {
        stereo_slave_cmd_handle(MSG_POWER_OFF, 0xff);
    } else {
        stereo_host_cmd_handle(MSG_POWER_OFF, 0);
    }
    os_time_dly(5);
#endif
#endif

}
void bt_tws_delete_addr()
{
#if BT_TWS
    delete_stereo_addr();
    if (is_check_stereo_slave()) {
        stereo_slave_cmd_handle(MSG_BT_TWS_DELETE_ADDR, 0);
    } else {
        stereo_host_cmd_handle(MSG_BT_TWS_DELETE_ADDR, 0);
    }
#endif

}

static OS_MUTEX spp_mutex;
int spp_mutex_init(void)
{
    return os_mutex_create(&spp_mutex);
}
static inline int spp_mutex_enter(void)
{
    os_mutex_pend(&spp_mutex, 0);
    return 0;
}
static inline int spp_mutex_exit(void)
{
    return os_mutex_post(&spp_mutex);
}
int spp_mutex_del(void)
{
    return os_mutex_del(&spp_mutex, OS_DEL_ALWAYS);
}

/*发送串口数据的接口，发送完再返回*/
u32 bt_spp_send_data(u8 *data, u16 len)
{
    u32 err;
    spp_mutex_enter();
    err = user_send_cmd_prepare(USER_CTRL_SPP_SEND_DATA, len, data);
    spp_mutex_exit();
    return err;
}

void bt_spp_disconnect(void)
{
    spp_mutex_enter();
    user_send_cmd_prepare(USER_CTRL_SPP_DISCONNECT, 0, 0);
    spp_mutex_exit();
}

static u8 page_scan_disable = 0;
void bt_disconnect_page_scan_disable(u8 flag)
{
    page_scan_disable = flag;
}

/*开关可发现可连接的函数接口*/
static void bt_page_scan(u8 enble)
{
    if (enble) {
        if (page_scan_disable) {
            printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
            return ;
        }
        if (!is_1t2_connection()) {
            if (enble == 3) {
                user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_DISABLE, 0, NULL);
                user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE_KEY, 0, NULL);
            } else if (enble == 2) {
                user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_ENABLE_KEY, 0, NULL);
                user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE_KEY, 0, NULL);
            } else {
                user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_ENABLE, 0, NULL);
                user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
            }

        }
    } else {
        user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_DISABLE, 0, NULL);
        user_send_cmd_prepare(USER_CTRL_WRITE_CONN_DISABLE, 0, NULL);
    }
}

/*协议栈状态变化用户处理接口*/
static void btstack_status_change_deal(void *ptr, u8 status)
{
    int msg_echo_start = MSG_ECHO_START;
    update_bt_prev_status(get_bt_newest_status());
    printf("status %d %2x\n", status, get_resume_flag());
    while (get_resume_flag()) { //bt stack is require resume,wait
        os_time_dly(1);
    }
    switch (status) {

    case BT_STATUS_RESUME:
        user_ctrl_prompt_tone_play(BT_STATUS_RESUME, NULL);
        led_fre_set(5, 0);
        break;
    case BT_STATUS_RESUME_BTSTACK:
        //协议栈要求回
#if BT_PHONE_NUMBER
        if (user_val->is_phone_number_come) {
            update_bt_current_status(NULL, BT_STATUS_PHONE_NUMBER, 0);
        }
#endif
        puts("stack back to bluetooth\n");
        break;
    case BT_STATUS_POWER_ON:
        user_ctrl_prompt_tone_play(BT_STATUS_POWER_ON, NULL);
        led_fre_set(5, 0);
        break;
    case BT_STATUS_INIT_OK:
#if NFC_EN
        /*nfc_init(NFC_BLE_OOB);*/
        nfc_init(NFC_BREDR_OOB);
        if (NFC_BREDR_OOB == get_NFC_mode()) {
            puts("NFC bredr mode\n");
        } else if (NFC_BLE_OOB == get_NFC_mode()) {
            puts("NFC ble mode\n");
        }
#if NFC_INFROM_EN
        //参数说明 开关、读完成消息返回任务名、读完返回消息、计数清0时长*2ms(建议100~250)
        if (NFC_BREDR_OOB == get_NFC_mode()) { //ble暂时无法实现该功能
            nfc_read_ok_infrom_init(1, "btmsg", MSG_NFC_READ_OK, 250);
            timer_reg_isr_fun(timer0_hl, 1, nfc_ok_timer_scan, NULL);
        }
#endif
#endif

#if BT_TWS
        bd_work_mode = get_stereo_salve(TWS_SLAVE_WAIT_CON);
#endif
        if (bd_work_mode == BD_PAGE_MODE) { /*初始化完成进入配对模式*/
            led_fre_set(7, 1);
#if BT_TWS
            stereo_clear_current_search_index();
#endif
#if BT_TWS_SCAN_ENBLE
            bt_page_scan(3);
#else
            bt_page_scan(1);
#endif
        } else { /*初始化完成进入回连模式*/
            printf("BT_STATUS_INIT_OK\n");
            /* while(play_sel_busy()) {
            	os_time_dly(1);
            }
            dac_off_control(); */
            bt_power_max(4);
            user_send_cmd_prepare(USER_CTRL_START_CONNECTION, 0, NULL);
            led_fre_set(7, 1);
#if BT_KTV_EN
            /* set_esco_sr(8000); */
            set_esco_sr(16000L);
            echo_msg_deal_api((void **)&bt_reverb, &msg_echo_start);
#endif
        }
        break;
    case BT_STATUS_FIRST_CONNECTED:
        bt_power_max(0xFF);
        //if(is_auto_connect()){}  //可以判断“是回连成功返回1，非回连连接返回0”
        user_ctrl_prompt_tone_play(BT_STATUS_FIRST_CONNECTED, NULL);
        led_fre_set(100, 0);
        break;
    case BT_STATUS_SECOND_CONNECTED:
        bt_power_max(0xFF);
        //if(is_auto_connect()){}  //可以判断“是回连成功返回1，非回连连接返回0”
        user_ctrl_prompt_tone_play(BT_STATUS_SECOND_CONNECTED, NULL);
        led_fre_set(100, 0);
        break;
    case BT_STATUS_FIRST_DISCONNECT:
        //printf("first_disconn:%d\n",get_total_connect_dev());
        user_ctrl_prompt_tone_play(BT_STATUS_FIRST_DISCONNECT, NULL);
        if (get_total_connect_dev() == 0) {
            led_fre_set(7, 1);
        }
        break;
    case BT_STATUS_SECOND_DISCONNECT:
        //printf("second_disconn:%d\n",get_total_connect_dev());
        user_ctrl_prompt_tone_play(BT_STATUS_SECOND_DISCONNECT, NULL);
        if (get_total_connect_dev() == 0) {
            led_fre_set(7, 1);
        }
        break;
    case BT_STATUS_PHONE_INCOME:
        digit_auto_mute_set(0, -1, -1, -1); // 关自动mute
        dac_mute(0, 1);
        user_send_cmd_prepare(USER_CTRL_HFP_CALL_CURRENT, 0, NULL); //发命令获取电话号码
        os_time_dly(5);
        user_ctrl_prompt_tone_play(BT_STATUS_PHONE_INCOME, NULL);
        break;
    case BT_STATUS_PHONE_OUT:
        puts("phone_out\n");
        //dac_mute(0,1);
        break;

#if BT_PHONE_NUMBER
    case BT_STATUS_PHONE_NUMBER:
        user_ctrl_prompt_tone_play(BT_STATUS_PHONE_NUMBER, NULL);
        break;
#endif
    case BT_STATUS_PHONE_ACTIVE:
        user_val->play_phone_number_flag = 0;
        if (user_val->phone_prompt_tone_playing_flag) {
            notice_player_stop();
            user_val->phone_prompt_tone_playing_flag = 0;
        }
        break;
    case BT_STATUS_PHONE_HANGUP:
        digit_auto_mute_set(1, -1, -1, -1); // 开自动mute
        user_val->play_phone_number_flag = 0;
        user_val->is_phone_number_come = 0;
        if (user_val->phone_prompt_tone_playing_flag) {
            notice_player_stop();
            user_val->phone_prompt_tone_playing_flag = 0;
        }
        //后台电话回来，电话完了自动退出
        user_send_cmd_prepare(USER_CTRL_BTSTACK_SUSPEND, 0, NULL);  //go back to background
        break;

    default:
        break;
    }
}

void stereo_led_deal()
{
#if BT_TWS
    if (BT_CURRENT_CONN_PHONE == get_bt_current_conn_type()) {

    } else if (BT_CURRENT_CONN_STEREO_MASTER == get_bt_current_conn_type()) {

    } else if (BT_CURRENT_CONN_STEREO_SALVE == get_bt_current_conn_type()) {

    } else if (BT_CURRENT_CONN_STEREO_MASTER_PHONE == get_bt_current_conn_type()) {

    } else if (BT_CURRENT_CONN_STEREO_PHONE_MASTER == get_bt_current_conn_type()) {

    } else {
    }
#endif

}
#if BLE_DYNAMIC_NAME_EN
extern void set_ble_change_name(u8 flag);
extern u8 get_ble_change_name_flag(void);
#endif

extern void ble_server_disconnect(void);
extern void gatt_service_process_init(void);

#if BREDR_DYNAMIC_NAME_EN
extern void set_bredr_change_name(u8 flag);
extern u8 get_bredr_change_name_flag(void);
extern void send_hci_to_update_name(void);
#endif


extern void set_osc_2_vm(void);
static void btstack_key_handler(void *ptr, int *msg)
{
    static u8 play_st_ctl = 0;
#if BT_TWS
    if (is_check_stereo_slave()) {
        switch (msg[0]) {
        case MSG_VOL_DOWN:
        case MSG_VOL_UP:
        case MSG_BT_PP:
        case MSG_BT_NEXT_FILE:
        case MSG_BT_PREV_FILE:
        case MSG_BT_ANSWER_CALL:
        case MSG_BT_CALL_LAST_NO:
        case MSG_BT_CALL_REJECT:
        case MSG_BT_CALL_CONTROL:
            stereo_slave_cmd_handle(msg[0], 0);
            return;
        default:
            break;
        }
    }
#endif

    switch (msg[0]) {
#if BT_TWS
#if EQ_EN
    case  MSG_BT_SYNC_STEREO_EQ:
        if (user_val->bt_eq_mode == 0) {
            return;
        }
        if (is_check_stereo_slave()) {
            stereo_slave_cmd_handle(MSG_BT_MUSIC_EQ, user_val->bt_eq_mode);
        } else {
            stereo_host_cmd_handle(MSG_BT_MUSIC_EQ, user_val->bt_eq_mode);
        }
        os_taskq_post("btmsg", 2, MSG_BT_MUSIC_EQ, user_val->bt_eq_mode);
        break;
    case  MSG_BT_STEREO_EQ:
        if (is_check_stereo_slave()) {
            stereo_slave_cmd_handle(MSG_BT_MUSIC_EQ, 0);
        } else {
            stereo_host_cmd_handle(MSG_BT_MUSIC_EQ, 0);
        }


        os_taskq_post("btmsg", 1, MSG_BT_MUSIC_EQ);
        break;
#endif

    case MSG_VOL_STEREO://主机操作按键调音量时同步从机
        /* puts("bt MSG_VOL_STEREO\n"); */
        stereo_host_cmd_handle(msg[0], msg[1]);
        break;
#endif
    case MSG_BT_PP:
        puts("MSG_BT_PP\n");
        if (msg[1] != 0) {
            play_st_ctl = (u8)msg[1];
            break;
        }

        printf("call_status:%d\n", get_call_status());
        if ((get_call_status() == BT_CALL_OUTGOING) ||
            (get_call_status() == BT_CALL_ALERT)) {
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
        } else if (get_call_status() == BT_CALL_INCOMING) {
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_ANSWER, 0, NULL);
        } else if (get_call_status() == BT_CALL_ACTIVE) {
            user_send_cmd_prepare(USER_CTRL_SCO_LINK, 0, NULL);
        } else {
            user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
        }
        break;
#if NFC_INFROM_EN
    case MSG_NFC_READ_OK:
        puts("NFC read ok!!\n");
        if (NFC_BREDR_OOB == get_NFC_mode()) {
            if ((BT_STATUS_CONNECTING == get_bt_connect_status())   ||
                (BT_STATUS_TAKEING_PHONE == get_bt_connect_status()) ||
                (BT_STATUS_PLAYING_MUSIC == get_bt_connect_status())) { /*连接状态*/
                puts("nfc bt_disconnect\n");/*手动断开连接*/
                user_send_cmd_prepare(USER_CTRL_DISCONNECTION_HCI, 0, NULL);
            }
            puts("nfc bredr disconnet\n");
        }
        //else if(NFC_BLE_OOB==get_NFC_mode())
        //{
        //ble_server_disconnect();//ble停止连接
        //puts("nfc ble disconnet\n");
        //}
        break;
#endif

    case MSG_BT_NEXT_FILE:
        puts("MSG_BT_NEXT_FILE\n");
        if (get_call_status() == BT_CALL_ACTIVE) {
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_VOLUME_UP, 0, NULL);
        } else {
            user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_NEXT, 0, NULL);
        }

        break;

    case MSG_BT_PREV_FILE:
        puts("MSG_BT_PREV_FILE\n");
        if (get_call_status() == BT_CALL_ACTIVE) {
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_VOLUME_DOWN, 0, NULL);
        } else {
            user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PREV, 0, NULL);
        }
        break;

#if EQ_EN
    case MSG_BT_MUSIC_EQ:
        set_bt_eq_mode(user_val->bt_eq_mode + 1);
        printf("MSG_BT_EQ %d\n", user_val->bt_eq_mode);

        break;
#endif // EQ_EN

    case MSG_BT_PAGE_SCAN:
        puts("MSG_BT_PAGE_SCAN\n");
        bt_page_scan(2);
        break;
#if BT_TWS
    case MSG_BT_STEREO_SEARCH_DEVICE:
        puts("MSG_BT_STEREO_SEARCH_DEVICE\n");
        if (fast_test) {
            puts("fast_test MSG_BT_STEREO_SEARCH_DEVICE\n");
            break;
        }
        if (BT_TWS_CHANNEL_SELECT && BT_TWS_CHANNEL_SELECT == TWS_CHANNEL_RIGHT) {
            puts(" TWS_CHANNEL_RIGHT not search\n");
            break;
        }
        user_send_cmd_prepare(USER_CTRL_SEARCH_DEVICE, 0, NULL);
#if BT_TWS_SCAN_ENBLE
        if (!is_check_stereo_slave()) {
            bt_page_scan(2);
        }
#endif
        break;
    case MSG_BT_TWS_DELETE_ADDR:
        puts("MSG_BT_TWS_DELETE_ADDR\n");
        delete_stereo_addr();
        break;
#endif
    case MSG_BT_ANSWER_CALL:
        puts("MSG_BT_ANSWER_CALL\n");
        if (get_call_status() == BT_CALL_INCOMING) {
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_ANSWER, 0, NULL);
        }
        break;

    case MSG_BT_CALL_LAST_NO:
        puts("MSG_BT_CALL_LAST_NO\n");
        if (get_call_status() != BT_CALL_HANGUP) {
            break;
        }
        if (get_last_call_type() == BT_CALL_OUTGOING) {
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_LAST_NO, 0, NULL);
        } else if (get_last_call_type() == BT_CALL_INCOMING) {
            user_send_cmd_prepare(USER_CTRL_DIAL_NUMBER, user_val->phone_num_len,
                                  user_val->income_phone_num);
        }
        break;

    case MSG_BT_CALL_REJECT:
        puts("MSG_BT_CALL_REJECT\n");
        if (get_call_status() != BT_CALL_HANGUP) {
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
        }
        break;

    case MSG_BT_CALL_CONTROL:   //物理按键少的时候合用
        if (get_call_status() != BT_CALL_HANGUP) {
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
        } else {
            if (get_last_call_type() == BT_CALL_OUTGOING) {
                user_send_cmd_prepare(USER_CTRL_HFP_CALL_LAST_NO, 0, NULL);
            } else if (get_last_call_type() == BT_CALL_INCOMING) {
                user_send_cmd_prepare(USER_CTRL_DIAL_NUMBER, user_val->phone_num_len,
                                      user_val->income_phone_num);
            }
        }
        break;

    case MSG_BT_CONNECT_CTL:
        puts("MSG_BT_CONNECT_CTL\n");
        if ((BT_STATUS_CONNECTING == get_bt_connect_status())   ||
            (BT_STATUS_TAKEING_PHONE == get_bt_connect_status()) ||
            (BT_STATUS_PLAYING_MUSIC == get_bt_connect_status())) { /*连接状态*/
            puts("bt_disconnect\n");/*手动断开连接*/
            user_send_cmd_prepare(USER_CTRL_DISCONNECTION_HCI, 0, NULL);
        } else {
            puts("bt_connect\n");/*手动连接*/
            user_send_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR_MANUALLY, 0, NULL);
        }
        break;

#if BT_TWS
    case MSG_BT_CONNECT_STEREO_CTL:
        puts("MSG_BT_CONNECT_STEREO_CTL\n");
        if ((BT_STATUS_CONNECTING == get_stereo_bt_connect_status())   ||
            (BT_STATUS_TAKEING_PHONE == get_stereo_bt_connect_status()) ||
            (BT_STATUS_PLAYING_MUSIC == get_stereo_bt_connect_status())) { /*连接状态*/
            puts("bt_disconnect\n");/*手动对箱断开连接*/
            user_send_cmd_prepare(USER_CTRL_DISCONNECTIO_STEREO_HCI, 0, NULL);
        } else {
            puts("bt_connect\n");/*手动对箱连接*/
            user_send_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR_STEREO, 0, NULL);
        }
        break;
#endif
    case MSG_BT_HID_CTRL:
        puts("MSG_BT_HID_CTRL\n");
        if (get_curr_channel_state() & HID_CH) {
            user_send_cmd_prepare(USER_CTRL_HID_DISCONNECT, 0, NULL);
        } else {
            user_send_cmd_prepare(USER_CTRL_HID_CONN, 0, NULL);
        }

        break;
    case MSG_BT_HID_TAKE_PIC:
        puts("MSG_BT_HID_TAKE_PIC\n");
        if (get_curr_channel_state() & HID_CH) {
            user_send_cmd_prepare(USER_CTRL_HID_BOTH, 0, NULL);
        }
        break;

    case MSG_PROMPT_PLAY:
        puts("MSG_PROMPT_PLAY\n");
        bt_prompt_play_by_name(BPF_TEST_MP3, NULL);
        break;

#if BT_DYNAMIC_UPDATE_NAME_EN //注意这里分成了bredr 与 ble（2.1与4.0）
    case MSG_BT_CHANGE_NAME://动态修改蓝牙名字
        puts("MSG_BT_CHANGE_NAME~\n");

#if BREDR_DYNAMIC_NAME_EN//2.1
        if ((BT_STATUS_CONNECTING == get_bt_connect_status())   ||
            (BT_STATUS_TAKEING_PHONE == get_bt_connect_status()) ||
            (BT_STATUS_PLAYING_MUSIC == get_bt_connect_status())) { /*连接状态*/
            puts("bt_disconnect\n");/*手动断开连接*/
            user_send_cmd_prepare(USER_CTRL_DISCONNECTION_HCI, 0, NULL);
        }
        puts("bredr stop  scan\n");
        bt_page_scan(0);

        //设置标志
        if (0 == get_bredr_change_name_flag()
            || 2 == get_bredr_change_name_flag()) {
            set_bredr_change_name(1);
        } else if (1 == get_bredr_change_name_flag()) {
            set_bredr_change_name(2);
        }
#endif

#if BLE_DYNAMIC_NAME_EN//4.0
        puts("ble stop advertise\n");
        ble_server_disconnect();//ble停止连接
        server_advertise_disable();//ble停止广播

        //设置标志
        if (0 == get_ble_change_name_flag()
            || 2 == get_ble_change_name_flag()) {
            set_ble_change_name(1);
        } else if (1 == get_ble_change_name_flag()) {
            set_ble_change_name(2);
        }
#endif
        //重新初始化参数
        puts("bt info init again\n");
        bt_info_init();

#if BREDR_DYNAMIC_NAME_EN//2.1
        send_hci_to_update_name();
#endif

#if BLE_DYNAMIC_NAME_EN//4.0
        gatt_service_process_init();
#endif
        //重开搜索或广播
        puts("start scan\n");
#if BREDR_DYNAMIC_NAME_EN //2.1
        bt_page_scan(1);
#endif
#if BLE_DYNAMIC_NAME_EN  //4.0
        server_advertise_enable();//ble重开广播
#endif
        break;
#endif

        /***************************************************************************************/
        /*******************************k歌宝demo事例*******************************************/
        /***************************************************************************************/
#if BT_KTV_EN
///k歌宝外挂flash录音测试， 这里录音开始， 录音结束与播放处理
    case MSG_KTV_SPI_REC:
        if (spi_rec_flag == 0) {
            notice_player_stop();
            spirec_api_dac();
            /* spirec_api_ladc(ENC_MIC_CHANNEL); */
        } else {
            tbool err;
            spirec_api_dac_stop();
            /* spirec_api_ladc_stop(); */
            err = spirec_api_play_to_mix("btmsg", spirec_filenum, sound_mix_api_get_obj(), NULL, NULL);
            if (err != true) {
                printf("spirec_api_play fail or break\n");
            } else {
                printf("spirec_api_play ok\n");
            }
        }
        spi_rec_flag = !spi_rec_flag;
        break;

///以下消息为测试蓝牙在播放歌曲或者通话过程中叠加提示音测试
///注意由于资源有限，蓝牙播放过程中叠加音频只能叠加没有压缩的wav, 即可以通过调用wav_play接口播放的音源
    case MSG_KTV_NOTICE_ADD_PLAY_TEST:
        puts("MSG_BT_PP\n");
        wav_play_by_path_to_mix("btmsg", "/test.wav", NULL, NULL, sound_mix_api_get_obj());
        break;
///提示播放结束处理， 录音播放结束也是走这个分支
    case SYS_EVENT_PLAY_SEL_END:
        printf("add mix notice end !!\n");
        notice_player_stop();
        break;
#endif//BT_KTV_EN
        /***************************************************************************************/

#if BT_REC_EN
//    case MSG_REC_PLAY:
//        if (notice_player_get_status() != 0) {
//            notice_player_stop();
//        } else {
//            exit_rec_api(&rec_bt_api);//exit rec when esco change
//            puts("doesn't support bt record replay !!\n");
//            /* bt_prompt_play_by_name(rec_file_name_last, (void *)rec_let_last); */
//        }
//        break;
#endif

    case MSG_POWER_OFF:
        puts("bt_power_off\n");
        if ((BT_STATUS_CONNECTING == get_bt_connect_status())   ||
            (BT_STATUS_TAKEING_PHONE == get_bt_connect_status()) ||
            (BT_STATUS_PLAYING_MUSIC == get_bt_connect_status())) {
            puts("bt_disconnect\n");/*连接状态下先断开连接再关机*/
            user_send_cmd_prepare(USER_CTRL_POWER_OFF, 0, NULL);
        }

        /*关机提示音*/
//            user_ctrl_prompt_tone_play(BT_STATUS_POWER_OFF,NULL);
//            do{
//                os_time_dly(1);
//            }while(play_sel_busy());
//            puts("power_off_tone_end\n");
        break;

#if LCD_SUPPORT_MENU
    case MSG_ENTER_MENULIST:
        if (get_bt_connect_status() == BT_STATUS_TAKEING_PHONE) {
            UI_menu_arg(MENU_LIST_DISPLAY, UI_MENU_LIST_ITEM | (1 << 8));
        } else {
            UI_menu_arg(MENU_LIST_DISPLAY, UI_MENU_LIST_ITEM);
        }
        break;
#endif

    case MSG_BT_SPP_UPDATA:
        puts("MSG_BT_SPP_UPDATA\n");
        user_send_cmd_prepare(USER_CTRL_SPP_UPDATA_DATA, 0, NULL);
        user_val->wait_app_updata_flag = 1;
        break;
    case MSG_HALF_SECOND:
        //puts(" BT_H ");


       //printf("scl_cont = %d,sda_cont = %d\n",scl_cont,sda_cont);
        stereo_led_deal();
        if (fast_test) {
            user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_DISABLE, 0, NULL);
            user_send_cmd_prepare(USER_CTRL_WRITE_CONN_DISABLE, 0, NULL);

        }
#if BT_TWS
#if (BT_TWS_SCAN_ENBLE==0)
        if (0 == get_total_connect_dev()) {
            if (user_val->discover_control_cnt == 1) {
                user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_ENABLE, 0, NULL);
            } else if (user_val->discover_control_cnt == 15) {
                user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_DISABLE, 0, NULL);
            }
            user_val->discover_control_cnt++;
            if (user_val->discover_control_cnt >= 20) {
                user_val->discover_control_cnt = 1;
            }
        }
#endif
#endif

        if ((BT_STATUS_CONNECTING == get_bt_connect_status())   ||
            (BT_STATUS_TAKEING_PHONE == get_bt_connect_status()) ||
            (BT_STATUS_PLAYING_MUSIC == get_bt_connect_status())) { /*连接状态*/
            if (BT_MUSIC_STATUS_STARTING == a2dp_get_status()) {    /*播歌状态*/
                //puts("BT_MUSIC\n");
                //led_fre_set(15,0);                                /*播歌慢闪*/
                play_st_ctl = 0;
                user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_GET_PLAY_TIME, 0, NULL);
            } else if (BT_STATUS_TAKEING_PHONE == get_bt_connect_status()) {
                ///puts("bt_phone\n");
            } else {

                ///puts("bt_other\n");
                play_st_ctl = 0;
                //puts("BT_CONN\n");
                //led_fre_set(0,0);                                 /*暂停常亮*/
            }
            user_val->auto_shutdown_cnt = AUTO_SHUT_DOWN_TIME;
        } else  if (BT_STATUS_WAITINT_CONN == get_bt_connect_status() && user_val->auto_shutdown_cnt) {
            //puts("BT_STATUS_WAITINT_CONN\n");
            user_val->auto_shutdown_cnt--;
            //printf("power cnt:%d\n",user_val->auto_shutdown_cnt);
            if (user_val->auto_shutdown_cnt == 0) {
                //软关机
                puts("*****shut down*****\n");
                going_to_pwr_off = 1;
                os_taskq_post("MainTask", 2, MSG_POWER_OFF_KEY_MSG, 0x44);
            }
        }
#if (SNIFF_MODE_CONF&SNIFF_EN)
        if (check_in_sniff_mode()) {
            printf("check_in_sniff_mode ok\n");
            vm_check_all(0);
            user_send_cmd_prepare(USER_CTRL_SNIFF_IN, 0, NULL);
            //user_send_cmd_prepare(USER_CTRL_SNIFF_EXIT,0,NULL);
        }
#endif

        if ((bt_ui_var.ui_bt_connect_sta != get_bt_connect_status()) ||
            (bt_ui_var.ui_bt_a2dp_sta != a2dp_get_status())) {
            bt_ui_var.ui_bt_connect_sta = get_bt_connect_status();
            bt_ui_var.ui_bt_a2dp_sta = a2dp_get_status();
            UI_menu(MENU_REFRESH);
        }
        if (user_val->wait_app_updata_flag) {
            if (check_bt_app_updata_run()) {
                user_val->wait_app_updata_flag = 0;
            }
        }

        calling_del_page();

//           UI_menu(MENU_MAIN);
        UI_menu(MENU_HALF_SEC_REFRESH);
        break;

    case MSG_BT_STACK_STATUS_CHANGE:
        //该消息比较特殊，不属于按键产生，是协议栈状态变化产生。
        puts("MSG_BT_STACK_STATUS_CHANGE\n");
        btstack_status_change_deal(NULL, msg[1]);
        break;
    case SYS_EVENT_DEL_TASK:
        puts("SYS_EVENT_DEL_TASK\n");
        //断开和关闭协议栈
        user_send_cmd_prepare(USER_CTRL_POWER_OFF, 0, NULL);
        //删除协议栈
        delete_stack_task();
        puts("del stack ok\n");
        notice_player_stop();
        if (dac_ctl.toggle == 0) {
            dac_on_control();
        }
        //删除消息线程
        if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
            puts("del_ket_masg");
            echo_exit_api((void **)&bt_reverb);
            os_task_del_res_name(OS_TASK_SELF); 	//确认可删除，此函数不再返回
        }
        break;
    case MSG_BT_FAST_TEST:
        puts("MSG_BT_FAST_TEST \n");
        set_sys_vol(26, 26, FADE_ON);
        digit_auto_mute_set(0, -1, -1, -1);
        microphone_open(60, 1);
        fast_test = 1;
        break;

#if BT_BLE_WECHAT
    case MSG_BT_WECHAT_AUTH_INIT:

        if (msg[1] == 0x01) {
            wechat_auth_fun();
        } else if (msg[1] == 0x02) {
            wechat_init_fun();
        }
        break;

    case MSG_BT_WECHAT_SEND_DATA:
        wechat_send_fun();
        break;

#endif

    case MSG_OSC_INTER_CAP_WRITE:
        puts("MSG_OSC_INTER_CAP_WRITE\n");
        set_osc_2_vm();
        break;
    default:
        if (get_call_status() != BT_CALL_HANGUP) {
            /* puts("phone call ignore echo msg\n"); */
        } else {
            /* puts("bt_ctl_echo\n"); */
            echo_msg_deal_api((void **)&bt_reverb, msg);
        }
        break;
    }
}
#if BT_REC_EN
void bt_rec_msg_deal(int *msg)
{
#if BT_KTV_EN
    /* printf("get_bt_connect_status() = 0x%x\n", get_bt_connect_status()); */
    if (msg[0] == MSG_REC_START) {
        if ((BT_STATUS_TAKEING_PHONE == get_bt_connect_status())
            || (BT_STATUS_PLAYING_MUSIC == get_bt_connect_status())) {
            if (!notice_player_get_status()) { //play_sel is not BUSY
                rec_msg_deal_api(&rec_bt_api, msg);
            }
        }
    } else {
        rec_msg_deal_api(&rec_bt_api, msg);
    }
#else
    if ((BT_STATUS_TAKEING_PHONE != get_bt_connect_status()) && (msg[0] == MSG_REC_START))
        if (!get_sco_connect_status() && (msg[0] == MSG_REC_START)) {
            return;
        }
    rec_msg_deal_api(&rec_bt_api, msg);
#endif
}
#endif
/*消息处理*/
void TaskBtMsgStack(void *p_arg)
{
    //p_arg = p_arg;
    key_msg_reg("btmsg", &script_bt_key);
    //key_msg_register("btmsg",bt_ad_table,bt_io_table,bt_ir_table,NULL);
    os_sem_create(&tone_manage_semp, 1);

    while (1) {
        int msg[6];
        u32 res;
        memset(msg, 0x00, sizeof(msg)); ///need do it
        res = os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();
        if (res != OS_NO_ERR) {
            msg[0] = 0xff;
        }

#if SUPPORT_APP_RCSP_EN
        rcsp_bt_task_msg_deal_before(msg);
#endif
        //printf("bt_msg:%d",msg[0]);
        btstack_key_handler(NULL, msg);

#if BT_REC_EN
        /* bt_rec_msg_deal(msg); */
#endif
#if SUPPORT_APP_RCSP_EN
        rcsp_bt_task_msg_deal_after(msg);
#endif
    }

}

/*主要处理上电或者有些情况蓝牙处于不能切换模式状态*/
/*不属于用户接口，协议栈回调函数*/
extern int is_bt_stack_cannot_exist(void);
int msg_mask_off_in_bt()
{
#if BT_BACKMODE
    return is_bt_stack_cannot_exist();
#else
    return false;
#endif
}

/*通过任务切换进入蓝牙时回调接口*/
/*不属于用户接口，协议栈回调函数*/
void enter_btstack_task()
{
    puts("\n************************BLUETOOTH TASK********************\n");
    key_msg_reg("btmsg", &script_bt_key);
    //key_msg_register("btmsg",bt_ad_table,bt_io_table,bt_ir_table,NULL);
    /* spp_mutex_init(); */
    user_val->auto_shutdown_cnt = AUTO_SHUT_DOWN_TIME;
    dac_channel_on(BT_CHANNEL, FADE_ON);

    user_ctrl_prompt_tone_play(BT_STATUS_POWER_ON, NULL);
    printf("-------led-1\n");
    led_fre_set(7, 1);

    bt_ui_var.bt_eq_mode = &(user_val->bt_eq_mode);
    bt_ui_var.ui_bt_connect_sta = get_bt_connect_status();
    bt_ui_var.ui_bt_a2dp_sta = a2dp_get_status();
    ui_open_bt(&bt_ui_var, sizeof(BT_DIS_VAR));

#if EQ_EN
    eq_enable();
    user_val->bt_eq_mode = get_eq_default_mode();
#endif // EQ_EN

    SET_UI_MAIN(MENU_BT_MAIN);
    UI_menu(MENU_BT_MAIN);
}

/*不属于用户接口，协议栈回调函数*/
/*通过任务切换退出蓝牙时回调接口*/
void exist_btstack_task()
{
    user_val->play_phone_number_flag = 0;
    user_val->is_phone_number_come = 0;
    if (user_val->phone_prompt_tone_playing_flag) {
        notice_player_stop();
        user_val->phone_prompt_tone_playing_flag = 0;
    }
    puts("----exit bt task---\n");
    /* spp_mutex_del(); */
    dac_channel_off(BT_CHANNEL, FADE_ON);
    ui_close_bt();

#if EQ_EN
    eq_disable();
#endif // EQ_EN

    SET_UI_MAIN(MENU_WAIT);
    UI_menu(MENU_WAIT);

#if BT_BACKMODE
    background_suspend();
#else
#if NFC_EN
    nfc_close();
#endif
    no_background_suspend();
#endif

    echo_exit_api((void **)&bt_reverb);

    spirec_api_dac_stop();
    /* exit_rec_api(&rec_bt_api);//exit rec when esco change */
}
extern void sys_time_auto_connection();
void sys_time_auto_connection_caback(u8 *addr)
{
    if (user_val->auto_connection_counter && get_bt_prev_status() != BT_STATUS_SUSPEND) {
        bt_page_scan(0);
        printf("auto_conn_cnt1:%d\n", user_val->auto_connection_counter);
        user_val->auto_connection_counter--;
        user_send_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR, 6, addr);
    }
}
/*不属于用户接口，协议栈回调函数*/
void bt_discon_complete_handle(u8 *addr, int reason)
{
    printf("bt_discon_complete:0x%x\n", reason);
    if (reason == 0 || reason == 0x40) {
        //连接成功
#if BT_TWS
        if ((user_val->auto_connection_stereo) && reason == 0x40) {
            puts("-----------not clean auto_connection_stereo1\n");
        } else {
            user_val->auto_connection_counter = 0;
            user_val->auto_connection_stereo = 0;
        }
#else
        user_val->auto_connection_counter = 0;
#endif
        return ;
    } else if (reason == 0xfc) {
        //新程序没有记忆地址是无法发起回连
        puts("vm no bd_addr\n");
        bt_page_scan(1);
        return ;
    }
#if BT_TWS
    else if (reason == 0x10 || reason == 0xf || reason == 0x13 || reason == 0x14) {
        puts("conneciton accept timeout\n");
        bt_page_scan(1);
        return ;
    } else if (reason == 0x09) {
        puts("bt stereo search device timeout\n");
        return ;
    }
#else
    else if ((reason == 0x10) || (reason == 0xf)) {
        puts("conneciton accept timeout\n");
        bt_page_scan(1);
        return ;
    }
#endif

#if BT_TWS
    clear_a2dP_sink_addr(addr);
#endif

    if (reason == 0x16) {
        puts("Conn Terminated by Local Host\n");
#if BT_TWS
        if (get_current_search_index() >= 1) {
            //继续搜索下一个设备
            user_send_cmd_prepare(USER_CTRL_START_CONNECTION, 0, NULL);
        } else {
            bt_page_scan(1);
        }
#else
        bt_page_scan(1);
#endif
        vm_check_all(0);
    } else if (reason == 0x08) {
        puts("\nconntime timeout\n");
        if (!get_remote_test_flag()) {

#if BT_TWS
            if (get_bt_prev_status() != BT_STATUS_SUSPEND &&
                (get_call_status() == BT_CALL_HANGUP) &&
                (BD_CONN_MODE == get_stereo_salve(1)))
#else
            if (get_bt_prev_status() != BT_STATUS_SUSPEND &&
                (get_call_status() == BT_CALL_HANGUP))
#endif
            {
                user_val->auto_connection_counter = 6;
                puts("\nsuper timeout\n");
#if BT_TWS
                if (BT_STATUS_STEREO_WAITING_CONN == get_stereo_bt_connect_status()) { ///手机连接掉线，回连手机之后继续回连对箱
                    puts("\nafter auto_connection_stereo\n");
                    after_auto_connection_stereo(1, 0);
                    user_send_cmd_prepare(USER_DEL_PAGE_STEREO_HCI, 0, NULL);
                    user_val->auto_connection_stereo = 1;
                    user_val->auto_connection_counter = 4;
                }
#endif
                user_send_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR, 6, addr);
            }
        } else {
            user_val->auto_connection_counter = 0;
            bt_page_scan(1);
        }
    } else if (reason == 0x04) {
        if (! user_val->auto_connection_counter) {
            puts("page timeout---\n");
            if (get_current_search_index() >= 1) {
                //继续搜索下一个设备
                user_send_cmd_prepare(USER_CTRL_START_CONNECTION, 0, NULL);
            } else {
#if BT_TWS
                if (user_val->auto_connection_stereo) {
                    user_val->auto_connection_stereo = 0;
                    puts("-----------clean auto_connection_stereo2\n");
                    if ((BT_STATUS_WAITINT_CONN == get_stereo_bt_connect_status())) {
                        puts("\nstart2 auto_connection_stereo\n");
                        bt_page_scan(1);
                        after_auto_connection_stereo(0, 1);
                        return;
                    }
                }
                user_send_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR_STEREO, 0, NULL);
#endif
                bt_page_scan(1);
                bt_power_max(0xFF);
            }
        } else {
            if (get_bt_prev_status() != BT_STATUS_SUSPEND) {
                printf("auto_conn_cnt:%d\n", user_val->auto_connection_counter);
                user_val->auto_connection_counter--;
                if (user_val->auto_connection_counter % 2) {
                    bt_page_scan(1);
                    sys_time_auto_connection(addr);
                } else {
                    bt_page_scan(0);
                    user_send_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR, 6, addr);
                }
            }
        }
    } else if (reason == 0x0b) {
        puts("Connection Exist\n");
        user_send_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR, 6, addr);
    } else if (reason == 0x06) {
        //connect continue after link missing
        //user_send_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR,6,addr);
    }
}
