#include "dlms/profile/profile_c_api.h"

#include "dlms/transport/fake_transport.hpp"

#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

TEST(ProfileCApi, WrapperTcpSendApdu)
{
  dlms::transport::FakeByteStream stream;
  dlms_profile_channel_options_t options;
  dlms_profile_default_channel_options(&options);

  dlms_profile_channel_t* channel =
    dlms_profile_create_wrapper_tcp_channel(&stream, &options);
  ASSERT_NE(nullptr, channel);

  const std::uint8_t apdu[] = {0xc0, 0x01};
  EXPECT_EQ(DLMS_PROFILE_STATUS_OK, dlms_profile_open(channel));
  EXPECT_EQ(DLMS_PROFILE_STATUS_OK,
            dlms_profile_send_apdu(channel, apdu, sizeof(apdu)));
  EXPECT_EQ(1, dlms_profile_is_open(channel));
  EXPECT_EQ(1u, stream.Writes().size());

  dlms_profile_destroy_channel(channel);
}

TEST(ProfileCApi, RejectsNullChannel)
{
  std::size_t written = 1u;
  EXPECT_EQ(DLMS_PROFILE_STATUS_INVALID_ARGUMENT,
            dlms_profile_receive_apdu(nullptr, nullptr, 0u, &written));
}

