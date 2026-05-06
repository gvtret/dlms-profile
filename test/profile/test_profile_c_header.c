#include "dlms/profile/profile_c_api.h"

int dlms_profile_c_header_compiles(void)
{
  dlms_profile_channel_options_t options;
  dlms_profile_default_channel_options(&options);
  return (int)options.hdlc_client_address;
}

