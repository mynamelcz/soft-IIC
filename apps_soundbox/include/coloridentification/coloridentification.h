#ifndef _COLORIDENTIFICATION_H
#define _COLORIDENTIFICATION_H
#include "typedef.h"
//#define IDENTIFICATION_EN
///blue gree red
#define BLUELIGHT BIT(2)
#define GREELIGHT BIT(3)
#define REDLIGHT BIT(5)
///light on or off
#define LIGHTON  1
#define LIGHTOFF 0
///color
typedef enum {
    BLACK,
    RED,
    GREE,
    BLUE,
    YELLOW,
    PINK,
    LIGHTBLUE,
    WHITE,
    SCANCOLOR,
    MAXCOLOR,
} _color;

typedef enum {
    TABLEBLUE,
    TABLEORANGE,
    TABLERED,
    TABLEGREE,
    TABLEGRAY,
    TABLEPURPLE,
    TABLEYELLOW,
    TABLEWHITE,
    TABLEBLACK,
    TABLEMAXCOLOR,
} _colorTable;
///color threshold
///red
#define REDMINTHREAHOLD  4
#define REDMAXTHREAHOLD  10
///gree
#define GREEMINTHREASHOLD 4
#define GREEMAXTHREASHOLD 10
///blue
#define BLUEMINTHREASHOLD 4
#define BLUEMAXTHREASHOLD 10
///yellow
#define YELLOWMINTHREASHOLD 4
#define YELLOWMAXTHREASHOLD 10
///pink
#define PINKMINTHREASHOLD 4
#define PINKMAXTHREASHOLD 10
///light blue
#define LIGHTBLUEMINTHREASHOLD 4
#define LIGHTBLUEMAXTHREASHOLD 10
///white
#define WHITEMINTHREASHOLD 4
#define WHITEMAXTHREASHOLD 10
///scan time
#define COLORSCANTIME 20
///color filter
#define COLORFILTERTIMES 3
///black time
#define BLACKTIME 3000
///black wait time
#define BLACKWAITTIME 1500
typedef struct __blackMode {
    u32 blackTimeCnt;
    u32 timeWaitCnt;
    u8 en;
} _blackMode;
typedef struct __ad_value {
    u16 value;
} _ad_value;
typedef struct __colorCtl {
    u8 colorScanEnable;
    u16 scanColor;
    u16 scanColorBuf[MAXCOLOR];
    u8 getColor;
    u16 scanTime;
} _colorCtl;
typedef struct __color_info {
    _color currentScanColor;
    _color currentCard;
    _colorTable tempColor;
    _colorCtl colorCtl;
} _color_info;
typedef struct __color_api {
    _ad_value adValue;
    _color_info colorInfo;
    _blackMode blackMode;
} _color_api;

///获取颜色
extern _color GetColor(void);
///启动扫描
extern void COlorScanEnable(void);
///关闭扫描
extern void ColorScanDisable(void);
void GetVmColor(u8 color, u16 *red, u16 *gree, u16 *blue);
void SaveColor(u8 color);
#endif

