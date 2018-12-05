#include "usb_device.h"
#include "sdmmc/sd_host_api.h"
#include "dev_pc.h"
#include "common/msg.h"
#include "dev_manage/device_driver.h"
#include "dac/dac.h"
#include "dac/dac_api.h"
#include "vm/vm_api.h"
#include "usb/usb_slave_api.h"
#include "sdk_cfg.h"
#include "dac/ladc.h"
#include "usb/mango_dev_usb_slave.h"
#include "cbuf/circular_buf.h"
#include "common/app_cfg.h"
#ifdef TOYS_EN
#include "script_switch.h"
#endif

#ifdef MIO_USB_DEBUG
#include "mio_api/mio_api.h"
# ifdef CLASS_CONFIG
# undef CLASS_CONFIG
# endif
# define CLASS_CONFIG 	MASSSTORAGE_CLASS
#endif

#include "spi/spifat0.h"
#include "spi/spifat1.h"

#if USB_PC_EN

const u8 IMANUFACTURE_STR[] = {
    0x30, 0x03,
    'Z', 0, 'h', 0, 'u', 0, 'H', 0, 'a', 0, 'i', 0, ' ', 0,
    'J', 0, 'i', 0, 'e', 0, 'L', 0, 'i', 0, ' ', 0,
    'T', 0, 'e', 0, 'c', 0, 'h', 0, 'n', 0, 'o', 0, 'l', 0, 'o', 0, 'g', 0, 'y', 0,
};

const u8 IPRODUCT_STR[] = {
    0x16, 0x03,
    'J', 0, 'i', 0, 'e', 0, 'L', 0, 'i', 0, ' ', 0,
    'B', 0, 'R', 0, '1', 0, '7', 0,
};

const u8 SCSIInquiryData[] = {
    0x00,//  // Peripheral Device Type: direct access devices  0x05,//
    0x80,   // Removable: UFD is removable
    0x00,   // ANSI version
    0x00,   // Response Data Format: compliance with UFI
    //  0x00,   // Additional Length (Number of UINT8s following this one): 31, totally 36 UINT8s
    0x1F,
    0x00, 0x00, 0x00,   // reserved
    'B',    //"N" -- Vender information start
    'R',    //"N" -- Vender information start
    '1',    //"t"
    '7',    //"a"
    ' ',    //"c"
    ' ',    //" "
    ' ',    //" "
    ' ',   //" " -- Vend Information end
    ' ',    //"O" -- Production Identification start
    'D',    //"n"
    'E',    //"l"
    'V',    //"y"
    'I',    //"D"
    'C',    //"i"
    'E',    //"s"
    ' ',    //"k"
    'V',    //" "
    '1',    //" "
    '.',    //" "
    '0',    //" "
    '0',    //" "
    ' ',    //" "
    ' ',    //" "
    ' ',    //" " -- Production Identification end
    0x31,   //"4" -- Production Revision Level start
    0x2e,   //"."
    0x30,   //"0"
    0x30    //"5" -- Production Revision Level end
};

#define USB_PC_PROTECT_EN	0

#undef 	NULL
#define NULL				0

#define USB_SLAVE_DEV_IO_REG() \
    usb_slave_dev_io *dev_io = (usb_slave_dev_io *)(&susb_io);

#define __this(member) (dev_io->member)
/* #define __this(member) \ */
/* 		( dev_io->member = \ */
/* 		  (dev_io->member == NULL)? io_member_err_msg_output : dev_io->member \ */
/* 		) */

/* static void io_member_err_msg_output(void) */
/* { */
/* 	puts("susb io member not reg"); */
/* 	while(1); */
/* } */

#if USB_PC_PROTECT_EN

#define PROTECT_NUM		1
usb_pc_protect *protect_p = NULL;
static void usb_pc_protect_open(void)
{
    USB_SLAVE_DEV_IO_REG();

    u32 need_buf = sizeof(usb_pc_protect) + (PROTECT_NUM * 2 * sizeof(u32));
    printf("---need_buf = %d\n", need_buf);

    protect_p = malloc(need_buf);
    printf("protect_p = %x\n", protect_p);
    if (protect_p) {
        protect_p->gc_ProtectFATNum = PROTECT_NUM;
        protect_p->ProtectedFATSectorIndex[0] = 0x00;
        protect_p->ProtectedFATSectorIndex[1] = 0x04;
        //printf_buf(protect_p,3*4);
        __this(dev_ioctrl)((void *)protect_p, USB_SLAVE_MD_PROTECT);
    } else {
        puts("usb_pc_protect_malloc_err\n");
    }
}

static void usb_pc_protect_close(void)
{
    USB_SLAVE_DEV_IO_REG();
    __this(dev_ioctrl)((void *)NULL, USB_SLAVE_MD_PROTECT);

    if (protect_p) {
        free(protect_p);
        protect_p = NULL;
    }
}
#endif

/*----------------------------------------------------------------------------*/
/**@brief  PC 模式静音状态设置
   @param  mute_status：mute状态控制；fade_en：淡入淡出设置
   @return 无
   @note   void pc_dac_mute(bool mute_status, u8 fade_en)
*/
/*----------------------------------------------------------------------------*/
void pc_dac_mute(bool mute_status, u8 fade_en)
{
    dac_mute(mute_status, fade_en);
}

/*----------------------------------------------------------------------------*/
/**@brief  PC 模式DAC通道开启和音量设置
   @param  无
   @return 无
   @note   void pc_dac_on(void)
*/
/*----------------------------------------------------------------------------*/
void pc_dac_channel_on(void)
{
    dac_channel_on(UDISK_CHANNEL, FADE_ON);
//    set_sys_vol(dac_var.cur_sys_vol_l, dac_var.cur_sys_vol_r, FADE_ON);
}

/*----------------------------------------------------------------------------*/
/**@brief  PC 模式DAC通道关闭
   @param  无
   @return 无
   @note   void pc_dac_off(void)
*/
/*----------------------------------------------------------------------------*/
void pc_dac_channel_off(void)
{
    set_sys_vol(dac_ctl.sys_vol_l, dac_ctl.sys_vol_r, FADE_OFF);//resume sys vol
    dac_channel_off(UDISK_CHANNEL, FADE_ON);
//    dac_var.bMute = 0;

#if USB_PC_PROTECT_EN
    usb_pc_protect_close();	//close at last
#endif
}

/*----------------------------------------------------------------------------*/
/**@brief  PC 模式AUDIO音量设置消息钩子函数
   @param  msg：需要投递的消息
   @return 无
   @note   void hook_susb_msg(u32 msg)
*/
/*----------------------------------------------------------------------------*/
static void hook_susb_msg(u32 msg)
{
    //pc_puts("PC_VOL_POST_MSG\n");
#ifndef TOYS_EN
    os_taskq_post_msg(PC_TASK_NAME, 1, msg);
#else
    os_taskq_post_msg(SCRIPT_TASK_NAME, 1, msg);
#endif
}

/*----------------------------------------------------------------------------*/
/**@brief  从VM获取记忆音量值
   @param  none
   @return 记忆值
   @note   u8 get_usb_slave_volume_from_vm(void)
*/
/*----------------------------------------------------------------------------*/
static u8 get_usb_slave_volume_from_vm(void)
{
    u8 	pc_vol;

    //取出VM保存的上次PC音量值
    if (VM_PC_VOL_LEN != vm_read_api(VM_PC_VOL, &pc_vol)) {
        pc_vol = MAX_SYS_VOL_L;
    }
    pc_printf("vm_pc_vol=%d\n", pc_vol);
    return pc_vol;
}

/*----------------------------------------------------------------------------*/
/**@brief  从VM获取mic记忆音量值
   @param  none
   @return 记忆值
   @note   u8 get_usb_slave_mic_volume_from_vm(void)
*/
/*----------------------------------------------------------------------------*/
static u8 get_usb_slave_mic_volume_from_vm(void)
{
    u8 pc_mic_vol;

    //取出VM保存的上次PC音量值
    if (VM_PC_MIC_VOL_LEN != vm_read_api(VM_PC_MIC_VOL, &pc_mic_vol)) {
        pc_mic_vol = MAX_SYS_VOL_MIC;
    }
    pc_printf("vm_pc_mic_vol=%d\n", pc_mic_vol);
    return pc_mic_vol;
}

/*----------------------------------------------------------------------------*/
/**@brief  get left volume from vm
   @param  none
   @return left volume
   @note   u8 get_usb_slave_audio_left_volume(void)
*/
/*----------------------------------------------------------------------------*/
static u8 get_usb_slave_audio_left_volume(void)
{
    u8 spk_vol;

    spk_vol = get_usb_slave_volume_from_vm();
    spk_vol = (spk_vol >= MAX_SYS_VOL_L) ? 0xff : spk_vol << 3;

    return spk_vol;
}

/*----------------------------------------------------------------------------*/
/**@brief  get right volume from vm
   @param  none
   @return right volume
   @note   u8 get_usb_slave_audio_right_volume(void)
*/
/*----------------------------------------------------------------------------*/
static u8 get_usb_slave_audio_right_volume(void)
{
    u8 spk_vol;

    spk_vol = get_usb_slave_volume_from_vm();
    spk_vol = (spk_vol >= MAX_SYS_VOL_R) ? 0xff : spk_vol << 3;

    return spk_vol;
}

/*----------------------------------------------------------------------------*/
/**@brief  get right volume from vm
   @param  none
   @return right volume
   @note   u8 get_usb_slave_audio_right_volume(void)
*/
/*----------------------------------------------------------------------------*/
static u16 get_usb_slave_audio_mic_volume(void)
{
    u16 mic_vol;

    mic_vol = get_usb_slave_mic_volume_from_vm();
    mic_vol = (mic_vol >= MAX_SYS_VOL_MIC) ? 0x170d : mic_vol * 192;

    return mic_vol;
}

/*----------------------------------------------------------------------------*/
/**@brief  reset class config
   @param  dev_io:usb slave driver io
   @return none
   @note   void usb_slave_class_config_reset(usb_slave_dev_io *dev_io)
*/
/*----------------------------------------------------------------------------*/
static void usb_slave_class_config_reset(usb_slave_dev_io *dev_io)
{
    u32 parm = CLASS_CONFIG;

    __this(dev_ioctrl)(&parm, USB_SLAVE_CLASS_CONFIG_RESET);
}

/*----------------------------------------------------------------------------*/
/**@brief  register card reader
   @param  dev_io:usb slave driver io
   @return none
   @note   void usb_slave_card_reader_reg(usb_slave_dev_io *dev_io)
*/
/*----------------------------------------------------------------------------*/
static void usb_slave_card_reader_reg(usb_slave_dev_io *dev_io)
{
    card_reader_parm *reg_parm = NULL;

    /* memset(reg_parm, 0x00, sizeof(card_reader_parm)); */
    /*注册读卡器的SD控制器接口*/
#if SDMMC0_EN
    /* card_reader_parm reset_parm; */
    /* reg_parm = &reset_parm; */
    /* reset_parm.card_r_parm.usb_get_dev_capacity 	= xxx; */
    /* reset_parm.card_r_parm.sDevName 				= xxx; */
    /* reset_parm.card_r_parm.bWriteProtectFlag		= xxx; */
    __this(dev_ioctrl)(reg_parm, USB_SLAVE_CARD_READER0_REG);
#endif

#if SDMMC1_EN
    __this(dev_ioctrl)(reg_parm, USB_SLAVE_CARD_READER1_REG);
#endif

#if VR_SPI0_EN
    {
        u8 cnt = 10;
        while (cnt--) {
            if (get_spifat0_working()) {
                break;
            }
            os_time_dly(2);
        }
        if (get_spifat0_working() && spifat0_ctrl_open(0, 1)) {
            pc_puts("DEV_SPIFAT_0\n");
            sUSB_DEV_IO spi0_dev;
            my_memset((u8 *)&spi0_dev, 0x0, sizeof(sUSB_DEV_IO));

            spi0_dev.card_r_parm.usb_get_dev_capacity = spifat0_ctrl_get_capacity;
            spi0_dev.usb_write_dev = spifat0_ctrl_write;
            spi0_dev.usb_read_dev = spifat0_ctrl_read;
            spi0_dev.usb_dev_init = spifat0_ctrl_init;
            spi0_dev.usb_wiat_sd_end = spifat0_ctrl_wiat;
            spi0_dev.bAttr = DEV_SPIFAT_0;

            extern USB_SLAVE_VAR *g_susb_var;
            extern void usb_device_register(sUSB_DEV_IO * ptr);
            memcpy((u8 *) & (g_susb_var->ep0_var.spi0_dev), (u8 *)&spi0_dev, sizeof(sUSB_DEV_IO));
            //register card_reader
            usb_device_register((sUSB_DEV_IO *) & (g_susb_var->ep0_var.spi0_dev));
        } else {
            pc_puts("DEV_SPIFAT_0 err\n");
        }
    }
#endif

#if VR_SPI1_EN
    {
        u8 cnt = 10;
        while (cnt--) {
            if (get_spifat1_working()) {
                break;
            }
            os_time_dly(2);
        }
        if (get_spifat1_working() && spifat1_ctrl_open(0, 1)) {
            pc_puts("DEV_SPIFAT_1\n");
            sUSB_DEV_IO spi1_dev;
            my_memset((u8 *)&spi1_dev, 0x0, sizeof(sUSB_DEV_IO));

            spi1_dev.card_r_parm.usb_get_dev_capacity = spifat1_ctrl_get_capacity;
            spi1_dev.usb_write_dev = spifat1_ctrl_write;
            spi1_dev.usb_read_dev = spifat1_ctrl_read;
            spi1_dev.usb_dev_init = spifat1_ctrl_init;
            spi1_dev.usb_wiat_sd_end = spifat1_ctrl_wiat;
            spi1_dev.bAttr = DEV_SPIFAT_1;

            extern USB_SLAVE_VAR *g_susb_var;
            extern void usb_device_register(sUSB_DEV_IO * ptr);
            memcpy((u8 *) & (g_susb_var->ep0_var.spi1_dev), (u8 *)&spi1_dev, sizeof(sUSB_DEV_IO));
            //register card_reader
            usb_device_register((sUSB_DEV_IO *) & (g_susb_var->ep0_var.spi1_dev));
        } else {
            pc_puts("DEV_SPIFAT_1 err\n");
        }
    }

#endif

}

/*----------------------------------------------------------------------------*/
/**@brief  init usb slave parm
   @param  dev_io:usb slave driver io
   @return init status
   @note   s32 usb_slave_var_init(usb_slave_dev_io *dev_io)
*/
/*----------------------------------------------------------------------------*/
static s32 usb_slave_var_init(usb_slave_dev_io *dev_io)
{
    usb_slave_init_parm susb_init_parm;

    susb_init_parm.os_msg_post_init = (void (*)(void))hook_susb_msg;
    susb_init_parm.vol_left 		= get_usb_slave_audio_left_volume();
    susb_init_parm.vol_right 		= get_usb_slave_audio_right_volume();
    susb_init_parm.vol_mic 			= get_usb_slave_audio_mic_volume();

    return __this(dev_init)(&susb_init_parm);
}

#include "sound_mix_api.h"
#include "notice_player.h"
#define PC_SRC_TASK_NAME 				"PCSrcTASK"
#define USB_SPK_MIX_NAME				"SPK_MIX"
#define USB_SPK_MIX_OUTPUT_BUF_LEN		(4*1024)
#define USB_SPK_MIX_OUTPUT_DEC_LIMIT	(50)//这是一个百分比，表示启动解码时输出buf的数据量百分比

extern cbuffer_t audio_cb;

static u32 usb_spk_decode_get_output_data_len(void *priv)
{
    SOUND_MIX_CHL *mix_chl = (SOUND_MIX_CHL *)priv;

    return __sound_mix_chl_get_data_len(mix_chl);
}

static u32 usb_spk_set_info(void *priv, dec_inf_t *inf, tbool wait)
{
    printf("___________sr = %d\n", inf->sr);
    dac_set_samplerate(inf->sr, wait);
    return 0;
}

static u32 get_obuf_len(void *priv)
{
    return audio_cb.data_len;
}

static void dac_write_obuf(void *priv, u8 *obuf, u32 Len)
{
    if (!cbuf_write(&audio_cb, obuf, Len)) {
        //PORTA_DIR &= ~BIT(9);
        //PORTA_OUT ^= BIT(9);
    }

}

static void usb_sbk_decode_before_callback(USB_SPK_DATA_STREAM *stream, void *priv)
{
    printf("---------------------usb spk bf----------------------\n");
    if (stream == NULL) {
        return ;
    }
#if SOUND_MIX_GLOBAL_EN
    notice_player_stop();
    sound_mix_api_close();
    sound_mix_api_init(48000L);
    SOUND_MIX_OBJ *mix_obj = sound_mix_api_get_obj();//(SOUND_MIX_OBJ *)priv;
    if (mix_obj == NULL) {
        return ;
    }

    printf("usb spk to mix!!!\n");
    SOUND_MIX_P mix_p;
    memset((u8 *)&mix_p, 0x0, sizeof(SOUND_MIX_P));
    mix_p.key = USB_SPK_MIX_NAME;
    mix_p.out_len = USB_SPK_MIX_OUTPUT_BUF_LEN;
    /* mix_p.set_info = &(mapi->dop_api->io->set_music_info); */
    mix_p.set_info_cb = &stream->set_info_cb;
    mix_p.input = &stream->output;
    mix_p.op_cb = &stream->op_cb;
    mix_p.limit = USB_SPK_MIX_OUTPUT_DEC_LIMIT;
    mix_p.max_vol = DAC_DIGIT_TRACK_MAX_VOL;
    mix_p.default_vol = DAC_DIGIT_TRACK_DEFAULT_VOL;
    mix_p.src_p.task_name = PC_SRC_TASK_NAME;
    mix_p.src_p.task_prio = TaskPCsrcPrio;
    stream->mix = (void *)sound_mix_chl_add(mix_obj, &mix_p);
    ASSERT(stream->mix);
    stream->getlen.cbk = usb_spk_decode_get_output_data_len;
    stream->getlen.priv = stream->mix;
    stream->obuf_size = USB_SPK_MIX_OUTPUT_BUF_LEN;
#else

    stream->set_info_cb.priv = NULL;
    stream->set_info_cb._cb = usb_spk_set_info;
    stream->output.output = (void *)dac_write_obuf;
    stream->obuf_size = cbuf_get_space(&audio_cb);
    stream->getlen.priv = NULL;
    stream->getlen.cbk = (void *)get_obuf_len;
#endif//SOUND_MIX_GLOBAL_EN
}

static void usb_sbk_decode_after_callback(USB_SPK_DATA_STREAM *stream, void *priv)
{
    printf("-------------------- usb spk af------------------------\n");
    if (stream == NULL) {
        return ;
    }

#if SOUND_MIX_GLOBAL_EN
    if (stream->mix) {
        sound_mix_chl_del_by_chl((SOUND_MIX_CHL **)&stream->mix);
    }

    notice_player_stop();
    sound_mix_api_close();
    sound_mix_api_init(SOUND_MIX_API_DEFUALT_SAMPLERATE);
#endif//SOUND_MIX_GLOBAL_EN

}

/*----------------------------------------------------------------------------*/
/**@brief  init usb slave moudle
   @param  none
   @return init status
   @note   s32 app_usb_slave_init(void)
*/
/*----------------------------------------------------------------------------*/
s32 app_usb_slave_init(void)
{
    s32 res;
    USB_SLAVE_DEV_IO_REG();

    usb_speaker_output_before_register(usb_sbk_decode_before_callback, NULL);
    usb_speaker_output_after_register(usb_sbk_decode_after_callback, NULL);

    /*usb slave var init*/
    res = usb_slave_var_init(dev_io);
    if (res == NULL) {
        puts("susb var malloc error\n");
        return NULL;
    }

    /*audio dac init*/
    //for speaker
    if (CLASS_CONFIG & SPEAKER_CLASS) {
        /* dac_set_samplerate(48000, 0);     //DAC采样率设置为48K */
        pc_dac_channel_on(); //开PC DAC模拟通道
    }

    /*MASS STORAGE、AUDIO和HID功能重新设置:默认全开*/
    usb_slave_class_config_reset(dev_io);

    /*register card_reader 属性*/
    usb_slave_card_reader_reg(dev_io);

    /*挂载USB SLAVE设备*/
    __this(dev_open)(NULL);

    //for mic
    if (CLASS_CONFIG & MIC_CLASS) {
        ladc_reg_init(ENC_MIC_CHANNEL, LADC_SR48000);
    }

#if USB_PC_PROTECT_EN
    usb_pc_protect_open();
#endif

    return res;
}

/*----------------------------------------------------------------------------*/
/**@brief  close usb slave moudle
   @param  none
   @return close status
   @note   s32 app_usb_slave_close(void)
*/
/*----------------------------------------------------------------------------*/
s32 app_usb_slave_close(void)
{
    s32 res;
    USB_SLAVE_DEV_IO_REG();

    res =  __this(dev_close)(NULL);

    if (CLASS_CONFIG & SPEAKER_CLASS) {
        pc_dac_channel_off(); //关PC DAC通道
    }
    if (CLASS_CONFIG & MIC_CLASS) {
        ladc_close(ENC_MIC_CHANNEL);	//关闭ladc模块
    }

    return res;
}
#define SD_BLOCK_SIZE   512
#define SD_PROTECT_EN 1
u32 protect_addr1;
u32 protect_addr2;
u32 protect_addr3;
u32 protect_addr4;
u32 protect_len1;
u32 protect_len2;
u32 protect_len3;
u32 protect_len4;
u8 protect_enable = 0;
s32 sd0_read_api(u8 *buf, u32 lba, u32 len);
s32 sd1_read_api(u8 *buf, u32 lba, u32 len);

u8 *d_buf = NULL;
#define SD_PROTECT_SECTOR_ADDR 1
extern s32 sd0_read_api(u8 *buf, u32 lba, u32 len);
extern s32 sd1_read_api(u8 *buf, u32 lba, u32 len);
void susb_cardreader_addrs_protect(u8 sd_num)
{
    u32 res;

    d_buf = malloc(SD_BLOCK_SIZE);
    if (d_buf == NULL) {
        printf("malloc d_buf error while(1) \n");
        while (1);
    }
    if (sd_num == DEV_SDCRAD_0) {
        res = sd0_read_api(d_buf, SD_PROTECT_SECTOR_ADDR, 1);
        printf("-----res:%d \n ", res);
        printf_buf(d_buf, 512);
        if (d_buf[508] == 0x77 && d_buf[509] == 0x77 && d_buf[510] == 0x88 && d_buf[511] == 0x88) {
#if SD_PROTECT_EN
            protect_enable  = 1;
            printf("\n tf card protect enable");
#else
            protect_enable  = 0;
            printf("\n tf card protect disable");
#endif
        }
        if (res != 0) {
            puts("protect_read_sd0\n");
            protect_addr1 = (d_buf[0] << 28) | (d_buf[1] << 16) | (d_buf[2] << 8) | (d_buf[3]);
            protect_len1 = (d_buf[4] << 28) | (d_buf[5] << 16) | (d_buf[6] << 8) | (d_buf[7]);

            protect_addr2 = (d_buf[8] << 28) | (d_buf[9] << 16) | (d_buf[10] << 8) | (d_buf[11]);
            protect_len2 = (d_buf[12] << 28) | (d_buf[13] << 16) | (d_buf[14] << 8) | (d_buf[15]);

            protect_addr3 = (d_buf[16] << 28) | (d_buf[17] << 16) | (d_buf[18] << 8) | (d_buf[19]);
            protect_len3 = (d_buf[20] << 28) | (d_buf[21] << 16) | (d_buf[22] << 8) | (d_buf[23]);

            protect_addr4 = (d_buf[24] << 28) | (d_buf[25] << 16) | (d_buf[26] << 8) | (d_buf[27]);
            protect_len4 = (d_buf[28] << 28) | (d_buf[29] << 16) | (d_buf[30] << 8) | (d_buf[31]);


        }
    } else if (sd_num == DEV_SDCRAD_1) {
        puts("protect_read_sd1\n");
        res = sd1_read_api(d_buf, SD_PROTECT_SECTOR_ADDR, 1);
        //printf_buf(d_buf,512);
        if (d_buf[508] == 0x77 && d_buf[509] == 0x77 && d_buf[510] == 0x88 && d_buf[511] == 0x88) {
#if SD_PROTECT_EN
            protect_enable  = 1;
            puts("\n tf card protect enable");
#else
            protect_enable  = 0;
            puts("\n tf card protect disable");
#endif
        }
        //printf_buf(d_buf,512);
        if (res != 0) {
            protect_addr1 = (d_buf[0] << 28) | (d_buf[1] << 16) | (d_buf[2] << 8) | (d_buf[3]);
            protect_len1 = (d_buf[4] << 28) | (d_buf[5] << 16) | (d_buf[6] << 8) | (d_buf[7]);

            protect_addr2 = (d_buf[8] << 28) | (d_buf[9] << 16) | (d_buf[10] << 8) | (d_buf[11]);
            protect_len2 = (d_buf[12] << 28) | (d_buf[13] << 16) | (d_buf[14] << 8) | (d_buf[15]);

            protect_addr3 = (d_buf[16] << 28) | (d_buf[17] << 16) | (d_buf[18] << 8) | (d_buf[19]);
            protect_len3 = (d_buf[20] << 28) | (d_buf[21] << 16) | (d_buf[22] << 8) | (d_buf[23]);

            protect_addr4 = (d_buf[24] << 28) | (d_buf[25] << 16) | (d_buf[26] << 8) | (d_buf[27]);
            protect_len4 = (d_buf[28] << 28) | (d_buf[29] << 16) | (d_buf[30] << 8) | (d_buf[31]);
            printf("\n protect_addr1  0x%x  protect_len1 0x%x", protect_addr1, protect_len1);
            printf("\n protect_addr2  0x%x  protect_len2 0x%x", protect_addr2, protect_len2);
            printf("\n protect_addr3  0x%x  protect_len3 0x%x", protect_addr3, protect_len3);
            printf("\n protect_addr4  0x%x  protect_len4 0x%x", protect_addr4, protect_len4);
        }
    } else {
        puts("protect_read_sd_err\n");
    }

    free(d_buf);
}
#if 1
extern u8 MCU_SRAMToUSB(void *pBuf, u16 uCount);
extern u8 MCU_USBToSRAM(void *pBuf, u16 uCount, sUSB_SLAVE_MASS *mass_var);
volatile u8 flash_buf[18] AT(.data_usb);
SUSB_SCSI_INTERCEPT_RESULT susb_scsi_intercept(void *var, u8 sd_num)
{
    USB_SCSI_CBW *scsi_cbw;
    u32 w_address;

    memset((void *)flash_buf, 0x00, 18);

    scsi_cbw = &(((sUSB_SLAVE_MASS *)var)->USB_bulk_only_Rx.CBW);
    ((sUSB_SLAVE_MASS *)var)->private_scsi.protect_en = protect_enable;

    switch (scsi_cbw->operationCode) {
    /*case 0x28:
        return FALSE;*/
    case 0xfd:
        if (scsi_cbw->LUN == 0x0a &&
            scsi_cbw->LBA[0] == 0 &&
            scsi_cbw->LBA[1] == 0 &&
            scsi_cbw->LBA[2] == 0 &&
            scsi_cbw->LBA[3] == 0 &&
            scsi_cbw->Reserved == 0 &&
            scsi_cbw->LengthH == 0 &&
            scsi_cbw->LengthL == 0 &&
            scsi_cbw->XLength == 0) {
            puts("\n 写序列号");
            MCU_USBToSRAM((void *)flash_buf, 18, var);
            //AD200_vm_write(VM_XULIEHAO,(const void *)flash_buf,18);
            printf_buf((u8 *)flash_buf, 18);
            return SCSI_INTERCEPT_SUCC;
        }
        break;

    case 0xfc:
        if (scsi_cbw->LUN == 0x0a &&
            scsi_cbw->LBA[0] == 0 &&
            scsi_cbw->LBA[1] == 0 &&
            scsi_cbw->LBA[2] == 0 &&
            scsi_cbw->LBA[3] == 0 &&
            scsi_cbw->Reserved == 0 &&
            scsi_cbw->LengthH == 0 &&
            scsi_cbw->LengthL == 0 &&
            scsi_cbw->XLength == 0) {
            puts("\n 读序列号");
            //AD200_vm_read(VM_XULIEHAO,(void*)flash_buf,18);
            printf_buf((u8 *)flash_buf, 18);
            MCU_SRAMToUSB((void *)flash_buf, 18);
            return SCSI_INTERCEPT_SUCC;
        }
        break;

    case 0x2a:
        if (!protect_enable) {
            return SCSI_INTERCEPT_FAIL;
        }
        puts("intercept_WRITE\n");
        put_u32hex0(scsi_cbw->LBA[0]);
        put_u32hex0(scsi_cbw->LBA[1]);
        put_u32hex0(scsi_cbw->LBA[2]);
        put_u32hex0(scsi_cbw->LBA[3]);
        w_address = (scsi_cbw->LBA[0] << 28) | (scsi_cbw->LBA[1] << 16) | (scsi_cbw->LBA[2] << 8) | (scsi_cbw->LBA[3]);
        put_u32hex0(w_address);
        if (((w_address >= protect_addr1) && (w_address <= (protect_addr1 + protect_len1 - 1))) || \
            ((w_address >= protect_addr2) && (w_address <= (protect_addr2 + protect_len2 - 1))) || \
            ((w_address >= protect_addr3) && (w_address <= (protect_addr3 + protect_len3 - 1))) || \
            ((w_address >= protect_addr4) && (w_address <= (protect_addr4 + protect_len4 - 1)))) {
#if SD_PROTECT_EN
            puts("intercept_unknow\n");
            return SCSI_INTERCEPT_UNKNOW_WRITE; //拦截写

#else
            puts("\n sd protect disabel");
#endif
        }

        return SCSI_INTERCEPT_FAIL;

    default:
#ifdef MIO_USB_DEBUG
        return mio_scsi_intercept(var);
#endif
        break;
    }

    return SCSI_INTERCEPT_FAIL;
}
#endif
/*----------------------------------------------------------------------------*/
/**@brief  run card reader moudle
   @param  none
   @return run status
   @note   s32 app_usb_slave_card_reader(u32 cmd)
*/
/*----------------------------------------------------------------------------*/
s32 app_usb_slave_card_reader(u32 cmd)
{
    USB_SLAVE_DEV_IO_REG();

    return __this(dev_ioctrl)(&cmd, USB_SLAVE_RUN_CARD_READER);
}

/*----------------------------------------------------------------------------*/
/**@brief  run hid moudle
   @param  none
   @return run status
   @note   s32 app_usb_slave_hid(u32 hid_cmd)
*/
/*----------------------------------------------------------------------------*/
s32 app_usb_slave_hid(u32 hid_cmd)
{
    USB_SLAVE_DEV_IO_REG();

    return __this(dev_ioctrl)(&hid_cmd, USB_SLAVE_HID);
}

/*----------------------------------------------------------------------------*/
/**@brief  PC 模式AUDIO音量设置
   @param  pc_mute_status：mute状态
   @return AUDIO音量
   @note   u8 app_pc_set_speaker_vol(u32 pc_mute_status)
*/
/*----------------------------------------------------------------------------*/
u8 app_pc_set_speaker_vol(u32 pc_mute_status)
{
    u32 spk_vol_l, spk_vol_r;
    USB_SLAVE_DEV_IO_REG();

    __this(dev_ioctrl)(&spk_vol_l, USB_SLAVE_GET_SPEAKER_VOL);
    spk_vol_l &= 0xff;
    //pc_printf("api_vol=%x, %x\n", ep0_var->usb_class_info.bAudioCurL[0], ep0_var->usb_class_info.bAudioCurR[0]);
    if (spk_vol_l) {
        spk_vol_l >>= 3;
        spk_vol_l = (spk_vol_l == 0) ? 1 : spk_vol_l;
        spk_vol_l = (spk_vol_l >= MAX_SYS_VOL_L) ? MAX_SYS_VOL_L : spk_vol_l;
    }

    __this(dev_ioctrl)(&spk_vol_r, USB_SLAVE_GET_SPEAKER_VOL);
    spk_vol_r >>= 8;
    spk_vol_r &= 0xff;
    if (spk_vol_r) {
        spk_vol_r >>= 3;
        spk_vol_r = (spk_vol_r == 0) ? 1 : spk_vol_r;
        spk_vol_r = (spk_vol_r >= MAX_SYS_VOL_L) ? MAX_SYS_VOL_L : spk_vol_r;
    }

    pc_dac_mute(pc_mute_status, FADE_ON);

    set_sys_vol(spk_vol_l, spk_vol_r, FADE_ON);

    //vm_write_api(VM_PC_VOL, &spk_vol_l);
    vm_cache_write(VM_PC_VOL, &spk_vol_l, 2);

    return spk_vol_l;
}

/*----------------------------------------------------------------------------*/
/**@brief  PC 模式mic音量设置
   @param  pc_mic_mute_status：mute状态
   @return AUDIO音量
   @note   u8 app_pc_set_mic_vol(u32 pc_mute_status)
*/
/*----------------------------------------------------------------------------*/
extern volatile u8 digital_vol;
u8 app_pc_set_mic_vol(u32 pc_mic_mute_status)
{
    u32 mic_vol;
    USB_SLAVE_DEV_IO_REG();

    __this(dev_ioctrl)(&mic_vol, USB_SLAVE_GET_MIC_VOL);
    mic_vol &= 0xffff;
    if (mic_vol == 0x8000) {
        mic_vol = 0;
    } else {
        mic_vol = mic_vol / 192;
    }

    /* pc_mic_mute(pc_mic_mute_status, FADE_ON); */

    /* set_mic_vol(mic_vol, FADE_ON); */
    digital_vol = mic_vol;
    pc_printf("digital vol = %d \n", digital_vol);

    //vm_write_api(VM_PC_MIC_VOL, &mic_vol);
    vm_cache_write(VM_PC_MIC_VOL, &mic_vol, 2);

    return mic_vol;
}

/*----------------------------------------------------------------------------*/
/**@brief  get usb slave online status
   @param  none
   @return run online status
   @note   u32 app_usb_slave_online_status(void)
*/
/*----------------------------------------------------------------------------*/
u32 app_usb_slave_online_status(void)
{
    u32 online_status;
    USB_SLAVE_DEV_IO_REG();

    return __this(dev_ioctrl)(&online_status, USB_SLAVE_GET_ONLINE_STATUS);
}

/*----------------------------------------------------------------------------*/
/**@brief  PC 在线列表
   @note   const u8 pc_event_tab[]
*/
/*----------------------------------------------------------------------------*/
static const u8 pc_event_tab[] = {
    SYS_EVENT_PC_OUT, //PC拔出
    SYS_EVENT_PC_IN,  //PC插入
};


/*----------------------------------------------------------------------------*/
/**@brief  PC 在线检测API函数
   @param  无
   @return 无
   @note   void pc_check_api(void)
*/
/*----------------------------------------------------------------------------*/
void pc_check_api(u32 info)
{
    USB_SLAVE_DEV_IO_REG();
    s32 dev_status;

    __this(dev_ioctrl)(&dev_status, USB_SLAVE_ONLINE_DETECT);
    if (dev_status != DEV_HOLD) {
        os_taskq_post_event(MAIN_TASK_NAME, 2, pc_event_tab[dev_status], info);
    }
}

#undef	NULL

#endif
