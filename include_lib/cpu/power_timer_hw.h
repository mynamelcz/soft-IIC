#ifndef __POWER_TIMER_HW_H__
#define __POWER_TIMER_HW_H__
#include "typedef.h"

void __periodic_timer_init(u32 unit_ms, u32 timer_id);
void *periodic_timer_add(u32 period_ms, void (*func)(void));
void periodic_timer_remove(void *priv);

#endif// __POWER_TIMER_HW_H__
