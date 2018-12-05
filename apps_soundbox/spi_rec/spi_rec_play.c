/***********************************Jieli tech************************************************
  File : spi_rec_play.c
  By   : Huxi
  Email: xi_hu@zh-jieli.com
  date : 2016-11-30 14:30
********************************************************************************************/
#include "sdk_cfg.h"
#include "dac/dac_api.h"
#include "spi/nor_fs.h"
#include "spi_rec.h"



extern void adpcm_decoder(char indata[], short outdata[], int *index, int *valpred);

typedef struct {
    void *hdl;
    int  adpcm_index;
    int  adpcm_valpred;
    u8   buf[DAC_SAMPLE_POINT / 2];
} recfile_adpcm_t;
static recfile_adpcm_t     recfile_adpcm;



static u16 recfile_read(void *hdl, u8 *buf, u32 addr, u16 length)
{
    recfile_adpcm_t *pinfo = hdl;
    u16 temp_len = length;

    if (addr == 0) {
        pinfo->adpcm_index = 0;
        pinfo->adpcm_valpred = 0;
    }
//    printf("\n addr = %x \n", addr);
    recf_seek(pinfo->hdl, NOR_FS_SEEK_SET, addr / 4 + 0x10);

    while (length >= (DAC_SAMPLE_POINT * 2)) {
        recf_read(pinfo->hdl, pinfo->buf, DAC_SAMPLE_POINT / 2);
        adpcm_decoder((char *)pinfo->buf, (short int *)buf, &(pinfo->adpcm_index), &(pinfo->adpcm_valpred));
        buf += (DAC_SAMPLE_POINT * 2);
        length -= (DAC_SAMPLE_POINT * 2);
    }
    if (length) {
        memset(buf, 0, length);
    }
    return temp_len;
}

static u16 spirec_get_dac_rate(void)
{
#if (SPI_REC_EN == 0)
    return 0;
#endif
    return spirec_dac_rate;
}

/*----------------------------------------------------------------------------*/
/**@brief spi外挂flash录音播放
   @param
	w_play:wav播放器句柄，filenum：文件号
   @return wav叠加器控制句柄, 失败返回NULL
   @note
*/
/*----------------------------------------------------------------------------*/
tbool spirec_wav_play(WAV_PLAYER *w_play, u32 filenum)
{
#if (SPI_REC_EN == 0)
    return false;
#endif
    WAV_INFO info;
    u32 openfile;
    openfile = open_recfile(filenum, &recfs, &recfile);
    if (filenum != openfile) {
        printf("open recfile %d err, open file=%d \n", filenum, openfile);
        return false;
    }

    memset(&info, 0, sizeof(WAV_INFO));
    memset(&recfile_adpcm, 0, sizeof(recfile_adpcm_t));

    info.total_len = recfile.len;

    if (info.total_len >= 0x10) {
        info.total_len -= 0x10;
    }
    info.total_len <<= 2;
    /* printf("\n rec file len = %x \n", info.total_len); */
    /* printf("\n rec file samplerate ================= = %d \n", recfile.sr); */
    info.dac_sr = recfile.sr;//spirec_get_dac_rate();//spirec_dac_rate;
    info.track = SPIREC_DAC_TACK;
    recfile_adpcm.hdl = &recfile;


    __wav_player_set_info(w_play, &info);
    __wav_player_set_read_cbk(w_play, recfile_read, (void *)&recfile_adpcm);

    return true;
}

