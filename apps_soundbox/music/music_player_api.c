#include "music_player_api.h"

#define MUSIC_PLAYER_API_DEBUG_EN
#ifdef MUSIC_PLAYER_API_DEBUG_EN
#define music_player_api_printf printf
#else
#define music_player_api_printf(...)
#endif

int music_player_err_check(MUSIC_PLAYER_OBJ *mapi, u32 err, u8 auto_next_file, u8 auto_next_dev, u32 *totalerrfile)
{
    u32 result = -1;
    u32 status;
    if (mapi == NULL) {
        return -1;
    }

    music_player_api_printf("player err = %x\n", err);
    switch (err) {
    case SUCC_MUSIC_START_DEC:
        result = 0;
        break;
    case FILE_OP_ERR_INIT:              ///<文件选择器初始化错误
    case FILE_OP_ERR_OP_HDL:            ///<文件选择器指针错误
    case FILE_OP_ERR_LGDEV_NULL:        ///<没有设备
    case FILE_OP_ERR_NO_FILE_ALLDEV:    ///<没有文件（所有设备）
        __music_player_file_operate_ctl(mapi, FOP_CLOSE_LOGDEV, 0, 0);
        result = -1;
        break;
    case FILE_OP_ERR_OPEN_BPFILE:
        __music_player_set_file_index(mapi, 1);
        __music_player_set_file_sel_mode(mapi, PLAY_FIRST_FILE);
        __music_player_set_dev_sel_mode(mapi, DEV_SEL_CUR);
        result = 1;
        break;
    case FILE_OP_ERR_NUM:               ///<选择文件的序号出错
        __music_player_set_file_index(mapi, 1);
        __music_player_set_file_sel_mode(mapi, PLAY_SPEC_FILE);
        __music_player_set_dev_sel_mode(mapi, DEV_SEL_CUR);
        result = 1;
        break;
    case FILE_OP_ERR_LGDEV_MOUNT:
    case FILE_OP_ERR_NO_FILE_ONEDEV:    ///<当前选择的设备没有文件
        if (auto_next_dev == 0) {
            result = -1;
            break;
        }
        __music_player_set_file_index(mapi, 1);
        __music_player_set_file_sel_mode(mapi, PLAY_BREAK_POINT);
        __music_player_set_dev_sel_mode(mapi, DEV_SEL_NEXT);
        result = 1;
        break;
    case FILE_OP_ERR_LGDEV_NO_FIND:     ///<没找到指定的逻辑设备
        if (auto_next_dev == 0) {
            result = -1;
            break;
        }
        __music_player_set_file_index(mapi, 1);
        __music_player_set_file_sel_mode(mapi, PLAY_SPEC_FILE);
        __music_player_set_dev_sel_mode(mapi, DEV_SEL_NEXT);
        result = 1;
        break;
    case FILE_OP_ERR_OPEN_FILE:         ///<打开文件失败
    case ERR_MUSIC_START_DEC:
        if (totalerrfile == NULL) {
            result = -1;
            break;
        }
        status = __music_player_file_operate_ctl(mapi, FOP_DEV_STATUS, 0, 0);
        if (status == FILE_OP_ERR_OP_HDL) {
            ///<逻辑设备不再链表
            __music_player_file_operate_ctl(mapi, FOP_CLOSE_LOGDEV, 0, 0);
            result = -1;
            break;
        } else if (!status) {
            ///<逻辑设备掉线
            if (auto_next_dev == 0) {
                result = -1;
                break;
            }
            __music_player_set_file_index(mapi, 1);
            __music_player_set_file_sel_mode(mapi, PLAY_SPEC_FILE);
            __music_player_set_dev_sel_mode(mapi, DEV_SEL_NEXT);
            (*totalerrfile) = 0;
            result = 1;
            break;
        }
        (*totalerrfile) += 1;
        if ((*totalerrfile) >= __music_player_get_file_total(mapi, 1)) {
            ///<当前设备中音乐文件全部不可以解码，做好标识

            if (auto_next_dev == 0) {
                result = -1;
                break;
            }

            __music_player_file_operate_ctl(mapi, FOP_ALLFILE_ERR_LGDEV, 0, 0);

            __music_player_set_file_index(mapi, 1);
            __music_player_set_file_sel_mode(mapi, PLAY_BREAK_POINT);
            __music_player_set_dev_sel_mode(mapi, DEV_SEL_NEXT);
            (*totalerrfile) = 0;
            result = 1;
            break;
        }
        if (auto_next_file) {
            __music_player_set_file_sel_mode(mapi, PLAY_NEXT_FILE);
            __music_player_set_dev_sel_mode(mapi, DEV_SEL_CUR);
        } else {
            __music_player_set_file_sel_mode(mapi, PLAY_PREV_FILE);
            __music_player_set_dev_sel_mode(mapi, DEV_SEL_CUR);
        }
        result = 1;
        break;
    case ERR_MUSIC_API_NULL:
        result = -1;
        break;
    case FILE_OP_ERR_END_FILE:
        __music_player_set_file_index(mapi, 1);
        __music_player_set_file_sel_mode(mapi, PLAY_FIRST_FILE);
        if (auto_next_dev == 0) {
            __music_player_set_dev_sel_mode(mapi, DEV_SEL_CUR);
        } else {
            __music_player_set_dev_sel_mode(mapi, DEV_SEL_NEXT);
        }

        __music_player_set_save_breakpoint_enable(mapi, 0);
        __music_player_clear_bp_info(mapi);
        result = 1;
        break;
    case FILE_OP_ERR_PRE_FILE:
        __music_player_set_file_index(mapi, 1);
        __music_player_set_file_sel_mode(mapi, PLAY_LAST_FILE);
        if (auto_next_dev == 0) {
            __music_player_set_dev_sel_mode(mapi, DEV_SEL_CUR);
        } else {
            __music_player_set_dev_sel_mode(mapi, DEV_SEL_PREV);
        }
        result = 1;
        break;
    default:
        __music_player_set_file_sel_mode(mapi, PLAY_NEXT_FILE);
        __music_player_set_dev_sel_mode(mapi, DEV_SEL_CUR);
        result = 1;
        break;
    }
    return result;
}



/*----------------------------------------------------------------------------*/
/**@brief 音乐播放器解码错误处理函数
   @param
    mapi -- 音乐播放控制句柄
    err -- 播放错误列表
	auto_next_file -- 1：自动播放下一曲，0：自动播放上一曲
   @return 错误处理已成功播放放回true， 播放失败返回false
   @note
*/
/*----------------------------------------------------------------------------*/

tbool music_player_err_deal(MUSIC_PLAYER_OBJ *mapi, u32 err, u8 auto_next_file, u8 auto_next_dev)
{
    u32 totalerrfile = 0;
    int ret;
    while (1) {
        ret  = music_player_err_check(mapi, err, auto_next_file, auto_next_dev, &totalerrfile);
        if (ret == 0) {
            music_player_api_printf("music player err deal ok %d !!!\n", __LINE__);
            return true;
        } else if (ret == 1) {
            music_player_api_printf("music player err deal continue %d !!!\n", __LINE__);
            err = music_player_play(mapi);
            continue;
        } else {
            music_player_api_printf("music player err deal fail %d !!!\n", __LINE__);
            return false;
        }
    }

}

tbool music_player_mulfile_play_err_deal(MUSIC_PLAYER_OBJ *mapi, u32 err, u8 auto_next_file, u8 auto_next_dev, u32 StartAddr, u8 medType, mulfile_func_t *func)
{
    u32 totalerrfile = 0;
    int ret;
    while (1) {
        ret  = music_player_err_check(mapi, err, auto_next_file, auto_next_dev, &totalerrfile);
        if (ret == 0) {
            music_player_api_printf("music player err deal ok %d !!!\n", __LINE__);
            return true;
        } else if (ret == 1) {
            music_player_api_printf("music player err deal continue %d !!!\n", __LINE__);
            err = music_player_mulfile_play(mapi, StartAddr, medType, func);
            continue;
        } else {
            music_player_api_printf("music player err deal fail %d !!!\n", __LINE__);
            return false;
        }
    }

}


