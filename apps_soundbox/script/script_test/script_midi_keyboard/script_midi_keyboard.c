#include "script_midi_keyboard.h"
#include "midi_keyboard.h"
#include "common/app_cfg.h"
#include "common/msg.h"
#include "script_midi_keyboard_key.h"
#include "notice_player.h"
#include "dac/dac_api.h"
#include "cbuf/circular_buf.h"

///**************************this is a empty script midi_keyboard*******************************///

#define SCRIPT_MIDI_KEYBOARD_DEBUG_ENABLE
#ifdef SCRIPT_MIDI_KEYBOARD_DEBUG_ENABLE
#define script_midi_keyboard_printf	printf
#else
#define script_midi_keyboard_printf(...)
#endif


static void *script_midi_keyboard_init(void *priv)
{
    script_midi_keyboard_printf("script midi_keyboard init ok\n");
    dac_channel_on(MUSIC_CHANNEL, FADE_ON);
    MIDI_KEYBOARD *obj = midi_keyboard_creat();
    __midi_keyboard_set_first_note(obj, 60);//选定琴键起始键
    return (void *)obj;
}

static void script_midi_keyboard_exit(void **hdl)
{
    script_midi_keyboard_printf("script midi_keyboard exit ok\n");
    midi_keyboard_destroy((MIDI_KEYBOARD **)hdl);

    dac_channel_off(MUSIC_CHANNEL, FADE_ON);
}

static void script_midi_keyboard_task(void *parg)
{
    int msg[6] = {0};
    u8 test_note_flag = 0;
    u8 test_prog_change_flag = 0;
    MIDI_KEYBOARD *obj = (MIDI_KEYBOARD *)parg;
    tbool ret = midi_keyboard_play(obj);
    if (ret == false) {
        os_taskq_post(MAIN_TASK_NAME, 1, MSG_CHANGE_WORKMODE);
    } else {

    }

    while (1) {
        memset((u8 *)msg, 0x0, (sizeof(msg) / sizeof(msg[0])));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        clear_wdt();

        if (ret == false) {
            if (msg[0] != SYS_EVENT_DEL_TASK) {
                continue ;
            }
        }


        switch (msg[0]) {
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                script_midi_keyboard_printf("script_midi_keyboard_del \r");
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;


        case MSG_MUSIC_PP:
            test_prog_change_flag = !test_prog_change_flag;
            //改变乐器类型设置测试
            __midi_keyboard_set_channel_prog(obj, (test_prog_change_flag ? 1 : 0));
            break;

        case MSG_HALF_SECOND:
            test_note_flag = !test_note_flag;
            if (test_note_flag) {
                ///模拟按键按下测试, 以起始音符作为基准
                __midi_keyboard_set_on(obj, 0);
                __midi_keyboard_set_on(obj, 1);
                __midi_keyboard_set_on(obj, 2);
                __midi_keyboard_set_on(obj, 3);
                __midi_keyboard_set_on(obj, 4);
                __midi_keyboard_set_on(obj, 5);
                __midi_keyboard_set_on(obj, 6);
                __midi_keyboard_set_on(obj, 7);
            } else {
                ///模拟按键松开测试, 保证每次按下与松开成对出现
                __midi_keyboard_set_off(obj, 0);
                __midi_keyboard_set_off(obj, 1);
                __midi_keyboard_set_off(obj, 2);
                __midi_keyboard_set_off(obj, 3);
                __midi_keyboard_set_off(obj, 4);
                __midi_keyboard_set_off(obj, 5);
                __midi_keyboard_set_off(obj, 6);
                __midi_keyboard_set_off(obj, 7);
            }

            break;
        default:
            break;
        }
    }
}

const SCRIPT script_midi_keyboard_info = {
    .skip_check = NULL,
    .init = script_midi_keyboard_init,
    .exit = script_midi_keyboard_exit,
    .task = script_midi_keyboard_task,
    .key_register = &script_midi_keyboard_key,
};


