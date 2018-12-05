/***********************************Jieli tech************************************************
  File : spi_rec.h
  By   : Huxi
  Email: xi_hu@zh-jieli.com
  date : 2016-11-30 14:30
********************************************************************************************/
#ifndef __SPI_REC_H_
#define __SPI_REC_H_

#include "cbuf/circular_buf.h"
#include "spi/nor_fs.h"
#include "wav_play.h"

typedef struct _dac_save_ {
    cbuffer_t cbuf;
    u8        *buf;
    u32       buflen;
    void      *priv;
    void (*save_buf)(void *hdl, void *buf, u32 len);
} dac_save_t;
extern volatile dac_save_t *g_p_adc;

enum {
    SAVE_TYPE_BUF = 0,
    SAVE_TYPE_DAC,
    SAVE_TYPE_LADC,
};
extern volatile u8 spirec_save_type;

extern RECFILESYSTEM recfs;
extern REC_FILE recfile;
extern u32 spirec_filenum;

// #define SPIREC_LADC_RATE     16000
#define SPIREC_LADC_SR       LADC_SR16000

#define SPIREC_DAC_TACK      1   //1 or 2

#define SPIREC_DAC_BUF_LEN   (1024*3)

#define SPI_ADPCM_W_BUF_MIN   256
#define SPI_ADPCM_HIGH_SAMPLERAT_W_BUF_LEN   (SPI_ADPCM_W_BUF_MIN*32)	//>16K
#define SPI_ADPCM_LOW_SAMPLERATE_W_BUF_LEN   (SPI_ADPCM_W_BUF_MIN*16)	//<=16k

void spiflash_rec_init();

void spirec_start(u16 sr);
void spirec_stop(void);

void spirec_save_buf(void *buf, u16 len);
void spirec_save_dac(void *dacbuf);
void spirec_save_ladc(void *buf0, void *buf1);

///////////////////////////////////////////////////////////////////
//                      spi rec play
///////////////////////////////////////////////////////////////////
tbool spirec_api_play(void *father_name, u32 filenum, tbool(*_cb)(int msg[], void *priv), void *priv);
tbool spirec_api_play_to_mix(void *father_name, u32 filenum, SOUND_MIX_OBJ *mix_obj, tbool(*_cb)(int msg[], void *priv), void *priv);
void spirec_api_play_stop(void);
// void spirec_play(void *father_name, u32 filenum, u16 rate);
// void spirec_play_stop(void);
// void spirec_play_stop_wait(void);

///////////////////////////////////////////////////////////////////
//                      api
///////////////////////////////////////////////////////////////////
extern u16 spirec_dac_rate;
extern u8  spirec_ladc_ch;

void spirec_api_dac_stop(void);
void spirec_api_dac(void);

void spirec_api_ladc_stop(void);
void spirec_api_ladc(u8 ch_sel);


tbool spirec_wav_play(WAV_PLAYER *w_play, u32 filenum);
// tbool spirec_api_play(void *father_name, u32 filenum);
// void spirec_api_play_stop_wait(void);



#endif
