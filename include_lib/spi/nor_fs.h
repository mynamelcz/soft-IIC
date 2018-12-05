/***********************************Jieli tech************************************************
  File : nor_fs.h
  By   : Huxi
  Email: xi_hu@zh-jieli.com
  date : 2016-11-30 14:30
********************************************************************************************/
#ifndef _NOR_FS_H_
#define _NOR_FS_H_

// #include "sdk_cfg.h"
#include "typedef.h"



#define REC_FILE_END 0xFE


//文件索引
typedef struct __RECF_INDEX_INFO {
    u32 index;  //文件索引号
    u16 sector; //文件所在扇区
} RECF_INDEX_INFO ;

#define FLASH_PAGE_SIZE 256
//文件系统句柄
typedef struct __RECFILESYSTEM {
    RECF_INDEX_INFO index;
    u8 buf[FLASH_PAGE_SIZE];
    u16 total_file;
    u16 first_sector;
    u16 last_sector;
    // u8 *buf;
    u8 sector_size;
    void (*eraser)(u32 address);
    s32(*read)(u8 *buf, u32 addr, u32 len);
    s32(*write)(u8 *buf, u32 addr, u32 len);
} RECFILESYSTEM, *PRECFILESYSTEM ;



//文件句柄
typedef struct __REC_FILE {
    RECF_INDEX_INFO index;
    RECFILESYSTEM *pfs;
    u32 len;
    u32 w_len;
    u32 rw_p;
    u16 sr;
} REC_FILE;

enum {
    NOR_FS_SEEK_SET = 0x01,
    NOR_FS_SEEK_CUR = 0x02
};


u8 recf_seek(REC_FILE *pfile, u8 type, u32 offsize);
u16 recf_read(REC_FILE *pfile, u8 *buff, u16 btr);
u16 recf_write(REC_FILE *pfile, u8 *buff, u16 btw);
u32 create_recfile(RECFILESYSTEM *pfs, REC_FILE *pfile);
u32 close_recfile(REC_FILE *pfile);
u32 open_recfile(u32 index, RECFILESYSTEM *pfs, REC_FILE *pfile);
void recf_save_sr(REC_FILE *pfile, u16 sr);

u32 recfs_scan(RECFILESYSTEM *pfs);
void init_nor_fs(RECFILESYSTEM *pfs, u16 sector_start, u16 sector_end, u8 sector_size);

#endif
