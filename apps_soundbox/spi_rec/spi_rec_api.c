/***********************************Jieli tech************************************************
  File : spi_rec_api.c
  By   : Huxi
  Email: xi_hu@zh-jieli.com
  date : 2016-11-30 14:30
********************************************************************************************/
#include "sdk_cfg.h"
#include "spi/nor_fs.h"
#include "spi_rec/spi_rec.h"
#include "key_drv/key_voice.h"
#include "dac.h"
#include "dac/ladc.h"
#include "dac/dac_api.h"
#include "notice_player.h"




u16 spirec_dac_rate = 16000; ///<保存录音时的采样率
u8  spirec_ladc_ch;         ///<保存录ladc时的通道号

/*----------------------------------------------------------------------------*/
/**@brief   spiflash dac录音结束
   @param   void
   @return  void
   @author  huxi
   @note    void spirec_api_dac_stop(void)
*/
/*----------------------------------------------------------------------------*/
void spirec_api_dac_stop(void)
{
#if SPI_REC_EN
    spirec_stop();
#endif
}
/*----------------------------------------------------------------------------*/
/**@brief   spiflash dac录音开始
   @param   void
   @return  void
   @author  huxi
   @note    void spirec_api_dac(void)
*/
/*----------------------------------------------------------------------------*/
void spirec_api_dac(void)
{
#if SPI_REC_EN
    spirec_save_type = SAVE_TYPE_DAC;
#if 0
    dac_set_samplerate(SPIREC_LADC_SR, 1);
    spirec_dac_rate = SPIREC_LADC_SR;
#else
    spirec_dac_rate = key_voice.dac_rate;
#endif
    spirec_start(spirec_dac_rate);
#endif
}

/*----------------------------------------------------------------------------*/
/**@brief   spiflash ladc录音结束
   @param   void
   @return  void
   @author  huxi
   @note    void spirec_api_ladc_stop(void)
*/
/*----------------------------------------------------------------------------*/
void spirec_api_ladc_stop(void)
{
#if SPI_REC_EN
    spirec_stop();
    /* disable_ladc(spirec_ladc_ch); */
    ladc_disable(spirec_ladc_ch);
#endif
}
/*----------------------------------------------------------------------------*/
/**@brief   spiflash ladc录音开始
   @param   ch_sel      录音通道，ENC_MIC_CHANNEL、ENC_DAC_CHANNEL
   @return  void
   @author  huxi
   @note    void spirec_api_ladc(u8 ch_sel)
*/
/*----------------------------------------------------------------------------*/
void spirec_api_ladc(u8 ch_sel)
{
#if SPI_REC_EN
    spirec_dac_rate = SPIREC_LADC_SR;
    spirec_ladc_ch  = ch_sel;
    spirec_save_type = SAVE_TYPE_LADC;
    /* ladc_reg_init(ch_sel, SPIREC_LADC_SR); */
    /* ladc_enable(ch_sel, SPIREC_LADC_SR, VCOMO_EN); */
    ladc_reg_init(ch_sel, SPIREC_LADC_SR);
    spirec_start(spirec_dac_rate);
#endif
}

/*----------------------------------------------------------------------------*/
/**@brief   spiflash 录音文件播放
   @param   filenum     文件序号，一般为spirec_filenum
   @return  void
   @author  huxi
   @note    void spirec_api_ladc_stop(void)
*/
/*----------------------------------------------------------------------------*/
tbool spirec_api_play(void *father_name, u32 filenum, tbool(*_cb)(int msg[], void *priv), void *priv)
{
#if SPI_REC_EN
    NOTICE_PlAYER_ERR n_err;
    n_err =	notice_player_play_spec_dev(father_name, _cb, priv, 0, 1, 0, NOTICE_PLAYER_METHOD_BY_SPI_REC_NUM | NOTICE_USE_WAV_PLAYER, 1, filenum);
    if (n_err >= NOTICE_PLAYER_NO_ERR_WITHOUT_MSG_DEAL) {
        return false;
    }
    return true;
#else
    return false;
#endif
}

tbool spirec_api_play_to_mix(void *father_name, u32 filenum, SOUND_MIX_OBJ *mix_obj, tbool(*_cb)(int msg[], void *priv), void *priv)
{
#if SPI_REC_EN
    NOTICE_PlAYER_ERR n_err;
    n_err =	notice_player_play_spec_dev_to_mix(father_name, _cb, priv, 0, 1, 0, mix_obj, NOTICE_PLAYER_METHOD_BY_SPI_REC_NUM | NOTICE_USE_WAV_PLAYER, 1, filenum);
    if (n_err >= NOTICE_PLAYER_NO_ERR_WITHOUT_MSG_DEAL) {
        return false;
    }
    return true;
#else
    return false;
#endif
}
/*----------------------------------------------------------------------------*/
/**@brief   spiflash录音播放结束
   @param   void
   @return  void
   @author  huxi
   @note    void spirec_api_play_stop(void)
*/
/*----------------------------------------------------------------------------*/
void spirec_api_play_stop(void)
{
#if SPI_REC_EN
    notice_player_stop();
    /* spirec_play_stop(); */
#endif
}

