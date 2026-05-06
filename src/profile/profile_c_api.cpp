#include "dlms/profile/profile_c_api.h"

#include "dlms/profile/hdlc_profile_channel.hpp"
#include "dlms/profile/wrapper_tcp_profile_channel.hpp"
#include "dlms/profile/wrapper_udp_profile_channel.hpp"

struct dlms_profile_channel_t
{
  dlms::profile::IApduChannel* impl;
};

namespace {

dlms_profile_status_t ToCStatus(dlms::profile::ProfileStatus status)
{
  return static_cast<dlms_profile_status_t>(static_cast<int>(status));
}

dlms::profile::HdlcProfileDirection ToCppDirection(
  dlms_profile_hdlc_direction_t direction)
{
  return direction == DLMS_PROFILE_HDLC_SERVER_TO_CLIENT
    ? dlms::profile::HdlcProfileDirection::ServerToClient
    : dlms::profile::HdlcProfileDirection::ClientToServer;
}

dlms::profile::HdlcProfileRole ToCppRole(dlms_profile_hdlc_role_t role)
{
  return role == DLMS_PROFILE_HDLC_ROLE_SERVER
    ? dlms::profile::HdlcProfileRole::Server
    : dlms::profile::HdlcProfileRole::Client;
}

dlms::profile::ApduChannelOptions ToCppOptions(
  const dlms_profile_channel_options_t* options)
{
  dlms::profile::ApduChannelOptions cppOptions =
    dlms::profile::DefaultApduChannelOptions();
  if (options == 0) {
    return cppOptions;
  }

  cppOptions.localWrapperPort = options->local_wrapper_port;
  cppOptions.remoteWrapperPort = options->remote_wrapper_port;
  cppOptions.hdlcClientAddress = options->hdlc_client_address;
  cppOptions.hdlcLogicalDeviceAddress =
    options->hdlc_logical_device_address;
  cppOptions.hdlcPhysicalDeviceAddress =
    options->hdlc_physical_device_address;
  cppOptions.hdlcDirection = ToCppDirection(options->hdlc_direction);
  cppOptions.maximumApduSize = options->maximum_apdu_size;
  cppOptions.scratchBufferSize = options->scratch_buffer_size;
  cppOptions.hdlcRole = ToCppRole(options->hdlc_role);
  cppOptions.hdlcUseSession = options->hdlc_use_session != 0;
  cppOptions.hdlcMaxInformationFieldLengthTransmit =
    options->hdlc_max_information_field_length_transmit;
  cppOptions.hdlcMaxInformationFieldLengthReceive =
    options->hdlc_max_information_field_length_receive;
  cppOptions.hdlcWindowSizeTransmit =
    options->hdlc_window_size_transmit;
  cppOptions.hdlcWindowSizeReceive =
    options->hdlc_window_size_receive;
  cppOptions.hdlcRetryCount = options->hdlc_retry_count;
  cppOptions.hdlcRetryDelayMilliseconds =
    options->hdlc_retry_delay_milliseconds;
  return cppOptions;
}

dlms_profile_channel_t* WrapChannel(dlms::profile::IApduChannel* impl)
{
  if (impl == 0) {
    return 0;
  }

  try {
    dlms_profile_channel_t* channel = new dlms_profile_channel_t;
    channel->impl = impl;
    return channel;
  } catch (...) {
    delete impl;
    return 0;
  }
}

} // namespace

void dlms_profile_default_channel_options(
  dlms_profile_channel_options_t* options)
{
  if (options == 0) {
    return;
  }

  const dlms::profile::ApduChannelOptions cppOptions =
    dlms::profile::DefaultApduChannelOptions();
  options->local_wrapper_port = cppOptions.localWrapperPort;
  options->remote_wrapper_port = cppOptions.remoteWrapperPort;
  options->hdlc_client_address = cppOptions.hdlcClientAddress;
  options->hdlc_logical_device_address = cppOptions.hdlcLogicalDeviceAddress;
  options->hdlc_physical_device_address = cppOptions.hdlcPhysicalDeviceAddress;
  options->hdlc_direction = cppOptions.hdlcDirection ==
      dlms::profile::HdlcProfileDirection::ServerToClient
    ? DLMS_PROFILE_HDLC_SERVER_TO_CLIENT
    : DLMS_PROFILE_HDLC_CLIENT_TO_SERVER;
  options->maximum_apdu_size = cppOptions.maximumApduSize;
  options->scratch_buffer_size = cppOptions.scratchBufferSize;
  options->hdlc_role = cppOptions.hdlcRole ==
      dlms::profile::HdlcProfileRole::Server
    ? DLMS_PROFILE_HDLC_ROLE_SERVER
    : DLMS_PROFILE_HDLC_ROLE_CLIENT;
  options->hdlc_use_session = cppOptions.hdlcUseSession ? 1 : 0;
  options->hdlc_max_information_field_length_transmit =
    cppOptions.hdlcMaxInformationFieldLengthTransmit;
  options->hdlc_max_information_field_length_receive =
    cppOptions.hdlcMaxInformationFieldLengthReceive;
  options->hdlc_window_size_transmit = cppOptions.hdlcWindowSizeTransmit;
  options->hdlc_window_size_receive = cppOptions.hdlcWindowSizeReceive;
  options->hdlc_retry_count = cppOptions.hdlcRetryCount;
  options->hdlc_retry_delay_milliseconds =
    cppOptions.hdlcRetryDelayMilliseconds;
}

dlms_profile_channel_t* dlms_profile_create_wrapper_tcp_channel(
  void* byte_stream,
  const dlms_profile_channel_options_t* options)
{
  if (byte_stream == 0) {
    return 0;
  }

  try {
    return WrapChannel(new dlms::profile::WrapperTcpProfileChannel(
      *static_cast<dlms::transport::IByteStream*>(byte_stream),
      ToCppOptions(options)));
  } catch (...) {
    return 0;
  }
}

dlms_profile_channel_t* dlms_profile_create_wrapper_udp_channel(
  void* datagram_transport,
  const dlms_profile_channel_options_t* options)
{
  if (datagram_transport == 0) {
    return 0;
  }

  try {
    return WrapChannel(new dlms::profile::WrapperUdpProfileChannel(
      *static_cast<dlms::transport::IDatagramTransport*>(datagram_transport),
      ToCppOptions(options)));
  } catch (...) {
    return 0;
  }
}

dlms_profile_channel_t* dlms_profile_create_hdlc_channel(
  void* byte_stream,
  const dlms_profile_channel_options_t* options)
{
  if (byte_stream == 0) {
    return 0;
  }

  try {
    return WrapChannel(new dlms::profile::HdlcProfileChannel(
      *static_cast<dlms::transport::IByteStream*>(byte_stream),
      ToCppOptions(options)));
  } catch (...) {
    return 0;
  }
}

void dlms_profile_destroy_channel(dlms_profile_channel_t* channel)
{
  if (channel == 0) {
    return;
  }

  delete channel->impl;
  channel->impl = 0;
  delete channel;
}

dlms_profile_status_t dlms_profile_open(dlms_profile_channel_t* channel)
{
  if (channel == 0 || channel->impl == 0) {
    return DLMS_PROFILE_STATUS_INVALID_ARGUMENT;
  }

  return ToCStatus(channel->impl->Open());
}

dlms_profile_status_t dlms_profile_close(dlms_profile_channel_t* channel)
{
  if (channel == 0 || channel->impl == 0) {
    return DLMS_PROFILE_STATUS_INVALID_ARGUMENT;
  }

  return ToCStatus(channel->impl->Close());
}

int dlms_profile_is_open(const dlms_profile_channel_t* channel)
{
  if (channel == 0 || channel->impl == 0) {
    return 0;
  }

  return channel->impl->IsOpen() ? 1 : 0;
}

dlms_profile_status_t dlms_profile_connect_data_link(
  dlms_profile_channel_t* channel)
{
  if (channel == 0 || channel->impl == 0) {
    return DLMS_PROFILE_STATUS_INVALID_ARGUMENT;
  }

  dlms::profile::HdlcProfileChannel* hdlc =
    dynamic_cast<dlms::profile::HdlcProfileChannel*>(channel->impl);
  if (hdlc == 0) {
    return DLMS_PROFILE_STATUS_UNSUPPORTED_FEATURE;
  }
  return ToCStatus(hdlc->ConnectDataLink());
}

dlms_profile_status_t dlms_profile_accept_data_link(
  dlms_profile_channel_t* channel)
{
  if (channel == 0 || channel->impl == 0) {
    return DLMS_PROFILE_STATUS_INVALID_ARGUMENT;
  }

  dlms::profile::HdlcProfileChannel* hdlc =
    dynamic_cast<dlms::profile::HdlcProfileChannel*>(channel->impl);
  if (hdlc == 0) {
    return DLMS_PROFILE_STATUS_UNSUPPORTED_FEATURE;
  }
  return ToCStatus(hdlc->AcceptDataLink());
}

dlms_profile_status_t dlms_profile_disconnect_data_link(
  dlms_profile_channel_t* channel)
{
  if (channel == 0 || channel->impl == 0) {
    return DLMS_PROFILE_STATUS_INVALID_ARGUMENT;
  }

  dlms::profile::HdlcProfileChannel* hdlc =
    dynamic_cast<dlms::profile::HdlcProfileChannel*>(channel->impl);
  if (hdlc == 0) {
    return DLMS_PROFILE_STATUS_UNSUPPORTED_FEATURE;
  }
  return ToCStatus(hdlc->DisconnectDataLink());
}

dlms_profile_status_t dlms_profile_send_apdu(
  dlms_profile_channel_t* channel,
  const uint8_t* apdu,
  size_t apdu_size)
{
  if (channel == 0 || channel->impl == 0) {
    return DLMS_PROFILE_STATUS_INVALID_ARGUMENT;
  }

  dlms::profile::ProfileByteView view;
  view.data = apdu;
  view.size = apdu_size;
  return ToCStatus(channel->impl->SendApdu(view));
}

dlms_profile_status_t dlms_profile_receive_apdu(
  dlms_profile_channel_t* channel,
  uint8_t* output,
  size_t output_size,
  size_t* written_size)
{
  if (channel == 0 || channel->impl == 0) {
    return DLMS_PROFILE_STATUS_INVALID_ARGUMENT;
  }

  dlms::profile::ProfileMutableBuffer buffer;
  buffer.data = output;
  buffer.size = output_size;
  buffer.writtenSize = written_size;
  return ToCStatus(channel->impl->ReceiveApdu(buffer));
}
