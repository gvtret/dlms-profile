#include "dlms/profile/profile_c_api.h"

#include "dlms/transport/fake_transport.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace {

struct CallbackByteStream
{
  bool open;
  std::vector<std::vector<std::uint8_t> > writes;
};

struct CallbackDatagram
{
  bool open;
  std::vector<std::vector<std::uint8_t> > sends;
};

dlms_profile_status_t CallbackByteStreamOpen(void* userData)
{
  static_cast<CallbackByteStream*>(userData)->open = true;
  return DLMS_PROFILE_STATUS_OK;
}

dlms_profile_status_t CallbackByteStreamClose(void* userData)
{
  static_cast<CallbackByteStream*>(userData)->open = false;
  return DLMS_PROFILE_STATUS_OK;
}

int CallbackByteStreamIsOpen(const void* userData)
{
  return static_cast<const CallbackByteStream*>(userData)->open ? 1 : 0;
}

dlms_profile_status_t CallbackByteStreamReadSome(
  void*,
  std::uint8_t*,
  std::size_t,
  std::size_t* bytesRead)
{
  if (bytesRead != nullptr) {
    *bytesRead = 0u;
  }
  return DLMS_PROFILE_STATUS_WOULD_BLOCK;
}

dlms_profile_status_t CallbackByteStreamWriteAll(
  void* userData,
  const std::uint8_t* input,
  std::size_t inputSize)
{
  CallbackByteStream* stream = static_cast<CallbackByteStream*>(userData);
  stream->writes.push_back(std::vector<std::uint8_t>(input,
                                                     input + inputSize));
  return DLMS_PROFILE_STATUS_OK;
}

dlms_profile_byte_stream_callbacks_t MakeByteStreamCallbacks(
  CallbackByteStream* stream)
{
  dlms_profile_byte_stream_callbacks_t callbacks;
  callbacks.user_data = stream;
  callbacks.open = &CallbackByteStreamOpen;
  callbacks.close = &CallbackByteStreamClose;
  callbacks.is_open = &CallbackByteStreamIsOpen;
  callbacks.read_some = &CallbackByteStreamReadSome;
  callbacks.write_all = &CallbackByteStreamWriteAll;
  return callbacks;
}

dlms_profile_status_t CallbackDatagramOpen(void* userData)
{
  static_cast<CallbackDatagram*>(userData)->open = true;
  return DLMS_PROFILE_STATUS_OK;
}

dlms_profile_status_t CallbackDatagramClose(void* userData)
{
  static_cast<CallbackDatagram*>(userData)->open = false;
  return DLMS_PROFILE_STATUS_OK;
}

int CallbackDatagramIsOpen(const void* userData)
{
  return static_cast<const CallbackDatagram*>(userData)->open ? 1 : 0;
}

dlms_profile_status_t CallbackDatagramSend(
  void* userData,
  const std::uint8_t* input,
  std::size_t inputSize)
{
  CallbackDatagram* datagram = static_cast<CallbackDatagram*>(userData);
  datagram->sends.push_back(std::vector<std::uint8_t>(input,
                                                      input + inputSize));
  return DLMS_PROFILE_STATUS_OK;
}

dlms_profile_status_t CallbackDatagramReceive(
  void*,
  std::uint8_t*,
  std::size_t,
  std::size_t* bytesRead)
{
  if (bytesRead != nullptr) {
    *bytesRead = 0u;
  }
  return DLMS_PROFILE_STATUS_WOULD_BLOCK;
}

dlms_profile_datagram_callbacks_t MakeDatagramCallbacks(
  CallbackDatagram* datagram)
{
  dlms_profile_datagram_callbacks_t callbacks;
  callbacks.user_data = datagram;
  callbacks.open = &CallbackDatagramOpen;
  callbacks.close = &CallbackDatagramClose;
  callbacks.is_open = &CallbackDatagramIsOpen;
  callbacks.send = &CallbackDatagramSend;
  callbacks.receive = &CallbackDatagramReceive;
  return callbacks;
}

} // namespace

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

TEST(ProfileCApi, WrapperTcpCallbackChannelSendApdu)
{
  CallbackByteStream stream;
  stream.open = false;

  dlms_profile_channel_options_t options;
  dlms_profile_default_channel_options(&options);
  dlms_profile_byte_stream_callbacks_t callbacks =
    MakeByteStreamCallbacks(&stream);

  dlms_profile_channel_t* channel =
    dlms_profile_create_wrapper_tcp_channel_from_callbacks(&callbacks,
                                                           &options);
  ASSERT_NE(nullptr, channel);

  const std::uint8_t apdu[] = {0xc0, 0x01};
  EXPECT_EQ(DLMS_PROFILE_STATUS_OK, dlms_profile_open(channel));
  EXPECT_EQ(DLMS_PROFILE_STATUS_OK,
            dlms_profile_send_apdu(channel, apdu, sizeof(apdu)));
  EXPECT_EQ(1, dlms_profile_is_open(channel));
  EXPECT_EQ(1u, stream.writes.size());

  dlms_profile_destroy_channel(channel);
}

TEST(ProfileCApi, WrapperUdpCallbackChannelSendApdu)
{
  CallbackDatagram datagram;
  datagram.open = false;

  dlms_profile_channel_options_t options;
  dlms_profile_default_channel_options(&options);
  dlms_profile_datagram_callbacks_t callbacks =
    MakeDatagramCallbacks(&datagram);

  dlms_profile_channel_t* channel =
    dlms_profile_create_wrapper_udp_channel_from_callbacks(&callbacks,
                                                           &options);
  ASSERT_NE(nullptr, channel);

  const std::uint8_t apdu[] = {0xc0, 0x01};
  EXPECT_EQ(DLMS_PROFILE_STATUS_OK, dlms_profile_open(channel));
  EXPECT_EQ(DLMS_PROFILE_STATUS_OK,
            dlms_profile_send_apdu(channel, apdu, sizeof(apdu)));
  EXPECT_EQ(1, dlms_profile_is_open(channel));
  ASSERT_EQ(1u, datagram.sends.size());
  EXPECT_GT(datagram.sends[0].size(), sizeof(apdu));

  dlms_profile_destroy_channel(channel);
}

TEST(ProfileCApi, CallbackChannelsValidateRequiredCallbacks)
{
  dlms_profile_byte_stream_callbacks_t callbacks;
  callbacks.user_data = nullptr;
  callbacks.open = nullptr;
  callbacks.close = nullptr;
  callbacks.is_open = nullptr;
  callbacks.read_some = nullptr;
  callbacks.write_all = nullptr;

  EXPECT_EQ(nullptr,
            dlms_profile_create_wrapper_tcp_channel_from_callbacks(
              &callbacks,
              nullptr));
  EXPECT_EQ(nullptr,
            dlms_profile_create_hdlc_channel_from_callbacks(&callbacks,
                                                            nullptr));
  EXPECT_EQ(nullptr,
            dlms_profile_create_wrapper_udp_channel_from_callbacks(nullptr,
                                                                   nullptr));
}

TEST(ProfileCApi, RejectsNullChannel)
{
  std::size_t written = 1u;
  EXPECT_EQ(DLMS_PROFILE_STATUS_INVALID_ARGUMENT,
            dlms_profile_receive_apdu(nullptr, nullptr, 0u, &written));
}

TEST(ProfileCApi, DefaultOptionsExposeHdlcSessionFields)
{
  dlms_profile_channel_options_t options;
  dlms_profile_default_channel_options(&options);

  EXPECT_EQ(2u, DLMS_PROFILE_C_API_VERSION);
  EXPECT_EQ(sizeof(options), DLMS_PROFILE_CHANNEL_OPTIONS_SIZE);
  EXPECT_EQ(DLMS_PROFILE_HDLC_ROLE_CLIENT, options.hdlc_role);
  EXPECT_EQ(0, options.hdlc_use_session);
  EXPECT_EQ(3u, options.hdlc_retry_count);
}

TEST(ProfileCApi, SessionLifecycleRequiresHdlcChannel)
{
  dlms::transport::FakeByteStream stream;
  dlms_profile_channel_options_t options;
  dlms_profile_default_channel_options(&options);

  dlms_profile_channel_t* channel =
    dlms_profile_create_wrapper_tcp_channel(&stream, &options);
  ASSERT_NE(nullptr, channel);

  EXPECT_EQ(DLMS_PROFILE_STATUS_UNSUPPORTED_FEATURE,
            dlms_profile_connect_data_link(channel));
  EXPECT_EQ(DLMS_PROFILE_STATUS_UNSUPPORTED_FEATURE,
            dlms_profile_accept_data_link(channel));
  EXPECT_EQ(DLMS_PROFILE_STATUS_UNSUPPORTED_FEATURE,
            dlms_profile_disconnect_data_link(channel));

  dlms_profile_destroy_channel(channel);
}
