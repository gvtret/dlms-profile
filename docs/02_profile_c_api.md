# Profile C API

The C ABI exposes opaque channel handles. Handles do not own the lower transport
object; callers must keep the transport alive until the handle is destroyed.

## Core Calls

```c
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
dlms_profile_status_t dlms_profile_send_apdu(...);
dlms_profile_status_t dlms_profile_receive_apdu(...);
```

## Rules

- Status values mirror the C++ `ProfileStatus` contract.
- Receive uses caller-provided storage and reports `OutputBufferTooSmall`.
- Null pointers are rejected except for destroy.
- The C ABI does not expose C++ exceptions, STL types, or ownership of lower
  transports.

