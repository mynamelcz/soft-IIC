#ifndef __MIDI_PROG_TAB_H__
#define __MIDI_PROG_TAB_H__
#include "sdk_cfg.h"
enum {
    MIDI_PROG_PIANO = 0x0,
    MIDI_PROG_GUITAR,
    MIDI_PROG_SOPRANO_SAX,
    MIDI_PROG_ORGAN,
    MIDI_PROG_VIOLIN,
    MIDI_PROG_TRUMPET,
    MIDI_PROG_SAWTOOTH,
    MIDI_PROG_FLUTE,
    MIDI_PROG_MUSIC_BOX,
    MIDI_PROG_MAX,
};

const u8 prog_list[MIDI_PROG_MAX];
const u16 prog_ex_vol_list[MIDI_PROG_MAX];



#endif//__MIDI_PROG_TAB_H__
