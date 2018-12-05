#ifndef __LINEIN_DIGITAL_H__
#define __LINEIN_DIGITAL_H__
#include "typedef.h"

void linein_aux_ladc_2_digtial(u8 *buf, u16 len);
void linein_digital_enable(u8 en);
void linein_digital_vocal_enable(u8 en);
void linein_digital_mute(u8 mute);
tbool linein_digital_mute_status(void);
u8 linein_digital_get_vocal_status(void);

#endif//__LINEIN_DIGITAL_H__
