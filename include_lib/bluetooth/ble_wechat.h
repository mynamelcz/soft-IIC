#ifndef __BLE_WECHAT_H__
#define __BLE_WECHAT_H__


#include "typedef.h"



///***注意：该文件的枚举与库编译密切相关，主要是给用户提供调用所用。用户不能自己在中间添加值。*/

typedef struct {
    unsigned char bMagicNumber;
    unsigned char bVer;
    unsigned short nLength;
    unsigned short nCmdId;
    unsigned short nSeq;
} __attribute__((packed)) Protobuf_Head;


typedef enum {
    sendTextReq = 0x01,
} BleDemo2CmdID;

typedef enum {
    cmdid_none = 0,
    cmdid_req_auth = 10001,
    cmdid_req_sendData = 10002,
    cmdid_req_init = 10003,
    cmdid_resp_auth = 20001,
    cmdid_resp_sendData = 20002,
    cmdid_resp_init = 20003,
    cmdid_push_recvData = 30001,
    cmdid_push_switchView = 30002,
    cmdid_push_switchBackgroud = 30003,
    cmdid_err_decode = 29999
} EmCmdId;


typedef struct {
    void *none;
} BaseRequest;

typedef enum {
    EAM_md5 = 1,
    EAM_macNoEncrypt = 2
} EmAuthMethod;

typedef struct {
    uint8_t *data;
    int len;
} Mtu_type;

typedef struct {
    char *str;
    int len;
} String;

typedef struct {
    BaseRequest *base_request;
    bool has_md5_device_type_and_device_id;
    Mtu_type md5_device_type_and_device_id;
    int32_t proto_version;
    int32_t auth_proto;
    EmAuthMethod auth_method;
    bool has_aes_sign;
    Mtu_type aes_sign;
    bool has_mac_address;
    Mtu_type mac_address;
    bool has_time_zone;
    String time_zone;
    bool has_language;
    String language;
    bool has_device_name;
    String device_name;
} AuthRequest;

typedef struct {
    const uint8_t *unpack_buf;
    uint8_t *pack_buf;
    int buf_len;
    int buf_offset;
} Epb;


typedef struct {
    BaseRequest *base_request;
    bool has_resp_field_filter;
    Mtu_type resp_field_filter;
    bool has_challenge;
    Mtu_type challenge;
} InitRequest;


typedef struct {
    const uint8_t *data;
    int len;
} Cmtu;

typedef enum {
    EDDT_manufatureSvr = 0,
    EDDT_wxWristBand = 1,
    EDDT_wxDeviceHtmlChatView = 10001
} EmDeviceDataType;

typedef struct {
    void *none;
} BasePush;

typedef struct {
    BasePush *base_push;
    Cmtu data;
    bool has_type;
    EmDeviceDataType type;
} RecvDataPush;

typedef struct {
    uint8_t m_magicCode[2];
    uint16_t m_version;
    uint16_t m_totalLength;
    uint16_t m_cmdid;
    uint16_t m_seq;
    uint16_t m_errorCode;
} __attribute__((packed)) BlueDemoHead;

typedef struct {
    BaseRequest *base_request;
    Mtu_type data;
    bool has_type;
    EmDeviceDataType type;
} SendDataRequest;

typedef struct {
    int32_t err_code;
    bool has_err_msg;
    String err_msg;
} BaseResponse;

typedef struct {
    BaseResponse *base_response;
    bool has_data;
    Cmtu data;
} SendDataResponse;



typedef enum {
    ESVO_enter = 1,
    ESVO_exit = 2
} EmSwitchViewOp;

typedef enum {
    EVI_deviceChatView = 1,
    EVI_deviceChatHtmlView = 2
} EmViewId;

typedef struct {
    BasePush *base_push;
    EmSwitchViewOp switch_view_op;
    EmViewId view_id;
} SwitchViewPush;


typedef enum {
    ESBO_enterBackground = 1,
    ESBO_enterForground = 2,
    ESBO_sleep = 3
} EmSwitchBackgroundOp;

typedef struct {
    BasePush *base_push;
    EmSwitchBackgroundOp switch_background_op;
} SwitchBackgroudPush;

typedef enum {
    EIS_deviceChat = 1,
    EIS_autoSync = 2
} EmInitScence;

typedef enum {
    EPT_ios = 1,
    EPT_andriod = 2,
    EPT_wp = 3,
    EPT_s60v3 = 4,
    EPT_s60v5 = 5,
    EPT_s40 = 6,
    EPT_bb = 7
} EmPlatformType;

typedef struct {
    BaseResponse *base_response;
    uint32_t user_id_high;
    uint32_t user_id_low;
    bool has_challeange_answer;
    uint32_t challeange_answer;
    bool has_init_scence;
    EmInitScence init_scence;
    bool has_auto_sync_max_duration_second;
    uint32_t auto_sync_max_duration_second;
    bool has_user_nick_name;
    String user_nick_name;
    bool has_platform_type;
    EmPlatformType platform_type;
    bool has_model;
    String model;
    bool has_os;
    String os;
    bool has_time;
    int32_t time;
    bool has_time_zone;
    int32_t time_zone;
    bool has_time_string;
    String time_string;
} InitResponse;

#define CMD_AUTH 1
#define CMD_INIT 2
#define CMD_SENDDAT 3

#define EAM_macNoEncrypt 2

#define CONTINUOUS_MASK 		0x80
#define WIRETYPE_MASK			0x07

#define SEND_TEXT_WECHAT "Hello,WeChat!"

typedef enum {
    WIRETYPE_VARINT = 0,
    WIRETYPE_FIXED64 = 1,
    WIRETYPE_LENGTH_DELIMITED = 2,
    WIRETYPE_FIXED32 = 5
} WireType;

#define MPBLEDEMO2_MAGICCODE_H 0xfe
#define MPBLEDEMO2_MAGICCODE_L 0xcf
#define MPBLEDEMO2_VERSION 0x01
#define PROTO_VERSION 0x010004
#define AUTH_PROTO 1
#define AUTH_METHOD EAM_macNoEncrypt
#define MD5_TYPE_AND_ID_LENGTH 0
#define CIPHER_TEXT_LENGTH 0
#define CHALLENAGE_LENGTH 4


#define TAG_BaseResponse_ErrCode												0x08
#define TAG_BaseResponse_ErrMsg													0x12

#define TAG_AuthRequest_BaseRequest												0x0a
#define TAG_AuthRequest_Md5DeviceTypeAndDeviceId								0x12
#define TAG_AuthRequest_ProtoVersion											0x18
#define TAG_AuthRequest_AuthProto												0x20
#define TAG_AuthRequest_AuthMethod												0x28
#define TAG_AuthRequest_AesSign													0x32
#define TAG_AuthRequest_MacAddress												0x3a
#define TAG_AuthRequest_TimeZone												0x52
#define TAG_AuthRequest_Language												0x5a
#define TAG_AuthRequest_DeviceName												0x62

#define TAG_AuthResponse_BaseResponse											0x0a
#define TAG_AuthResponse_AesSessionKey											0x12

#define TAG_InitRequest_BaseRequest												0x0a
#define TAG_InitRequest_RespFieldFilter											0x12
#define TAG_InitRequest_Challenge												0x1a

#define TAG_InitResponse_BaseResponse											0x0a
#define TAG_InitResponse_UserIdHigh												0x10
#define TAG_InitResponse_UserIdLow												0x18
#define TAG_InitResponse_ChalleangeAnswer										0x20
#define TAG_InitResponse_InitScence												0x28
#define TAG_InitResponse_AutoSyncMaxDurationSecond								0x30
#define TAG_InitResponse_UserNickName											0x5a
#define TAG_InitResponse_PlatformType											0x60
#define TAG_InitResponse_Model													0x6a
#define TAG_InitResponse_Os														0x72
#define TAG_InitResponse_Time													0x78
#define TAG_InitResponse_TimeZone												0x8001
#define TAG_InitResponse_TimeString												0x8a01



#define TAG_SendDataRequest_BaseRequest											0x0a
#define TAG_SendDataRequest_Data												0x12
#define TAG_SendDataRequest_Type												0x18

#define TAG_SendDataResponse_BaseResponse										0x0a
#define TAG_SendDataResponse_Data												0x12

#define TAG_RecvDataPush_BasePush												0x0a
#define TAG_RecvDataPush_Data													0x12
#define TAG_RecvDataPush_Type													0x18

#define TAG_SwitchViewPush_BasePush												0x0a
#define TAG_SwitchViewPush_SwitchViewOp											0x10
#define TAG_SwitchViewPush_ViewId												0x18

#define TAG_SwitchBackgroudPush_BasePush										0x0a
#define TAG_SwitchBackgroudPush_SwitchBackgroundOp								0x10

extern int epb_auth_request_pack_size(AuthRequest *request);
extern int epb_pack_auth_request(AuthRequest *request, uint8_t *buf, int buf_len);
extern int epb_init_request_pack_size(InitRequest *request);
extern int epb_pack_init_request(InitRequest *request, uint8_t *buf, int buf_len);
extern int epb_send_data_request_pack_size(SendDataRequest *request);
extern int epb_pack_send_data_request(SendDataRequest *request, uint8_t *buf, int buf_len);
extern SendDataResponse *epb_unpack_send_data_response(const uint8_t *buf, int buf_len);
extern SwitchViewPush *epb_unpack_switch_view_push(const uint8_t *buf, int buf_len);
extern SwitchBackgroudPush *epb_unpack_switch_backgroud_push(const uint8_t *buf, int buf_len);
extern InitResponse *epb_unpack_init_response(const uint8_t *buf, int buf_len);
#endif
