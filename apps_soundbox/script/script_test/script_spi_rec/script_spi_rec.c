#include "script_spi_rec.h"
#include "common/app_cfg.h"
#include "common/msg.h"
#include "script_spi_rec_key.h"
#include "notice_player.h"
#include "dac/dac_api.h"
#include "cbuf/circular_buf.h"
#include "spi_rec/spi_rec.h"

#define SCRIPT_SPI_REC_DEBUG_ENABLE
#ifdef SCRIPT_SPI_REC_DEBUG_ENABLE
#define script_spi_rec_printf	printf
#else
#define script_spi_rec_printf(...)
#endif


#define MSG_SPI_REC		MSG_DEMO_TEST0
static tbool script_test_spi_rec_play_msg_callback(int msg[], void *priv)
{
    if (msg[0] != MSG_HALF_SECOND) {
        script_spi_rec_printf("test_spi_rec_play_msg = %d\n", msg[0]);
    }
    ///this is an example
//	if(msg[0] == MSG_MUSIC_PREV_FILE)
//		return true;
    return false;
}

static void *script_spi_rec_init(void *priv)
{
    dac_channel_on(MUSIC_CHANNEL, FADE_ON);
    script_spi_rec_printf("script spi_rec init ok\n");

    return (void *)NULL;
}

static void script_spi_rec_exit(void **hdl)
{
    script_spi_rec_printf("script spi_rec exit ok\n");

    dac_channel_off(MUSIC_CHANNEL, FADE_ON);
}

static void script_spi_rec_task(void *parg)
{
    int msg[6] = {0};
    u8 spi_rec_flag = 0;

    script_spi_rec_printf("----------------------spi rec demo test ----------------------\n");
    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();
        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                script_spi_rec_printf("script_test_del \n");
                spirec_api_ladc_stop();
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;

        case MSG_SPI_REC:
            if (spi_rec_flag == 0) {
                spirec_api_ladc(ENC_MIC_CHANNEL);
            } else {
                tbool err;
                spirec_api_ladc_stop();
                err = spirec_api_play(SCRIPT_TASK_NAME, spirec_filenum, script_test_spi_rec_play_msg_callback, NULL);
                if (err != true) {
                    script_spi_rec_printf("spirec_api_play fail or break\n");
                } else {
                    script_spi_rec_printf("spirec_api_play ok\n");
                }
            }
            spi_rec_flag = !spi_rec_flag;
            break;
        default:
            break;
        }
    }
}

const SCRIPT script_spi_rec_info = {
    .skip_check = NULL,
    .init = script_spi_rec_init,
    .exit = script_spi_rec_exit,
    .task = script_spi_rec_task,
    .key_register = &script_spi_rec_key,
};


