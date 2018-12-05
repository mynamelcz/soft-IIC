#ifndef _IIC_SAVE_H
#include "includes.h"

void init_slave_iic(void (*received_cb_fun)(u8 *buffer,u8 len),u8 *send_buffer);
typedef enum _IIC_STATE
{
    IDLE_STATE,
    START_STATE,
    RECEIVE_STATE,
    SEND_STATE,
    ACK_STATE,
    SACK_STATE,
    NACK_STATE,
    ACK_END,
    SACK_END,
    NACK_END,

    RARK_STATE,
    RARK_END,

    STOP_STATE,

}IIC_STATE;



#endif // _IIC_SAVE_H
