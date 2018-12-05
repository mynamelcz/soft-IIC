#include "wav_play.h"
#include "sdk_cfg.h"
#include "common/msg.h"
#include "dec/sup_dec_op.h"
#include "dev_manage/dev_ctl.h"
#include "fat/tff.h"
#include "file_operate/file_op.h"
#include "fat/fs_io.h"
#include "fat/tff.h"
#include "common/app_cfg.h"
#include "file_operate/logic_dev_list.h"
#include "dac/dac_api.h"
#include "vm/vm_api.h"
#include "timer.h"
#include "hw_cpu.h"
#include "sdk_cfg.h"
#include "pll1167.h"

#if PLL1167_EN
void rf_task()
{
    int msg[4];
    u8 cmd = 0;
    pll_1167_init0();
    send_rf_cmd(0x33);
    s32 ret;
    ret = timer_reg_isr_fun(timer0_hl, 5, (void *)check_rf, NULL);
    // ret = timer_reg_isr_fun(timer0_hl, 250, (void *)send_cmd, NULL);

    while (1) {
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        switch (msg[0]) {
        case MSG_SEND_CMD:
            puts("send msg \n");
            send_rf_cmd(0x33);
            puts("send msg end\n");
            break;
        case MSG_READ_CMD:
            cmd = RX_packet();
            printf("cmd:%d \n", cmd);
            os_time_dly(10);
            //send_rf_cmd(0x33);
            break;

        default:
            printf("rf task error!\r\n");
            break;
        }
    }
}
void RF_task_init()
{
    os_task_create(rf_task, (void *)0, TaskRfPrio, 32, RF_TASK_NAME);
}
#endif
