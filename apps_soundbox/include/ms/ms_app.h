#ifndef MS_APP_H_INCLUDED
#define MS_APP_H_INCLUDED

#include "asm_type.h"
#include "sdk_cfg.h"

#define MS_ENABLE 1

#if MS_ENABLE
/*------------------------------------------
安静时能量为:       vsum
端点检测起始能量为: energie
端点检测结束能量为: end_energie

energie     = vsum * Mic_beginFactorH + vsum/10 * Mic_beginFactorL;
end_energie = vsum * Mic_endFactorH   + vsum/10 * Mic_endFactorL;
-------------------------------------------*/
#define Mic_beginFactorH    2//2
#define Mic_beginFactorL    8//5

#define Mic_endFactorH      2
#define Mic_endFactorL      5

//
#define ENERGY_WAITING       0x00
#define ENERGY_RECODER       0x01
#define ENERGY_RECODER_END   0x02

#define ENERGY_THT  0x60000000

//----------------------------------------------------------------------



//#define MS_DAC_PUT_BUFFER_TEST

#define MS_PLAY_ENABLED 0x01
#define MS_PLAY_DISABLE 0X00

//#define adpcm_total_len 1024*24

#define INTABS(x)   ((x)>0? (x):-(x))
#define MAXSAMPLE  32767
#define MINSAMPLE  -32768
#define RECODER_LEN (1024*32)
#define START_BUFFER_LEN  (96*32)

typedef struct _MS_INDEX_ {
    short speed_coff;
    unsigned short pitch_coff;
    unsigned char robort_flag;
} _MS_INDEX;

enum {
    MS_OUT_BUFFER_INDEX1 = 0,
    MS_OUT_BUFFER_INDEX2 = 64,
    MS_OUT_BUFFER_INDEX3 = 128,
    MS_OUT_BUFFER_INDEX4 = 192,
};

typedef struct adpcm_t {
    int predval_c;
    int index_c;
    int predval_d;
    int index_d;
} adpcm_t;

#define testLEN  512
typedef struct __MS_CTL {
    s16  adda_msbuf[testLEN];
    s16  ms_buf[2][256];
    u8   outbuf2[256];
    u8   ms_adpcmbuf[RECODER_LEN];
    u8   ms_inbuf[testLEN * 2];
    u8   mspcm_buf[1024 * 2];
    u8   msdata_buf[1024 * 2];
    adpcm_t adpcm_temp;
    u16  ms_adpcm_ptr;
    u16 ms_adpcm_startrec_ptr;
    u16 ms_adpcm_endrec_ptr;
    u16 ms_rec_len;
    u16 ms_rec_len_buffer;
    u16 adpcm_len;
    u32 pend_energie;
    u32 end_energie;
    u32 energie;
    u32 energy_tht;
    u32 adpcm_endcnt;
    u32 adpcm_len2;
    u32 max_energyStep;
    u32 indexInBuffer;
    u32 energyCount;
    u32 agc_step;
    u32 energyStep;
    u32 energyStep2;
    u32 flag;
    u32 flag2;
    u32 vsum;
    u32 vcnt;
    u32 pcm_ms_ptr;
    u32 mspcm_wptr;
    u32 mspcm_rptr;
    u16 mspcm_len;
    u16 msdata_len;
    u16 msdata_ptr;
    u16 msdata_rptr;
    u8  ms_bufnum;
    s8  ms_play_enable;
    u8  ms_start_enable;
    u8  energyFlag;
    u8  adpcm_flg;
    u8  adda_on;
    u16 ladc_cut_head_cnt;//ladc初始化后前面一段时间是垃圾数据，通过计数去掉
    char *ms_father_task;
} _MS_CTL;


_MS_CTL *ms_ctl;


int temp_index_d;
int temp_predval_d;
int temp_adpcmptr;
u8 decoder_flg;
char *ps_obj;
#ifdef MS_DAC_PUT_BUFFER_TEST
extern unsigned char dac_data[9502];
#endif

void ms_task_init(short speed, unsigned short pitch, unsigned char robort);
void ms_task_exit(void);
void dac_ms_isr_callback(void *dac_buf, u8 buf_flag);





#endif
#endif // MS_APP_H_INCLUDED

