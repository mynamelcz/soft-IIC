#include "wav_play.h"
#include "common/msg.h"
#include "dec/sup_dec_op.h"
#include "dev_manage/dev_ctl.h"
#include "fat/tff.h"
#include "file_operate/file_op.h"
#include "fat/fs_io.h"
#include "fat/tff.h"
#include "common/app_cfg.h"
#include "file_operate/logic_dev_list.h"
#include "dac/dac_api.h"
#include "vm/vm_api.h"
#include "coloridentification.h"
#include "timer.h"
#ifdef IDENTIFICATION_EN
_color_api colorApi;
void LightCtl(u16 color, u8 mode)
{
    JL_PORTD->DIR &= ~color;
    JL_PORTD->PD &= ~color;
    JL_PORTD->PU &= ~color;
    JL_PORTD->HD |= color;

    if (mode == LIGHTON) {
        JL_PORTD->OUT &= ~color;
    } else {
        JL_PORTD->OUT |= color;
    }
}
void LightOffAll(void)
{
    LightCtl(BLUELIGHT, LIGHTOFF);
    LightCtl(GREELIGHT, LIGHTOFF);
    LightCtl(REDLIGHT, LIGHTOFF);
}
void ColorIdentificationInitLight(void)
{
    memset(&colorApi, 0x00, sizeof(_color_api));
    LightCtl(BLUELIGHT, LIGHTOFF);
    LightCtl(GREELIGHT, LIGHTOFF);
    LightCtl(REDLIGHT, LIGHTOFF);
}

void ColorSensorEnable(void)
{
    JL_PORTA->DIR &= ~BIT(3);
    JL_PORTA->PD &= ~BIT(3);
    JL_PORTA->PU &= ~BIT(3);

    JL_PORTA->OUT &= ~BIT(3);
}
void ColorSensorDisable(void)
{
    JL_PORTA->DIR &= ~BIT(3);
    JL_PORTA->PD &= ~BIT(3);
    JL_PORTA->PU &= ~BIT(3);

    JL_PORTA->OUT |= BIT(3);
}
void SetColorAdValue(u16 value)
{
    colorApi.adValue.value = value;
}

u16 GetColorAdValue(void)
{
//    printf("colorApi.adValue.value:%d \n",colorApi.adValue.value);
    return colorApi.adValue.value;
}

void COlorScanEnable(void)
{
    colorApi.colorInfo.colorCtl.colorScanEnable = 1;
}
void ColorScanDisable(void)
{
    colorApi.colorInfo.colorCtl.colorScanEnable = 0;
    LightOffAll();
}

/*
蓝色: red:250   gree:570  blue:868
橙色：red:570   gree:526  blue:520
红色：red:605   gree:409  blue:480
绿色：red:345   gree:760  blue:545
灰色：red:350   gree:550  blue:650
紫色：red:380   gree:380  blue:580
黄色：red:640   gree:850  blue:570
*/
#if 1
u32 colorTable[][4] = {
    {0, 90, 224, 279},
    {0, 202, 253, 211},
    {0, 238, 206, 197},
    {0, 126, 298, 245},
    {0, 121, 260, 249},
    {0, 151, 208, 234},
    {0, 255, 503, 265},
    {0, 254, 527, 513},
    {0, 76, 160, 203},
};
#else
const u32 colorTable[][3] = {
    {250, 570, 868},
    {570, 526, 520},
    {605, 409, 480},
    {345, 760, 545},
    {350, 550, 650},
    {380, 380, 580},
    {640, 850, 570},
};
#endif
static u16 red;
static u16 gree;
static u16 blue;
void SetColorTable(void)
{
    u8 color;
    for (color = TABLEBLUE; color < TABLEBLACK; color++) {
        GetVmColor(color, &red, &gree, &blue);
        colorTable[color][1] = red;
        colorTable[color][2] = gree;
        colorTable[color][3] = blue;
        printf("red:%d gree:%d blue:%d   \n", red, gree, blue);
    }
}

void GetColorAction(color)
{
    printf("color:%d \n", color);
}
_color GetColor(void)
{
#define IDCOLOR 26
    u8 color = TABLEMAXCOLOR;
    static u8 tempColor = TABLEMAXCOLOR;
    static u8 colorCnt = 0;

    if (colorApi.colorInfo.colorCtl.getColor) {
        ///解码颜色

        if (((colorTable[TABLEBLUE][RED] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[RED] && colorApi.colorInfo.colorCtl.scanColorBuf[RED] < (colorTable[TABLEBLUE][RED] + 30))\
            && ((colorTable[TABLEBLUE][GREE] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[GREE] && colorApi.colorInfo.colorCtl.scanColorBuf[GREE] < (colorTable[TABLEBLUE][GREE] + 30))\
            && ((colorTable[TABLEBLUE][BLUE] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[BLUE] && colorApi.colorInfo.colorCtl.scanColorBuf[BLUE] < (colorTable[TABLEBLUE][BLUE] + 30))) {
            color = TABLEBLUE;
//            printf("蓝色 \n");
        }

        if (((colorTable[TABLEORANGE][RED] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[RED] && colorApi.colorInfo.colorCtl.scanColorBuf[RED] < (colorTable[TABLEORANGE][RED] + IDCOLOR))\
            && ((colorTable[TABLEORANGE][GREE] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[GREE] && colorApi.colorInfo.colorCtl.scanColorBuf[GREE] < (colorTable[TABLEORANGE][GREE] + IDCOLOR))\
            && ((colorTable[TABLEORANGE][BLUE] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[BLUE] && colorApi.colorInfo.colorCtl.scanColorBuf[BLUE] < (colorTable[TABLEORANGE][BLUE] + IDCOLOR))) {
            color = TABLEORANGE;
//            printf("橙色 \n");
        }

        if (((colorTable[TABLERED][RED] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[RED] && colorApi.colorInfo.colorCtl.scanColorBuf[RED] < (colorTable[TABLERED][RED] + IDCOLOR))
            && ((colorTable[TABLERED][GREE] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[GREE] && colorApi.colorInfo.colorCtl.scanColorBuf[GREE] < (colorTable[TABLERED][GREE] + IDCOLOR))
            && ((colorTable[TABLERED][BLUE] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[BLUE] && colorApi.colorInfo.colorCtl.scanColorBuf[BLUE] < (colorTable[TABLERED][BLUE] + IDCOLOR))) {
            color = TABLERED;
//            printf("红色 \n");
        }

        if (((colorTable[TABLEGREE][RED] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[RED] && colorApi.colorInfo.colorCtl.scanColorBuf[RED] < (colorTable[TABLEGREE][RED] + IDCOLOR))
            && ((colorTable[TABLEGREE][GREE] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[GREE] && colorApi.colorInfo.colorCtl.scanColorBuf[GREE] < (colorTable[TABLEGREE][GREE] + IDCOLOR))
            && ((colorTable[TABLEGREE][BLUE] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[BLUE] && colorApi.colorInfo.colorCtl.scanColorBuf[BLUE] < (colorTable[TABLEGREE][BLUE] + IDCOLOR))) {
            color = TABLEGREE;
//            printf("绿色 \n");
        }

        if (((colorTable[TABLEGRAY][RED] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[RED] && colorApi.colorInfo.colorCtl.scanColorBuf[RED] < (colorTable[TABLEGRAY][RED] + IDCOLOR))
            && ((colorTable[TABLEGRAY][GREE] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[GREE] && colorApi.colorInfo.colorCtl.scanColorBuf[GREE] < (colorTable[TABLEGRAY][GREE] + IDCOLOR))
            && ((colorTable[TABLEGRAY][BLUE] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[BLUE] && colorApi.colorInfo.colorCtl.scanColorBuf[BLUE] < (colorTable[TABLEGRAY][BLUE] + IDCOLOR))) {
            color = TABLEGRAY;
//            printf("灰色 \n");
        }

        if (((colorTable[TABLEPURPLE][RED] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[RED] && colorApi.colorInfo.colorCtl.scanColorBuf[RED] < (colorTable[TABLEPURPLE][RED] + IDCOLOR))
            && ((colorTable[TABLEPURPLE][GREE] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[GREE] && colorApi.colorInfo.colorCtl.scanColorBuf[GREE] < (colorTable[TABLEPURPLE][GREE] + IDCOLOR))
            && ((colorTable[TABLEPURPLE][BLUE] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[BLUE] && colorApi.colorInfo.colorCtl.scanColorBuf[BLUE] < (colorTable[TABLEPURPLE][BLUE] + IDCOLOR))) {
            color = TABLEPURPLE;
//            printf("紫色 \n");
        }

        if (((colorTable[TABLEYELLOW][RED] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[RED] && colorApi.colorInfo.colorCtl.scanColorBuf[RED] < (colorTable[TABLEYELLOW][RED] + IDCOLOR))
            && ((colorTable[TABLEYELLOW][GREE] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[GREE] && colorApi.colorInfo.colorCtl.scanColorBuf[GREE] < (colorTable[TABLEYELLOW][GREE] + IDCOLOR))
            && ((colorTable[TABLEYELLOW][BLUE] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[BLUE] && colorApi.colorInfo.colorCtl.scanColorBuf[BLUE] < (colorTable[TABLEYELLOW][BLUE] + IDCOLOR))) {
            color = TABLEYELLOW;
//            printf("黄色 \n");
        }

        if (((colorTable[TABLEWHITE][RED] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[RED] && colorApi.colorInfo.colorCtl.scanColorBuf[RED] < (colorTable[TABLEWHITE][RED] + IDCOLOR))
            && ((colorTable[TABLEWHITE][GREE] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[GREE] && colorApi.colorInfo.colorCtl.scanColorBuf[GREE] < (colorTable[TABLEWHITE][GREE] + IDCOLOR))
            && ((colorTable[TABLEWHITE][BLUE] - IDCOLOR) < colorApi.colorInfo.colorCtl.scanColorBuf[BLUE] && colorApi.colorInfo.colorCtl.scanColorBuf[BLUE] < (colorTable[TABLEWHITE][BLUE] + IDCOLOR))) {
            color = TABLEWHITE;
//            printf("白色 \n");
        }

        if (((colorTable[TABLEBLACK][RED] - 20) < colorApi.colorInfo.colorCtl.scanColorBuf[RED] && colorApi.colorInfo.colorCtl.scanColorBuf[RED] < (colorTable[TABLEBLACK][RED] + 20))
            && ((colorTable[TABLEBLACK][GREE] - 20) < colorApi.colorInfo.colorCtl.scanColorBuf[GREE] && colorApi.colorInfo.colorCtl.scanColorBuf[GREE] < (colorTable[TABLEBLACK][GREE] + 20))
            && ((colorTable[TABLEBLACK][BLUE] - 20) < colorApi.colorInfo.colorCtl.scanColorBuf[BLUE] && colorApi.colorInfo.colorCtl.scanColorBuf[BLUE] < (colorTable[TABLEBLACK][BLUE] + 20))) {
//            printf("黑色 \n");
            colorApi.blackMode.blackTimeCnt++;
            if (colorApi.blackMode.blackTimeCnt * COLORSCANTIME * (BLUE + 1) > BLACKTIME) {
                colorApi.blackMode.timeWaitCnt = 0;
                colorApi.blackMode.en = 1;
            }
        }
    }
    /*连续检测到n次都是同一个颜色才下去处理*/
    if (color ==  tempColor) {
        colorCnt++;
        if (colorCnt < COLORFILTERTIMES) {
            return BLACK;
        }
    } else {
        colorCnt = 0;
        tempColor = color;
        return BLACK;
    }

    if (colorApi.colorInfo.tempColor < TABLEMAXCOLOR && colorApi.colorInfo.tempColor == color) {

    } else {
        colorApi.colorInfo.tempColor = color;
        if (colorApi.colorInfo.tempColor != TABLEMAXCOLOR) {
            switch (colorApi.colorInfo.tempColor) {
            case TABLEBLUE:
                printf("蓝色 \n");
                break;
            case TABLEORANGE:
                printf("橙色 \n");
                break;
            case TABLERED:
                printf("红色 \n");
                break;
            case TABLEGREE:
                printf("绿色 \n");
                break;
            case TABLEGRAY:
                printf("灰色 \n");
                break;
            case TABLEPURPLE:
                printf("紫色 \n");
                break;
            case TABLEYELLOW:
                printf("黄色 \n");
                break;
            case TABLEWHITE:
                printf("白色 \n");
                break;
            default:
                break;
            }
            GetColorAction(color);
        }
    }

    ///还没有扫描出来颜色
    return color;
}

static bool ColorActionActive(u32 color)
{
    if (color == 0) {
        return true;
    }

    return false;
}
extern u8 GetIsCheckColor(void);
void ColorScan(void)
{
    if (GetIsCheckColor()) {
        return;
    }

    if (colorApi.colorInfo.colorCtl.colorScanEnable) {
//        if (colorApi.blackMode.en && colorApi.blackMode.timeWaitCnt*COLORSCANTIME<BLACKWAITTIME)
//        {///每隔BLACKWAITTIME这么长时间就扫描一次
//            colorApi.blackMode.timeWaitCnt++;
//            colorApi.colorInfo.tempColor = TABLEMAXCOLOR;
//            return;
//        }

        ColorSensorEnable();
        colorApi.colorInfo.colorCtl.scanTime++;
        if (colorApi.colorInfo.colorCtl.scanTime >= (COLORSCANTIME / 10)) {
            colorApi.colorInfo.colorCtl.scanTime = 0;
            colorApi.colorInfo.colorCtl.scanColorBuf[colorApi.colorInfo.colorCtl.scanColor] = GetColorAdValue();
            LightOffAll();
            colorApi.colorInfo.colorCtl.scanColor ++;
            if (colorApi.colorInfo.colorCtl.scanColor > BLUE) {
                colorApi.colorInfo.colorCtl.getColor = 1;
//                printf("BLACK:%d  \n",colorApi.colorInfo.colorCtl.scanColorBuf[BLACK]);
//                printf("BLACK:%d  RED:%d  GREE:%d  BLUE:%d \n",colorApi.colorInfo.colorCtl.scanColorBuf[BLACK],colorApi.colorInfo.colorCtl.scanColorBuf[RED],colorApi.colorInfo.colorCtl.scanColorBuf[GREE],colorApi.colorInfo.colorCtl.scanColorBuf[BLUE]);
                GetColor();
                colorApi.colorInfo.colorCtl.scanColor = BLACK;
            }
        }

        if (ColorActionActive(colorApi.colorInfo.colorCtl.scanColorBuf[BLACK]) == false) {
            ///整个机器立起来的时候才进行颜色识别
            colorApi.colorInfo.colorCtl.scanColor = BLACK;
            colorApi.colorInfo.tempColor = TABLEMAXCOLOR;
        }

        switch (colorApi.colorInfo.colorCtl.scanColor) {
        case BLACK:
            break;
        case RED:
            LightCtl(REDLIGHT, LIGHTON);
            break;

        case GREE:
            LightCtl(GREELIGHT, LIGHTON);
            break;

        case BLUE:
            LightCtl(BLUELIGHT, LIGHTON);
            break;

        default:
            break;
        }
    } else {
        colorApi.colorInfo.tempColor = TABLEMAXCOLOR;
        ColorSensorDisable();
        colorApi.colorInfo.colorCtl.getColor = 0;
        colorApi.colorInfo.colorCtl.scanTime = 0;
        colorApi.colorInfo.colorCtl.scanColor = RED;
    }

}

static u8 isCheckColor = 0;
void SetIsCheckColor(u8 flg)
{
    isCheckColor = flg;
}
u8 GetIsCheckColor(void)
{
    return isCheckColor;
}

void CheckColorScan(void)
{
    if (colorApi.colorInfo.colorCtl.colorScanEnable) {
        return;
    }

    if (GetIsCheckColor()) {
//        if (colorApi.blackMode.en && colorApi.blackMode.timeWaitCnt*COLORSCANTIME<BLACKWAITTIME)
//        {///每隔BLACKWAITTIME这么长时间就扫描一次
//            colorApi.blackMode.timeWaitCnt++;
//            colorApi.colorInfo.tempColor = TABLEMAXCOLOR;
//            return;
//        }

        ColorSensorEnable();
        colorApi.colorInfo.colorCtl.scanTime++;
        if (colorApi.colorInfo.colorCtl.scanTime >= (COLORSCANTIME / 10)) {
            colorApi.colorInfo.colorCtl.scanTime = 0;
            colorApi.colorInfo.colorCtl.scanColorBuf[colorApi.colorInfo.colorCtl.scanColor] = GetColorAdValue();
            LightOffAll();
            colorApi.colorInfo.colorCtl.scanColor ++;
            if (colorApi.colorInfo.colorCtl.scanColor > BLUE) {
                colorApi.colorInfo.colorCtl.getColor = 1;
//                printf("BLACK:%d  \n",colorApi.colorInfo.colorCtl.scanColorBuf[BLACK]);
//                printf("BLACK:%d  RED:%d  GREE:%d  BLUE:%d \n",colorApi.colorInfo.colorCtl.scanColorBuf[BLACK],colorApi.colorInfo.colorCtl.scanColorBuf[RED],colorApi.colorInfo.colorCtl.scanColorBuf[GREE],colorApi.colorInfo.colorCtl.scanColorBuf[BLUE]);
                colorApi.colorInfo.colorCtl.scanColor = BLACK;
            }
        }

        if (ColorActionActive(colorApi.colorInfo.colorCtl.scanColorBuf[BLACK]) == false) {
            ///整个机器立起来的时候才进行颜色识别
            colorApi.colorInfo.colorCtl.scanColor = BLACK;
            colorApi.colorInfo.tempColor = TABLEMAXCOLOR;
        }

        switch (colorApi.colorInfo.colorCtl.scanColor) {
        case BLACK:
            break;
        case RED:
            LightCtl(REDLIGHT, LIGHTON);
            break;

        case GREE:
            LightCtl(GREELIGHT, LIGHTON);
            break;

        case BLUE:
            LightCtl(BLUELIGHT, LIGHTON);
            break;

        default:
            break;
        }
    } else {
        colorApi.colorInfo.tempColor = TABLEMAXCOLOR;
        ColorSensorDisable();
        colorApi.colorInfo.colorCtl.getColor = 0;
        colorApi.colorInfo.colorCtl.scanTime = 0;
        colorApi.colorInfo.colorCtl.scanColor = RED;
    }

}
void SaveColor(u8 color)
{
    printf("BLACK:%d  RED:%d  GREE:%d  BLUE:%d \n", colorApi.colorInfo.colorCtl.scanColorBuf[BLACK], colorApi.colorInfo.colorCtl.scanColorBuf[RED], colorApi.colorInfo.colorCtl.scanColorBuf[GREE], colorApi.colorInfo.colorCtl.scanColorBuf[BLUE]);
    printf("red:%d gree:%d blue:%d \n", VM_CHECKCOLOR + 3 * color, VM_CHECKCOLOR + 3 * color + 1, VM_CHECKCOLOR + 3 * color + 2);
    br17_vm_write(VM_CHECKCOLOR + 3 * color, &colorApi.colorInfo.colorCtl.scanColorBuf[RED], sizeof(colorApi.colorInfo.colorCtl.scanColorBuf[RED]));
    br17_vm_write(VM_CHECKCOLOR + 3 * color + 1, &colorApi.colorInfo.colorCtl.scanColorBuf[GREE], sizeof(colorApi.colorInfo.colorCtl.scanColorBuf[GREE]));
    br17_vm_write(VM_CHECKCOLOR + 3 * color + 2, &colorApi.colorInfo.colorCtl.scanColorBuf[BLUE], sizeof(colorApi.colorInfo.colorCtl.scanColorBuf[BLUE]));
}
void GetVmColor(u8 color, u16 *red, u16 *gree, u16 *blue)
{
    br17_vm_read(VM_CHECKCOLOR + 3 * color, red, sizeof(*red));
    br17_vm_read(VM_CHECKCOLOR + 3 * color + 1, gree, sizeof(*gree));
    br17_vm_read(VM_CHECKCOLOR + 3 * color + 2, blue, sizeof(*blue));
}

void ColorIdentificationInit(void)
{

    s32 ret;
    ret = timer_reg_isr_fun(timer0_hl, 5, (void *)ColorScan, NULL);
    if (ret != TIMER_NO_ERR) {
        printf("battery_check err = %x\n", ret);
    }

    ret = timer_reg_isr_fun(timer0_hl, 5, (void *)CheckColorScan, NULL);
    if (ret != TIMER_NO_ERR) {
        printf("battery_check err = %x\n", ret);
    }
}
#endif
