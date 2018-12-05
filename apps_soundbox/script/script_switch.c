#include "script_switch.h"
#include "script_list.h"
#include "common/app_cfg.h"
#include "rcsp/rcsp_interface.h"
#include "vm/vm.h"

/* #define SCRIPT_SWITCH_DEBUG_ENABLE */
#ifdef SCRIPT_SWITCH_DEBUG_ENABLE
#define script_switch_printf    printf
#define script_malloc_status	malloc_stats
#else
#define script_switch_printf(...)
#define script_malloc_status()
#endif

typedef struct __SCRIPT_CTRL {
    void 			*hdl;
    SCRIPT_ID_TYPE   _id;
    SCRIPT_ID_TYPE   _last_id;
    u8				 active;
} SCRIPT_CTR;



static SCRIPT_CTR script_control = {
    .hdl = NULL,
    ._id = SCRIPT_ID_UNACTIVE,
    ._last_id = SCRIPT_ID_UNACTIVE,
    .active = 0,

};


/*----------------------------------------------------------------------------*/
/**@brief 脚本切换函数， 此函数仅允许main task 统一调用
   @param
    target -- 目标脚本
	priv -- 脚本切换需要传递的参数， 该参数将会传递到对应脚本的init中
   @return void
   @note
*/
/*----------------------------------------------------------------------------*/
void script_switch(SCRIPT_ID_TYPE target, void *priv)
{
    SCRIPT *cur_script_info = NULL;
    if (target >= SCRIPT_ID_MAX) {
        if (target == SCRIPT_ID_PREV) {
            if (script_control._id >= SCRIPT_ID_MAX) {
                script_switch_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
                return;
            }
            target = script_control._id;
            if (target) {
                target--;
            } else {
                target = SCRIPT_ID_MAX - 1;
            }
        } else if (target == SCRIPT_ID_NEXT) {
            if (script_control._id >= SCRIPT_ID_MAX) {
                script_switch_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
                return;
            }
            target = script_control._id;
            target++;
            if (target >= SCRIPT_ID_MAX) {
                target = 0;
            }
        } else {
            script_switch_printf("SCRIPT_ID err\n");
            return ;
        }
    }

    if (os_task_del_req(SCRIPT_TASK_NAME) != OS_TASK_NOT_EXIST) {
        os_taskq_post_event(SCRIPT_TASK_NAME, 1, SYS_EVENT_DEL_TASK);
        do {
            OSTimeDly(1);
        } while (os_task_del_req(SCRIPT_TASK_NAME) != OS_TASK_NOT_EXIST);
        script_switch_printf("del SCRIPT_TASK_NAME succ\n");
    }

    if (script_control._id <  SCRIPT_ID_MAX) {
        script_control._last_id = script_control._id;
        cur_script_info = (SCRIPT *)script_list[script_control._id];
        script_switch_printf("script_control._id = %d\n", script_control._id);
        if (cur_script_info->exit) {
            script_control.active = 0;
            cur_script_info->exit(&script_control.hdl);
            script_switch_printf("--------------------------exit-------------------------------\n");
            script_malloc_status();
        }
    }


    cur_script_info = (SCRIPT *)script_list[target];
    /*******make sure must have one no skip mode**********/
    while (1) {
        script_switch_printf("&");
        if (cur_script_info->skip_check) {
            if (cur_script_info->skip_check() == 0) {
                target++;
                if (target >= SCRIPT_ID_MAX) {
                    target = 0;
                }
                cur_script_info = (SCRIPT *)script_list[target];
            } else {
                break;
            }
        } else {
            break;
        }
    }


    if (cur_script_info->init) {
        script_switch_printf("--------------------------init-------------------------------\n");
        script_malloc_status();
        script_control.hdl = cur_script_info->init(priv);
        script_switch_printf("script app init ok!!~~\n");

        if (cur_script_info->task == NULL) {
            script_switch_printf("warnning script app task is null, attention yourself msg control task!!~~\n");
            //	return;
        } else {
            if (os_task_create(cur_script_info->task,
                               (void *)script_control.hdl,
#if (SOUND_MIX_GLOBAL_EN)
                               TaskMIXScriptPrio,
#else
                               TaskScriptPrio,
#endif//SOUND_MIX_GLOBAL_EN;
                               SCRIPT_MSG_MAX_SIZE,
                               SCRIPT_TASK_NAME)) {
                script_control.active = 0;
                if (cur_script_info->exit) {
                    cur_script_info->exit(&script_control.hdl);
                }

                script_switch_printf("script app creat error!!~~\n");
                return;
            }
            if (cur_script_info->key_register) {
                key_msg_reg(SCRIPT_TASK_NAME, cur_script_info->key_register);
            }
        }

        script_control._id = target;
        script_control.active = 1;

        script_switch_printf("script switch ok!!\n");
    } else {
        script_switch_printf("warning script app init is null !!!\n");
    }

    vm_check_all(1);

#if SUPPORT_APP_RCSP_EN
    rcsp_app_start(NULL);
#endif
}





/*----------------------------------------------------------------------------*/
/**@brief 获取当前运行脚本id
   @param void
   @return 当前脚本id
   @note
*/
/*----------------------------------------------------------------------------*/
SCRIPT_ID_TYPE script_get_id(void)
{
    if (script_control.active) {
        return script_control._id;
    } else {
        return SCRIPT_ID_UNACTIVE;
    }
}


/*----------------------------------------------------------------------------*/
/**@brief 获取上一次运行的脚本id
   @param void
   @return 当前脚本id
   @note
*/
/*----------------------------------------------------------------------------*/
SCRIPT_ID_TYPE script_get_last_id(void)
{
    return script_control._last_id;
}

