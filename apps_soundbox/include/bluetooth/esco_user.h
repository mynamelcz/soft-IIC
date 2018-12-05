#ifndef __ESCO_USER_H__
#define __ESCO_USER_H__
#include "typedef.h"
#include "bluetooth/avctp_user.h"

void esco_decode_before_callback(BT_ESCO_DATA_STREAM *stream, void *priv);
void esco_decode_after_callback(BT_ESCO_DATA_STREAM *stream, void *priv);

#endif// __ESCO_USER_H__

