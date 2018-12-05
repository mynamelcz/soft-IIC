#include "script_list.h"
#include "script_bt.h"
#include "script_music.h"
#include "script_htk.h"
#include "script_midi.h"
#include "script_linein.h"
#include "script_rec.h"
#include "script_fm.h"
#include "script_usb.h"
#include "script_ms.h"
#include "sdk_cfg.h"
#include "script_midi_update.h"

#ifdef DEMO_ENABLE
#include "script_midi_keyboard/script_midi_keyboard.h"
#include "script_interact/script_interact.h"
#include "script_notice/script_notice.h"
#include "script_wav_mix/script_wav_mix.h"
#include "script_sound_mix/script_sound_mix.h"
#include "script_spi_rec/script_spi_rec.h"
#include "script_echo_pitch/script_echo_pitch.h"
#endif

/*
 *脚本模式配置说明：
 *在SCRIPT_ID_TYPE中添加对应的SCRIPT_ID_XXX
 *在script_list中添加对应的模式info，[SCRIPT_ID_XXX] = &script_xxx_info,
 *在script目录下创建对应的script_xxx脚本目录，目录文件包括script_xxx.c、script_xxx.h、script_xxx_key.c及script_xxx_key.h
 *实现script_xxx_info中的对应接口，具体info内容参考SCRIPT结构体(主要实现script_xxx_init，script_xxx_exit，script_xxx_task及按键表script_xxx_key)
 *******skip_check判断是否需要进入指定的模式， 比如说像music模式， 当没有设备在线应该跳过该模式
 *******script_xxx_init主要负责模式资源申请，及模式相关的初始化如开dac，显示初始化等， 但是绝不允许长时间等待的操作在改函数中实现，该函数的返回值可以是模式资源控制的的总控制句柄，该返回值会传递到script_xxx_task及script_xxx_exit中，可以实现参数传递及资源释放
 *******script_xxx_exit主要负责模式退出及模式资源释放，在模式切换的过程中会先被执行，先释放之前的模式资源， 再切换到对应的target脚本
 *******script_xxx_task主要负责脚本主控制流程的控制，消息处理等相关操作
 *******script_xxx_key脚本按键表， 每个脚本有独立的按键表配置， 在模式切换的时候会被注册，模式主控制流程script_xxx_task执行时会响应对应的按键消息
 *模式切换script_switch必须在main task中调用，调用的时候指定对应的target脚本， 如果有需要传递参数， 通过priv传递到script_xxx_init中

 * */

const SCRIPT *script_list[SCRIPT_ID_MAX] = {
#if(SCRIPT_BT_EN)
    [SCRIPT_ID_BT]			 = &script_bt_info,
#endif

#if(SCRIPT_MUSIC_EN)
    [SCRIPT_ID_MUSIC]		 = &script_music_info,
#endif

#if(SCRIPT_FM_EN)
    [SCRIPT_ID_FM]		 	 = &script_fm_info,
#endif

#if(SCRIPT_LINEIN_EN)
    [SCRIPT_ID_LINEIN]       = &script_linein_info,
#endif

#if(SCRIPT_USB_EN)
    [SCRIPT_ID_USB]          = &script_usb_info,
#endif

#if(SCRIPT_HTK_EN)
    [SCRIPT_ID_HTK]			 = &script_htk_info,
#endif

#if(SCRIPT_MIDI_EN)
    [SCRIPT_ID_MIDI]		 = &script_midi_info,
#endif

#if (SCRIPT_MIDI_UPDATE_EN)
    [SCRIPT_ID_MIDI_UPDATE] = &script_midi_update_info,
#endif

#if(SCRIPT_REC_EN)
    [SCRIPT_ID_REC]          = &script_rec_info,
#endif

#if(SCRIPT_MS_EN)
    [SCRIPT_ID_MS]           = &script_ms_info,
#endif

#ifdef DEMO_ENABLE
    [SCRIPT_ID_TEST_SEL_MIDI_KEYBOARD] = &script_midi_keyboard_info,
#if (BLE_MULTI_EN == 1)
    [SCRIPT_ID_TEST_SEL_INTERACT] = &script_interact_info,
#endif
    [SCRIPT_ID_TEST_SEL_NOTICE] = &script_notice_info,
    [SCRIPT_ID_TEST_SEL_ECHO_PITCH] = &script_echo_pitch_info,
    [SCRIPT_ID_TEST_SEL_SOUND_MIX] = &script_sound_mix_info,
    [SCRIPT_ID_TEST_SEL_SPI_REC] = &script_spi_rec_info,
    [SCRIPT_ID_TEST_SEL_WAV_MIX] = &script_wav_mix_info,
#endif//DEMO_ENABLE

};

