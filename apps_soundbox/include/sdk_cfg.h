/*********************************************************************************************
    *   Filename        : sdk_cfg.h

    *   Description     : Config for Sound Box Case

    *   Author          : Bingquan

    *   Email           : bingquan_cai@zh-jieli.com

    *   Last modifiled  : 2016-12-01 15:36

    *   Copyright:(c)JIELI  2011-2016  @ , All Rights  Reserved.
*********************************************************************************************/

#ifndef _CONFIG_
#define _CONFIG_

#include "includes.h"


///任务堆栈大小配置测试
#define VM_TASK_STACK_SIZE          (1024 * 1)
#define MAIN_TASK_STACK_SIZE        (1024 * 2) //(1024 * 1)可行
#define MUSIC_TASK_STACK_SIZE       (1024 * 4)
#define MUSIC_PHY_TASK_STACK_SIZE   (1024 * 4)
#define TONE_TASK_STACK_SIZE        (1024 * 4)
#define TONE_PHY_TASK_STACK_SIZE    (1024 * 2)
#define UI_TASK_STACK_SIZE          (1024 * 2)

//配置对箱
///< 蓝牙对箱使能,对箱暂时不开放.ble,后台和app
#define   BT_TWS                      0
///对箱从机开机是否进入被连接还是回链状态
#define    TWS_SLAVE_WAIT_CON         1
///对箱主从是否一起关机
#define   BT_TWS_POWEROFF_TOGETHER    0

///对箱没连接成功之前，通过按键开启可发现使能,即主从同时按下配对按键才进行配对
#define    BT_TWS_SCAN_ENBLE         0


#define BT_BLE_WECHAT    0

/********************************************************************************/
/*
 *          --------调试类配置
 */
////<开启系统打印调试功能
#define __DEBUG

///<LED指示使能
#define LED_EN                  0
///<串口升级
#define UART_UPDATA_EN          0
///<电量监测
#define SYS_LVD_EN              1


#if (BT_TWS == 1)
#define BT_KTV_EN			0
#else
///<蓝牙K歌宝
#define BT_KTV_EN			0
#endif



/********************************************************************************/

/********************************************************************************/
/*
 *           --------手机app控制 配置
 */
#if (BT_TWS == 1)
#define SUPPORT_APP_RCSP_EN    0
#define RCSP_LIGHT_HW	      0
#define BLE_FINGER_SPINNER_EN  0
#else
///配置是否支持手机app功能，  (1-支持,0-不支持)
#define SUPPORT_APP_RCSP_EN   0
///配置是否支持蓝牙灯硬件控制 (1-支持,0-不支持)
#define RCSP_LIGHT_HW	     0
///配置是否支持指尖陀螺BLE数据传输 (1-支持,0-不支持) //此配置和 SUPPORT_APP_RCSP_EN不能同时打开
#define BLE_FINGER_SPINNER_EN 0
#endif

/********************************************************************************/

/********************************************************************************/
/*
 *           --------电源类配置
 */
///   0:  no change
#define    PWR_NO_CHANGE        0
///   1:  LDOIN 5v -> VDDIO 3.3v -> DVDD 1.2v
#define    PWR_LDO33            1
///   2:  LDOIN 5v -> LDO   1.5v -> DVDD 1.2v, support bluetooth
#define    PWR_LDO15            2
///   3:  LDOIN 5v -> DCDC  1.5v -> DVDD 1.2v, support bluetooth
#define    PWR_DCDC15           3

///可选配置：PWR_NO_CHANGE/PWR_LDO33/PWR_LDO15/PWR_DCDC15
#define PWR_MODE_SELECT         PWR_LDO15//PWR_DCDC15

//DCDC时BTAVDD,用LDO模式时可以忽略 0:1.61v  1:1.51v  2:1.43v  3:1.35v
//AC6904封装要用1.51v,即应设置为1
#define VDC15_LEV     3

//0:0.703V  1:0.675V  2:0.631V  3:0.592V
//4:0.559V  5:0.527V  6:0.493V  7:0.462V
//LC6904封装建议设置为5
#define POWER_DOWN_DVDD_LEV     7

///蓝牙无连接自动关机计时，u16类型，0表示不自动关机
#define AUTO_SHUT_DOWN_TIME     0

///<按键双击功能
#define KEY_DOUBLE_CLICK        0

///<电池电量低，是否切换电源输出配置
#define SWITCH_PWR_CONFIG		0
/*
	SYS_LDO_Level:3.53v-3.34v-3.18v-3.04v-2.87v-2.73v-2.62v-2.52v
	FM_LDO_Level:3.3v-3.04v-2.76v-2.5v
*/
///<Normal LDO level
#define SYS_LDO_NORMAL_LEVEL	1	//range:0~7:FM_LDO和VDDIO绑一起，建议:level=2
#define FM_LDO_NORMAL_LEVEL		0	//range:0~3
///<Reduce LDO level
#define SYS_LDO_REDUCE_LEVEL	3	//range:0~7
#define FM_LDO_REDUCE_LEVEL		1	//range:0~3

/********************************************************************************/

/********************************************************************************/
/*
 *           --------音效类配置
 */
///<EQ模块开关
#define EQ_EN			        1
//软件EQ,置1开启，置0关闭，对系统时钟要求高，一般要192M，
//开启后不兼容以前的eq文件，用新的EQ工具，注意eq文件名改为"cfg_eq.bin"
//由于软件eq速度要求高，不支持无损格式，不支持对箱
#define USE_SOFTWARE_EQ   		0
///<EQ uart online debug
#define EQ_UART_BDG	    		0
///<dac声道合并
#define DAC_SOUNDTRACK_COMPOUND 0
///<自动mute
#if BT_KTV_EN
#define DAC_AUTO_MUTE_EN		0
#else
#define DAC_AUTO_MUTE_EN		0
#endif
///<按键音
#define KEY_TONE_EN     	    0
///<非0表示使用默认音量
#define SYS_DEFAULT_VOL         0
///<人声消除
#define VOCAL_REMOVER			0


/********************************************************************************/


/********************************************************************************/
/*
 *           --------DAC VCOMO 配置
 */
///是否选择VCMO直推耳机
#define VCOMO_EN 	            0
///可选配置：DAC_L_R_CHANNEL/DAC_L_CHANNEL/DAC_R_CHANNEL
#define DAC_CHANNEL_SLECT       DAC_L_R_CHANNEL
//DAC 声音输出到IIS
#define DAC2IIS_EN                      0      //1:使能DAC声音从IIS以固定采样率44.1K输出。
#if(DAC2IIS_EN == 1)
#define DAC2IIS_OUTCLK_AUTO_CLOSE  O    //1:当没有声音时，自动关闭MCLK,SCLK,LRCLK时钟输出。 0:不关闭
#define IISCHIP_WM8978_EN          1    //1: 使能WM8978 IIS芯片。
#else
#define DAC2IIS_OUTCLK_AUTO_CLOSE  0    //1:当没有声音时，自动关闭MCLK,SCLK,LRCLK时钟输出。 0:不关闭
#define IISCHIP_WM8978_EN          0    //1: 使能WM8978 IIS芯片。
#endif
/********************************************************************************/

/********************************************************************************/
/*
 *           --------外设类配置
 */
#if (BT_TWS == 1)
#define SDMMC0_EN           0
#define SDMMC1_EN           0
#define USB_DISK_EN         0
#define USB_PC_EN           0
#else
#define SDMMC0_EN           1
#define SDMMC1_EN           1
#define USB_DISK_EN         1
#define USB_PC_EN           1
#endif

//usb_sd引脚复用，需要测试兼容性
#define USB_SD0_MULT_EN     0	//<需要测试兼容性
#define USB_SD1_MULT_EN		0	//<需要测试兼容性

#if(USB_SD0_MULT_EN == 1)||(USB_SD1_MULT_EN == 1)
#undef USB_PC_EN
#define USB_PC_EN       0
#endif


#define VR_SPI0_EN      	0   ///<程序flash虚拟文件系统使能

#define SPI1_EN         	0   ///<外挂flash使能

#define NOR_FLASH       	0
#define NAND_FLASH      	1
#define SPI1_FLASH_CHIP     NOR_FLASH   ///<外挂flash类型
#if SPI1_EN
#define VR_SPI1_EN     		1   ///<外挂flash虚拟文件系统使能
#endif

#if (SPI1_EN==0)
#undef VR_SPI1_EN
#define VR_SPI1_EN     		0
#endif


#define SPI_REC_EN      	0   ///<外挂flash录音使能


/********************************************************************************/

/********************************************************************************/
/*
 *           --------蓝牙类配置
 */
#include "bluetooth/bluetooth_api.h"

#define NFC_EN          0  ///<NFC ENABLE

#if(NFC_EN)
#define  NFC_INFROM_EN     1///nfc被读完后是否发通知
#else
#define  NFC_INFROM_EN     0
#endif

#define USER_CONN_NAME_EN   0//用户可修改蓝牙连接名字
#define PHONESEARCHHIDEBLE  0
///可选配置：NORMAL_MODE/TEST_BQB_MODE/TEST_FCC_MODE/TEST_FRE_OFF_MODE/TEST_BOX_MODE
#define BT_MODE             NORMAL_MODE

#if (BT_MODE == TEST_FCC_MODE)
#undef UART_UPDATA_EN
#define UART_UPDATA_EN    0
#endif

///模拟配置
#define BT_ANALOG_CFG       0
#define BT_XOSC             0//

///<蓝牙后台
#if (SUPPORT_APP_RCSP_EN == 1)
#define BT_BACKMODE         1
#else
#define BT_BACKMODE         0
#endif
///dependency
#if (BT_BACKMODE == 0)
///<HID拍照的独立模式只支持非后台
#define BT_HID_INDEPENDENT_MODE  0
#endif

///<来电报号
#define BT_PHONE_NUMBER     1


/*
 *           --------蓝牙低功耗设置
 *   使能该功能后只能是纯蓝牙功能，没有显示功能
 */
///可选配置：SNIFF_EN/SNIFF_TOW_CONN_ENTER_POWERDOWN_EN
#define SNIFF_MODE_CONF   0//SNIFF_EN

///可选配置：BT_POWER_DOWN_EN/BT_POWER_OFF_EN
#define BT_LOW_POWER_MODE  0//BT_POWER_DOWN_EN


#define BT_OSC              0
#define RTC_OSCH            1
#define RTC_OSCL            2

///可选配置：BT_OSC/RTC_OSCH/RTC_OSCL
#define LOWPOWER_OSC_TYPE   BT_OSC

///可选配置：32768L//24000000L
#define LOWPOWER_OSC_HZ     24000000L



///可选配置：BT_BREDR_EN/BT_BLE_EN/(BT_BREDR_EN|BT_BLE_EN)
#if (BT_TWS == 1)
#define BLE_BREDR_MODE      (BT_BREDR_EN)
#elif (BT_KTV_EN == 1)
#define BLE_BREDR_MODE      (BT_BREDR_EN)
#else
#define BLE_BREDR_MODE      (BT_BREDR_EN)//(BT_BREDR_EN|BT_BLE_EN)//
#endif

///TWS 固定左右耳选择 可选配置：TWS_CHANNEL_LEFT/TWS_CHANNEL_RIGHT)
#define    BT_TWS_CHANNEL_SELECT         0////固定左右耳时,左耳为主,右耳不进行搜索配对

///BLE 使能支持同时两个channel同时发送和接收数据, 例如语音传输的同时可以发送命令和接收命令
#define    BLE_MULTI_EN    	 			 0 //0：使用sdk默認的從機服務， 1：使能支持同时两个channel同时发送和接收数据
#if (BLE_MULTI_EN)
#define BLE_MULTI_GATT_ROLE_SERVER		 0
#define BLE_MULTI_GATT_ROLE_CLIENT		 1
#define BLE_MULTI_GATT_ROLE_CFG    	 	 BLE_MULTI_GATT_ROLE_SERVER//
#endif


//蓝牙动态改名
#define BT_DYNAMIC_UPDATE_NAME_EN    0

#if(BT_DYNAMIC_UPDATE_NAME_EN)
#if (BT_BREDR_EN) //蓝牙2.1
#define BREDR_DYNAMIC_NAME_EN  1
#endif

#if(BT_BLE_EN)   //蓝牙4.0(ios系统不能立即更新显示ble的名字)
#define BLE_DYNAMIC_NAME_EN    1
#endif
#else
#define BREDR_DYNAMIC_NAME_EN  0
#define BLE_DYNAMIC_NAME_EN    0
#endif //end BT_DYNAMIC_UPDATE_NAME_EN
/********************************************************************************/




//#define POWER_TIMER_HW_ENABLE //该功能主要是为需要进入powerdown功能方案提供较为准确的timer

#ifdef POWER_TIMER_HW_ENABLE

#define TIMETICK_UNIT					 (10)//固定值. 不可改
#define TIMETICK_STEP					 (5)//非0值

#define POWER_TIMER_HW_UNIT				 (TIMETICK_UNIT * TIMETICK_STEP)
#else//POWER_TIMER_HW_ENABLE

#define TIMETICK_UNIT					 (10)//固定值. 不可改
#define TIMETICK_STEP					 (1)//固定值. 不可改

#endif//POWER_TIMER_HW_ENABLE


///变速变调使能，注意：该功能使能要求较高的系统频率，建议开到192M(在clock.h中配置)
#define SPEED_PITCH_EN					 0





/********************************************************************************/
/*
 *           --------芯片封装配置
 */
///RTCVDD口没有绑出来要置1,目前对应封装芯片AC6905
#define RTCVDD_TYPE              0
#define BTAVDD_TYPE              0

/********************************************************************************/


/********************************************************************************/
/*
 *           --------MUSIC MACRO
 */
//SMP加密文件支持
#define MUSIC_DECRYPT_EN 		0
//AB断点支持
#define MUSIC_AB_RPT_EN 		0

///<MP3
#define DEC_TYPE_MP3_ENABLE     1
///<SBC
#define DEC_TYPE_SBC_ENABLE     1
///<AAC
#define DEC_TYPE_AAC_ENABLE		0

///<3K_code_space
#define DEC_TYPE_WAV_ENABLE     1
///<5K_code_space
#define DEC_TYPE_FLAC_ENABLE    0
///<8K_code_space
#define DEC_TYPE_APE_ENABLE     0
///<30K_code_space
#define DEC_TYPE_WMA_ENABLE     0
///<30K_code_space
#define DEC_TYPE_F1A_ENABLE     0

/********************************************************************************/


/********************************************************************************/
/*
 *           --------FM MACRO
 */
///<标准SDK
#if (BT_TWS == 1)
#define FM_RADIO_EN         0
#else
#define FM_RADIO_EN         1
#endif

///dependency
#if (FM_RADIO_EN == 1)
///<RDA5807FM
#define RDA5807                 0
///<BK1080FM
#define BK1080                  0
///<QN8035FM
#define QN8035                  0
///<芯片内部FM
#define FM_INSIDE               1
#endif

/********************************************************************************/

/********************************************************************************/
/*
 *           --------RTC MACRO
 */
///<标准SDK RTC时钟模式
#if (BT_TWS == 1)
#define RTC_CLK_EN          0
#else
#define RTC_CLK_EN          0
#endif
///dependency
#if (RTC_CLK_EN == 1)
///<RTC闹钟模式
#define RTC_ALM_EN          1
#endif

/********************************************************************************/

/********************************************************************************/
/*
 *           --------ECHO MACRO
 *			开启混响，必须关闭	DAC_AUTO_MUTE_EN
 */



///dependency
#if (BT_BACKMODE == 1)
///<不支持与蓝牙后台开启混响
#define ECHO_EN             0
///<混响模式 标准SDK
#define ECHO_TASK_EN        0
#else
#if BT_KTV_EN
///<混响功能 KTV_SDK
#define ECHO_EN             1
#else
///<混响功能 标准SDK
#define ECHO_EN             0
#endif

///<混响模式 标准SDK
#define ECHO_TASK_EN        0
#endif

///<变声
#define PITCH_EN				0

///<混响第一声模拟开关，混响可选择第一声是模拟或者是数字，变声必须是数字, 0:数字 	1:模拟
#if PITCH_EN
///<变调使能，第一声必须是数字
#define ECHO_WITH_ANALOG_EN		0	///<此处不能修改
#else
///<混响，第一声可选择走模拟还是数字
#define ECHO_WITH_ANALOG_EN		0	///<混响通道配置
#endif

///<啸叫抑制
#define HOWLING_SUPPRESSION_EN	0



///<2.4G pll1167 控制使能
#define PLL1167_EN          0

/********************************************************************************/

/********************************************************************************/
/*
 *           --------REC MACRO
 */
///dependency
#if (BT_BACKMODE == 1)
///<不支持与蓝牙后台开启录音
#define REC_EN             0
#else
///<标准SDK录音功能
#define REC_EN            1
#endif

///>独立MIC录音模式
//#define REC_MODE_EN				0

///>录音源选择,	0:other		1:dac
#if (BT_KTV_EN == 1)
#define REC_SOURCE			1
#else
#define REC_SOURCE			0
#endif

#if (REC_EN == 1)
///<蓝牙录音使能
#define BT_REC_EN		0
///<MIC录音使能
#define MIC_REC_EN		1
///<FM录音使能
#define FM_REC_EN		0
///<AUX录音使能
#define AUX_REC_EN		1
#endif

/********************************************************************************/

/********************************************************************************/
/*
 *           --------UI MACRO
 */
///<UI_显示
#if (RCSP_LIGHT_HW == 1)
#define UI_ENABLE                0
#else
#define UI_ENABLE                0
#endif
///dependency
#if (UI_ENABLE == 1)
#define LCD_128X64_EN        1      ///<lcd 支持
#define LED_7_EN             0      ///<led 支持
#else
///都不支持
#define LCD_128X64_EN        0
#define LED_7_EN             0
#endif
#if ((LCD_128X64_EN == 1) && (LED_7_EN == 1))
#error  "UI driver support only one"
#endif

#if (LCD_128X64_EN == 1)
///菜单显示
#define LCD_SUPPORT_MENU     1
///LRC歌词显示
#define LRC_LYRICS_EN        1
#else
#define LCD_SUPPORT_MENU     0
#define LRC_LYRICS_EN        0
#endif

/********************************************************************************/

#if (BT_TWS == 1)
///<蓝牙对箱使能,对箱暂时不开放ble,后台和app.
#if ((BT_KTV_EN)||(SUPPORT_APP_RCSP_EN)||(BLE_BREDR_MODE&BT_BLE_EN))
#error  "-------stereo not support this config ,please check sdk_cfg.h------------"
#endif
#endif

#if (BT_LOW_POWER_MODE == BT_POWER_DOWN_EN)
#if ((SDMMC0_EN)||(SDMMC1_EN)||(USB_DISK_EN)||(UI_ENABLE))
#error  "-------powerdown not support this config ,please check sdk_cfg.h------------"
#endif
#endif


#if (BT_MODE != NORMAL_MODE)
#if ((SNIFF_MODE_CONF)||(BT_LOW_POWER_MODE))
#error  "-------BT MODE not support this config ,please check sdk_cfg.h------------"
#endif
#endif

///<音频叠加使能
//暂时支持蓝牙、fm、midi全局音频叠加
//蓝牙、fm模式音频叠加， 建议使用简易提示音播放接口（simple_wav_play_by_pathi，主要是因为蓝牙、fm模式资源紧张）、使能全局音频叠加，不支持蓝牙双模同时打开
#define SOUND_MIX_GLOBAL_EN			0///<使能全局音频叠加功能
#if SOUND_MIX_GLOBAL_EN
#define SOUND_MIX_API_DEFUALT_SAMPLERATE	(24000L)//(16000L)//(22050L)//(32000L)//(44100L)//(48000L)//建议使能全局叠加使用默认采样率，使用更高采样率，需要客户自行测试评估, 同时使能spi录音和echo的情况下只能支持16K采样
#endif//SOUND_MIX_GLOBAL_EN

///<midi升级功能配置， 升级使用的是ble传输， 所以升级必须是能ble, 并且是能蓝牙后台
#if ((BLE_BREDR_MODE&BT_BLE_EN) && (BT_BACKMODE != 0) && (BLE_MULTI_EN == 0))
#define MIDI_UPDATE_EN	0
#else
#define MIDI_UPDATE_EN	0
#endif//((BLE_BREDR_MODE&BT_BLE_EN) && (BT_BACKMODE != 0) && (BLE_MULTI_EN == 0))


/*********************************************************************************/
/**************************模式宏开关********************************************/
#define  SCRIPT_BT_EN       			1
#define  SCRIPT_MUSIC_EN    			1
//不要fm模式请关闭FM_RADIO_EN
#if FM_RADIO_EN
#define  SCRIPT_FM_EN       			1
#else
#define  SCRIPT_FM_EN       			0
#endif//FM_RADIO_EN

#define  SCRIPT_LINEIN_EN   			1

//不要pc模式请关闭USB_PC_EN
#if USB_PC_EN
#define  SCRIPT_USB_EN      			1
#else
#define  SCRIPT_USB_EN      			0
#endif//USB_PC_EN

#if(0==SUPPORT_APP_RCSP_EN)
#define  SCRIPT_HTK_EN      			1
#define  SCRIPT_MIDI_EN     			1

#if MIDI_UPDATE_EN
#define  SCRIPT_MIDI_UPDATE_EN      	1
#else
#define  SCRIPT_MIDI_UPDATE_EN      	0
#endif//MIDI_UPDATE_EN

//不要rec模式请关闭MIC_REC_EN
#if MIC_REC_EN
#define  SCRIPT_REC_EN      			1
#else
#define  SCRIPT_REC_EN      			0
#endif//MIC_REC_EN

#if (BT_BACKMODE != 0)
#define  SCRIPT_MS_EN       			0
#else
#define  SCRIPT_MS_EN       			1
#endif//(BT_BACKMODE != 0)

#else
#define  SCRIPT_HTK_EN      			0
#define  SCRIPT_MIDI_EN     			0
#define  SCRIPT_MIDI_UPDATE_EN      	0
#define  SCRIPT_REC_EN      			0
#define  SCRIPT_MS_EN       			0
#endif//(0==SUPPORT_APP_RCSP_EN)


#if (SOUND_MIX_GLOBAL_EN)
///如果使能全局叠加，linein需要走数字
#define LINEIN_DIGTAL_EN		1
#else
///如果不使能全局叠加，也可以单独开启linein走数字
#define LINEIN_DIGTAL_EN		0
#endif//SOUND_MIX_GLOBAL_EN


/********************************************************************************/
/*
 *           -------- MIO MACRO
 */

// #define MUSIC_IO_ENABLE

#if USB_PC_EN
#ifdef MUSIC_IO_ENABLE
# define MIO_USB_DEBUG
#endif
#endif

#endif//_CONFIG_



