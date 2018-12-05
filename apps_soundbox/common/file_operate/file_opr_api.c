
#include "file_operate/file_op.h"
#include "file_operate/file_op_err.h"
#include "fat/fs_io.h"
#include "fat/tff.h"
#include "malloc.h"


//#define FILE_OP_DEBUG

#ifdef FILE_OP_DEBUG
#define file_op_deg 		printf
#define file_op_deg_buf 	printf_buf
#else
#define file_op_deg(...)
#define file_op_deg_buf(...)
#endif

extern u32 file_operate_ini(void *parm, u32 *chg_flag);
extern u32 lg_dev_mount(void *parm, u8 first_let, u32 total_lgdev);
extern u32 file_operate_ini_again(void *parm, u32 *chg_flag);

FILE_OPERATE *file_opr_api_create(u8 dev)
{
    u8 ret;
    u32 phy_dev_chg;
    FILE_OPERATE *p_dev_fop;

    printf("------------------file opr api create ----------------- \n");
    p_dev_fop = malloc(sizeof(FILE_OPERATE));
    if (p_dev_fop == NULL) {
        file_op_deg("_%s,%d, malloc err \n", __func__, __LINE__);
        goto __create_err_exit;
    }
    memset(p_dev_fop, 0, sizeof(FILE_OPERATE));

    p_dev_fop->fop_init = malloc(sizeof(FILE_OPERATE_INIT));
    if (p_dev_fop->fop_init == NULL) {
        file_op_deg("_%s,%d, malloc err \n", __func__, __LINE__);
        goto __create_err_exit;
    }
    memset(p_dev_fop->fop_init, 0, sizeof(FILE_OPERATE_INIT));

    p_dev_fop->fop_init->cur_dev_mode = DEV_SEL_SPEC;
    p_dev_fop->fop_init->dev_let = dev;
    ret = file_operate_ini(p_dev_fop, &phy_dev_chg);
    if (ret != FILE_OP_NO_ERR) {
        file_op_deg("_%s,%d, opr init err0 \n", __func__, __LINE__);
        goto __create_err_exit;
    }

    if (p_dev_fop->cur_lgdev_info->fat_type == 0) {
        ret = lg_dev_mount(p_dev_fop->cur_lgdev_info->lg_hdl->phydev_item,
                           p_dev_fop->cur_lgdev_info->dev_let, 1);
        if (ret != FILE_OP_NO_ERR) {
            file_op_deg("_%s,%d, dev mount err \n", __func__, __LINE__);
            goto __create_err_exit;
        }

        ret = file_operate_ini_again(p_dev_fop, &phy_dev_chg);
        if (ret != FILE_OP_NO_ERR) {
            file_op_deg("_%s,%d, opr init err1 \n", __func__, __LINE__);
            goto __create_err_exit;
        }
    }

    return p_dev_fop;

__create_err_exit:
    if (p_dev_fop) {
        if (p_dev_fop->fop_init) {
            free(p_dev_fop->fop_init);
        }
        free(p_dev_fop);
    }
    return NULL;
}

void file_opr_api_close(FILE_OPERATE *p_dev_fop)
{
    if (p_dev_fop) {
        fs_close(p_dev_fop->cur_lgdev_info->lg_hdl->file_hdl);
        file_operate_ctl(FOP_CLOSE_LOGDEV, p_dev_fop, 0, 0);
        if (p_dev_fop->fop_init) {
            free(p_dev_fop->fop_init);
        }
        free(p_dev_fop);
    }
}

//
//#include "sdk_cfg.h"
//void fs_example(void)
//{
//	FILE_OPERATE *fs_op = file_opr_api_create('B');
//	ASSERT(fs_op);
//	//获取文件总数
//	u32 total_file = fs_get_file_total(fs_op->cur_lgdev_info->lg_hdl->fs_hdl, "MP3WAV", NULL);
//	printf("total_file = %d\n", total_file);
//	u32 i;
//	u32 cur_file_index;
//	for(i=0;i<total_file; i++)
//	{
//		char tpath[128];
//		char lfn_tmp_buf[LFN_MAX_SIZE];
//		LONG_FILE_NAME file_name;
//		LONG_FILE_NAME dir_name;
//		FS_DISP_NAME fs_tmp_name;
//
//		cur_file_index=i+1;
//		fs_get_file_byindex(fs_op->cur_lgdev_info->lg_hdl->fs_hdl,&(fs_op->cur_lgdev_info->lg_hdl->file_hdl), cur_file_index, lfn_tmp_buf);
//
//		memset((u8*)&dir_name, 0, sizeof(LONG_FILE_NAME));
//		memset((u8*)&file_name, 0, sizeof(LONG_FILE_NAME));
//		memset((u8*)&fs_tmp_name, 0, sizeof(FS_DISP_NAME));
//
//		fs_tmp_name.tpath = tpath;
//		fs_tmp_name.dir_name = &dir_name;
//		fs_tmp_name.file_name = &file_name;
//		s16 status = fs_file_name(fs_op->cur_lgdev_info->lg_hdl->fs_hdl, fs_op->cur_lgdev_info->lg_hdl->file_hdl, &fs_tmp_name);
//		if(status)
//		{
//			printf("get file name fail\n");
//		}
//		else
//		{
//			printf("get file name ok\n");
//			if(fs_tmp_name.file_name->lfn_cnt)
//			{
//				printf("long name\n");
//				printf_buf((u8*)fs_tmp_name.file_name->lfn, fs_tmp_name.file_name->lfn_cnt);
//			}
//			else
//			{
//				printf("short name\n");
//				printf("%s\n", fs_tmp_name.tpath);
//			}
//		}
//	}
//	file_opr_api_close(fs_op);
//}


/* #define _get_dev_struct_head(cur_list) list_entry(cur_list, lg_dev_list, list); */
extern void vfs_close(void *parm, u32 fs_type);
extern struct list_head *lg_dev_list_head;	         ///<盘符设备链表头指针
void  dev_fs_close_all(void)
{
    struct list_head *p_list_pos;
    lg_dev_list *pnode;
    lg_dev_info *pinfo;
    u8 i;

    if (lg_dev_list_head == NULL) {
        return ;
    }
    /* lg_dev_mutex_enter(); */

    p_list_pos = NULL;
    list_for_each(p_list_pos, lg_dev_list_head) {
        pnode = list_entry(p_list_pos, lg_dev_list, list);

        if (pnode->p_cur_dev->dev_let == 'A') {
            continue;
        }
#if 0
        pinfo = pnode->p_cur_dev;
        i = pnode->max_lg_hdl;
        pinfo++;
        i--;
        while (i) {
            if (pinfo->fat_type) {
                break;
            }

            pinfo++;
            i--;
        }
        if (i) {
            continue;
        }
#endif
        pinfo = pnode->p_cur_dev;
        vfs_close(pinfo->lg_hdl, pinfo->fat_type);
        pinfo->fat_type = 0;

    }

    /* lg_dev_mutex_exit(); */
}

//////////////////////////////////////////////////////////////////
extern u32 get_sys_2msCount(void);

#if 0
u8 dev_cap_buf[1024];
void dev_cap_test(u8 dev)
{
    FILE_OPERATE *cur_fop = NULL;

    if ((dev == 0) || (dev == 'A')) {
        return ;
    }

__get_cap_loop:
    malloc_stats();

    cur_fop = file_opr_api_create(dev);
    if (cur_fop == NULL) {
        goto __cap_test_exit;
    }
    memset(dev_cap_buf, 0, 1024);

    fs_free_cap_set_buf(cur_fop->cur_lgdev_info->lg_hdl->fs_hdl,
                        dev_cap_buf, 1024);

    {
        u32 free_cls = 0;
        u32 cls_size = 0;
        u32 limit = 0;

        u32 timer_cnt0, timer_cnt1;

        timer_cnt0 = get_sys_2msCount();
        free_cls = fs_free_cap_get_total(cur_fop->cur_lgdev_info->lg_hdl->fs_hdl,
                                         /* limit, &cls_size,FREE_CLS_USE_TOTAL_FLAG); */
                                         limit, &cls_size, FREE_CLS_RESET_ALL);
        timer_cnt1 = get_sys_2msCount();

        printf("cls_size=0x%x, free_cls=0x%x \n", cls_size, free_cls);
        printf("t0=%d, t1=%d, tt=%d \n", timer_cnt0, timer_cnt1, (timer_cnt1 - timer_cnt0) * 2);
    }

__cap_test_exit:
    if (cur_fop) {
        file_opr_api_close(cur_fop);
    }

    malloc_stats();

    os_time_dly(100);
    goto __get_cap_loop;
}
#endif

