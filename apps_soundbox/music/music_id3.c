#include "music_id3.h"
#include "fat/tff.h"

#define ID3_DEBUG_ENABLE
#ifdef ID3_DEBUG_ENABLE
#define id3_printf printf
#define id3_printf_buf printf_buf
#else
#define id3_printf(...)
#define id3_printf_buf(...)
#endif

struct __MP3_ID3_OBJ {
    u8 *id3_buf;
    u32 id3_len;
};

#define ID3_SIZEOF_ALIN(var,al)     ((((var)+(al)-1)/(al))*(al))
#define ID3_V1_HEADER_SIZE			(128)
#define ID3_V2_HEADER_SIZE			(10)
static tbool id3_v1_match(u8 *buf)
{
    if (buf[0] == 'T' && buf[1] == 'A' && buf[2] == 'G') {
        return true;
    } else {
        return false;
    }
}

static tbool id3_v2_match(u8 *buf)
{
    //printf_buf(buf, 16);
    if ((buf[0] == 'I')
        && (buf[1] == 'D')
        && (buf[2] == '3')
        && (buf[3] != 0xff)
        && (buf[4] != 0xff)
        && ((buf[6] & 0x80) == 0)
        && ((buf[7] & 0x80) == 0)
        && ((buf[8] & 0x80) == 0)
        && ((buf[9] & 0x80) == 0)) {
        return true;
    } else {
        return false;
    }
}

static u32 id3v2_get_tag_len(u8 *buf)
{
    u32 len = (((u32)buf[6] & 0x7f) << 21) +
              (((u32)buf[7] & 0x7f) << 14) +
              (((u32)buf[8] & 0x7f) << 7) +
              (buf[9] & 0x7f) +
              ID3_V2_HEADER_SIZE;

    if (buf[5] & 0x10) {
        len += ID3_V2_HEADER_SIZE;
    }

    dec_phy_printf("----id3v2_len%d---\n", len);
    return len;
}


void id3_obj_post(MP3_ID3_OBJ **obj)
{
    if (obj == NULL || (*obj) == NULL) {
        return ;
    }
    free(*obj);
    *obj = NULL;
}

MP3_ID3_OBJ *id3_v1_obj_get(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return NULL;
    }

    DEC_API_IO *_io = mapi->dop_api->io;
    if (_io->f_seek == NULL
        || _io->f_read == NULL
        || _io->f_length == NULL) {
        return NULL;
    }

    u8 *need_buf;
    u32 need_buf_size = ID3_SIZEOF_ALIN(sizeof(MP3_ID3_OBJ), 4)
                        + ID3_SIZEOF_ALIN(ID3_V1_HEADER_SIZE, 4);

    need_buf = (u8 *)calloc(1, need_buf_size);
    if (need_buf == NULL) {
        return NULL;
    }

    MP3_ID3_OBJ *obj = (MP3_ID3_OBJ *)need_buf;
    obj->id3_buf = (u8 *)(need_buf + ID3_SIZEOF_ALIN(sizeof(MP3_ID3_OBJ), 4));
    obj->id3_len = ID3_V1_HEADER_SIZE;

    u32 file_len = _io->f_length(_io->f_p);
    _io->f_seek(_io->f_p, SEEK_SET, (file_len - ID3_V1_HEADER_SIZE));
    _io->f_read(_io->f_p, obj->id3_buf, ID3_V1_HEADER_SIZE);

    if (id3_v1_match(obj->id3_buf) == true) {
        id3_printf("find id3 v1 !!\n");
        //user analyze
        id3_printf_buf(obj->id3_buf, obj->id3_len);
        return obj;
    } else {
        free(need_buf);
        return NULL;
    }
}


MP3_ID3_OBJ *id3_v2_obj_get(MUSIC_PLAYER_OBJ *mapi)
{
    if (mapi == NULL) {
        return NULL;
    }

    DEC_API_IO *_io = mapi->dop_api->io;
    if (_io->f_seek == NULL
        || _io->f_read == NULL) {
        return NULL;
    }

    u8 tmp_buf[ID3_V2_HEADER_SIZE];

    _io->f_seek(_io->f_p, SEEK_SET, 0);
    _io->f_read(_io->f_p, tmp_buf, ID3_V2_HEADER_SIZE);

    if (id3_v2_match(tmp_buf) == true) {
        id3_printf("find id3 v2 !!\n");
        u32 tag_len = id3v2_get_tag_len(tmp_buf);

        if (tag_len > (3 * 1024)) {
            id3_printf("id3 v2 info is too big!!\n", tag_len);
            return NULL;
        }

        u8 *need_buf;
        u32 need_buf_size = ID3_SIZEOF_ALIN(sizeof(MP3_ID3_OBJ), 4)
                            + ID3_SIZEOF_ALIN(tag_len, 4);

        need_buf = (u8 *)calloc(1, need_buf_size);
        if (need_buf == NULL) {
            id3_printf("no space for id3 v2  %d\n", tag_len);
            return NULL;
        }

        MP3_ID3_OBJ *obj = (MP3_ID3_OBJ *)need_buf;
        obj->id3_buf = (u8 *)(need_buf + ID3_SIZEOF_ALIN(sizeof(MP3_ID3_OBJ), 4));
        obj->id3_len = tag_len;

        _io->f_seek(_io->f_p, SEEK_SET, 0);
        _io->f_read(_io->f_p, obj->id3_buf, obj->id3_len);

        id3_printf("id3_v2 size = %d\n", obj->id3_len);
        id3_printf_buf(obj->id3_buf, obj->id3_len);

        return obj;
    } else {
        return NULL;
    }
}

