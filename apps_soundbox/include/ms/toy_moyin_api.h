#ifndef __TOY_MOYIN_API_H__
#define __TOY_MOYIN_API_H__

#define  RET_OK   0
#define  RET_ERR  1


#define SAMPLE_UP    32000
#define SAMPLE_DOWN  8000

#define SPEED_UP    320
#define SPEED_DOWN  80

/*魔音的IO接口*/
typedef struct _ps_audio_IO_ {
    char *outpriv;
    void (*output)(char *ptr, short *data, unsigned short len);
} ps_audio_IO;


/*参数设置说明： speed_coff*pitch_coff 两者的乘积 为最后的速度，对应的不变速不变调的值为160*16000*/
/* pitch_coff 调整 这个值的时候， 最好保证低位还是0，大步长调整，例如按 16000,18000,2000,14000,12000.. 这样来取;降低运算噪音
   speed_coff 这个 值可以微调。 如果在 速度确定的时候，最好，先选好pitch_coff，然后再用 final_speed/pitch_coff 来得到 speed_coff，这个值微调不会带来噪音*/

/*魔音参数*/
typedef struct _EF_PS_PARM_ {
    short speed_coff;                         // 范围                         SPEED_DOWN 至 SPEED_UP
    unsigned short pitch_coff;                //外部给的变速变调参数           SAMPLE_DOWN 至 SAMPLE_UP
    char pitch_on;                            //是否要变调                    为1的时候，变速变调。。 为0的时候，只变速不变调
    char speed_on;
    char robort_on;
} EF_PS_PARM;



/*open 跟 run 都是 成功 返回 RET_OK，错误返回 RET_ERR*/
/*魔音结构体*/
typedef struct __moyin_toy_ops_ {
    unsigned int (*need_buf)();                               //魔音需要的空间,由外面申请了传入
    unsigned int (*open)(char *ptr, EF_PS_PARM *ptr_parm, ps_audio_IO *ps_audio_obj);      //前面的ptr为内部使用空间，中间的为魔音参数，最后一个是输出接口，由外面实现，给魔音调用
    unsigned int (*run)(char *ptr, short *inbuf, void *coeff_ptr);
} moyin_toy_ops;

extern moyin_toy_ops  *get_moyin_obj();

moyin_toy_ops  *test_moyin_ops;
#endif
