#include "script_demo.h"
#include "common/app_cfg.h"
#include "common/msg.h"
#include "script_demo_key.h"
#include "notice_player.h"
#include "dac/dac_api.h"
#include "cbuf/circular_buf.h"

///**************************this is a empty script demo*******************************///

#define SCRIPT_DEMO_DEBUG_ENABLE
#ifdef SCRIPT_DEMO_DEBUG_ENABLE
#define script_demo_printf	printf
#else
#define script_demo_printf(...)
#endif


static void *script_demo_init(void *priv)
{
    script_demo_printf("script demo init ok\n");

    return (void *)NULL;
}

static void script_demo_exit(void **hdl)
{
    script_demo_printf("script demo exit ok\n");
}

static void script_demo_task(void *parg)
{
    int msg[6] = {0};

    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();
        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                script_demo_printf("script_demo_del \r");
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;
        default:
            break;
        }
    }
}

const SCRIPT script_demo_info = {
    .skip_check = NULL,
    .init = script_demo_init,
    .exit = script_demo_exit,
    .task = script_demo_task,
    .key_register = &script_demo_key,
};


