
#ifndef __MIO_API_H_
#define __MIO_API_H_


#include "sdk_cfg.h"
#include "mio/mio_ctrl.h"


#ifdef MIO_USB_DEBUG
// #include "usb_device.h"
// #include "sd_host_api.h"
#include "usb/mango_dev_usb_slave.h"
// #include "dev_pc.h"
void music_usb_read(void);
void music_usb_start(void);
void music_usb_stop(void);
void music_usb_mio_cap(u32 start, u32 len);
SUSB_SCSI_INTERCEPT_RESULT mio_scsi_intercept(void *var);
#endif


void mio_api_close(mio_ctrl_t **priv);
mio_ctrl_t *mio_api_start(void *file_hdl, u32 start_addr);//file_hdl: _FIL_HDL*
void mio_api_run(mio_ctrl_t *p_mio_var);
u32 mio_api_get_size(mio_ctrl_t *p_mio_var);
u8  mio_api_get_status(mio_ctrl_t *p_mio_var);
u8  mio_api_set_offset(mio_ctrl_t *p_mio_var, u32 dac_offset);

extern volatile mio_ctrl_t *g_mio_var;


#endif

