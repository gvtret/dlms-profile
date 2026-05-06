#ifndef DLMS_PROFILE_PROFILE_C_API_H
#define DLMS_PROFILE_PROFILE_C_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum dlms_profile_status_t
{
  DLMS_PROFILE_STATUS_OK = 0,
  DLMS_PROFILE_STATUS_NEED_MORE_DATA = 1,
  DLMS_PROFILE_STATUS_OUTPUT_BUFFER_TOO_SMALL = 2,
  DLMS_PROFILE_STATUS_INVALID_ARGUMENT = 3,
  DLMS_PROFILE_STATUS_NOT_OPEN = 4,
  DLMS_PROFILE_STATUS_ALREADY_OPEN = 5,
  DLMS_PROFILE_STATUS_OPEN_FAILED = 6,
  DLMS_PROFILE_STATUS_READ_FAILED = 7,
  DLMS_PROFILE_STATUS_WRITE_FAILED = 8,
  DLMS_PROFILE_STATUS_TIMEOUT = 9,
  DLMS_PROFILE_STATUS_CONNECTION_CLOSED = 10,
  DLMS_PROFILE_STATUS_WOULD_BLOCK = 11,
  DLMS_PROFILE_STATUS_INVALID_FRAME = 12,
  DLMS_PROFILE_STATUS_INVALID_LENGTH = 13,
  DLMS_PROFILE_STATUS_INVALID_ADDRESS = 14,
  DLMS_PROFILE_STATUS_PAYLOAD_TOO_LARGE = 15,
  DLMS_PROFILE_STATUS_UNSUPPORTED_FEATURE = 16,
  DLMS_PROFILE_STATUS_INTERNAL_ERROR = 17
} dlms_profile_status_t;

typedef enum dlms_profile_hdlc_direction_t
{
  DLMS_PROFILE_HDLC_CLIENT_TO_SERVER = 0,
  DLMS_PROFILE_HDLC_SERVER_TO_CLIENT = 1
} dlms_profile_hdlc_direction_t;

typedef enum dlms_profile_hdlc_role_t
{
  DLMS_PROFILE_HDLC_ROLE_CLIENT = 0,
  DLMS_PROFILE_HDLC_ROLE_SERVER = 1
} dlms_profile_hdlc_role_t;

typedef struct dlms_profile_channel_options_t
{
  uint16_t local_wrapper_port;
  uint16_t remote_wrapper_port;
  uint8_t hdlc_client_address;
  uint16_t hdlc_logical_device_address;
  uint16_t hdlc_physical_device_address;
  dlms_profile_hdlc_direction_t hdlc_direction;
  size_t maximum_apdu_size;
  size_t scratch_buffer_size;
  dlms_profile_hdlc_role_t hdlc_role;
  int hdlc_use_session;
  size_t hdlc_max_information_field_length_transmit;
  size_t hdlc_max_information_field_length_receive;
  uint8_t hdlc_window_size_transmit;
  uint8_t hdlc_window_size_receive;
  uint8_t hdlc_retry_count;
  uint32_t hdlc_retry_delay_milliseconds;
} dlms_profile_channel_options_t;

typedef struct dlms_profile_channel_t dlms_profile_channel_t;

void dlms_profile_default_channel_options(
  dlms_profile_channel_options_t* options);

dlms_profile_channel_t* dlms_profile_create_wrapper_tcp_channel(
  void* byte_stream,
  const dlms_profile_channel_options_t* options);

dlms_profile_channel_t* dlms_profile_create_wrapper_udp_channel(
  void* datagram_transport,
  const dlms_profile_channel_options_t* options);

dlms_profile_channel_t* dlms_profile_create_hdlc_channel(
  void* byte_stream,
  const dlms_profile_channel_options_t* options);

void dlms_profile_destroy_channel(dlms_profile_channel_t* channel);

dlms_profile_status_t dlms_profile_open(dlms_profile_channel_t* channel);
dlms_profile_status_t dlms_profile_close(dlms_profile_channel_t* channel);
int dlms_profile_is_open(const dlms_profile_channel_t* channel);

dlms_profile_status_t dlms_profile_connect_data_link(
  dlms_profile_channel_t* channel);

dlms_profile_status_t dlms_profile_accept_data_link(
  dlms_profile_channel_t* channel);

dlms_profile_status_t dlms_profile_disconnect_data_link(
  dlms_profile_channel_t* channel);

dlms_profile_status_t dlms_profile_send_apdu(
  dlms_profile_channel_t* channel,
  const uint8_t* apdu,
  size_t apdu_size);

dlms_profile_status_t dlms_profile_receive_apdu(
  dlms_profile_channel_t* channel,
  uint8_t* output,
  size_t output_size,
  size_t* written_size);

#ifdef __cplusplus
}
#endif

#endif /* DLMS_PROFILE_PROFILE_C_API_H */
