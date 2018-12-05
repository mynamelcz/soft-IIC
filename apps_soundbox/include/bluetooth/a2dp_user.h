#ifndef __A2DP_USER_H__
#define __A2DP_USER_H__
#include "typedef.h"
#include "dec/decoder_phy.h"

void a2dp_decode_before_callback(DEC_API_IO *dec_io, void *priv);
void a2dp_decode_after_callback(DEC_API_IO *dec_io, void *priv);

#endif// __A2DP_USER_H__
