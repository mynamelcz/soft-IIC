
#ifndef __FAT12_H
#define __FAT12_H

#include "typedef.h"


#define SPI_SEC_LEN         (4*1024L)
#define SPI_SEC_MIN         (256*1024L)
#define SPIFAT_ALGIN(var)   ((((var)+SPI_SEC_LEN-1)/SPI_SEC_LEN)*SPI_SEC_LEN)

typedef struct __fat12_io {
    void *hdev;
    u8(*disk_read)(void *hdev, u8 *buf, u32 spisec);		/* device read function */
    u8(*disk_write)(void *hdev, u8 *buf, u32 spisec);		/* device write function */
    u8(*disk_eraser)(void *hdev, u32 spisec);
} fat12_io;

typedef struct __fat12_ops {
    char *name ;
    u8(*open)(void *work_buf, const struct __fat12_io *i_o);
    u8(*sync)(void *work_buf);
    u8(*fs_check)(void *work_buf);
    u8(*fs_reset)(void *work_buf, u32 cap);
    u32(*need_size)() ;
    u32(*read)(void *work_buf, u8 *buf, u32 addr, u16 lenght);
    u32(*write)(void *work_buf, u8 *buf, u32 addr, u16 lenght);
} fat12_ops;

typedef enum {
    FAT12_ERR_OK = 0,
    FAT12_ERR_READ,
    FAT12_ERR_WRITE,
    FAT12_ERR_CHECK,
    FAT12_ERR_CAP_LASS,
} FAT12_ERR;

extern struct __fat12_ops *get_fat12_ops(void);


#endif
