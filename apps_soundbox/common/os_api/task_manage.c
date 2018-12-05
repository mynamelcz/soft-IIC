#include "sdk_cfg.h"
#include "rtos/task_manage.h"
#include "rtos/os_api.h"
#include "common/msg.h"
#include "linein/linein.h"
#include "usb_device/usb_device.h"
#include "fm/fm_radio.h"
#include "music/music.h"
#include "file_operate/file_op.h"
#include "dev_pc.h"
#include "linein/dev_linein.h"
#include "idle/idle.h"
#include "common/common.h"
#include "echo/echo.h"
#include "vm/vm.h"
#include "rcsp/rcsp_interface.h"

#if REC_EN
#include "encode/record.h"
#include "encode/encode.h"
#endif


const struct task_dev_info music_task_2_dev = {
    .name = "MusicTask",
    .dev_check = file_operate_get_total_phydev,
};

const struct task_dev_info record_task_2_dev = {
    .name = "RECTask",
    .dev_check = file_operate_get_total_phydev,
};

#if AUX_DETECT_EN
const struct task_dev_info linein_task_2_dev = {
    .name = "LineinTask",
    .dev_check = aux_is_online,
};
#endif/*AUX_DETECT_EN*/


u32 app_usb_slave_online_status_null(void)
{
    return 0;
}



#if USB_PC_EN
const struct task_dev_info pc_task_2_dev = {
    .name = "USBdevTask",
    .dev_check = app_usb_slave_online_status,
};
#endif/*USB_PC_EN*/

const struct task_dev_info *task_connect_dev[] AT(.task_info);
const struct task_dev_info *task_connect_dev[] = {
    &music_task_2_dev,

#if AUX_DETECT_EN
    &linein_task_2_dev,
#endif/*AUX_DETECT_EN*/

#if USB_PC_EN
    &pc_task_2_dev,
#endif/*USB_PC_EN*/

#if REC_EN
    &record_task_2_dev,
#endif/*REC_EN*/

};
const int task_connect_dev_valsize = sizeof(task_connect_dev);



