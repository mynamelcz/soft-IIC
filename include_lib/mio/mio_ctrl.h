
#ifndef __MIO_CTRL_H_
#define __MIO_CTRL_H_

#include "mio.h"
#include "cbuf/circular_buf.h"

#define MIO_API_STU_READ        BIT(0)
#define MIO_API_STU_CHECK       BIT(1)
#define MIO_API_STU_USB_DEG     BIT(7)

typedef struct __mio_file {
    void *filehdl;  //type:_FIL_HDL*
    u16(*read)(void *p_f_hdl, u8 *buff, u16 len);
    bool (*seek)(void *p_f_hdl, u8 type, u32 offsize);
    u32(*tell)(void *p_f_hdl);
} mio_file_t;

typedef struct __mio_ctrl {
    void *father_name;
    u32 start_addr;
    u32 r_ptr;
    u32 w_ptr;
    volatile u8 status;
    void *inbuf;
    void *dcbuf;
    mio_dec_io_t    dec_io;
    mio_ops_t       *dec;
    cbuffer_t       cbuf;
    mio_file_t      file;
    void (*need_input)(void *mio_hdl);
} mio_ctrl_t;


extern int mio_ctrl_mutex_enter(void);
extern int mio_ctrl_mutex_exit(void);

extern u32 mio_ctrl_read_data(mio_ctrl_t *p_mio_var);
extern u16 mio_ctrl_input(void *priv, u32 addr, void *buf, u16 len);


#endif

