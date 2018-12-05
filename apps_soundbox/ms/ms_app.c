#include "ms_app.h"
#include "dac/dac.h"
#include "includes.h"
#include "dac/ladc_api.h"
#include "dac/dac_api.h"
#include "dac/ladc.h"
#include "toy_moyin_api.h"
#include "common/app_cfg.h"
#include "sdk_cfg.h"
#include "encode/encode_flash.h"
#include "key_drv/key_voice.h"

//#define MQC_MS_DEBUG

#if MS_ENABLE
//s16 *adda_ms_buf;

//ms声音检测阀值
#define MS_ENERGY_DET  0xa000000


_MS_INDEX ms_index;

short ms_speed_coff;                         // 范围                         SPEED_DOWN 至 SPEED_UP
unsigned short ms_pitch_coff;                //外部给的变速变调参数
u8 dac_buffer[DAC_BUF_LEN];

extern int dac_fadein_cnt;
extern int dac_fadeout_cnt;
extern u8 *outbuf;
extern u8 dac_buffer[DAC_BUF_LEN];
extern u32 dac_buf[2][DAC_SAMPLE_POINT];
extern s16 ladc_buf[2][DAC_SAMPLE_POINT] __attribute__((aligned(4)));

void adpcm_decoder(char indata[], short outdata[], int *index, int *valpred);
void adpcm_coder_ms(char indata[], char outdata[], int *index, int *valpred);

const int indexTable[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
};

const int stepsizeTable[89] = {
    7, 8, 9, 10, 11, 12, 13, 14,
    16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
    7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767
};


//打开TF卡文件进行魔音
/*----------------------------------------------------------------------------*/
/**@brief  打开录音文件
   @param    u8 *  文件名/路径
   @return  0打开文件失败   正常：返回文件句柄
   @note    u8 ms_fs_init(u8 *path)
*/
/*----------------------------------------------------------------------------*/
//FILE_OPERATE * ms_fs_init(char *path)
//{
//  s16 ret;
//
//  FILE_OPERATE *f_op = create_new_fs_hdl('B');
//  ASSERT(f_op);
//
//  ret = fs_open(f_op->cur_lgdev_info->lg_hdl->fs_hdl,&(f_op->cur_lgdev_info->lg_hdl->file_hdl),path,NULL,FA_OPEN_EXISTING);
//
//  if(ret != FR_OK && ret != FR_EXIST)//文件不存在
//  {
//	   puts("找不到录音文件\n");
//	   destroy_file_hdl(&f_op);
//	   return NULL;
//  }
//
//  return f_op;
//}

/*----------------------------------------------------------------------------*/
/**@brief   统计整数参数的值为1的二进制位的个数
   @param   u8 data
   @return  countbc
   @note    int  bitCount(u8 data)
*/
/*----------------------------------------------------------------------------*/
int  bitCount(u8 data)
{
    int countbc = 0;
    while (data) {
        countbc++;
        data = data & (data - 1);
    }
    return countbc;
}

//设置端点检测起始能量
int get_start_energy()
{
    return (ms_ctl->vsum * Mic_beginFactorH + ms_ctl->vsum / 5 * Mic_beginFactorL);
    //return 0x40000;
}
//设置端点检测结束能量
int  get_end_energy()
{
    return (3 * ms_ctl->vsum + ms_ctl->vsum / 2 + ms_ctl->vsum / 5);
    //return 0x40000;
}
void clear_energy(void)
{
    ms_ctl->agc_step = 0;
    ms_ctl->energyStep2 = 0;
}
void get_energy(const short data)
{
    ms_ctl->agc_step += INTABS(data);//set_age使用
    ms_ctl->energyStep += (INTABS(data) * INTABS(data));
    ms_ctl->energyStep2 += INTABS(data);
}

void set_energy(u32 energy)
{
    ms_ctl->energy_tht = energy;
    ms_ctl->energie = energy;
}

void get_ms_rec_len(u16 *startaddr, u16 *endaddr, const int adpcm_lenght)
{
    u16 tem_endaddr;

    tem_endaddr = *endaddr;
    if (*endaddr <= *startaddr) {
        *endaddr += RECODER_LEN;
    }

    if (adpcm_lenght < START_BUFFER_LEN) {
        ms_ctl->ms_rec_len  = *endaddr - *startaddr + adpcm_lenght;
        *startaddr = 0;
    } else {
        ms_ctl->ms_rec_len  = *endaddr - *startaddr + START_BUFFER_LEN;
        if (*startaddr >= START_BUFFER_LEN) {
            *startaddr -= START_BUFFER_LEN;
        } else {
            *startaddr += RECODER_LEN;
            *startaddr -= START_BUFFER_LEN;
        }
    }
}


void run_adpcm_decoder()
{
    adpcm_decoder((char *)&ms_ctl->ms_adpcmbuf[ms_ctl->ms_adpcm_ptr],
                  (short *)&ms_ctl->mspcm_buf[ms_ctl->mspcm_wptr],
                  (int *)&ms_ctl->adpcm_temp.index_d,
                  (int *)&ms_ctl->adpcm_temp.predval_d);
}
#if 1
//void dac_adda_isr()
//{
////    u32 cpu_sr;
////    static u8 temp;
//    if(DAC_CON & BIT(7))
//    {
//        u8 dac_used_buf;
//        DAC_CON |= BIT(6);
//        if(DAC_CON & (BIT(8)))
//            dac_used_buf = 0 ;
//        else
//            dac_used_buf = 1;
//
//        if (ms_ctl->adda_on == 1)
//        {
//            vonvert_single_to_double(&ms_ctl->msdata_buf[ms_ctl->msdata_rptr]);
//            ms_ctl->msdata_rptr+= 64*2;
//            if(ms_ctl->msdata_rptr== 1024*2)
//                ms_ctl->msdata_rptr = 0;
//            memcpy(dac_buf[dac_used_buf],&dac_buffer[0], 256) ;
//        }
//        else
//        {
//
//            memset(dac_buf[dac_used_buf],0x00, DAC_BUF_LEN) ;
//        }
//    }
//}
//
//void dac_magic_sound(void)
//{
////    u32 cpu_sr;
//    if(DAC_CON & BIT(7))
//    {
//        u8 dac_used_buf;
//        DAC_CON |= BIT(6);
//        if(DAC_CON & (BIT(8)))
//            dac_used_buf = 0 ;
//        else
//            dac_used_buf = 1;
//
//        if (ms_ctl->ms_play_enable == MS_PLAY_ENABLED)
//        {
//            vonvert_single_to_double(&ms_ctl->outbuf2[0]);
//            memcpy(dac_buf[dac_used_buf],&dac_buffer[0], 128) ;
////            memset(dac_buf[dac_used_buf],0x00, DAC_BUF_LEN) ;//ray debug
//            os_taskq_post("MsTask", 1, MS_PLAY_CMD);
//        }
//        else
//        {
//            memset(dac_buf[dac_used_buf],0x00, 128) ;
//        }
//
//    }
//}
void vonvert_single_to_double(u8 buffer[DAC_BUF_LEN / 2])
{
    u16 cnt = 0;
    while (cnt < DAC_BUF_LEN / 2) {
        dac_buffer[2 * cnt] = buffer[cnt];
        dac_buffer[2 * cnt + 1] = buffer[cnt + 1];
        dac_buffer[2 * cnt + 2] = buffer[cnt];
        dac_buffer[2 * cnt + 3] = buffer[cnt + 1];
        cnt += 2;
    }
}

//unsigned char testsindata[320] = {
//
//	0x00, 0x00, 0xC8, 0x1F, 0x74, 0x3C, 0x35, 0x53, 0xD1, 0x61, 0xDA, 0x66, 0xD1, 0x61, 0x35, 0x53,
//	0x74, 0x3C, 0xC8, 0x1F, 0x00, 0x00, 0x38, 0xE0, 0x8C, 0xC3, 0xCB, 0xAC, 0x2F, 0x9E, 0x26, 0x99,
//	0x2F, 0x9E, 0xCB, 0xAC, 0x8C, 0xC3, 0x38, 0xE0, 0x00, 0x00, 0xC8, 0x1F, 0x74, 0x3C, 0x35, 0x53,
//	0xD1, 0x61, 0xDA, 0x66, 0xD1, 0x61, 0x35, 0x53, 0x74, 0x3C, 0xC8, 0x1F, 0x00, 0x00, 0x38, 0xE0,
//	0x8C, 0xC3, 0xCB, 0xAC, 0x2F, 0x9E, 0x26, 0x99, 0x2F, 0x9E, 0xCB, 0xAC, 0x8C, 0xC3, 0x38, 0xE0,
//	0x00, 0x00, 0xC8, 0x1F, 0x74, 0x3C, 0x35, 0x53, 0xD1, 0x61, 0xDA, 0x66, 0xD1, 0x61, 0x35, 0x53,
//	0x74, 0x3C, 0xC8, 0x1F, 0x00, 0x00, 0x38, 0xE0, 0x8C, 0xC3, 0xCB, 0xAC, 0x2F, 0x9E, 0x26, 0x99,
//	0x2F, 0x9E, 0xCB, 0xAC, 0x8C, 0xC3, 0x38, 0xE0, 0x00, 0x00, 0xC8, 0x1F, 0x74, 0x3C, 0x35, 0x53,
//	0xD1, 0x61, 0xDA, 0x66, 0xD1, 0x61, 0x35, 0x53, 0x74, 0x3C, 0xC8, 0x1F, 0x00, 0x00, 0x38, 0xE0,
//	0x8C, 0xC3, 0xCB, 0xAC, 0x2F, 0x9E, 0x26, 0x99, 0x2F, 0x9E, 0xCB, 0xAC, 0x8C, 0xC3, 0x38, 0xE0,
//	0x00, 0x00, 0xC8, 0x1F, 0x74, 0x3C, 0x35, 0x53, 0xD1, 0x61, 0xDA, 0x66, 0xD1, 0x61, 0x35, 0x53,
//	0x74, 0x3C, 0xC8, 0x1F, 0x00, 0x00, 0x38, 0xE0, 0x8C, 0xC3, 0xCB, 0xAC, 0x2F, 0x9E, 0x26, 0x99,
//	0x2F, 0x9E, 0xCB, 0xAC, 0x8C, 0xC3, 0x38, 0xE0, 0x00, 0x00, 0xC8, 0x1F, 0x74, 0x3C, 0x35, 0x53,
//	0xD1, 0x61, 0xDA, 0x66, 0xD1, 0x61, 0x35, 0x53, 0x74, 0x3C, 0xC8, 0x1F, 0x00, 0x00, 0x38, 0xE0,
//	0x8C, 0xC3, 0xCB, 0xAC, 0x2F, 0x9E, 0x26, 0x99, 0x2F, 0x9E, 0xCB, 0xAC, 0x8C, 0xC3, 0x38, 0xE0,
//	0x00, 0x00, 0xC8, 0x1F, 0x74, 0x3C, 0x35, 0x53, 0xD1, 0x61, 0xDA, 0x66, 0xD1, 0x61, 0x35, 0x53,
//	0x74, 0x3C, 0xC8, 0x1F, 0x00, 0x00, 0x38, 0xE0, 0x8C, 0xC3, 0xCB, 0xAC, 0x2F, 0x9E, 0x26, 0x99,
//	0x2F, 0x9E, 0xCB, 0xAC, 0x8C, 0xC3, 0x38, 0xE0, 0x00, 0x00, 0xC8, 0x1F, 0x74, 0x3C, 0x35, 0x53,
//	0xD1, 0x61, 0xDA, 0x66, 0xD1, 0x61, 0x35, 0x53, 0x74, 0x3C, 0xC8, 0x1F, 0x00, 0x00, 0x38, 0xE0,
//	0x8C, 0xC3, 0xCB, 0xAC, 0x2F, 0x9E, 0x26, 0x99, 0x2F, 0x9E, 0xCB, 0xAC, 0x8C, 0xC3, 0x38, 0xE0
//
//};
void dac_ms_isr_callback(void *dac_buf, u8 buf_flag)
{
    if (ms_ctl->ms_play_enable == MS_PLAY_ENABLED) {

        vonvert_single_to_double(&ms_ctl->outbuf2[0]);
        memcpy(dac_buf, &dac_buffer[0], 128) ;
//            memset(dac_buf,0x00, DAC_BUF_LEN) ;//ray debug
        os_taskq_post(ms_ctl->ms_father_task, 1, MSG_MS_PLAY_CMD);
    } else {
        //	putchar('0');
        memset(dac_buf, 0x00, 128) ;
    }
#if DAC_AUTO_MUTE_EN
    dac_digit_energy_value(dac_buf, DAC_BUF_LEN);
#endif
#if KEY_TONE_EN
    add_key_voice((s16 *)dac_buf, DAC_SAMPLE_POINT * 2);
#endif
}


void output_ps(char *ptr, short *buf, unsigned short len)
{
    int i;
    char *msptr;
    msptr = (char *)buf;
    //printf("len:%d\n",len)
    for (i = 0; i < len * 2; i++) {
        ms_ctl->msdata_buf[ms_ctl->msdata_ptr] = *msptr++;
        ms_ctl->msdata_ptr++;
        if (ms_ctl->msdata_ptr >= 1024 * 2) {
            ms_ctl->msdata_ptr = 0;
        }
    }
    ms_ctl->msdata_len += len * 2;

    ms_ctl->adda_on = 1;
}


//void ms_adc_isr()
//{
//    s16 *buf1,i;
//    if( (LADC_CON&BIT(7)) && (LADC_CON&BIT(5)) )
//    {
//        LADC_CON |= BIT(6);
//
//        if(LADC_CON & BIT(8))
//            buf1 = &ladc_buf[0][0];
//        else
//            buf1 = &ladc_buf[1][0];//&ladc_buf[1][0];
//
//        if (!ms_ctl->ms_start_enable)
//        {
//            return;
//        }
//
//        clear_energy();
//
//        for(i=0; i<32; i++)
//        {
//            if (buf1[i]<MINSAMPLE)
//                buf1[i] = MINSAMPLE;
//            else if (buf1[i]>MAXSAMPLE)
//                buf1[i] = MAXSAMPLE;
//
//            get_energy(buf1[i]);
//            ms_ctl->ms_buf[ms_ctl->ms_bufnum][i] = buf1[i];
//        }
//
//        //set_agc();
//        if(ms_ctl->max_energyStep<ms_ctl->energyStep2)
//        {
//            ms_ctl->max_energyStep = ms_ctl->energyStep2;
//        }
//
//        if (ms_ctl->adpcm_flg != ENERGY_WAITING)
//        {
//            //开始录音
//    //        puts("\n start recoder:");
//            ms_ctl->adpcm_len2 += 16;
//            if(ms_ctl->adpcm_flg==ENERGY_RECODER_END)
//            {
//                ms_ctl->adpcm_endcnt++;
//            }
//            if ((ms_ctl->adpcm_len2>=(RECODER_LEN-START_BUFFER_LEN)) || (ms_ctl->adpcm_endcnt>96))
//            {
//                puts("\nrecoder end");
//                ms_ctl->ms_adpcm_endrec_ptr = ms_ctl->ms_adpcm_ptr;//记录结束录音的位置
//
//                get_ms_rec_len(&ms_ctl->ms_adpcm_startrec_ptr, &ms_ctl->ms_adpcm_endrec_ptr,ms_ctl->ms_rec_len_buffer);
//                ms_ctl->flag = 0;
//                ms_ctl->adpcm_flg = ENERGY_WAITING;
//                ms_ctl->adpcm_len2 = 0;
//                ms_ctl->ms_rec_len_buffer = 0;
//                os_taskq_post("MsTask", 1, MS_RECEND_CMD);
//                return;
//            }
//        }
//
//        ms_ctl->indexInBuffer +=32;
//        ms_ctl->energyCount++;//相当于一个状态标志位
//
//        if (ms_ctl->energyCount == 16)
//        {
//            ms_ctl->energyFlag <<= (0x01);
//            //if (energyStep > 0x60000000)
//            if (ms_ctl->energyStep > 0xa000000)
//            {
//                ms_ctl->energyFlag++;
//            }
//
//            if (ms_ctl->energyStep > ms_ctl->energy_tht)
//            {
//                ms_ctl->vcnt = 0;
//                ms_ctl->vsum = 0;
//            }
//            else
//            {
//                ms_ctl->vcnt++;
//                ms_ctl->vsum += ms_ctl->energyStep;
//            }
//
//            ms_ctl->energyCount = bitCount(ms_ctl->energyFlag & 0x0f);
//
//            if (ms_ctl->flag == 0)
//            {
//                if (ms_ctl->energyCount >= 3)
//                {
//                    printf("-----start----\n");
//                    decoder_flg = 2;
//                    ms_ctl->flag = 1;
//                    ms_ctl->energie = ms_ctl->pend_energie;
//                    ms_ctl->adpcm_flg = ENERGY_RECODER;//启动录音
//                    ms_ctl->adpcm_endcnt = 0;
//
//                }
//                else
//                {
//                    if(ms_ctl->vcnt==4)
//                    {
//                        ms_ctl->pend_energie = ms_ctl->end_energie;
//                        ms_ctl->vsum/=ms_ctl->vcnt;
//
//                        ms_ctl->energie = get_start_energy();
//                        if(ms_ctl->energie > ms_ctl->energy_tht)
//                            ms_ctl->energie = ms_ctl->energy_tht;
//
//                        ms_ctl->end_energie = get_end_energy();
//                        if(ms_ctl->end_energie>ms_ctl->energy_tht*2)
//                            ms_ctl->end_energie = ms_ctl->energy_tht*2;
//
//                        ms_ctl->vsum = 0;
//                        ms_ctl->vcnt = 0;
//                    }
//                }
//            }
//            else
//            {
//                ms_ctl->energyCount = bitCount(ms_ctl->energyFlag & 0xff);//0x3f为多少bit持续为低
//                if ((ms_ctl->energyCount == 0)&&(ms_ctl->flag2 == 0))
//                {
//                    //
//                    ms_ctl->adpcm_flg = 2;
//                }
//            }
//            ms_ctl->energyCount = 0;
//            ms_ctl->energyStep = 0;
//            ms_ctl->energyStep2 = 0;
//        }
//
//        ms_ctl->ms_bufnum ^= 1;
//        os_taskq_post("MsTask", 1, MS_ADPCM_CODER_CMD);
//    }
//}

u32 ladc_int_cnt = 0;
u32 ladc_0_cnt = 0;

void ms_ladc_callback(void *ladc_buf, u32 buf_flag, u32 buf_len)
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

    s16 *buf1, i;

    buf1 = (s16 *)ladc_mic;

//	printf("%d ",INTABS(buf1[0]));
//	printf("%d ",INTABS(buf1[1]));
//	printf("%d ",INTABS(buf1[2]));
//	printf("%d\n",INTABS(buf1[20]));

    if (ms_ctl->ladc_cut_head_cnt < 20) {
        ms_ctl->ladc_cut_head_cnt++;
        if (ms_ctl->ladc_cut_head_cnt == 20) {
            // puts("ms ladc start after cut haead\n");
        }
        return;
    }


    if (!ms_ctl->ms_start_enable) {
        return;
    }

    clear_energy();

    for (i = 0; i < 32; i++) {
        if (buf1[i] < MINSAMPLE) {
            buf1[i] = MINSAMPLE;
        } else if (buf1[i] > MAXSAMPLE) {
            buf1[i] = MAXSAMPLE;
        }

        get_energy(buf1[i]);
        ms_ctl->ms_buf[ms_ctl->ms_bufnum][i] = buf1[i];
    }

    //set_agc();
    if (ms_ctl->max_energyStep < ms_ctl->energyStep2) {
        ms_ctl->max_energyStep = ms_ctl->energyStep2;
    }

    if (ms_ctl->adpcm_flg != ENERGY_WAITING) {
        //开始录音
        //        puts("\n start recoder:");
        ms_ctl->adpcm_len2 += 16;
        if (ms_ctl->adpcm_flg == ENERGY_RECODER_END) {
            ms_ctl->adpcm_endcnt++;
        }
        if ((ms_ctl->adpcm_len2 >= (RECODER_LEN - START_BUFFER_LEN)) || (ms_ctl->adpcm_endcnt > 96)) {
            puts("recoder end\n");
            ms_ctl->ms_adpcm_endrec_ptr = ms_ctl->ms_adpcm_ptr;//记录结束录音的位置

            get_ms_rec_len(&ms_ctl->ms_adpcm_startrec_ptr, &ms_ctl->ms_adpcm_endrec_ptr, ms_ctl->ms_rec_len_buffer);
            ms_ctl->flag = 0;
            ms_ctl->adpcm_flg = ENERGY_WAITING;
            ms_ctl->adpcm_len2 = 0;
            ms_ctl->ms_rec_len_buffer = 0;
            os_taskq_post(ms_ctl->ms_father_task, 1, MSG_MS_REC_END_CMD);
            return;
        }
    }

    ms_ctl->indexInBuffer += 32;
    ms_ctl->energyCount++;//相当于一个状态标志位

    if (ms_ctl->energyCount == 16) {
        ms_ctl->energyFlag <<= (0x01);
        //if (energyStep > 0x60000000)
        if (ms_ctl->energyStep > MS_ENERGY_DET) {
//                printf("s=%x\n",ms_ctl->energyStep);
            ms_ctl->energyFlag++;
        }

        if (ms_ctl->energyStep > ms_ctl->energy_tht) {
            ms_ctl->vcnt = 0;
            ms_ctl->vsum = 0;
        } else {
            ms_ctl->vcnt++;
            ms_ctl->vsum += ms_ctl->energyStep;
        }

        ms_ctl->energyCount = bitCount(ms_ctl->energyFlag & 0x0f);

        if (ms_ctl->flag == 0) {
            if (ms_ctl->energyCount >= 3) {
                printf("-start ms rec-\n");
                decoder_flg = 2;
                ms_ctl->flag = 1;
                ms_ctl->energie = ms_ctl->pend_energie;
                ms_ctl->adpcm_flg = ENERGY_RECODER;//启动录音
                ms_ctl->adpcm_endcnt = 0;

            } else {
                if (ms_ctl->vcnt == 4) {
                    ms_ctl->pend_energie = ms_ctl->end_energie;
                    ms_ctl->vsum /= ms_ctl->vcnt;

                    ms_ctl->energie = get_start_energy();
                    if (ms_ctl->energie > ms_ctl->energy_tht) {
                        ms_ctl->energie = ms_ctl->energy_tht;
                    }

                    ms_ctl->end_energie = get_end_energy();
                    if (ms_ctl->end_energie > ms_ctl->energy_tht * 2) {
                        ms_ctl->end_energie = ms_ctl->energy_tht * 2;
                    }

                    ms_ctl->vsum = 0;
                    ms_ctl->vcnt = 0;
                }
            }
        } else {
            ms_ctl->energyCount = bitCount(ms_ctl->energyFlag & 0xff);//0x3f为多少bit持续为低
            if ((ms_ctl->energyCount == 0) && (ms_ctl->flag2 == 0)) {
                //
                ms_ctl->adpcm_flg = 2;
            }
        }
        ms_ctl->energyCount = 0;
        ms_ctl->energyStep = 0;
        ms_ctl->energyStep2 = 0;
    }

    ms_ctl->ms_bufnum ^= 1;
    os_taskq_post(ms_ctl->ms_father_task, 1, MSG_MS_ADPCM_CODER_CMD);

}


#ifdef MQC_MS_DEBUG
extern u8 ad_to_da_enable;
#endif

_MS_CTL *ms_ctrl_hdl_create(void)
{
    _MS_CTL *ms_ctrl_api;

    ms_ctrl_api = malloc(sizeof(_MS_CTL));
    if (!ms_ctrl_api) {
        puts("ms ctrl hdl create err!!!!\n");
    }
    ASSERT(ms_ctrl_api);
    memset(ms_ctrl_api, 0x00, sizeof(_MS_CTL));

    puts("ms hdl create OK\n");
    return ms_ctrl_api;
}

void ms_ctrl_hdl_destroy(_MS_CTL **ms_ctrl_api)
{
    if (*ms_ctrl_api) {
        free(*ms_ctrl_api);
        *ms_ctrl_api = NULL;
    }
}


//speed：魔音的语速，160是正常语速(最大512)   spitch：魔音的语调，16000是正常的
/*----------------------------------------------------------------------------*/
/**@brief  魔音初始化及启动
   @param   speed_coff:速度参数
   @param   pitch_coff:音调参数
   @param   fater_task:魔音消息返回线程
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void ms_init_and_start(s16 speed_coff, u16 pitch_coff, char *fater_task)
{
    ms_ctl = ms_ctrl_hdl_create();
    if (ms_ctl == NULL) {
        puts("the ms_ctl is NULL!!!!!!!can not init\n");
        return;
    }

    ms_ctl->ms_father_task = fater_task;//设置父线程
//	printf("fater_task = %s\n",ms_ctl->ms_father_task);
//    adda_ms_buf = (s16 *)&ms_ctl->ms_adpcmbuf[0];
    ms_ctl->mspcm_len   = 0;
    ms_ctl->mspcm_wptr  = 0;
    ms_ctl->mspcm_rptr  = 0;
    ms_ctl->msdata_ptr  = 0;
    ms_ctl->msdata_rptr = 0;
    ms_ctl->msdata_len  = 0;

    ms_ctl->adpcm_temp.index_c = 0;
    ms_ctl->adpcm_temp.index_d = 0;
    ms_ctl->adpcm_temp.predval_c = 0;
    ms_ctl->adpcm_temp.predval_d = 0;
    decoder_flg = 0;
    set_energy(ENERGY_THT);

    ms_speed_coff = speed_coff;                         // 范围                         SPEED_DOWN 至 SPEED_UP
    ms_pitch_coff = pitch_coff;                //外部给的变速变调参数


#if DAC_AUTO_MUTE_EN
    digit_auto_mute_set(AUTO_MUTE_ENABLE, 0, 0, 0xff);//关闭自动mute
#endif
    clear_dac_dma_buf(1);
    dac_reg_isr_cb_api(3, dac_ms_isr_callback);//回调函数注册,设置对应的dac回调
    //这里要设置回调函数


    dac_set_samplerate(16000, 0);
    dac_channel_on(MUSIC_CHANNEL, FADE_OFF);


    // ladc_reg_init(ENC_MIC_CHANNEL,LADC_SR16000);
    ladc_reg_isr_cb_api(ms_ladc_callback);
    ladc_enable(ENC_MIC_CHANNEL, LADC_SR16000, VCOMO_EN);
    ladc_mic_gain(30, 0); //设置mic音量,这一句要在ladc_enable(ENC_MIC_CHANNEL,LADC_SR16000, VCOMO_EN);后面

    ms_ctl->ms_start_enable = 1;
#ifdef MQC_MS_DEBUG
    ad_to_da_enable = 1;
#endif
}

void ms_close()
{
    puts("===ms_close=======\n");
    if (!ms_ctl) {
        puts("ms ctl is NULL\n");
        return;
    }
    //先关闭相关的标志位

    ms_ctl->ms_start_enable = 0;
    ms_ctl->ms_play_enable = MS_PLAY_DISABLE;

    //关闭对应的ladc dac通道
    ladc_close(ENC_MIC_CHANNEL);
    dac_channel_off(MUSIC_CHANNEL, FADE_ON);
    // 这里要把回调设回正常
#ifndef ENCODE_FLASH_API
    extern void dac_isr_callback(void *dac_buf, u8 buf_flag);
    dac_reg_isr_cb_api(3, dac_isr_callback);
#else
    extern void dac_isr_callback_fuc(void *dac_buf, u8 buf_flag);
    dac_reg_isr_cb_api(3, dac_isr_callback_fuc);
#endif

#if DAC_AUTO_MUTE_EN
    digit_auto_mute_set(AUTO_MUTE_CFG, 4, 1200, 200);//恢复自动mute
#endif
    OSTaskQFlush(ms_ctl->ms_father_task);
//    if(ms_ctl!=NULL)
//    {
//        free(ms_ctl);
//        ms_ctl = 0;
//    }
    ms_ctrl_hdl_destroy(&ms_ctl);

    if (ps_obj != NULL) {
        puts("free ps obj\n");
        free(ps_obj);
        ps_obj = NULL;
    }
}

#endif

void ms_calinit()
{
    unsigned int  buflen;
    EF_PS_PARM *parm_obj;
    ps_audio_IO *ps_audio_obj;

    parm_obj = calloc(1, sizeof(EF_PS_PARM));
    ps_audio_obj = calloc(1, sizeof(ps_audio_IO));       //为参数申请空间

    if (parm_obj == NULL || ps_audio_obj == NULL) {
        printf("ms calinit fail !!!!!!!\n");
        return;
    }

    //parm_obj->speed_coff * parm_obj->pitch_coff = 160 *16000;
    parm_obj->speed_coff = ms_speed_coff;
    parm_obj->pitch_coff = ms_pitch_coff; //20000;//16000;//10000;//1000;//21000;

    // printf("speed=%d,coff=%d\n",parm_obj->speed_coff,parm_obj->pitch_coff);

    parm_obj->pitch_on = 1;                                //魔音参数
    parm_obj->speed_on = 1;
    parm_obj->robort_on = ms_index.robort_flag;
    ps_audio_obj->output = output_ps;                  //output的函数指针
    /*  这里开始是调用主流程 */
    test_moyin_ops = get_moyin_obj();                  //获取函数结构体指针

    buflen = test_moyin_ops->need_buf();
    ps_obj = malloc(buflen);                                 //为魔音申请空间资源
    if (ps_obj == NULL) {
        printf("ms cal init fail !!!!!!\n");
        while (1);
    }

    test_moyin_ops->open(ps_obj, parm_obj, ps_audio_obj);   //传参

    free(parm_obj);
    free(ps_audio_obj);                                      //参数空间可以释放了
}

void ms_adpcm_coder()
{
    if (!ms_ctl) {
        puts("ms ctl is NULL1\n");
        return;
    }

    adpcm_coder_ms((char *)&ms_ctl->ms_buf[ms_ctl->ms_bufnum ^ 1],
                   (char *)&ms_ctl->ms_adpcmbuf[ms_ctl->ms_adpcm_ptr],
                   (int *)&ms_ctl->adpcm_temp.index_c,
                   (int *)&ms_ctl->adpcm_temp.predval_c);

    if ((decoder_flg == 0) && (ms_ctl->ms_adpcm_ptr == START_BUFFER_LEN)) {
        decoder_flg = 1;
        temp_adpcmptr = 0;
    }

    if (decoder_flg == 1) {
        adpcm_decoder((char *)&ms_ctl->ms_adpcmbuf[temp_adpcmptr],
                      (short *)&ms_ctl->outbuf2[0],
                      (int *)&ms_ctl->adpcm_temp.index_d,
                      (int *)&ms_ctl->adpcm_temp.predval_d);
        temp_adpcmptr += 16;
        if (temp_adpcmptr >= RECODER_LEN) {
            temp_adpcmptr = 0;
        }
    }

    ms_ctl->ms_adpcm_ptr += 16;
    if (ms_ctl->ms_adpcm_ptr >= RECODER_LEN) {
        ms_ctl->ms_adpcm_ptr = 0;
    }

    if (decoder_flg == 0) {
        ms_ctl->ms_rec_len_buffer += 16;
        if (ms_ctl->ms_rec_len_buffer >= START_BUFFER_LEN) {
            ms_ctl->ms_rec_len_buffer = START_BUFFER_LEN;
        }
    }

    if (decoder_flg == 2) {
        decoder_flg = 3;
        ms_ctl->ms_adpcm_startrec_ptr = ms_ctl->ms_adpcm_ptr;//记录开始录音的位置
        temp_index_d = ms_ctl->adpcm_temp.index_d;
        temp_predval_d = ms_ctl->adpcm_temp.predval_d;
    }
}


void magicsound_adpcm_decoder()
{
    u32 i;
//    u32 cpu_sr;
    if (!ms_ctl) {
        puts("ms ctl is NULL2\n");
        return;
    }

    while (1) {
        if (ms_ctl->mspcm_len < 1024) {
            //adpcm_decoder(&ms_ctl->ms_adpcmbuf[ms_ctl->ms_adpcm_ptr],&ms_ctl->mspcm_buf[ms_ctl->mspcm_wptr],&ms_ctl->adpcm_temp.index_d,&ms_ctl->adpcm_temp.predval_d);
            run_adpcm_decoder();
            ms_ctl->ms_adpcm_ptr += 16;
            ms_ctl->adpcm_len += 16;
            if (ms_ctl->ms_adpcm_ptr >= RECODER_LEN) {
                ms_ctl->ms_adpcm_ptr -= RECODER_LEN;
            }

            ms_ctl->mspcm_wptr += 64;
            ms_ctl->mspcm_len += 64;
            if (ms_ctl->mspcm_wptr >= 1024 * 2) {
                ms_ctl->mspcm_wptr = 0;
            }
            if (ms_ctl->adpcm_len >= ms_ctl->ms_rec_len) {
                ms_ctl->adpcm_len = 0;
                ms_ctl->ms_play_enable = MS_PLAY_DISABLE;
                free(ps_obj);
                ps_obj = 0;
                // set_adcbuf_len(32);
                JL_AUDIO->DAC_LEN = DAC_SAMPLE_POINT;//32 points
                os_taskq_post(ms_ctl->ms_father_task, 1, MSG_MS_REC_START_CMD);
                return;
            }
        }

        //这个判断要等魔音处理完的数据buf有空间才继续魔音处理。不做这个判断快慢速会丢失数据从而不正常
        if ((1024 * 2 - ms_ctl->msdata_len >= 160 * 2) && (ms_ctl->mspcm_len >= ms_speed_coff * 2)) {
            for (i = 0; i < ms_speed_coff * 2; i++) {
                ms_ctl->ms_inbuf[i] = ms_ctl->mspcm_buf[ms_ctl->mspcm_rptr];

                ms_ctl->mspcm_rptr++;
                if (ms_ctl->mspcm_rptr >= 1024 * 2) {
                    ms_ctl->mspcm_rptr = 0;
                }
            }
            ms_ctl->mspcm_len -= ms_speed_coff * 2;
            test_moyin_ops->run(ps_obj, (short *)ms_ctl->ms_inbuf, NULL);
            // output_ps(NULL,(short *)ms_ctl->ms_inbuf,ms_speed_coff);
            //	output_ps(NULL,(short *)testsindata,160);

        }

        if (ms_ctl->msdata_len >= 64) {
            memcpy(&ms_ctl->outbuf2[0], &ms_ctl->msdata_buf[ms_ctl->msdata_rptr], 64) ;
            ms_ctl->msdata_len -= 64;
            ms_ctl->msdata_rptr += 64;
            if (ms_ctl->msdata_rptr >= 1024 * 2) {
                ms_ctl->msdata_rptr = 0;
            }
            return;
        }
    }
}


void ms_play_init(void)
{

    if (!ms_ctl) {
        puts("ms ctl is NULL2\n");
        return;
    }
    ms_ctl->adpcm_temp.index_d = temp_index_d;
    ms_ctl->adpcm_temp.predval_d = temp_predval_d;
    ms_ctl->ms_adpcm_ptr = ms_ctl->ms_adpcm_startrec_ptr;

    ms_ctl->mspcm_wptr  = 0;
    ms_ctl->mspcm_rptr  = 0;
    ms_ctl->msdata_ptr  = 0;
    ms_ctl->msdata_rptr = 0;
    ms_ctl->msdata_len  = 0;

    //不设成64dac_isr那边推buffer就不对应了。
    //set_adcbuf_len(64);
    ms_calinit();
    ms_ctl->ms_play_enable = MS_PLAY_ENABLED;
}


int adpcm_coder_unit(short indata, int *index, int *valpred)
{
    unsigned char  delta;				/* Current adpcm output value */
    int step;				/* Stepsize */
    int diff, sign, vpdiff;			/* place to keep next 4-bit value */

    step = stepsizeTable[*index];

    diff = indata - (*valpred);
    sign = (diff < 0) ? 8 : 0;
    if (sign) {
        diff = (-diff);
    }
    delta = 0;
    vpdiff = (step >> 3);

    if (diff >= step) {
        delta = 4;
        diff -= step;
        vpdiff += step;
    }
    step >>= 1;
    if (diff >= step) {
        delta |= 2;
        diff -= step;
        vpdiff += step;
    }
    step >>= 1;
    if (diff >= step) {
        delta |= 1;
        vpdiff += step;
    }

    /* Step 3 - Update previous value */
    if (sign) {
        (*valpred) -= vpdiff;
    } else {
        (*valpred) += vpdiff;
    }
    delta |= sign;
    *index += indexTable[delta];
    if (*index < 0) {
        *index = 0;
    }
    if (*index > 88) {
        *index = 88;
    }
    return delta;
}


void adpcm_coder_ms(char indata[], char outdata[], int *index, int *valpred)
{
    unsigned char *inp;					/* Input buffer pointer */
    signed char *outp;			/* output buffer pointer */
    unsigned char delta[2];				/* Current adpcm output value */
    int i, j;
    short inputbuffer;

    outp = (signed char *)outdata;
    inp = (unsigned char *)indata;

    for (j = 0; j < 16; j++) {
        for (i = 0; i < 2; i++) {
            inputbuffer = ((*(char *)inp) & 0xff) | (*((char *)inp + 1) << 8);
            delta[i] = adpcm_coder_unit(inputbuffer, index, valpred);
            inp += 2;
        }
        *(outp++) = (delta[1] << 4) | delta[0];
    }
}


void adpcm_decoder_unit1(unsigned char  delta, int *valpred, int *index)
{
    int sign;				/* Current adpcm sign bit */
    int step;			/* Stepsize */
    int vpdiff;				/* Current change to valpred */

    step = stepsizeTable[(*index)];

    vpdiff = 0;
    if (delta & 4) {
        vpdiff += step;
    }
    if (delta & 2) {
        vpdiff += (step >> 1);
    }
    if (delta & 1) {
        vpdiff += (step >> 2);
    }
    vpdiff += (step >> 3);
    sign = delta & 8;
    if (sign) {
        *valpred -= vpdiff;
    } else {
        *valpred += vpdiff;
    }

    if (*valpred > 32767) {
        *valpred = 32767;
    } else if (*valpred < -32768) {
        *valpred = -32768;
    }

    (*index) += indexTable[delta];
    if ((*index) < 0) {
        (*index) = 0;
    }
    if ((*index) > 88) {
        (*index) = 88;
    }
}


void adpcm_decoder(char indata[], short outdata[], int *index, int *valpred)
{
    signed char *inp;	    /* Input buffer pointer */
    short *outp;			/* output buffer pointer */
    unsigned char  delta[2];			/* Current adpcm output value */
    int inputbuffer;		/* place to keep next 4-bit value */
    int i, j;		/* toggle between inputbuffer/input */

    outp = outdata;
    inp = (signed char *)indata;

    for (j = 0; j < 16; j++) {
        inputbuffer = *inp++;
        delta[0] = inputbuffer & 0xf;
        delta[1] = (inputbuffer >> 4) & 0xf;

        for (i = 0; i < 2; i++) {
            adpcm_decoder_unit1(delta[i], valpred, index);
            *(outp++) = *valpred;
        }
    }
}



void ms_task()
{
    int msg;
//    s16 i,*rptr;
    puts("----------ms_task-----------");
    //os_taskq_post("MsTask", 1, MSG_MS_MODE_START_CMD);
    //speed：魔音的语速，160是正常语速   spitch：魔音的语调，16000是正常的
    ms_init_and_start(ms_index.speed_coff, ms_index.pitch_coff, "MsTask");
    puts("ms init and start\n");

    while (1) {
        os_taskq_pend(0, 1, &msg);

        switch (msg) {
        //    case MSG_MS_MODE_START_CMD://第一次进入魔音时的初始化
        //        puts("ms init and start\n");
        //        ms_init_and_start(160,25000,"MsTask");
        //        break;
        case MSG_MS_ADPCM_CODER_CMD://录音过程中的ADPCM压缩
            ms_adpcm_coder();
            break;
        case MSG_MS_REC_END_CMD:
            ms_ctl->ms_start_enable = 0;
            os_taskq_post(ms_ctl->ms_father_task, 1, MSG_MS_PLAY_START_CMD);
            break;
        case MSG_MS_REC_START_CMD:
            //重新进入声音检测:步骤1
            decoder_flg = 0;
            memset(ms_ctl, 0, sizeof(_MS_CTL) - sizeof(char *) - sizeof(u16)); //这里要避免把fater_task及去头计数清空
            ms_ctl->ms_start_enable = 1;
            break;
        case MSG_MS_PLAY_START_CMD://魔音回放初始化
            puts("ms play init~\n");
            ms_play_init();
            break;
        case MSG_MS_PLAY_CMD://魔音回放过程中的ADPCM解码+魔音运算
            magicsound_adpcm_decoder();
            break;
        case MSG_MS_CLOSE_CMD:
            ms_close();
            break;
        case SYS_EVENT_DEL_TASK: 				//请求删除music任务
            puts("ms_task MSG_DEL_TASK\n");
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                ms_close();
                os_task_del_res_name(OS_TASK_SELF); 	//确认可删除，此函数不再返回
            }
            break;
        default:
            break;
        }
    }

}

/*----------------------------------------------------------------------------*/
/**@brief  魔音模块启动接口
   @param speed 速度 160为正常
   @param pitch 音调 16000为正常
   @param robort 1打开机器音
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void ms_task_init(short speed, unsigned short pitch, unsigned char robort)
{
    if (speed <= 320 && speed >= 80) {
        ms_index.speed_coff = speed;
    } else {
        ms_index.speed_coff = 160;
    }


    if (pitch <= 32000 && pitch >= 8000) {
        ms_index.pitch_coff = pitch;
    } else {
        ms_index.pitch_coff = 16000;
    }

    if (robort > 0) {
        ms_index.robort_flag = 1;
    } else {
        ms_index.robort_flag = 0;
    }
    puts("ms task init\n");
    os_task_create((void (*)(void *))ms_task, (void *)0, TaskMsPrio, 32, "MsTask");
}

/*----------------------------------------------------------------------------*/
/**@brief  魔音模块关闭接口
   @param
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void ms_task_exit(void)
{
#if 0
    os_taskq_post("MsTask", 1, MSG_MS_CLOSE_CMD);
#else
    if (os_task_del_req("MsTask") != OS_TASK_NOT_EXIST) {
        os_taskq_post_event("MsTask", 1, SYS_EVENT_DEL_TASK);
        do {
            OSTimeDly(1);
        } while (os_task_del_req("MsTask") != OS_TASK_NOT_EXIST);
        puts("ms_task_exit: succ\n");
    }
#endif
}
#endif

