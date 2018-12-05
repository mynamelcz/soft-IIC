#ifndef _MANGO_DEV_SPI_H_
#define _MANGO_DEV_SPI_H_

#include "typedef.h"
#include "dev_manage/device_driver.h"

enum {
    SPI_SUCC = 0,
    SPI_OFFLINE,
};

#define SPI_PARM_VALID_MASK	  0x5a5a0000
#define SPI_MODE_MASK         (0x00000c00|SPI_PARM_VALID_MASK)
#define SPI_CLK_MASK    	  0x000000ff

#define DEV_GET_DEV_ID          _IOR(DEV_GENERAL_MAGIC,0xe3,u32)	//获取设备的ID。SD/TF 卡返回“sdtf”(0x73647466)
#define DEV_SECTOR_ERASE        _IOW(DEV_GENERAL_MAGIC,0xe4,u32)    //设备页擦除
#define DEV_BLOCK_ERASE         _IOW(DEV_GENERAL_MAGIC,0xe5,u32)    //设备块擦除
#define DEV_CHIP_ERASE          _IOW(DEV_GENERAL_MAGIC,0xe6,u32)    //设备擦除
#define DEV_GET_TYPE            _IOR(DEV_GENERAL_MAGIC,0xe7,u32)    //返回设备的dev_type
#define DEV_CHECK_WPSTA         _IOR(DEV_GENERAL_MAGIC,0xe8,u32)


typedef enum FLASH_MODE {
    FAST_READ_OUTPUT_MODE = 0x0 | SPI_PARM_VALID_MASK,
    FAST_READ_IO_MODE = 0x00001000 | SPI_PARM_VALID_MASK,
    FAST_READ_IO_CONTINUOUS_READ_MODE = 0x00002000 | SPI_PARM_VALID_MASK,
} FLASH_MODE;
#define FLASH_MODE_MASK         (0x00003000|SPI_PARM_VALID_MASK)

#define SPI_FLASH_MAGIC         'F'
#define SPI_FLASH_READ_ID     _IOR(SPI_FLASH_MAGIC,1,u32)
#define SPI_FLASH_REMAP_CS    _IOW(SPI_FLASH_MAGIC,2,u32)
#define SPI_FLASH_PROTECT     _IOW(SPI_FLASH_MAGIC,2,u8)


typedef enum _spi_mode {
    SPI_2WIRE_MODE = 0x0 | SPI_PARM_VALID_MASK,
    SPI_ODD_MODE   = 0x00000400 | SPI_PARM_VALID_MASK,
    SPI_DUAL_MODE  = 0x00000800 | SPI_PARM_VALID_MASK,
    SPI_QUAD_MODE  = 0x00000c00 | SPI_PARM_VALID_MASK,
} spi_mode;

typedef enum _spi_clk {
    SPI_CLK_DIV1 = 0,
    SPI_CLK_DIV2,
    SPI_CLK_DIV3,
    SPI_CLK_DIV4,
    SPI_CLK_DIV5,
    SPI_CLK_DIV6,
    SPI_CLK_DIV7,
    SPI_CLK_DIV8,
    SPI_CLK_DIV9,
    SPI_CLK_DIV10,
    SPI_CLK_DIV11,
    SPI_CLK_DIV12,
    SPI_CLK_DIV13,
    SPI_CLK_DIV14,
    SPI_CLK_DIV15,
    SPI_CLK_DIV16,
    /*
    .
    .
    .
    */
    SPI_CLK_DIV256 = 255,
} spi_clk;


extern const struct DEV_IO *dev_reg_spi0(void *parm);
extern const struct DEV_IO *dev_reg_spi1(void *parm);
extern const struct DEV_IO *dev_reg_spifat0(void *parm);
extern const struct DEV_IO *dev_reg_spifat1(void *parm);

#endif //_MANGO_DEV_SPI0_H_

