#include "midi_update.h"
#include "sdk_cfg.h"
#include "common/msg.h"
#include "common/app_cfg.h"
#include "rtos/os_api.h"
#include "rtos/task_manage.h"
#include "file_operate/file_op.h"
#include "fat/fs_io.h"
#include "file_operate/logic_dev_list.h"
#include "sydf/syd_file.h"
#include "vm/vm.h"
#include "sys_detect.h"
#include "script_switch.h"

#if MIDI_UPDATE_EN

#define MIDI_UPDATE_DEBUG_ENABLE
#ifdef MIDI_UPDATE_DEBUG_ENABLE
#define midi_update_printf	printf
#define midi_update_printf_buf	printf_buf
#define	midi_time_clock_debug	debug_time_gap
#else
#define midi_update_printf(...)
#define midi_update_printf_buf(...)
#define midi_time_clock_debug(...)
#endif

enum {
    BLE_PLANT_CMD_GET_CAP = 0x0,
    BLE_PLANT_CMD_UPDATE_FILE_PREPARE,
    BLE_PLANT_CMD_UPDATE_FILE,

    BLE_PLANT_CMD_UNKNOW = 0xffff,
};

#define IS_LITTLE_ENDIAN  			0
#define IS_BIG_ENDIAN     			1
#define USE_ENDIAN_TYPE    			IS_LITTLE_ENDIAN

#define MIDI_UPDATE_WRITE_BIG_U16(a,src)   {*((u8*)(a)+0) = (u8)(src>>8); *((u8*)(a)+1) = (u8)(src&0xff); }
#define MIDI_UPDATE_WRITE_BIG_U32(a,src)   {*((u8*)(a)+0) = (u8)((src)>>24);  *((u8*)(a)+1) = (u8)(((src)>>16)&0xff);*((u8*)(a)+2) = (u8)(((src)>>8)&0xff);*((u8*)(a)+3) = (u8)((src)&0xff);}


#define MIDI_UPDATE_CAP				(1024*32)
#define PACKET_RECV_TMP_BUF_LEN		(512)
#define PACKET_SEND_TMP_BUF_LEN		(64)
#define PACKET_HEAD_LEN				(sizeof(PACKET_HEAD))
#define CRC_OFFSET	    			(4 + sizeof(u16))

#define TASK_MIDI_UPDATE_NAME       "TASK_MIDI_UPDATE"

#define SPI_CACHE_READ_ADDR(addr)   (u32)(0x1000000+(((u32)addr)))

#define FLASH_SECTOR_SIZE       	(4*1024)

#define U_UPDATE_NAME  				"midi.res"   //要进行更新文件的名字

typedef struct __PACKET_HEAD {
    u8 		mark[4];
    u16 	crc;
    u16		cmd;
    u32		pid;
    u32		length;
} PACKET_HEAD;


typedef struct __PACKET_CON {
    PACKET_HEAD  head;
    u8 			 recv_tmp_buf[PACKET_RECV_TMP_BUF_LEN];
    u8			 send_tmp_buf[PACKET_SEND_TMP_BUF_LEN];
    u32			 wait_data_len;
    u8			 wait_head_len;
    volatile u8  busy;
    u16			 cur_crc;
    u32			 timeout_tick;
} PACKET_CON;


PACKET_CON packet_hdl = {
    .wait_data_len = 0,
    .wait_head_len = 0,
    .busy 		   = 0,
    .cur_crc 	   = 0,
    .timeout_tick  = 0,
};


static u32 flash_addr_base = 0; //这个数据要在开始那里更新


extern u16 cal_crc16_deal(u8 *ptr, u16 len);
extern u16 cal_crc16_continue(u8 *ptr, u16 len, u16 cur_crc);
extern u32 app_data_send(u8 *data, u16 len);

static u16 htons(u16 n)
{
#if (USE_ENDIAN_TYPE == IS_BIG_ENDIAN)
    return n;
#elif (USE_ENDIAN_TYPE == IS_LITTLE_ENDIAN)
    return ((n & 0xff) << 8) | ((n & 0xff00) >> 8);
#endif
}


static void debug_time_gap(void *func, u32 line)
{
    static u32 last_time_tick = 0;
    u32 cur_time_tick = 0;
    cur_time_tick = os_time_get();
    midi_update_printf("cur time = %d, fun = %s, line = %d\n", cur_time_tick - last_time_tick, (char *)func, line);
    last_time_tick = cur_time_tick;
}



static u32 htonl(u32 n)
{
#if (USE_ENDIAN_TYPE == IS_BIG_ENDIAN)
    return n;
#elif (USE_ENDIAN_TYPE == IS_LITTLE_ENDIAN)
    return ((n & 0xff) << 24) |
           ((n & 0xff00) << 8) |
           ((n & 0xff0000UL) >> 8) |
           ((n & 0xff000000UL) >> 24);
#endif
}

void midi_update_set_flash_base_addr(u32 addr)
{
    flash_addr_base = addr;
}

tbool midi_update_get_info(u8 *midires_name, u32 *addr, u32 *len)
{
    u8 ret;
    FILE_OPERATE *f_op = NULL;
    _FIL_HDL *fil_h = NULL;
    SDFILE *file_s_h = NULL;

    f_op = file_opr_api_create('A');
    if (f_op == NULL) {
        midi_update_printf("f_op create err!!!\n");
        return FALSE;
    }

    ret = fs_get_file_bypath(f_op->cur_lgdev_info->lg_hdl->fs_hdl,
                             &(f_op->cur_lgdev_info->lg_hdl->file_hdl),
                             midires_name,
                             NULL);
    if (ret == false) {
        puts("fs file open  err!!\n");
        file_opr_api_close(f_op);
        f_op = NULL;
        return FALSE;
    }

    fil_h = f_op->cur_lgdev_info->lg_hdl->file_hdl;
    file_s_h = fil_h->hdl;

    midi_update_printf("addr:%x\nlen:%x\n", file_s_h->addr, file_s_h->length);
    *addr = file_s_h->addr;
    *len = file_s_h->length;

    file_opr_api_close(f_op);

    return true;
}

static tbool midi_update_erase_flash(u32 addr, u32 len)
{
    u16 cnt;
    u16 i;

    if (len % FLASH_SECTOR_SIZE != 0) {
        puts("len err\n");
        return false;
    }

    cnt = len / FLASH_SECTOR_SIZE;
    /* midi_update_printf("len cnt = %d\n",cnt); */

    for (i = 0; i < cnt; i++) {
        /* midi_update_printf("len cnt = %d\n",i); */
        if (false == sfc_erase(SECTOR_ERASER, addr + i * FLASH_SECTOR_SIZE)) {
            return false;
        }
    }

    return true;
}




tbool midi_update_to_flash(u8 *buf, u32 len)
{
    u32 ret;
    u32 op_addr;
    u32 op_len;
    u8 check_buf[256];
    //获取要更新文件的地址与长度
    ret =  midi_update_get_info((u8 *)U_UPDATE_NAME, &op_addr, &op_len);
    if (ret == FALSE) {
        printf("get midi info err\n");
        return false;
    }
    midi_update_printf("addr:%x,len:%x, flash_addr_base = %x\n", op_addr, op_len, flash_addr_base);

    op_addr += flash_addr_base;

    if ((op_addr % FLASH_SECTOR_SIZE != 0) || (op_len % FLASH_SECTOR_SIZE) != 0) {
        midi_update_printf("update addr or update len is illegal err !!!!!!, check your download.bat\n");
        return false;
    }

    //设置非保护区间
    set_no_protect_area(op_addr, op_len);

    //擦除对应的区域
    ret = midi_update_erase_flash(op_addr, op_len);
    if (ret == FALSE) {
        printf("erase  err\n");
        return false;
    }

    /* printf("buf before =\n"); */
    /* printf_buf(buf, 256); */

    memcpy(check_buf, buf, 256);

    //写buf的数据到flash对应的区域中
    ret = sfc_enc_write(buf + 256, op_addr + 256, len - 256);
    if (ret == FALSE) {
        printf("write  err\n");
        return false;
    }

    //write check head
    ret = sfc_enc_write(check_buf, op_addr, 256);
    if (ret == FALSE) {
        printf("write check head err\n");
        return false;
    }
    /* midi_update_printf_buf((u8*)(SPI_CACHE_READ_ADDR(op_addr - flash_addr_base)),256); */
    //关闭非保护区间
    set_no_protect_area(0, 0);
    return true;
}


static int midi_update_parse_head(u8 *buf, u16 len, u16 *parse_len)
{
    PACKET_HEAD *head = &(packet_hdl.head);
    u32 time_out = os_time_get() - packet_hdl.timeout_tick;

    if (time_out > 200) {
        packet_hdl.wait_data_len = 0;
        packet_hdl.wait_head_len = 0;
        memset((u8 *)head, 0x0, sizeof(PACKET_HEAD));
        midi_update_printf("time out !!!!!!!!!!!! fun = %s, line = %d\n", __FUNCTION__, __LINE__);
    }

    if (packet_hdl.wait_data_len) {
        /* midi_update_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__); */
        return 0;
    }

    if (len == 0) {
        midi_update_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (packet_hdl.wait_head_len) {
        if (len >= packet_hdl.wait_head_len) {
            memcpy(((u8 *)head) + (PACKET_HEAD_LEN - packet_hdl.wait_head_len), buf, packet_hdl.wait_head_len);
            goto __HEAD_PARSE;
        }

        memcpy(((u8 *)head) + (PACKET_HEAD_LEN - packet_hdl.wait_head_len), buf, len);
        packet_hdl.wait_head_len -= len;
        return 1;
    }

    if (len < 4) {
        midi_update_printf("data len is too short!!!, fun = %s, line = %d\n", __FUNCTION__, __LINE__);
        return -1;
    }
    if (buf[0] == 'J' && buf[1] == 'L' && buf[2] == 'M' && buf[3] == 'P') {
        packet_hdl.wait_head_len = PACKET_HEAD_LEN;
        if (len >= PACKET_HEAD_LEN) {
            midi_update_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
            memcpy((u8 *)head, buf, PACKET_HEAD_LEN);
            goto __HEAD_PARSE;
        } else {
            midi_update_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
            memcpy((u8 *)head, buf, len);
            packet_hdl.wait_head_len -= len;
            return 1;
        }
    } else {
        midi_update_printf("recv match fail!!!, fun = %s, line = %d\n", __FUNCTION__, __LINE__);
        return -1;
    }

__HEAD_PARSE:
    if (parse_len == NULL) {
        midi_update_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
        return -1;
    }

    *parse_len = packet_hdl.wait_head_len;

    midi_update_printf("parse_len = %d , %d\n", *parse_len, PACKET_HEAD_LEN);
    packet_hdl.wait_head_len = 0;

    u8 *ptr = (u8 *)head;
    packet_hdl.cur_crc = cal_crc16_deal(ptr + CRC_OFFSET, PACKET_HEAD_LEN - CRC_OFFSET);

    head->crc = htons(head->crc);
    head->cmd = htons(head->cmd);
    head->pid = htonl(head->pid);
    head->length = htonl(head->length);


    /* midi_update_printf("packet_hdl.cur_crc = %x \n", packet_hdl.cur_crc); */

    midi_update_printf("head->crc = %x \n", head->crc);
    midi_update_printf("head->cmd = %x \n", head->cmd);
    midi_update_printf("head->pid = %x \n", head->pid);
    midi_update_printf("head->length = %x \n", head->length);

    packet_hdl.wait_data_len = head->length;

    packet_hdl.timeout_tick = os_time_get();

    return 0;
}

static int midi_update_parse_data(u8 *buf, u16 len)
{
    PACKET_HEAD *head = &(packet_hdl.head);

    /* midi_update_printf("len = %d, packet_hdl.wait_data_len= %d, fun = %s, line = %d\n", len, packet_hdl.wait_data_len, __FUNCTION__, __LINE__);		 */
    if (len >= packet_hdl.wait_data_len) {
        if (head->length) {
            memcpy(packet_hdl.recv_tmp_buf + (head->length  - packet_hdl.wait_data_len), buf, packet_hdl.wait_data_len);
            packet_hdl.cur_crc = cal_crc16_continue(packet_hdl.recv_tmp_buf, (u16)head->length, packet_hdl.cur_crc);
            packet_hdl.wait_data_len = 0;
        }
        return 0;
    } else {
        memcpy(packet_hdl.recv_tmp_buf + (head->length  - packet_hdl.wait_data_len), buf, len);
        packet_hdl.wait_data_len -= len;
        /* midi_update_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__); */
        return 1;
    }
}

void midi_update_prase(u8 *buf, u16 len)
{
    u16 parse_len = 0;
    PACKET_HEAD *head = &(packet_hdl.head);

    if (packet_hdl.busy) {
        midi_update_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
        return ;
    }

    int ret = midi_update_parse_head(buf, len, &parse_len);
    if (ret != 0) {
        midi_update_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
        return ;
    }

    buf += parse_len;
    len -= parse_len;
    ret = midi_update_parse_data(buf, len);
    if (ret != 0) {
//		midi_update_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
        return ;
    }

    if (packet_hdl.cur_crc != head->crc) {
        midi_update_printf("%x,%x, fun = %s, line = %d\n", packet_hdl.cur_crc, head->crc, __FUNCTION__, __LINE__);
        packet_hdl.wait_data_len = 0;
        packet_hdl.wait_head_len = 0;
        return ;
    }
    debug_time_gap((void *)__FUNCTION__, (u32)__LINE__);
    packet_hdl.busy = 1;
    u8 err = os_taskq_post(TASK_MIDI_UPDATE_NAME, 5, MSG_MODULE_MIDI_UPDATE, head->cmd, packet_hdl.recv_tmp_buf, head->length, head->pid);
    if (err) {
        packet_hdl.busy = 0;
    }
}


static void midi_update_answer(u16 cmd, u32 pid, u32 param)
{
    u16 crc;
    PACKET_HEAD send_head;
    memset((u8 *)&send_head, 0x0, PACKET_HEAD_LEN);
    send_head.mark[0] = 'J';
    send_head.mark[1] = 'L';
    send_head.mark[2] = 'M';
    send_head.mark[3] = 'P';

    MIDI_UPDATE_WRITE_BIG_U16((u8 *) & (send_head.cmd), (u16)cmd);
    MIDI_UPDATE_WRITE_BIG_U32((u8 *) & (send_head.pid), (u32)pid);


    switch (cmd) {
    case BLE_PLANT_CMD_GET_CAP:
        MIDI_UPDATE_WRITE_BIG_U32((u8 *) & (send_head.length), (u32)4);
        MIDI_UPDATE_WRITE_BIG_U32(packet_hdl.send_tmp_buf + PACKET_HEAD_LEN, (u32)param);
        break;
    case BLE_PLANT_CMD_UPDATE_FILE_PREPARE:
        MIDI_UPDATE_WRITE_BIG_U32((u8 *) & (send_head.length), (u32)4);
        MIDI_UPDATE_WRITE_BIG_U32(packet_hdl.send_tmp_buf + PACKET_HEAD_LEN, (u32)param);
        break;
    case BLE_PLANT_CMD_UPDATE_FILE:
        MIDI_UPDATE_WRITE_BIG_U32((u8 *) & (send_head.length), (u32)4);
        MIDI_UPDATE_WRITE_BIG_U32(packet_hdl.send_tmp_buf + PACKET_HEAD_LEN, (u32)param);
        break;
    default:
        return;
    }

    memcpy(packet_hdl.send_tmp_buf, (u8 *)&send_head, PACKET_HEAD_LEN);

    crc = cal_crc16_deal(((u8 *)packet_hdl.send_tmp_buf) + CRC_OFFSET, PACKET_HEAD_LEN - CRC_OFFSET + 4);
    MIDI_UPDATE_WRITE_BIG_U16(packet_hdl.send_tmp_buf + 4, (u16)crc);

    u32 err = app_data_send(packet_hdl.send_tmp_buf, PACKET_HEAD_LEN + 4);

    /* midi_update_printf("-----------------err = %d, fun = %s, line = %d\n", err, __FUNCTION__, __LINE__); */
    /* midi_update_printf_buf(packet_hdl.send_tmp_buf, PACKET_HEAD_LEN + 4); */
}

/* static volatile u8 midi_update_prepare = 0; */
/* void midi_update_prepare_ok(void) */
/* { */
/* midi_update_prepare = 1;	 */
/* } */

static tbool midi_update_prepare_wait(void)
{
    u8 err = 0;

    if (script_get_id() == SCRIPT_ID_MIDI_UPDATE) {
        return true;
    }

    while (1) {
        err = os_taskq_post(MAIN_TASK_NAME, 2, SYS_EVENT_TASK_RUN_REQ, SCRIPT_ID_MIDI_UPDATE);
        if (err == OS_Q_FULL) {
            OSTimeDly(1);
        } else {
            if (err) {
                midi_update_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
                return false;
            }
            break;
        }
    }

    while (script_get_id() != SCRIPT_ID_MIDI_UPDATE) {
        OSTimeDly(1);
    }

    midi_update_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);

    return true;
}

static tbool midi_update_doing(u8 *buf, u32 len)
{
    u8 err = 0;
    while (1) {
        err = os_taskq_post(SCRIPT_TASK_NAME, 3, MSG_MODULE_MIDI_UPDATE_TO_FLASH, buf, len);
        if (err == OS_Q_FULL) {
            OSTimeDly(1);
        } else {
            if (err) {
                return false;
            }
            break;
        }
    }
    return true;
}

void midi_update_stop(void)
{
    u8 err = 0;
    while (1) {
        err = os_taskq_post(TASK_MIDI_UPDATE_NAME, 1, MSG_MODULE_MIDI_UPDATE_TO_FLASH_STOP);
        if (err == OS_Q_FULL) {
            OSTimeDly(1);
        } else {
            if (err) {
                midi_update_printf("update stop fail !!\n");
            }
            break;
        }
    }
}

void midi_update_disconnect(void)
{
    u8 err = 0;
    while (1) {
        err = os_taskq_post(TASK_MIDI_UPDATE_NAME, 1, MSG_MODULE_MIDI_UPDATE_DISCONNECT);
        if (err == OS_Q_FULL) {
            OSTimeDly(1);
        } else {
            if (err) {
                midi_update_printf("update tell disconnect fail !!\n");
            }
            break;
        }
    }
}

void midi_update_disconnect_msg_to_app(void)
{
    u8 err = 0;
    while (1) {
        err = os_taskq_post(SCRIPT_TASK_NAME, 1, MSG_MODULE_MIDI_UPDATE_DISCONNECT);
        if (err == OS_Q_FULL) {
            OSTimeDly(1);
        } else {
            if (err) {
                midi_update_printf("update tell disconnect fail !!\n");
            }
            break;
        }
    }
}


static volatile u8 update_check_flag = 0;
u32 midi_update_check(void)
{
    return update_check_flag;
}

u8 *midi_file_buf = NULL;
void midi_update_task_deal(void *parg)
{
    int msg[6];
    u32 err;
    u8 *recv_buf = NULL;
    u32 recv_len = 0;
    u32 cur_pid = 0;
    u32 last_pid = (u32) - 1;
    u32 update_file_size = 0;
    u32 had_recive_len = 0;
    u8  is_update_to_flash = 0;

    while (1) {
        memset(msg, 0x00, sizeof(msg));
        os_taskq_pend(0, ARRAY_SIZE(msg), msg);
        midi_update_printf("get_msg, 0x%x\n", msg[0]);
        clear_wdt();
        switch (msg[0]) {
        case MSG_MODULE_MIDI_UPDATE:
            switch ((u16)msg[1]) {
            case BLE_PLANT_CMD_GET_CAP:
                debug_time_gap((void *)__FUNCTION__, (u32)__LINE__);
                midi_update_answer(BLE_PLANT_CMD_GET_CAP, (u32)msg[4], (u32)MIDI_UPDATE_CAP);
                debug_time_gap((void *)__FUNCTION__, (u32)__LINE__);
                break;
            case BLE_PLANT_CMD_UPDATE_FILE_PREPARE:
                debug_time_gap((void *)__FUNCTION__, (u32)__LINE__);
                if (midi_file_buf != NULL) {
                    printf("\r\n is updating !!!!\r\n");
                    break;
                }
                recv_len = (u32)msg[3];
                recv_buf = (u8 *)msg[2];
                memcpy((u8 *)&update_file_size, recv_buf, 4);
                update_file_size = htonl(update_file_size);
                if (update_file_size > MIDI_UPDATE_CAP) {
                    midi_update_printf("update file is over limit err !!\n");
                    break;
                }

                update_check_flag = 1;
                if (midi_update_prepare_wait() == false) {
                    midi_update_printf("update file prepare fail !!\n");
                    midi_update_answer(BLE_PLANT_CMD_UPDATE_FILE_PREPARE, (u32)msg[4], (u32)1);
                    update_check_flag = 0;
                    break;
                }

                update_check_flag = 0;
                /* if (midi_file_buf == NULL) { */
                midi_update_printf("-----------------malloc ---------------------\n");
                midi_file_buf = (u8 *)calloc(1, update_file_size);
                /* } */

                if (midi_file_buf == NULL) {
                    midi_update_printf("malloc fail, update file len = %d, fun = %s, line = %d\n", update_file_size, __FUNCTION__, __LINE__);
                    midi_update_answer(BLE_PLANT_CMD_UPDATE_FILE_PREPARE, (u32)msg[4], (u32)1);
                    break;
                }

                last_pid = (u32) - 1;
                had_recive_len = 0;

                debug_time_gap((void *)__FUNCTION__, (u32)__LINE__);
                midi_update_answer(BLE_PLANT_CMD_UPDATE_FILE_PREPARE, (u32)msg[4], (u32)0);
                debug_time_gap((void *)__FUNCTION__, (u32)__LINE__);
                break;

            case BLE_PLANT_CMD_UPDATE_FILE:
                debug_time_gap((void *)__FUNCTION__, (u32)__LINE__);
                recv_buf = (u8 *)msg[2];
                recv_len = (u32)msg[3];
                cur_pid = (u32)msg[4];
                if (cur_pid != (last_pid + 1)) {
                    midi_update_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
                    debug_time_gap((void *)__FUNCTION__, (u32)__LINE__);
                    midi_update_answer(BLE_PLANT_CMD_UPDATE_FILE, (u32)msg[4], (u32)1);
                    break;
                }
                last_pid = cur_pid;

                memcpy(midi_file_buf + cur_pid * 512, recv_buf,  recv_len);
                midi_update_answer(BLE_PLANT_CMD_UPDATE_FILE, (u32)msg[4], (u32)0);
                debug_time_gap((void *)__FUNCTION__, (u32)__LINE__);
                had_recive_len += recv_len;
                if (had_recive_len >= update_file_size) {
                    midi_update_printf("recv file end !!!%x, %x, %d, fun = %s, line = %d\r\n", had_recive_len, update_file_size, cur_pid, __FUNCTION__, __LINE__);

                    if (midi_update_doing(midi_file_buf, update_file_size) == false) {
                        midi_update_printf("update midi flash file fail!!!\n\n", update_file_size);
                        if (midi_file_buf) {
                            midi_update_printf("-----------------free ---------------------\n");
                            free(midi_file_buf);
                            midi_file_buf = NULL;
                        }
                    } else {
                        is_update_to_flash = 1;
                    }
                }
                break;

            default:
                break;
            }
            packet_hdl.busy = 0;
            break;
        case MSG_MODULE_MIDI_UPDATE_DISCONNECT:
            if (is_update_to_flash) {
                break;
            }
            midi_update_disconnect_msg_to_app();
        case MSG_MODULE_MIDI_UPDATE_TO_FLASH_STOP:
            midi_update_printf("fun = %s, line = %d\n", __FUNCTION__, __LINE__);
            if (midi_file_buf) {
                midi_update_printf("-----------------free ---------------------\n");
                free(midi_file_buf);
                midi_file_buf = NULL;
            }
            is_update_to_flash = 0;
            break;
        case SYS_EVENT_DEL_TASK:
            if (os_task_del_req_name(OS_TASK_SELF) == OS_TASK_DEL_REQ) {
                os_task_del_res_name(OS_TASK_SELF);
            }
            break;

        default:
            break;
        }
    }
}


void midi_update_init(void)
{
    u8 err = os_task_create(midi_update_task_deal, (void *)0, TaskMidiUpdate, 10, TASK_MIDI_UPDATE_NAME);
    if (err) {
        midi_update_printf("NotePlayerTask creat  fail, err = %d\r\n", err);
    }
    midi_update_printf("NotePlayerTask creat  ok, \n");
}

#endif



