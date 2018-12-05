#ifndef __SCRIPT_LIST_H__
#define __SCRIPT_LIST_H__
#include "sdk_cfg.h"
#include "key_drv/key.h"
#include "rcsp/rcsp_interface.h"

// #define DEMO_ENABLE

typedef enum {
#if(SCRIPT_BT_EN)
    SCRIPT_ID_BT,
#endif
#if(SCRIPT_MUSIC_EN)
    SCRIPT_ID_MUSIC,
#endif
#if(SCRIPT_FM_EN)
    SCRIPT_ID_FM,
#endif
#if(SCRIPT_LINEIN_EN)
    SCRIPT_ID_LINEIN,
#endif
#if(SCRIPT_USB_EN)
    SCRIPT_ID_USB,
#endif
#if(SCRIPT_HTK_EN)
    SCRIPT_ID_HTK,
#endif
#if(SCRIPT_MIDI_EN)
    SCRIPT_ID_MIDI,
#endif
#if (SCRIPT_MIDI_UPDATE_EN)
    SCRIPT_ID_MIDI_UPDATE,
#endif
#if(SCRIPT_REC_EN)
    SCRIPT_ID_REC,
#endif
#if(SCRIPT_MS_EN)
    SCRIPT_ID_MS,
#endif

#ifdef DEMO_ENABLE
    SCRIPT_ID_TEST_SEL_MIDI_KEYBOARD,
#if (BLE_MULTI_EN == 1)
    SCRIPT_ID_TEST_SEL_INTERACT,
#endif//BLE_MULTI_EN
    SCRIPT_ID_TEST_SEL_NOTICE,
    SCRIPT_ID_TEST_SEL_ECHO_PITCH,
    SCRIPT_ID_TEST_SEL_SOUND_MIX,
    SCRIPT_ID_TEST_SEL_SPI_REC,
    SCRIPT_ID_TEST_SEL_WAV_MIX,
#endif//DEMO_ENABLE

    SCRIPT_ID_MAX,
    SCRIPT_ID_PREV,
    SCRIPT_ID_NEXT,
    SCRIPT_ID_UNACTIVE,
} SCRIPT_ID_TYPE;

typedef struct __SCRIPT {
    u32(*skip_check)(void);
    void *(*init)(void *priv);
    void (*exit)(void **hdl);
    void (*task)(void *parg);
    const KEY_REG *key_register;
} SCRIPT;

extern const SCRIPT *script_list[SCRIPT_ID_MAX];

#endif

