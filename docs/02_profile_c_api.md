# Profile C API

The C ABI exposes opaque channel handles. Handles do not own the lower transport
object; callers must keep the transport alive until the handle is destroyed.

## Core Calls

```c
#define DLMS_PROFILE_C_API_VERSION 2u
#define DLMS_PROFILE_CHANNEL_OPTIONS_SIZE \
  (sizeof(dlms_profile_channel_options_t))

dlms_profile_channel_t* dlms_profile_create_wrapper_tcp_channel(
  void* byte_stream,
  const dlms_profile_channel_options_t* options);

dlms_profile_channel_t* dlms_profile_create_wrapper_udp_channel(
  void* datagram_transport,
  const dlms_profile_channel_options_t* options);

dlms_profile_channel_t* dlms_profile_create_hdlc_channel(
  void* byte_stream,
  const dlms_profile_channel_options_t* options);

dlms_profile_channel_t* dlms_profile_create_wrapper_tcp_channel_from_callbacks(
  const dlms_profile_byte_stream_callbacks_t* callbacks,
  const dlms_profile_channel_options_t* options);

dlms_profile_channel_t* dlms_profile_create_wrapper_udp_channel_from_callbacks(
  const dlms_profile_datagram_callbacks_t* callbacks,
  const dlms_profile_channel_options_t* options);

dlms_profile_channel_t* dlms_profile_create_hdlc_channel_from_callbacks(
  const dlms_profile_byte_stream_callbacks_t* callbacks,
  const dlms_profile_channel_options_t* options);

void dlms_profile_destroy_channel(dlms_profile_channel_t* channel);
dlms_profile_status_t dlms_profile_open(dlms_profile_channel_t* channel);
dlms_profile_status_t dlms_profile_close(dlms_profile_channel_t* channel);
dlms_profile_status_t dlms_profile_connect_data_link(
  dlms_profile_channel_t* channel);
dlms_profile_status_t dlms_profile_accept_data_link(
  dlms_profile_channel_t* channel);
dlms_profile_status_t dlms_profile_disconnect_data_link(
  dlms_profile_channel_t* channel);
dlms_profile_status_t dlms_profile_send_apdu(...);
dlms_profile_status_t dlms_profile_receive_apdu(...);
```

## Rules

- Status values mirror the C++ `ProfileStatus` contract.
- `DLMS_PROFILE_C_API_VERSION` and `DLMS_PROFILE_CHANNEL_OPTIONS_SIZE` let
  callers assert the header contract they were compiled against.
- Existing `void*` constructors adapt already-created C++ lower-layer
  interfaces. Pure C callers can use the callback constructors instead.
- Receive uses caller-provided storage and reports `OutputBufferTooSmall`.
- Null pointers are rejected except for destroy.
- HDLC session lifecycle calls return `UnsupportedFeature` for non-HDLC
  channels.
- The C ABI does not expose C++ exceptions, STL types, or ownership of lower
  transports.

## Example

```c
dlms_profile_channel_options_t options;
dlms_profile_default_channel_options(&options);
options.hdlc_use_session = 1;
options.hdlc_role = DLMS_PROFILE_HDLC_ROLE_CLIENT;
options.hdlc_retry_count = 3;

dlms_profile_channel_t* channel =
  dlms_profile_create_hdlc_channel(byte_stream, &options);

dlms_profile_open(channel);
dlms_profile_connect_data_link(channel);
dlms_profile_send_apdu(channel, apdu, apdu_size);

uint8_t output[1024];
size_t written = 0;
dlms_profile_receive_apdu(channel, output, sizeof(output), &written);

dlms_profile_close(channel);
dlms_profile_destroy_channel(channel);
```

## Callback Transports

Callback constructors let C callers provide byte-stream or datagram operations
without depending on C++ transport interface objects. The profile channel owns
the adapter, but not the callback `user_data`.

```c
dlms_profile_byte_stream_callbacks_t callbacks;
callbacks.user_data = stream_state;
callbacks.open = stream_open;
callbacks.close = stream_close;
callbacks.is_open = stream_is_open;
callbacks.read_some = stream_read_some;
callbacks.write_all = stream_write_all;

dlms_profile_channel_t* channel =
  dlms_profile_create_wrapper_tcp_channel_from_callbacks(&callbacks, &options);
```
