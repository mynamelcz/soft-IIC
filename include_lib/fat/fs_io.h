#ifndef _FS_IO_H_
#define _FS_IO_H_

#include "typedef.h"
#include "diskio.h"
#include "file_operate/fs_io_h.h"
#include "rtos/os_api.h"

#ifdef TOYS_EN
# define FS_IO_MUTEX_ENTER(fs) do{if ((fs)->io_mutex!=NULL) os_mutex_pend((OS_EVENT *)((fs)->io_mutex),0);}while(0)
# define FS_IO_MUTEX_EXIT(fs)  do{if ((fs)->io_mutex!=NULL) os_mutex_post((OS_EVENT *)((fs)->io_mutex));}while(0)
#endif	/*TOYS_EN*/

//#define FS_IO_DEBUG

#ifdef FS_IO_DEBUG
#define fs_io_deg             printf
#define fs_io_deg_puts        puts
#define fs_io_deg_buf         printf_buf
#else
#define fs_io_deg(...)
#define fs_io_deg_puts(...)
#define fs_io_deg_buf(...)
#endif

#define GET_LFN     1

//typedef enum __DEV_FS_TYPE
//{
//    FAT_FS_T =1,
//    SYD_FS_T,
//}DEV_FS_TYPE;
//
//typedef struct _FS_BRK_INFO
//{
//    u32 bk_sclust;
//    u32 f_size;
//    u8 brk_buff[8];
//    u8 lg_dev_num;
//    u8 phy_dev_num;
//    u16 crc;
//}FS_BRK_INFO;
//
//
//typedef struct _FS_BRK_POINT
//{
//    FS_BRK_INFO inf;
//    u32 file_num;
//    tbool result;
//}FS_BRK_POINT;

typedef struct _FS_IO {
    DEV_FS_TYPE type;
    tbool(*drive_open)(void **p_fs_hdl, void *p_hdev, u32 drive_base);
#ifdef TOYS_EN
    tbool(*drive_open_bylg)(void **p_fs_hdl, void *p_lg_fs_hdl);
    bool(*get_file_by_flash_rec_addr)(void *fs, void **fp, u32 flash_addr);
#endif	/*TOYS_EN*/
    tbool(*drive_close)(void **p_fs_hdl);
    u32(*get_file_total)(void *p_fs_hdl, const char *file_type, void *p_brk);

    bool (*get_file_byindex)(void *p_fs_hdl, void **p_f_hdl, u32 file_number, char *lfn);
    bool(*get_file_bypath)(void *fs, void **fp, u8 *path, char *lfn); // get_file_bybrkinfo
    bool(*get_file_bybrkinfo)(void *fs, void **fp, void *brk, char *lfn);
    tbool(*folder_file)(void *p_f_hdl, u32 *start_file, u32 *end_file);
    bool (*file_close)(void *p_fs_hdl, void **p_f_hdl);
    tbool(*get_brk_info)(void *brk_info, void *p_f_hdl);
    u16(*read)(void *p_f_hdl, u8 *buff, u16 len);
    bool (*seek)(void *p_f_hdl, u8 type, u32 offsize);
    bool (*get_fn)(void *p_fs_hdl, void *p_f_hdl, void *name_buf);
    u32(*get_sclust)(void *p_f_hdl);

    //////-------------fs write api----------------
    u16(*mk_dir)(void *p_fs_hdl, char *path, u8 mode);
    u16(*open)(void *p_fs_hdl, void *p_f_hdl, char *path, char *lfn_buf, u8 mode);
    u16(*write)(void *p_f_hdl, u8 *buff, u16 len);
    u16(*fw_close)(void *p_f_hdl);
    u16(*f_delete)(void *p_f_hdl);
    u32(*f_get_rec_fileinfo)(void *p_fs_hdl, char *path, char *ext, u32 *first_fn);
    u32(*enter_dir)(void *p_fs_hdl, void **p_f_hdl, void *dir_info, const char *file_type);
    u32(*exit_dir)(void *p_fs_hdl, void **p_f_hdl);
    u32(*get_dir)(void *p_fs_hdl, void *p_f_hdl, u32 start_num, u32 total_num, void *buf_info);
    bool(*get_file_byname_indir)(void *fs, void *s_fp, void **t_fp, void *ext_name); // get_file_bybrkinfo

    //////-------------fs free capacity statistics----------
#ifdef TOYS_EN
    u32(*free_cap_get_total)(void *p_fs_hdl, u32 limit, u32 *cls_size, u8 parm);
    void (*free_cap_set_buf)(void *p_fs_hdl, void *buf, u32 len);
#endif	/*TOYS_EN*/

} fs_io_t;


typedef struct __FS_HDL {
    fs_io_t *io;
    void *hdl;
} _FS_HDL;

typedef struct __FIL_HDL {
    fs_io_t *io;
    void *hdl;
} _FIL_HDL;



u16 fs_read(void *p_f_hdl, u8 _xdata *buff, u16 len);
bool fs_file_close(void *p_fs_hdl, void **p_f_hdl);
bool fs_get_file_byindex(void *p_fs_hdl, void **p_f_hdl, u32 file_number, char *lfn);
bool fs_get_file_bypath(void *p_fs_hdl, void **p_f_hdl, u8 *path, char *lfn);
u32 fs_get_file_total(void *p_fs_hdl, const char *file_type, FS_BRK_POINT *p_brk);
tbool fs_drive_close(void **p_fs_hdl);
tbool fs_get_brk_info(FS_BRK_POINT *brk_info, void *p_f_hdl);
tbool fs_drive_open(void **p_fs_hdl, void *p_hdev, u32 drive_base);
#ifdef TOYS_EN
tbool fs_drive_open_by_flg(void **p_fs_hdl, void *p_flg_fs_hdl);
bool fs_get_file_by_flash_rec_addr(void *p_fs_hdl, void **p_f_hdl, u32 flash_addr);
#endif	/*TOYS_EN*/
bool fs_seek(void *p_f_hdl, u8 type, u32 offsize);
u32 fs_tell(void *p_f_hdl);
u32 fs_length(void *p_f_hdl);
tbool fs_folder_file(void *p_f_hdl, u32 *start_file, u32 *end_file);
bool fs_get_file_bybrkinfo(void *p_fs_hdl, void **p_f_hdl, FS_BRK_POINT *brk, char *lfn);
s16 fs_mk_dir(void *p_fs_hdl, char *path, u8 mode);
u16 fs_write(void *p_f_hdl, u8 _xdata *buff, u16 len);
s16 fs_open(void *p_fs_hdl, void **p_f_hdl, char *path, char *lfn_buf, u8 mode);
s16 fs_close(void *p_f_hdl);
s16 fs_delete(void *p_f_hdl);
s16 fs_get_fileinfo(void *p_fs_hdl, char *path, char *ext, u32 *first_fn, u32 *total);
u32 fs_open_direct(void *p_fs_hdl, void **p_f_hdl, void *dir_info, const char *file_type);
u32 fs_exit_direct(void *p_fs_hdl, void **p_f_hdl);
u32 fs_get_direct(void *p_fs_hdl, void **p_f_hdl, u32 start_num, u32 total_num, void *buf_info);
u32 fs_get_file_indir(void *p_fs_hdl, void *s_f_hdl, void **tP_f_hdl, void *ext_name);
s16 fs_file_name(void *p_fs_hdl, void *p_f_hdl, void *name_buf);
u32 get_sclust(void *p_f_hdl);

#ifdef TOYS_EN
u32 fs_free_cap_get_total(void *p_fs_hdl, u32 limit, u32 *cls_size, u8 parm);
void fs_free_cap_set_buf(void *p_fs_hdl, void *buf, u32 len);
#endif	/*TOYS_EN*/

#endif
