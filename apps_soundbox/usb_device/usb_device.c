#include "common/app_cfg.h"
#include "rtos/os_api.h"
#include "rtos/os_cfg.h"
#include "common/msg.h"
#include "rtos/task_manage.h"
#include "usb_device.h"
#include "dac/dac.h"
#include "dac/dac_api.h"
#include "dev_pc.h"
#include "dev_manage/dev_ctl.h"
#include "sys_detect.h"
#include "pc_ui.h"
#include "key_drv/key.h"
#include "rcsp/rcsp_interface.h"
#include "notice_player.h"


#include "spi/spifat0.h"
#include "spi/spifat1.h"

u8 usb_spk_vol_value;
u8 usb_mic_vol_value;

//extern volatile u32 dac_energy_value;
extern u32 usb_device_unmount(void);
extern DAC_CTL dac_ctl;


#if USB_PC_EN

#undef 	NULL
#define NULL	0
/*----------------------------------------------------------------------------*/
/**@brief  PC读卡器 任务
   @param  p：任务间参数传递指针
   @return 无
   @note   static void pc_card_task(void *p)
*/
/*----------------------------------------------------------------------------*/
static void pc_card_task(void *p)
{
    int msg[6];

    pc_puts("--------pc_card_task-----\n");
    while (1) {
        memset(msg, 0x00, sizeof(msg));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);

        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            pc_puts("PCCARD_SYS_EVENT_DEL_TASK\n");
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
#if VR_SPI0_EN
                spifat0_ctrl_close();
#endif
#if VR_SPI1_EN
                spifat1_ctrl_close();
#endif
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;
        }
        while (1) {
            if (0 == app_usb_slave_online_status()) {
                pc_puts("pc_tast_offline\n");
                break; //PC离线
            }
            if (0 == ((card_reader_status *)msg[0])->working) {
                pc_puts("pc_card_notworking\n");
                break;
            }
            ((card_reader_status *)msg[0])->busy = 1;
            app_usb_slave_card_reader(MSG_PC_UPDATA); //读卡器流程
        }
        ((card_reader_status *)msg[0])->busy = 0;
    }
}


/*----------------------------------------------------------------------------*/
/**@brief  PC读卡器 任务创建
   @param  priv：任务间参数传递指针
   @return 无
   @note   static void pc_card_task_init(void *priv)
*/
/*----------------------------------------------------------------------------*/
static tbool pc_card_task_init(void *priv)
{
    u8 ret = os_task_create(pc_card_task,
                            (void *)0,
                            TaskPcCardPrio,
                            10
#if OS_TIME_SLICE_EN > 0
                            , 1
#endif
                            , PC_CARD_TASK_NAME);

    if (ret != OS_NO_ERR) {
        return false;
    }

    return true;
}


/*----------------------------------------------------------------------------*/
/**@brief  PC读卡器 任务删除退出
   @param  无
   @return 无
   @note   static void pc_card_task_exit(void)
*/
/*----------------------------------------------------------------------------*/
static void pc_card_task_exit(void)
{
    if (os_task_del_req(PC_CARD_TASK_NAME) != OS_TASK_NOT_EXIST) {
        puts("send pc SYS_EVENT_DEL_TASK\n");
        os_taskq_post_event(PC_CARD_TASK_NAME, 1, SYS_EVENT_DEL_TASK);
        do {
            OSTimeDly(1);
            putchar('D');
        } while (os_task_del_req(PC_CARD_TASK_NAME) != OS_TASK_NOT_EXIST);
        pc_puts("del_pc_card_task: succ\n");
    }
}


/*----------------------------------------------------------------------------*/
/**@brief  PC 任务资源释放
   @param  susb_var：PC模式申请的变量资源指针
   @return 无
   @note   void close_usb_slave(card_reader_status *card_status)
*/
/*----------------------------------------------------------------------------*/
static void close_usb_slave(card_reader_status *card_status)
{
    card_status->working = 0;
    while (card_status->busy) {
        pc_puts("-busy-");
        os_time_dly(5);
    }
    app_usb_slave_close(); //关闭usb slave模块
    pc_card_task_exit(); //删除读卡器任务
}

static card_reader_status card_status;
tbool usb_pc_init(void)
{
    memset(&card_status, 0x00, sizeof(card_reader_status));
    if (NULL == app_usb_slave_init()) {
        return false;
    }

    if (pc_card_task_init(NULL) != true) {
        return false;
    }
    card_status.busy = 1;
    /*************** for CARD READER ******************/
    card_status.working = 1;
    os_taskq_post_msg(PC_CARD_TASK_NAME, 1, &card_status);
    //dac_set_samplerate(48000, 0); //DAC采样率设置为48K
    return true;
}

void usb_pc_exit(void)
{
    close_usb_slave(&card_status);
}


#undef 	NULL

#endif
