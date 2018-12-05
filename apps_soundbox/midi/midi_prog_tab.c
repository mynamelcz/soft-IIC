#include "midi_prog_tab.h"
#include "midi_prog_u.h"


const u8 prog_list[MIDI_PROG_MAX] = {
    [MIDI_PROG_PIANO]            = MIDI_PROG_ACOUSTIC_GRAND_PIANO,
    [MIDI_PROG_GUITAR]           = MIDI_PROG_NYLON_ACOUSTIC_GUITAR,
    [MIDI_PROG_SOPRANO_SAX]      = MIDI_PROG_SOPRANO_SAX_REED,
    [MIDI_PROG_ORGAN]            = MIDI_PROG_HAMMOND_ORGAN,
    [MIDI_PROG_VIOLIN]           = MIDI_PROG_VIOLIN_SOLO_STRINGS,
    [MIDI_PROG_SAWTOOTH]         = MIDI_PROG_SAWTOOTH_LEAD,
    [MIDI_PROG_FLUTE]            = MIDI_PROG_FLUTE_PIPE,
    [MIDI_PROG_MUSIC_BOX]        = MIDI_PROG_MUSIC_BOX_CHROMATIC_PERCUSSION,
};



const u16 prog_ex_vol_list[MIDI_PROG_MAX] = {
    [MIDI_PROG_PIANO]       = 1024,
    [MIDI_PROG_GUITAR]      = 1024,
    [MIDI_PROG_SOPRANO_SAX] = 1024,
    [MIDI_PROG_ORGAN]       = 1024,
    [MIDI_PROG_VIOLIN]      = 1024,
    [MIDI_PROG_SAWTOOTH]    = 1024,
    [MIDI_PROG_FLUTE]       = 1024,
    [MIDI_PROG_MUSIC_BOX]   = 1024,
};