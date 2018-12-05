#include "wav_play.h"
#include "common/msg.h"
#include "dec/sup_dec_op.h"
#include "dev_manage/dev_ctl.h"
#include "fat/tff.h"
#include "file_operate/file_op.h"
#include "fat/fs_io.h"
#include "fat/tff.h"
#include "common/app_cfg.h"
#include "file_operate/logic_dev_list.h"
#include "dac/dac_api.h"
#include "cpu/dac_param.h"
#include "dac/dac.h"
#include "dac/dac_track.h"
#include "cbuf/circular_buf.h"
#include "sydf/syd_file.h"
//#define WAV_PLAYER_DEBUG_ENABLE
#ifdef WAV_PLAYER_DEBUG_ENABLE
#define wav_player_printf	printf
#else
#define wav_player_printf(...)
#endif


typedef struct {
    void  *priv;
    u16(*read)(void *priv, u8 *buf, u32 addr, u16 len);
} WAV_READ_CBK;



struct __WAV_PLAYER {
    FLASHDATA_T flashdata;
    WAV_INFO info;
    WAV_READ_CBK r_cbk;
    _OP_IO output;
    DEC_OP_CB	 op_cb;
    DEC_SET_INFO_CB set_info_cb;
    void *mix;
    s16  *dec_buf;
    WAV_PLAYER_ST status;
};

extern cbuffer_t audio_cb ;
extern u8 dac_read_en;
extern DEV_HANDLE volatile cache;      ///>设备句柄


extern u32 fs_api_read(void *p_hdev, void *buf, u32 addr);



static FRESULT move_window_flashdata(u32 sector, FLASHDATA_T *win_buf)
{
    FRESULT res = FR_OK;
    if (win_buf->sector != sector) {
        res = fs_api_read(cache, win_buf->buf, sector);
        if (res != FR_OK) {
            return res;
        }
        win_buf->sector = sector;
    }
    return res;
}

u16 phy_flashdata_read(FLASHDATA_T *flashdata, u8 *buf, u32 addr, u16 len)
{
    u32 sector, sec_offset, len0;
    u32 length = len;
    u64 t_addr = addr;
    u8 _xdata *ptr;
    /* FLASHDATA_T *flashdata = (FLASHDATA_T *)hdl; */
    ptr = buf;
    while (length) {
        sector = t_addr / 512;
        sec_offset = t_addr % 512;
        move_window_flashdata(sector, flashdata);
        if (length > (512 - sec_offset)) {
            len0 = 512 - sec_offset;
        } else {
            len0 = length;
        }
        memcpy(ptr, &flashdata->buf[sec_offset], len0);
        length -= len0;
        t_addr += len0;
        ptr += len0;
    }
    return len;
}


u16 wav_player_default_read(void *hdl, u8 *buf, u32 addr, u16 len)
{
    return phy_flashdata_read((FLASHDATA_T *)hdl, buf, addr, len);
}

static void *wav_player_output(void *priv, void *buf, u32 len)
{
    /* printf("^"); */
    dac_write((u8 *)buf, len);
    return priv;
}


static tbool wav_player_data_over(void *priv)
{
    //printf("%4d %4d\r",audio_cb.data_len,DAC_BUF_LEN);

    if ((audio_cb.data_len < DAC_BUF_LEN) || (!dac_read_en)) {
        if (audio_cb.data_len > DAC_BUF_LEN) {
            printf("warning output disable when output buff has data\r");
        }
        return true;
    } else {
        return false;
    }
}

static void wav_player_data_clear(void *priv)
{
    //priv = priv;
    cbuf_clear(&audio_cb);
}


#define WAV_PLAYER_SIZEOF_ALIN(var,al)     ((((var)+(al)-1)/(al))*(al))
WAV_PLAYER *wav_player_creat(void)
{
    WAV_PLAYER *w_play;
    u32 ctr_buf_len = WAV_PLAYER_SIZEOF_ALIN(sizeof(WAV_PLAYER), 4) + WAV_PLAYER_SIZEOF_ALIN(sizeof(s16) * WAV_ONCE_READ_POINT_NUM, 4);
    wav_player_printf("wav player buf size = %d\n", ctr_buf_len);
    u8 *ctr_buf = (u8 *)calloc(1, ctr_buf_len);
    if (ctr_buf == NULL) {
        wav_player_printf("no space for mix_chl err !!!\n");
        return NULL;
    }
    w_play = (WAV_PLAYER *)(ctr_buf);

    ctr_buf += WAV_PLAYER_SIZEOF_ALIN(sizeof(WAV_PLAYER), 4);
    w_play->dec_buf = (s16 *)(ctr_buf);
    w_play->status = WAV_PLAYER_ST_STOP;

    return w_play;
}


void wav_player_destroy(WAV_PLAYER **hdl)
{
    if (hdl == NULL) {
        return ;
    }
    WAV_PLAYER *w_play = *hdl;

    if (w_play->op_cb.over) {
        while (!w_play->op_cb.over(w_play->op_cb.priv)) {
            OSTimeDly(1);
        }
    }
    if (w_play->mix) {
        sound_mix_chl_del_by_chl((SOUND_MIX_CHL **)&w_play->mix);
    }
    free(*hdl);
    *hdl = NULL;
}

tbool wav_file_decode(WAV_INFO *info, u16(*read)(void *priv, u8 *buf, u32 addr, u16 len), void *priv, s16 outbuf[WAV_ONCE_READ_POINT_NUM])
{
    /* WAV_INFO *info; */
    u32 data_cnt;
    s16 *tmp_dualbuf = outbuf;
    s16 *tmp_buf = &outbuf[WAV_ONCE_READ_POINT_NUM / 2];

    data_cnt = info->total_len - info->r_len;
    if (data_cnt >= WAV_ONCE_BUF_LEN) {
        data_cnt = WAV_ONCE_BUF_LEN;
    }
    if (info->r_len >= info->total_len) {
        {
            return false;
        }
    }

    if (data_cnt < WAV_ONCE_BUF_LEN) {
        memset((u8 *)tmp_dualbuf, 0x0, WAV_ONCE_BUF_LEN);
    }
    if (info->track == SINGLE_TRACK) {
        if (data_cnt >= (WAV_ONCE_BUF_LEN >> 1)) {
            data_cnt = (WAV_ONCE_BUF_LEN >> 1);
        }
        if (read) {
            read(priv, (u8 *)tmp_buf, info->addr + info->r_len, data_cnt);
        }
        single_to_dual(tmp_dualbuf, tmp_buf, data_cnt);
    } else {
        if (read) {
            read(priv, (u8 *)tmp_dualbuf, info->addr + info->r_len, data_cnt);
        }
    }
    info->r_len += data_cnt;
    return true;
}


tbool wav_player_decode(WAV_PLAYER *w_play)
{
    tbool ret;
    ASSERT(w_play);
    ret = wav_file_decode(&(w_play->info), w_play->r_cbk.read, w_play->r_cbk.priv, w_play->dec_buf);
    if (ret == false) {
        w_play->status = WAV_PLAYER_ST_STOP;
        return false;
    } else {

    }

    w_play->output.output(w_play->output.priv, w_play->dec_buf, WAV_ONCE_BUF_LEN);
    return true;
}

#define SPI_CACHE_READ_ADDR(addr)  (u32)(0x1000000+(((u32)addr)))

extern lg_dev_list *lg_dev_find(u32 drv);
extern tbool syd_drive_open(void **p_fs_hdl, void *p_hdev, u32 drive_base);
extern tbool syd_drive_close(void **p_fs_hdl);
extern bool syd_get_file_byindex(void *p_fs_hdl, void **p_f_hdl, u32 file_number, char *lfn);
extern bool syd_file_close(void *p_fs_hdl, void **p_f_hdl);
extern u16 syd_read(void *p_f_hdl, u8 _xdata *buff, u16 len);
extern bool syd_seek(void *p_f_hdl, u8 type, u32 offsize);
extern bool syd_get_file_bypath(void *p_fs_hdl, void **p_f_hdl, u8 *path, char *lfn);
extern tbool syd_get_file_addr(u32 *addr, void **p_f_hdl);



tbool wav_player_get_file_info(WAV_INFO *info, const char *path, u32 num)
{
    if (info == NULL) {
        return false;
    }
    bool ret = false;
    lg_dev_list *pnode;
    void *fs_hdl = NULL;  //文件系统句柄
    void *file_hdl = NULL;//文件句柄

    pnode = lg_dev_find('A');
#ifndef TOYS_EN
    if (!syd_drive_open(&fs_hdl, pnode->cur_dev.lg_hdl->phydev_item, 0)) //打开文件系统
#else /*TOYS_EN*/
    if (!syd_drive_open(&fs_hdl, pnode->p_cur_dev->lg_hdl->phydev_item, 0)) //打开文件系统
#endif /*TOYS_EN*/
    {
        wav_player_printf("syd_drive_open err!\n");
        return false;
    }
    ret = syd_get_file_bypath(fs_hdl, &file_hdl, (u8 *)path, 0);
    if (ret == false) {
        wav_player_printf("syd_get_file_bypath err!\n");
        goto __exit;
    } else {
        u32 len = strlen(path);
        const char *pfile = path;
        SDFILE *p2;
        u32 filecnt;
        u32 filetotal = 0;
        p2 = file_hdl;
        filecnt     = p2->index;
#ifdef SDFS_PLAY_DIR
        filetotal   = p2->fs_msg.file_total_indir;
#endif
        if (pfile[len - 1] == '/') {
            /* is not a whole path		 */

            wav_player_printf("\n num  = %d, filecnt = %d, filetotal %d\n", num, filecnt, filetotal);
            if (num <= 1) {
                num = filecnt;
                wav_player_printf("file_byindex is open ok!\n");
            } else if (num <= filetotal) {
                //路径+序列号
                num = filecnt + num - 1;
                ret = syd_get_file_byindex(fs_hdl, &file_hdl, num, 0);
                if (ret == false) {
                    wav_player_printf("syd_get_file_byindex err!\n");
                    goto __exit;
                }
            } else {
                //序列号超范围
                wav_player_printf("file_byindex is over limit err!\n");
                ret = false;
                goto __exit;
            }

        } else {

        }

        if (ret == true) {
            u8 buftmp[256];
            u8 fmtdata_offset, oth_offset;
            u32 tmp;
            syd_seek(file_hdl, SEEK_SET, 0);
            syd_read(file_hdl, buftmp, 256);
            memcpy(&tmp, buftmp, 4);
            if (tmp != 0x46464952) {
                wav_player_printf("wav head no 0x46464952, is 0x%x\n", tmp);
                ret = false;
                goto __exit;
            }
            memcpy(&tmp, buftmp + 0x10, 4);
            if ((tmp < 0x10) || (tmp > 0x20)) {
                wav_player_printf("wav fmt size err, is 0x%x\n", tmp);
                ret = false;
                goto __exit;
            }
            fmtdata_offset = tmp - 0x10;
            memcpy(&tmp, buftmp + 0x24 + fmtdata_offset, 4);
            oth_offset = 0;
            while ((tmp != 0x61746164) && (oth_offset < 128)) {
                memcpy(&tmp, buftmp + 0x24 + fmtdata_offset + oth_offset + 4, 4);
                oth_offset += tmp + 8;
                memcpy(&tmp, buftmp + 0x24 + fmtdata_offset + oth_offset, 4);
            }
            if (tmp != 0x61746164) {
                wav_player_printf("wav head no 0x61746164, is 0x%x\n", tmp);
                ret = false;
                goto __exit;
            }

            ret = syd_get_file_addr((u32 *) & (info->addr), &file_hdl);
            if (ret == false) {
                goto __exit;
            } else {
                memcpy(&info->track, buftmp + 0x16, 2);
                memcpy(&info->dac_sr, buftmp + 0x18, 2);
                memcpy(&info->total_len, buftmp + 0x28 + fmtdata_offset + oth_offset, 4);
                info->r_len = 0;
                info->addr += 0x2c + fmtdata_offset + oth_offset;
                wav_player_printf("total_len = 0x%x\n", info->total_len);
            }

            wav_player_printf("total_len = 0x%x\n", info->total_len);
        }
    }

__exit:
    syd_file_close(fs_hdl, &file_hdl); //完成操作后，关闭文件
    syd_drive_close(&fs_hdl);  //完成操作后，关闭文件系统
    return ret;
}



tbool wav_player_set_path(WAV_PLAYER *w_play, const char *path, u32 num)
{
    if (w_play == NULL) {
        return false;
    }
    return wav_player_get_file_info(&(w_play->info), path, num);
}


void __wav_player_set_info(WAV_PLAYER *w_play, WAV_INFO *info)
{
    if (w_play == NULL || info == NULL) {
        return ;
    }

    memcpy((u8 *) & (w_play->info), (u8 *)info, sizeof(WAV_INFO));
    wav_player_printf("w_play->info.track = %d\n", w_play->info.track);
    wav_player_printf("w_play->info.total_len = %d\n", w_play->info.total_len);
    wav_player_printf("w_play->info.dac_sr = %d\n", w_play->info.dac_sr);
    wav_player_printf("w_play->info.track = %d\n", w_play->info.track);
    wav_player_printf("w_play->info.track = %d\n", w_play->info.track);
}


void __wav_player_set_read_cbk(WAV_PLAYER *w_play, u16(*read)(void *priv, u8 *buf, u32 addr, u16 len), void *priv)
{
    if (w_play == NULL) {
        return ;
    }

    w_play->r_cbk.priv = priv;
    w_play->r_cbk.read = read;
}


void __wav_player_set_output_cbk(WAV_PLAYER *w_play, void *(*output)(void *priv, void *buf, u32 len), void *priv)
{
    if (w_play == NULL) {
        return ;
    }
    w_play->output.priv = priv;
    w_play->output.output = output;
}

tbool __wav_player_set_output_to_mix(WAV_PLAYER *w_play, SOUND_MIX_OBJ *mix_obj, const char *key, u32 mix_buf_len, u8 Threshold, u8 default_vol, u8 max_vol)
{
    if (w_play == NULL || mix_obj == NULL) {
        return false;
    }

    SOUND_MIX_P mix_p;
    memset((u8 *)&mix_p, 0x0, sizeof(SOUND_MIX_P));
    mix_p.key = key;
    mix_p.out_len = mix_buf_len;
    /* mix_p.set_info = &(mapi->dop_api->io->set_music_info); */
    mix_p.set_info_cb = &w_play->set_info_cb;
    mix_p.input = &w_play->output;
    mix_p.op_cb = &w_play->op_cb;
    mix_p.limit = Threshold;
    mix_p.default_vol = default_vol;
    mix_p.max_vol = max_vol;
    w_play->mix = (void *)sound_mix_chl_add(mix_obj, &mix_p);

    return (w_play->mix ? true : false);
}

static u32 wav_player_set_samplerate_defualt(void *priv, dec_inf_t *inf, tbool wait)
{
    /* printf("inf->sr %d %d\n", inf->sr, __LINE__); */
    dac_set_samplerate(inf->sr, wait);
    return 0;
}
tbool wav_player_play(WAV_PLAYER *w_play)
{
    if (w_play == NULL) {
        return false;
    }

    if (w_play->r_cbk.read == NULL) {
        w_play->r_cbk.priv = (void *) & (w_play->flashdata);
        w_play->r_cbk.read = (void *)wav_player_default_read;
    }

    if (w_play->output.output == NULL) {
        w_play->output.priv = NULL;
        w_play->output.output = (void *)wav_player_output;
    }

    if (w_play->op_cb.priv == NULL) {
        w_play->op_cb.priv = NULL;
    }

    if (w_play->op_cb.clr == NULL) {
        w_play->op_cb.clr = (void *)wav_player_data_clear;
    }

    if (w_play->op_cb.over == NULL) {
        w_play->op_cb.over = (void *)wav_player_data_over;
    }

    if (w_play->set_info_cb._cb == NULL) {
        w_play->set_info_cb.priv = NULL;
        w_play->set_info_cb._cb = (void *)wav_player_set_samplerate_defualt;
    }

    if (w_play->set_info_cb._cb) {
        dec_inf_t inf;
        memset((u8 *)&inf, 0x0, sizeof(dec_inf_t));
        inf.sr = w_play->info.dac_sr;
        w_play->set_info_cb._cb(w_play->set_info_cb.priv, &inf, 1);
    }

    if (w_play->op_cb.clr) {
        w_play->op_cb.clr(w_play->op_cb.priv);
    }
    wav_player_printf("wav player play ok!!\n");
    w_play->status = WAV_PLAYER_ST_PLAY;

    return true;
}

WAV_PLAYER_ST wav_player_pp(WAV_PLAYER *w_play)
{
    if (w_play == NULL) {
        return WAV_PLAYER_ST_STOP;
    }
    if (w_play->status == WAV_PLAYER_ST_STOP) {
        return WAV_PLAYER_ST_STOP;
    }
    if (w_play->status == WAV_PLAYER_ST_PAUSE) {
        w_play->status = WAV_PLAYER_ST_PLAY;
    } else {
        w_play->status = WAV_PLAYER_ST_PAUSE;
    }
    return w_play->status;
}

WAV_PLAYER_ST wav_player_get_status(WAV_PLAYER *w_play)
{
    if (w_play) {
        return w_play->status;
    }
    return WAV_PLAYER_ST_STOP;
}
