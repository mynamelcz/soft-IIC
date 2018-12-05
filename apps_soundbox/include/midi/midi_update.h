#ifndef __MIDI_UPDATE_H__
#define __MIDI_UPDATE_H__
#include "typedef.h"

void midi_update_init(void);
void midi_update_prase(u8 *buf, u16 len);
void midi_update_set_flash_base_addr(u32 addr);
tbool midi_update_to_flash(u8 *buf, u32 len);
void midi_update_stop(void);
void midi_update_disconnect(void);
u32 midi_update_check(void);

#endif// __MIDI_UPDATE_H__
