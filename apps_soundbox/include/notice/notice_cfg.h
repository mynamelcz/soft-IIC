#ifndef __NOTICE_CFG_H__
#define __NOTICE_CFG_H__

#define DIR_APPEND(x)					"/voice/"#x

#define BPF_BT_MP3_x              DIR_APPEND(bt.***)
// #define BPF_DIR					voice

#define BPF_MS_MP3              DIR_APPEND(ms.***)
#define BPF_BT_MP3              DIR_APPEND(bt.***)
#define BPF_PC_MP3              DIR_APPEND(pc.***)
#define BPF_RADIO_MP3           DIR_APPEND(radio.***)
#define BPF_LINEIN_MP3          DIR_APPEND(linein.***)
#define BPF_MUSIC_MP3           DIR_APPEND(music.***)
#define BPF_RTC_MP3             DIR_APPEND(rtc.***)
#define BPF_ECHO_MP3            DIR_APPEND(echo.***)
#define BPF_REC_MP3             DIR_APPEND(record.***)
#define BPF_HTK_MP3             DIR_APPEND(htk.***)
#define BPF_MIDI_MP3            DIR_APPEND(midi.***)

#define BPF_HTK_NEED_DO_MORE	DIR_APPEND(htk1.***)
#define BPF_HTK_UNKNOW			DIR_APPEND(htk2.***)
#define BPF_MUSIC_NO_FILE		DIR_APPEND(nofile.***)

#define BPF_WAIT_MP3            DIR_APPEND(wait.***)
#define BPF_POWER_OFF_MP3       DIR_APPEND(power_off.***)
#define BPF_CONNECT_MP3         DIR_APPEND(connect.***)
#define BPF_CONNECT_LEFT_MP3    DIR_APPEND(connect.***)
#define BPF_CONNECT_RIGHT_MP3   DIR_APPEND(connect.***)
#define BPF_CONNECT_ONLY_MP3    DIR_APPEND(connect.***)
#define BPF_DISCONNECT_MP3      DIR_APPEND(disconnect.***)
#define BPF_RING_MP3            DIR_APPEND(ring.***)

#define BPF_0_MP3               DIR_APPEND(0.***)
#define BPF_1_MP3               DIR_APPEND(1.***)
#define BPF_2_MP3               DIR_APPEND(2.***)
#define BPF_3_MP3               DIR_APPEND(3.***)
#define BPF_4_MP3               DIR_APPEND(4.***)
#define BPF_5_MP3               DIR_APPEND(5.***)
#define BPF_6_MP3               DIR_APPEND(6.***)
#define BPF_7_MP3               DIR_APPEND(7.***)
#define BPF_8_MP3               DIR_APPEND(8.***)
#define BPF_9_MP3               DIR_APPEND(9.***)

#define BPF_LOW_POWER_MP3       DIR_APPEND(test.***)
#define BPF_MUSIC_PLAY_MP3      DIR_APPEND(test.***)
#define BPF_MUSIC_PAUSE_MP3     DIR_APPEND(test.***)
#define BPF_TEST_MP3       		DIR_APPEND(test.***)

#define NUMBER_PATH_LIST \
                        /*00*/   BPF_0_MP3,\
                        /*01*/   BPF_1_MP3,\
                        /*02*/   BPF_2_MP3,\
                        /*03*/   BPF_3_MP3,\
                        /*04*/   BPF_4_MP3,\
                        /*05*/   BPF_5_MP3,\
                        /*06*/   BPF_6_MP3,\
                        /*07*/   BPF_7_MP3,\
                        /*08*/   BPF_8_MP3,\
                        /*09*/   BPF_9_MP3

#endif//__NOTICE_CFG_H__

