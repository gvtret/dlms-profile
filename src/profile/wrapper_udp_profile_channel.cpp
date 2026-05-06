#include "dlms/profile/wrapper_udp_profile_channel.hpp"

#include <algorithm>

namespace dlms {
namespace profile {

namespace {

dlms::wrapper::WrapperCodecLimits MakeWrapperLimits(
  const ApduChannelOptions& options)
{
  dlms::wrapper::WrapperCodecLimits limits =
    dlms::wrapper::DefaultWrapperCodecLimits();
  if (options.maximumApduSize != 0u &&
      options.maximumApduSize < limits.maximumDataSize) {
    limits.maximumDataSize = options.maximumApduSize;
    limits.maximumFrameSize =
      dlms::wrapper::kWrapperHeaderSize + options.maximumApduSize;
  }
  return limits;
}

} // namespace

WrapperUdpProfileChannel::WrapperUdpProfileChannel(
  dlms::transport::IDatagramTransport& datagram,
  const ApduChannelOptions& options)
  : datagram_(datagram)
  , options_(options)
  , wrapperLimits_(MakeWrapperLimits(options))
  , readBuffer_(options.scratchBufferSize == 0u ? 2048u : options.scratchBufferSize)
{
}

ProfileStatus WrapperUdpProfileChannel::Open()
{
  return MapTransportStatus(datagram_.Open());
}

ProfileStatus WrapperUdpProfileChannel::Close()
{
  return MapTransportStatus(datagram_.Close());
}

bool WrapperUdpProfileChannel::IsOpen() const
{
  return datagram_.IsOpen();
}

ProfileStatus WrapperUdpProfileChannel::SendApdu(ProfileByteView apdu)
{
  if (!IsValidByteView(apdu)) {
    return ProfileStatus::InvalidArgument;
  }
  if (options_.maximumApduSize != 0u && apdu.size > options_.maximumApduSize) {
    return ProfileStatus::PayloadTooLarge;
  }

  dlms::wrapper::WrapperFrame frame;
  frame.sourcePort = options_.localWrapperPort;
  frame.destinationPort = options_.remoteWrapperPort;
  frame.data = apdu.data;
  frame.dataSize = apdu.size;

  std::vector<std::uint8_t> wpdu;
  const ProfileStatus encodeStatus =
    MapWrapperStatus(dlms::wrapper::EncodeWpdu(frame, wrapperLimits_, wpdu));
  if (encodeStatus != ProfileStatus::Ok) {
    return encodeStatus;
  }

  const std::uint8_t* data = wpdu.empty() ? 0 : &wpdu[0];
  return MapTransportStatus(datagram_.Send(data, wpdu.size()));
}

ProfileStatus WrapperUdpProfileChannel::ReceiveApdu(
  std::vector<std::uint8_t>& apdu)
{
  apdu.clear();

  dlms::wrapper::WrapperFrameBuffer frame;
  const ProfileStatus status = ReceiveFrame(frame);
  if (status != ProfileStatus::Ok) {
    return status;
  }

  try {
    apdu = frame.data;
  } catch (...) {
    apdu.clear();
    return ProfileStatus::InternalError;
  }

  return ProfileStatus::Ok;
}

ProfileStatus WrapperUdpProfileChannel::ReceiveApdu(
  ProfileMutableBuffer output)
{
  if (!IsValidMutableBuffer(output)) {
    return ProfileStatus::InvalidArgument;
  }
  *output.writtenSize = 0u;

  dlms::wrapper::WrapperFrameBuffer frame;
  const ProfileStatus status = ReceiveFrame(frame);
  if (status != ProfileStatus::Ok) {
    return status;
  }

  return CopyFrameData(frame, output);
}

ProfileStatus WrapperUdpProfileChannel::ReceiveFrame(
  dlms::wrapper::WrapperFrameBuffer& frame)
{
  if (readBuffer_.empty()) {
    return ProfileStatus::InvalidArgument;
  }

  std::size_t bytesRead = 0u;
  const ProfileStatus readStatus =
    MapTransportStatus(datagram_.Receive(&readBuffer_[0],
                                         readBuffer_.size(),
                                         bytesRead));
  if (readStatus != ProfileStatus::Ok) {
    return readStatus;
  }
  if (bytesRead == 0u) {
    return ProfileStatus::NeedMoreData;
  }

  const ProfileStatus decodeStatus =
    MapWrapperStatus(dlms::wrapper::DecodeWpdu(&readBuffer_[0],
                                               bytesRead,
                                               wrapperLimits_,
                                               frame));
  return decodeStatus == ProfileStatus::NeedMoreData
    ? ProfileStatus::InvalidLength
    : decodeStatus;
}

ProfileStatus WrapperUdpProfileChannel::CopyFrameData(
  const dlms::wrapper::WrapperFrameBuffer& frame,
  ProfileMutableBuffer output)
{
  if (output.size < frame.data.size()) {
    return ProfileStatus::OutputBufferTooSmall;
  }

  if (!frame.data.empty()) {
    std::copy(frame.data.begin(), frame.data.end(), output.data);
  }
  *output.writtenSize = frame.data.size();
  return ProfileStatus::Ok;
}

} // namespace profile
} // namespace dlms
