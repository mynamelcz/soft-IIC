#include "htk.h"
#include "htk_app.h"
#include "dac/ladc_api.h"
#include "cpu/dac_param.h"
#include "file_operate/file_op.h"
#include "fat/fs_io.h"
#include "fat/tff.h"
#include "file_operate/logic_dev_list.h"



//#define HTK_DEBUG_ENABLE
#ifdef HTK_DEBUG_ENABLE
#define htk_printf printf
#else
#define htk_printf(...)
#endif

#define SIZEOF_ALIN(var,al)     ((((var)+(al)-1)/(al))*(al))

#define HTK_PCM_BUF_NUMS            (8)
#define HTK_PCM_BUF_ONE_SIZE        (1024*2)
#define HTK_RAM_LEN                 (25*1024)
#define HTK_GET_VOICE_DATA_LENGTH   (256)
#define HTK_FILE_NAME               "jlhtk.bin"

#define SPI_CACHE_READ_ADDR(addr)   (u32)(0x1000000 +(((u32)addr)))


typedef struct __MSG_IO {
    void *priv;
    msg_cb_fun  msg_cb;
} MSG_IO;


struct __HTK_PLAYER {
    u8              *pcm_buf;
    sr_config_t     *config;
    cbuffer_t        pcm_cbuf;
    OS_SEM           pcm_sem;
    u32              pcm_lost;
    u8				 msg_break;
    MSG_IO           msg;
};
typedef struct __HTK_PLAYER HTK_PLAYER;
HTK_PLAYER *htk_hdl = NULL;
extern lg_dev_list *lg_dev_find(u32 drv);
extern tbool syd_drive_open(void **p_fs_hdl, void *p_hdev, u32 drive_base);
extern tbool syd_drive_close(void **p_fs_hdl);
extern bool syd_get_file_byindex(void *p_fs_hdl, void **p_f_hdl, u32 file_number, char *lfn);
extern bool syd_file_close(void *p_fs_hdl, void **p_f_hdl);
extern u16 syd_read(void *p_f_hdl, u8 _xdata *buff, u16 len);
extern bool syd_get_file_bypath(void *p_fs_hdl, void **p_f_hdl, u8 *path, char *lfn);
extern tbool syd_get_file_addr(u32 *addr, void **p_f_hdl);

#if 1
static tbool open_htk_file(char *filename, int *address)
{
    /* htk_printf("\n open htk file ========="); */
    int htk_source_address;
    lg_dev_list *pnode;
    void *fs_hdl = NULL;  //文件系统句柄
    void *file_hdl = NULL;//文件句柄
    pnode = lg_dev_find('A');
    if (!syd_drive_open(&fs_hdl, pnode->p_cur_dev->lg_hdl->phydev_item, 0)) { //打开文件系统
        htk_printf("open file system errot \n");
        return false;
    }
    if (!syd_get_file_bypath(fs_hdl, &file_hdl, (u8 *)filename, (char *)filename)) { //打开1号文件
        //以下两部顺序不能变
        syd_file_close(fs_hdl, &file_hdl); //失败的情况下关闭文件
        syd_drive_close(&fs_hdl);//失败的情况下关闭文件系统
        htk_printf("open file errot \n");
        return false;
    }

    if (!syd_get_file_addr((u32 *)&htk_source_address, &file_hdl)) {
        htk_printf("syd_get_file_addr false \n");
        syd_file_close(fs_hdl, &file_hdl); //失败的情况下关闭文件
        syd_drive_close(&fs_hdl);//失败的情况下关闭文件系统
        return false;
    }
    htk_source_address = SPI_CACHE_READ_ADDR(htk_source_address);

    syd_file_close(fs_hdl, &file_hdl); //完成操作后，关闭文件
    syd_drive_close(&fs_hdl);  //完成操作后，关闭文件系统
    /* htk_printf("open htk file ok\r"); */

    if (address == NULL) {
        return false;
    }
    *address = htk_source_address;
    return true;
}
#endif

void write_flash_htk(void)
{
    ;
}

void read_flash_htk(void)
{
    ;
}

void erase_flash_htk(void)
{
    ;
}
void read_change_ptr(void)
{
    ;
}


static HTK_PLAYER *htk_player_creat(void)
{
    HTK_PLAYER *htk;
    u32 buf_len = (SIZEOF_ALIN(sizeof(HTK_PLAYER), 4)
                   + SIZEOF_ALIN(sizeof(sr_config_t), 4)
                   + SIZEOF_ALIN(HTK_PCM_BUF_ONE_SIZE, 4)
                   + SIZEOF_ALIN(HTK_RAM_LEN, 4));

    htk_printf("buf_len:%d\r", buf_len);
    u32 buf_index;
    u8 *ctr_buf = (u8 *)calloc(1, buf_len);
    if (ctr_buf == NULL) {
        return NULL;
    }

    buf_index = 0;
    htk = (HTK_PLAYER *)(ctr_buf + buf_index);

    buf_index += SIZEOF_ALIN(sizeof(HTK_PLAYER), 4);
    htk->config = (sr_config_t *)(ctr_buf + buf_index);


    buf_index += SIZEOF_ALIN(sizeof(sr_config_t), 4);
    htk->pcm_buf = (u8 *)(ctr_buf + buf_index);

    buf_index += SIZEOF_ALIN(HTK_PCM_BUF_ONE_SIZE, 4);
    htk->config->htk_ram = (unsigned char *)(ctr_buf + buf_index);

    os_sem_create(&(htk->pcm_sem), 0);
    cbuf_init(&(htk->pcm_cbuf), htk->pcm_buf, HTK_PCM_BUF_ONE_SIZE);
    htk->pcm_lost = 0;

    return htk;
}


static void htk_player_destroy(HTK_PLAYER **hdl)
{
    ladc_disable(ENC_MIC_CHANNEL);
    if ((hdl == NULL) || (*hdl == NULL)) {
        return ;
    }

    free(*hdl);
    *hdl = NULL;
}



static void htk_isr_callback(void *ladc_buf, u32 buf_flag, u32 buf_len)
{
    s16 *res;
    s16 *ladc_mic;

    //dual buf
    res = (s16 *)ladc_buf;
    res = res + (buf_flag * DAC_SAMPLE_POINT);

    //ladc_buf offset
    /* ladc_l = res; */
    /* ladc_r = res + DAC_DUAL_BUF_LEN; */
    ladc_mic = res + (2 * DAC_DUAL_BUF_LEN);

    cbuffer_t *cbuffer = &(htk_hdl->pcm_cbuf);
    if ((cbuffer->total_len - cbuffer->data_len) < DAC_DUAL_BUF_LEN) {
        htk_hdl->pcm_lost ++;
    } else {

        cbuf_write(cbuffer, (void *)ladc_mic, DAC_DUAL_BUF_LEN);
    }

    if (cbuffer->data_len >= HTK_GET_VOICE_DATA_LENGTH) {
        os_sem_set(&(htk_hdl->pcm_sem), 0);
        os_sem_post(&(htk_hdl->pcm_sem));
    }
}






int get_ad_voice(void *inout_spch, int len)
{
    while (1) {
        if (cbuf_get_data_size(&(htk_hdl->pcm_cbuf)) >= HTK_GET_VOICE_DATA_LENGTH) {
            cbuf_read(&(htk_hdl->pcm_cbuf), inout_spch, HTK_GET_VOICE_DATA_LENGTH);
            break;
        }
        os_sem_pend(&(htk_hdl->pcm_sem), 0);
    }

    return 1;
}




int get_ad_key(void)
{
    if (htk_hdl == NULL) {
        return 1;
    }

    int ret = 0;
    if (htk_hdl->msg.msg_cb) {
        ret = htk_hdl->msg.msg_cb(htk_hdl->msg.priv);
    }
    if (ret) {
        htk_hdl->msg_break = 1;
    }

    return ret;
}


/*----------------------------------------------------------------------------*/
/**@brief   语音识别及处理流程接口
   @param   priv:用户调用可以传递的私有参数, 将会被传递给act_cb和msg_cb
   @param   act_cb:识别完成后的处理过程
   @param   msg_cb:识别过程中响应消息处理， 可用于打断语音识别过程
   @param   timout:识别超时配置
   @return  void
   @note
*/
/*----------------------------------------------------------------------------*/
tbool htk_player_process(void *priv, act_cb_fun act_cb, msg_cb_fun msg_cb, u16 timeout)
{
    tbool res = true;
    u32 cpu_sr;
    int htk_file_addr;
    char  words_tab[10] = {0};
    char  words_cnt;
    int   htk_res;
    static char htk_init_noise = CONT_AD_DEFAULT_NOISE;
    while (1) {
        htk_printf("----------------------htk start----------------------------------\n");
        htk_hdl = htk_player_creat();

        if ((htk_hdl == NULL) || (htk_hdl->config == NULL)) {
            res = false;
            break;
        }

        res = open_htk_file(HTK_FILE_NAME, &htk_file_addr);
        if (res == false) {
            htk_printf("htk file open fail !!\n");
            break;
        }
        htk_hdl->msg_break = 0;
        htk_hdl->msg.priv = priv;
        htk_hdl->msg.msg_cb = msg_cb;

        sr_config_t *config = htk_hdl->config;
        config->cont_def_long = timeout * 1000 / 16;
        config->const_addr = htk_file_addr;
        config->init_noise = htk_init_noise;
        config->ad_const_init = CONT_AD_DELTA_SIL;
        config->words_cnt = 0;

        ladc_reg_isr_cb_api(htk_isr_callback);
        ladc_enable(ENC_MIC_CHANNEL, LADC_SR8000, VCOMO_EN);

        htk_res = htk_main(htk_hdl->config);
        if (htk_hdl->msg_break == 0) {
            words_cnt = config->words_cnt;
            memcpy((u8 *)words_tab, (u8 *)config->words_tab, sizeof(words_tab));

            htk_printf("config->words_tab[0] :%d, cnt = %d, htk_res = %d, pcm lost = %d\r", \
                       config->words_tab[0], words_cnt, htk_res, htk_hdl->pcm_lost);

            htk_init_noise = config->init_noise;
            htk_player_destroy(&htk_hdl);
            ///deal result
            if (act_cb) {
                tbool act_res = act_cb(priv, words_tab, words_cnt, htk_res);
                if (act_res == true) {
                    continue;
                } else {
                    res = false;
                    break;
                }
            } else {
                htk_printf("HTK nothing to do without cb\r");
                res = false;
                break;
            }
        } else {
            htk_printf("msg break htk !!\n");
            res = false;
            break;
        }

    }

    htk_player_destroy(&htk_hdl);
    return res;
}

