
#include "typedef.h"



#define NFC_BREDR_OOB  0
#define NFC_BLE_OOB    1
extern void nfc_init(u8 mode);
extern void nfc_close();
extern void nfc_read_ok_infrom_init(u8 on_off, char *return_task, u16 msg, u16 time_out);
extern void nfc_ok_timer_scan(void *p);
extern u8 get_NFC_mode(void);
