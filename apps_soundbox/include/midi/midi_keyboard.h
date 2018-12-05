#ifndef __MIDI_KEYBOARD_H__
#define __MIDI_KEYBOARD_H__
#include "typedef.h"
#include "dec/MIDI_DEC_API.h"
#include "sound_mix.h"

typedef struct __MIDI_KEYBOARD MIDI_KEYBOARD;
void __midi_keyboard_set_father_name(MIDI_KEYBOARD *obj, void *father_name);
void __midi_keyboard_set_first_note(MIDI_KEYBOARD *obj, u8 note);
void __midi_keyboard_set_note_total(MIDI_KEYBOARD *obj, u8 total);
void __midi_keyboard_set_notice_note_offset(MIDI_KEYBOARD *obj, u8 note_offset);
void __midi_keyboard_set_notice_channel(MIDI_KEYBOARD *obj, u8 channel);
void __midi_keyboard_set_main_channel(MIDI_KEYBOARD *obj, u8 channel);
void __midi_keyboard_set_on(MIDI_KEYBOARD *obj, u8 index);
void __midi_keyboard_set_off(MIDI_KEYBOARD *obj, u8 index);
void __midi_keyboard_set_channel_prog_on(MIDI_KEYBOARD *obj, u8 prog);
void __midi_keyboard_set_channel_prog_off(MIDI_KEYBOARD *obj);
void __midi_keyboard_set_channel_prog(MIDI_KEYBOARD *obj, u8 prog);
void __midi_keyboard_set_force_val(MIDI_KEYBOARD *obj, u8 val);
void __midi_keyboard_set_noteon_max_nums(MIDI_KEYBOARD *obj, s16 total);
void __midi_keyboard_set_samplerate(MIDI_KEYBOARD *obj, u8 samp);
void __midi_keyboard_set_melody_decay(MIDI_KEYBOARD *obj, u16 val);
tbool  __midi_keyboard_set_output_to_mix(MIDI_KEYBOARD *obj, SOUND_MIX_OBJ *mix_obj, const char *key, u32 mix_buf_len, u8 Threshold, u8 default_vol, u8 max_vol);
void  __midi_keyboard_set_output_to_mix_set_vol(MIDI_KEYBOARD *obj, u8 vol);

MIDI_KEYBOARD *midi_keyboard_creat(void);
void midi_keyboard_destroy(MIDI_KEYBOARD **obj);
tbool midi_keyboard_play(MIDI_KEYBOARD *obj);

#endif// __MIDI_KEYBOARD_H__
