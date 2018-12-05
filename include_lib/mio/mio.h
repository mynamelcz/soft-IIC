

#ifndef __MIO_H_
#define __MIO_H_

//-- MIO err type
typedef u8      mio_err_type;
#define MIO_NO_ERR          0
#define MIO_ERR_NO_MEM      -1
#define MIO_ERR_READ        -2
#define MIO_ERR_LOGO        -3
#define MIO_ERR_VER         -4
#define MIO_ERR_CHL         -5
#define MIO_ERR_END         -6
#define MIO_ERR_LOCATE      -7
#define MIO_ERR_PARM        -8
#define MIO_ERR_NO_INIT     -9

//-- MIO decode io
typedef struct __mio_dec_io {
    void *priv;
    u16(*input)(void *priv, u32 addr, void *buf, u16 len);

    void (*pwm_init)(u8 chl);
    void (*pwm_run)(u8 chl, u8 pwm_var);
    void (*io_init)(u16 mask);
    void (*io_run)(u16 mask, u16 io_var);
} mio_dec_io_t;

//-- MIO decoder interface
typedef struct __mio_decoder_ops {
    char *name ;
    mio_err_type(*open)(void *work_buf, const struct __mio_dec_io *decoder_io);
    mio_err_type(*locate)(void *work_buf, u32 *addr);
    mio_err_type(*check)(void *work_buf, u8 checkID3);
    mio_err_type(*run)(void *work_buf);
    u32(*need_workbuf_size)() ;
    u32(*get_total_size)(void *work_buf);
    u8(*get_status)(void *work_buf);
    u16(*get_rate)(void *work_buf);
    u8(*set_dac_offset)(void *workbuf, u32 dac_offset);
} mio_decoder_ops, mio_ops_t;


//-- MIO functions
extern mio_ops_t *get_mio_ops(void);




#endif

