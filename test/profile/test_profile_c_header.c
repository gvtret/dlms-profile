#include "dlms/profile/profile_c_api.h"

int dlms_profile_c_header_compiles(void)
{
  dlms_profile_channel_options_t options;
  dlms_profile_byte_stream_callbacks_t byte_stream_callbacks;
  dlms_profile_datagram_callbacks_t datagram_callbacks;

  dlms_profile_default_channel_options(&options);
  byte_stream_callbacks.user_data = 0;
  byte_stream_callbacks.open = 0;
  byte_stream_callbacks.close = 0;
  byte_stream_callbacks.is_open = 0;
  byte_stream_callbacks.read_some = 0;
  byte_stream_callbacks.write_all = 0;
  datagram_callbacks.user_data = 0;
  datagram_callbacks.open = 0;
  datagram_callbacks.close = 0;
  datagram_callbacks.is_open = 0;
  datagram_callbacks.send = 0;
  datagram_callbacks.receive = 0;

  return (int)(DLMS_PROFILE_C_API_VERSION +
               DLMS_PROFILE_CHANNEL_OPTIONS_SIZE +
               options.hdlc_client_address +
               (byte_stream_callbacks.open == 0 ? 1u : 0u) +
               (datagram_callbacks.send == 0 ? 1u : 0u));
}
