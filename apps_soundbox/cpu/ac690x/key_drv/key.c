#include "includes.h"
#include "key_drv/key.h"
#include "common/msg.h"
#include "key_drv/key_drv_ad.h"
#include "key_drv/key_drv_ad_pw.h"
#include "key_drv/key_drv_io.h"
#include "key_drv/key_drv_ir.h"
#include "key_drv/key_drv_tch.h"
#include "key_drv/key_voice.h"
#include "timer.h"
#ifdef TOYS_EN
#include "encode/encode_flash.h"
#endif /*TOYS_EN*/

#ifndef AT_ENC_ISR_KEY
#define AT_ENC_ISR_KEY
#endif
extern u8 fast_test;
u8 key_type;        ///<按键类型
char *keymsg_task_name;
u32 input_number;	    ///<输入的数字的实际值
u8 input_number_cnt;   //输入数字个数

#ifndef ENCODE_FLASH_API
const u16(*ad_msg_table)[KEY_REG_AD_MAX];
const u16(*io_msg_table)[KEY_REG_IO_MAX];
const u16(*ir_msg_table)[KEY_REG_IR_MAX];
const u16(*touch_msg_table)[KEY_REG_TOUCH_MAX];
const KEY_REG *key_msg = NULL;
#else
u16 ad_msg_table[4][KEY_REG_AD_MAX];
u16 io_msg_table[4][KEY_REG_IO_MAX];
u16 ir_msg_table[4][KEY_REG_IR_MAX];
u16 touch_msg_table[4][KEY_REG_TOUCH_MAX];
KEY_REG key_msg_var;
#define key_msg		(&key_msg_var)
#endif


volatile u8 key_invalid_flag = 0;
u8 get_key_invalid_flag(void)
{
    return key_invalid_flag;
}

char *menu_keytask_name;
u8 *menu_adkeymsg_table;
u8 *menu_irkeymsg_table;

#ifdef ENCODE_FLASH_API
tu8(*get_key_tab[])(void)  AT_ENC_ISR_KEY;
#else
tu8(*get_key_tab[])(void) AT(.const_tab);
#endif
tu8(*get_key_tab[])(void) = {
#if KEY_AD_EN
    get_adkey_value,
#endif

#if KEY_IO_EN
    get_iokey_value,
#endif

#if KEY_IR_EN
    get_irkey_value,
#endif

#if KEY_TCH_EN
    get_tchkey_value,
#endif
};

void key_init()
{
    key_puts("key init\n");
    s32 ret;
    keymsg_task_name = NULL;
    menu_keytask_name = NULL;
#if ( KEY_AD_EN || SYS_LVD_EN )/*ldo_in电压检测需要adc*/
    ad_key0_init();
#endif

#if KEY_IO_EN
    io_key_init();
#endif

#if KEY_IR_EN
    ret = ir_key_init();
    if (ret != TIMER_NO_ERR) {
        printf("IR key init error:%d!!!\n", ret);
    }
#endif

#if KEY_TCH_EN
    tch_key_init();
#endif

    ret = timer_reg_isr_fun(timer0_hl, 5, key_scan, NULL);
    if (ret != TIMER_NO_ERR) {
        printf("key_scan err = %x\n", ret);
    }
}


extern void le_multi_client_send_test_key_num(u8 key_num);
extern void le_multi_server_send_test_key_num(u8 key_num);
extern void ble_server_send_test_key_num(u8 key_num);
void ble_send_test_key_num(u32 msg)
{
    printf("msg_val:%d\n", msg);
#if (BLE_MULTI_EN)
#if (BLE_MULTI_GATT_ROLE_CFG == BLE_MULTI_GATT_ROLE_SERVER)
    le_multi_server_send_test_key_num(msg);
#else
    le_multi_client_send_test_key_num(msg);
#endif//BLE_MULTI_GATT_ROLE_CFG == BLE_MULTI_GATT_ROLE_SERVER
#else
#if BLE_GATT_ROLE_CFG
    ble_client_send_test_key_num(msg);
#else
    ble_server_send_test_key_num(msg);
#endif// BLE_GATT_ROLE_CFG
#endif//BLE_MULTI_EN
}

/*----------------------------------------------------------------------------*/
/**@brief   key_msg_sender
   @param   name：线程
   @param   msg：消息
   @return  void
   @note
*/
/*----------------------------------------------------------------------------*/
void key_msg_sender(char *name, u32 msg)
{
    u32 cnt = 0;
    os_taskq_count(name, &cnt);
    if (cnt > 8) {
        if (msg  != NO_MSG) {
            /* printf("--key_msg = %d	\n", msg); */
            os_taskq_post_msg(name, 1, msg);
#if (BLE_BREDR_MODE == BT_BLE_EN)
            ble_send_test_key_num(msg);
#endif//BLE_BREDR_MODE == BT_BLE_EN
        }
    } else {
        printf("--key_msg_sender = %s	", name);
        printf("msg = 0x%x\n", msg);
    }
}
/*----------------------------------------------------------------------------*/
/**@brief   按键-消息转换函数
   @param   key_status：按键状态
   @param   back_last_key：最后按键值
   @return  void
   @note
*/
/*----------------------------------------------------------------------------*/
AT_ENC_ISR_KEY void key2msg_filter(u8 key_status, u8 back_last_key)
{
    u16 msg = NO_MSG;

#if(BT_MODE==NORMAL_MODE)


    if (fast_test) {
        send_key_voice(500);
    } else {
#if KEY_TONE_EN
        send_key_voice(500);
#endif
    }

#if KEY_IR_EN
    if (KEY_TYPE_IR == key_type) {
        if (menu_keytask_name == NULL) {
            if (key_msg != NULL && key_msg->_ir) {
                msg = key_msg->_ir[key_status][back_last_key];
            }
//            msg = ir_msg_table[key_status][back_last_key];
            //msg = *(ir_msg_table+key_status*21+back_last_key);
        } else {
            //菜单短按表
            if (key_status == 0) {
                msg = menu_irkeymsg_table[back_last_key];
                /* os_taskq_post_msg(menu_keytask_name, 1, msg); */
                key_msg_sender(menu_keytask_name, msg);
            }
            return;
        }

    }
#endif

#if KEY_IO_EN
    if (KEY_TYPE_IO == key_type) {
        if (key_msg != NULL && key_msg->_io) {
            msg = key_msg->_io[key_status][back_last_key];
        }
        //msg = io_msg_table[key_status][back_last_key];
        //msg = *(io_msg_table+key_status*10 + back_last_key);
    }
#endif

#if KEY_TCH_EN
    if (KEY_TYPE_TOUCH == key_type) {
        if (key_msg != NULL && key_msg->_touch) {
            msg = key_msg->_touch[key_status][back_last_key];
        }
        //msg = touch_msg_table[key_status][back_last_key];
        //msg = *(touch_msg_table+key_status*10+back_last_key);
    }
#endif // KEY_TCH_EN

#if KEY_AD_EN
    if (KEY_TYPE_AD == key_type) {

        if (menu_keytask_name == NULL) {
            if (key_msg != NULL && key_msg->_ad) {
                msg = key_msg->_ad[key_status][back_last_key];
            }
            //msg = ad_msg_table[key_status][back_last_key];
            //msg = *(ad_msg_table+key_status*10 +back_last_key);
        } else {
            //菜单短按表
            if (key_status == 0) {
                msg = menu_adkeymsg_table[back_last_key];
                /* os_taskq_post_msg(menu_keytask_name, 1, msg); */
                key_msg_sender(menu_keytask_name, msg);
            }
            return;
        }

    }
#endif

#ifdef ENCODE_FLASH_API
    if (enc_flash_work) {
        if ((msg != NO_MSG) && (enc_flash_key == 0xffff)) {
            enc_flash_key = msg;
        }
        return;
    }
#endif

//    key_printf("msg:0x%02x\n",msg);
    if (msg < MSG_MAIN_MAX) {
        /* os_taskq_post_msg(MAIN_TASK_NAME, 1, msg); */
        key_msg_sender(MAIN_TASK_NAME, msg);
    } else if (keymsg_task_name) {
        /* os_taskq_post_msg(keymsg_task_name, 1, msg); */
        key_msg_sender(keymsg_task_name, msg);
    }
#endif
}

/*----------------------------------------------------------------------------*/
/**@brief   按键去抖函数，输出稳定键值
   @param   key：键值
   @return  稳定按键
   @note    u16 key_filter(tu8 key)
*/
/*----------------------------------------------------------------------------*/
AT_ENC_ISR_KEY static tu8 key_filter(tu8 key)
{
    static tu8 used_key = NO_KEY;
    static tu8 old_key;
    static tu8 key_counter;

    if (old_key != key) {
        key_counter = 0;
        old_key = key;
    } else {
        key_counter++;
        if (key_counter == KEY_BASE_CNT) {
            used_key = key;
        }
    }

    return used_key;
}

/*----------------------------------------------------------------------------*/
/**@brief   按键-消息转换函数,按键产生顺序：短按抬起/长按-->连按
   @param
   @param
   @return  void
   @note
*/
/*----------------------------------------------------------------------------*/
AT_ENC_ISR_KEY tu8 get_key_value(void)
{
    tu8 i;
    for (i = 0; i < MAX_TYPE_KEY; i++) {
        tu8 msg;
        tu8(*get_key)(void);

        get_key = get_key_tab[i];
#ifdef ENCODE_FLASH_API
        if (enc_flash_work) {
            if ((u32)get_key >= 0x0001000000) {
                //flash space
                continue;
            }
        }
#endif
        if (NULL != get_key) {
#if KEY_IR_EN
            if (i == KEY_TYPE_IR) {
                msg = get_key();
            } else
#endif
#if  KEY_TCH_EN
                if (i == KEY_TYPE_TOUCH) {
                    msg = get_key();
                } else
#endif

                {
                    msg = key_filter(get_key());
                }

            if (NO_KEY != msg) {
                key_type = i;
                return msg;
            }
        }
    }
    return NO_KEY;
}

/*----------------------------------------------------------------------------*/
/**@brief   按键-消息转换函数,按键产生顺序：短按抬起/长按-->连按
   @param
   @param
   @return  void
   @note
*/
/*----------------------------------------------------------------------------*/
#define KEY_DOUBLE_CLICK_CNT    35 //35*10ms
AT_ENC_ISR_KEY void key_scan(void *ptr)
{
    static tu8 last_key = NO_KEY;
    static tu8 key_press_counter;
    static u8  key_press_flag = 0;
    tu8 cur_key, key_status, back_last_key;

#ifdef ENCODE_FLASH_API
    if (!enc_flash_work)
#endif
    {
        key_voice_dac_channel();
    }

    if (key_press_flag) {
        key_press_flag++;
        if (key_press_flag > KEY_DOUBLE_CLICK_CNT) {
            key_puts(" One_Click ");
            key_press_flag = 0;
            key2msg_filter(KEY_SHORT_UP, 0);
        }
    }

    cur_key = NO_KEY;
    back_last_key = last_key;
    cur_key = get_key_value();

    /* if (cur_key != NO_KEY) { */
    /* key_invalid_flag = 1; */
    /* } else { */
    /* key_invalid_flag = 0; */
    /* } */

    if (cur_key == last_key) {                          //长时间按键
        if (cur_key == NO_KEY) {
            return;
        }

        key_press_counter++;

        if (key_press_counter == KEY_LONG_CNT) {        //长按
            key_status = KEY_LONG;
        } else if (key_press_counter == (KEY_LONG_CNT + KEY_HOLD_CNT)) {    //连按
            key_status = KEY_HOLD;
            key_press_counter = KEY_LONG_CNT;
        } else {
            return;
        }
    } else { //cur_key = NO_KEY, 抬键
        last_key = cur_key;
        if ((key_press_counter < KEY_LONG_CNT) && (cur_key != NO_KEY)) {
        }
        if ((key_press_counter < KEY_LONG_CNT) && (cur_key == NO_KEY)) {    //短按抬起
            key_press_counter = 0;
            key_status = KEY_SHORT_UP;
        } else if ((cur_key == NO_KEY) && (key_press_counter >= KEY_LONG_CNT)) { //长按抬起
            key_press_counter = 0;
            key_status = KEY_LONG_UP;
            /* puts("[UP]"); */
        } else {
            key_press_counter = 0;
            return;
        }
    }
#ifdef ENCODE_FLASH_API
    if (enc_flash_work) {
        key2msg_filter(key_status, back_last_key);
        return;
    }
#endif

#if KEY_DOUBLE_CLICK
    if (!strcmp(keymsg_task_name, "btmsg")) { //蓝牙模式才有按键双击功能
        if ((back_last_key == 0) && (key_status == KEY_SHORT_UP)) {
            if (key_press_flag == 0) {
                key_press_flag = 1;
            }
            if ((key_press_flag > 15) && (key_press_flag <= KEY_DOUBLE_CLICK_CNT)) {
                key_puts(" Double_Click ");
                key_press_flag = 0;
                key2msg_filter(key_status, 9);
            }
        } else {
            key2msg_filter(key_status, back_last_key);
        }
    } else
#endif/*KEY_DOUBLE_CLICK*/
    {
        key2msg_filter(key_status, back_last_key);
    }

}

void key_msg_reg(char *task_name, const KEY_REG *reg)
{
    if (task_name == NULL || reg == NULL) {
        return ;
    }
    CPU_INT_DIS();
    keymsg_task_name = task_name;
#ifndef ENCODE_FLASH_API
    key_msg = reg;
#else
    memset(key_msg, 0, sizeof(KEY_REG));
    if (reg->_ad) {
        memcpy(ad_msg_table, reg->_ad, sizeof(ad_msg_table));
        key_msg->_ad = ad_msg_table;
    }
    if (reg->_io) {
        memcpy(io_msg_table, reg->_io, sizeof(io_msg_table));
        key_msg->_io = io_msg_table;
    }
    if (reg->_ir) {
        memcpy(ir_msg_table, reg->_ir, sizeof(ir_msg_table));
        key_msg->_ir = ir_msg_table;
    }
    if (reg->_touch) {
        memcpy(touch_msg_table, reg->_touch, sizeof(touch_msg_table));
        key_msg->_touch = touch_msg_table;
    }
#endif
    CPU_INT_EN();
}



void menu_key_msg_register(char *task_name, u8 *menu_adkey_msg, u8 *menu_irkey_msg)
{
    menu_keytask_name = task_name;
    menu_adkeymsg_table = menu_adkey_msg;
    menu_irkeymsg_table = menu_irkey_msg;
}

void menu_key_msg_unregister(void)
{
    menu_keytask_name = NULL;
}
