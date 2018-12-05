#include "iic_save.h"
#include "irq_api.h"
#include "uart.h"

//#define DEBUG_IIC
#ifdef  DEBUG_IIC
#define iic_printf  printf
#define iic_putbuf  put_buf
#define iic_puts      puts
#else
#define iic_printf(...)
#define put_buf(...)
#define iic_puts(...)
#endif // DEBUG_IIC


#define IIC_SDA_PORT    JL_PORTA
#define IIC_SCL_PORT    JL_PORTA
#define IIC_SDA_BIT     5
#define IIC_SCL_BIT     8
#define IIC_SDA_EVENT   9
#define IIC_SCL_EVENT   2

#define sda_init_in()       do{IIC_SDA_PORT->DIR |=  BIT(IIC_SDA_BIT);IIC_SDA_PORT->PU |= BIT(IIC_SDA_BIT);}while(0)
#define sda_init_out()      do{IIC_SDA_PORT->DIR &= ~BIT(IIC_SDA_BIT);}while(0)
#define read_sda_io()       (IIC_SDA_PORT->IN & BIT(IIC_SDA_BIT))
#define sda_out_H()         do{IIC_SDA_PORT->OUT |=  BIT(IIC_SDA_BIT); }while(0)
#define sda_out_L()         do{IIC_SDA_PORT->OUT &= ~BIT(IIC_SDA_BIT); }while(0)


#define clk_init_in()       do{IIC_SCL_PORT->DIR |=  BIT(IIC_SCL_BIT);IIC_SCL_PORT->PU |= BIT(IIC_SCL_BIT);}while(0)
#define clk_init_out()      do{IIC_SCL_PORT->DIR &= ~BIT(IIC_SCL_BIT);}while(0)
#define read_scl_io()       (IIC_SCL_PORT->IN & BIT(IIC_SCL_BIT))
#define clk_out_H()         do{IIC_SCL_PORT->OUT |=  BIT(IIC_SCL_BIT); }while(0)
#define clk_out_L()         do{IIC_SCL_PORT->OUT |=  BIT(IIC_SCL_BIT); }while(0)

#define CLOSE_SDA_EDGE()    do{JL_WAKEUP->CON0 &= ~BIT(IIC_SDA_EVENT);}while(0)
#define CLOSE_SCL_EDGE()    do{JL_WAKEUP->CON0 &= ~BIT(IIC_SCL_EVENT);}while(0)
#define INIT_SDA_UP_EDGE()  do{JL_WAKEUP->CON0 |= BIT(IIC_SDA_EVENT); JL_WAKEUP->CON1 &= ~BIT(IIC_SDA_EVENT);}while(0)
#define INIT_SDA_DW_EDGE()  do{JL_WAKEUP->CON0 |= BIT(IIC_SDA_EVENT); JL_WAKEUP->CON1 |=  BIT(IIC_SDA_EVENT);}while(0)
#define INIT_CLK_UP_EDGE()  do{JL_WAKEUP->CON0 |= BIT(IIC_SCL_EVENT); JL_WAKEUP->CON1 &= ~BIT(IIC_SCL_EVENT);}while(0)
#define INIT_CLK_DW_EDGE()  do{JL_WAKEUP->CON0 |= BIT(IIC_SCL_EVENT); JL_WAKEUP->CON1 |=  BIT(IIC_SCL_EVENT);}while(0)
#define CLEAR_SDA_PEND()    do{JL_WAKEUP->CON2 |= BIT(IIC_SDA_EVENT);}while(0)
#define CLEAR_SCL_PEND()    do{JL_WAKEUP->CON2 |= BIT(IIC_SCL_EVENT);}while(0)
#define iic_ack()           do{IIC_SDA_PORT->DIR &= ~BIT(IIC_SDA_BIT);IIC_SDA_PORT->OUT &= ~BIT(IIC_SDA_BIT);}while(0)
#define iic_Nack()          do{IIC_SDA_PORT->DIR &= ~BIT(IIC_SDA_BIT);IIC_SDA_PORT->OUT |= BIT(IIC_SDA_BIT);}while(0)
#define IIC_CHIP_ID 0X7A



#define MAX_SEND_BUFF_SIZE      12
volatile u8 scl_cont = 0;
volatile u8 sda_cont = 0;
u8 send_data_index = 0;                         //发送数据索引
u8 receive_data = 0;                            //当前接收的数据
u8 received_chip_id = 0;                        //收到的ID号
u8 received_count = 0;                          //收到的数据个数
u8 receive_buffer[100];                          //接收数据的buffer
u8 send_data_buffer[MAX_SEND_BUFF_SIZE];        //发送数据buffer
u8 need_send_data = 0;                          //需要发送数据的标志
volatile u8 transport_bits = 0;                 //已传输的bits
IIC_STATE iic_state;
static inline void scl_isr_deal(void);
static inline void sda_isr_deal(void);
void (*send_data)(u8 *buffer);
void (*received_data_deal)(u8 *read_buffer,u8 received_len);
static void port_interrupt_isr()
{
    if(JL_WAKEUP->CON3 & BIT(IIC_SDA_EVENT))
    {
        CLEAR_SDA_PEND();
        sda_isr_deal();
        sda_cont ++;
    }

    if((JL_WAKEUP->CON3 & BIT(IIC_SCL_EVENT)))
    {
        CLEAR_SCL_PEND();
        scl_isr_deal();
        scl_cont ++;
    }
}
IRQ_REGISTER(IRQ_PORT_IDX,port_interrupt_isr);
void init_slave_iic(void (*received_cb_fun)(u8 *buffer,u8 len),u8 *send_buffer)
{
    INIT_SDA_DW_EDGE();                                          //下降沿触发 用于检测起始信号
    CLEAR_SDA_PEND();
    CLEAR_SCL_PEND();
    sda_init_in();
    clk_init_in();
    iic_state = IDLE_STATE;
    received_data_deal = received_cb_fun;
    send_buffer = send_data_buffer;
    IRQ_REQUEST(IRQ_PORT_IDX, port_interrupt_isr, 3);
}

static inline void scl_isr_deal(void)
{
    switch(iic_state)
    {
        case RECEIVE_STATE:                         ///接收数据
            if(read_sda_io())
            {
                receive_data  |=(0x80 >> transport_bits);
            }
            else
            {
                receive_data  &=~(0x80 >> transport_bits);
            }

            if(++transport_bits >= 8)
            {                                                       //收完一个字节
                transport_bits = 0;
                if(received_chip_id == 0)
                {                                                   //还没有检测ID  则本次接收的是ID
                    received_chip_id = receive_data;
                    if((receive_data != IIC_CHIP_ID)&&(receive_data != (IIC_CHIP_ID+1)))
                    {                                               //判断ID 读写ID 错误
                        iic_printf("\n^iic_addr_err!!! received_chip_id = 0x%x\n",received_chip_id);
                        iic_state = NACK_STATE;
                        INIT_CLK_DW_EDGE();     ///开始检测第八个时钟的下降沿
                        break;
                    }
                    if(received_chip_id & 0x01)
                    {                                                //主机要读数据
                        need_send_data = 1;
                    }
                }
                receive_buffer[received_count++]  = receive_data;
                receive_data = 0;
                iic_state = ACK_STATE;
                INIT_CLK_DW_EDGE();             ///开始检测第八个时钟的下降沿
            }
            break;
        case ACK_STATE:                         ///第八个时钟下降沿 应答开始
                iic_ack();
                if(need_send_data)
                {
                     iic_state = SACK_END;                      //变为发送状态
                }
                else
                {
                     iic_state = ACK_END;
                }
            break;
        case ACK_END:                            ///第九个时钟下降沿 应答结束
                    iic_state = RECEIVE_STATE;
                    sda_init_in();
                    INIT_CLK_UP_EDGE();                         //检测上升沿
            break;
        case NACK_STATE:                        ///第八个时钟下降沿 无应答开始
                iic_Nack();
                iic_state = NACK_END;
            break;
        case NACK_END:                          ///第八个时钟下降沿 无应答结束
                iic_state = IDLE_STATE;
                sda_init_in();
                INIT_CLK_UP_EDGE();
                INIT_SDA_DW_EDGE();
            break;
        case SACK_END:                          ///第九个时钟下降沿 应答结束
                    iic_state = SEND_STATE;                    //变为发送状态
                    sda_init_out();
            //break;                                            不用break 直接在前一个时钟的下降沿放入数据
        case SEND_STATE:
send_data_deal:
                 transport_bits++;
                if(transport_bits <= 8)
                {
                    if(send_data_buffer[send_data_index] & (0x80>>(transport_bits-1)))
                            sda_out_H();
                    else
                            sda_out_L();
                }
                else
                {                               ///第八个时钟的下降沿 数据已全部发送
                    if(++send_data_index >= MAX_SEND_BUFF_SIZE)
                         send_data_index = 0;
                     transport_bits = 0;
                     sda_init_in();             ///释放 SDA
                     iic_state = RARK_STATE;
                }
                break;
        case RARK_STATE:                         ///第九个发送时钟的下降沿
                if(read_sda_io())
                {                                ///收到主机的 NARK 代表数据已经结束 从机应释放数据线以便主机发送停止信号
                   ; // CLOSE_SCL_EDGE();
                }
                else
                {                               ///收到主机的 ARK
                    iic_state = SEND_STATE;
                    sda_init_out();
                    transport_bits = 0;
                    goto send_data_deal;
                }
            break;
        default:
            break;
    }
}
u16 clk_value;
static inline void sda_isr_deal(void)
{
    clk_value = read_scl_io();
    if(clk_value)
    {
        if(iic_state == IDLE_STATE)
        {///起始信号
            iic_puts("S\n");
            send_data_index = 0;
            receive_data = 0;
            transport_bits = 0;
            received_chip_id = 0;
            received_count = 0;
            need_send_data = 0;
            iic_state = RECEIVE_STATE;
            INIT_CLK_UP_EDGE();                         //检测时钟信号
            INIT_SDA_UP_EDGE();                         //检测停止信号
        }
        else
        {///结束信号
           iic_puts("P\n");
             if(received_data_deal)
                received_data_deal(receive_buffer,received_count);
            received_count = 0;
            iic_state = IDLE_STATE;
            INIT_CLK_UP_EDGE();
            INIT_SDA_DW_EDGE();
        }
    }
}








