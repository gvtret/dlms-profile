#include "dlms/profile/wrapper_tcp_profile_channel.hpp"

#include "dlms/wrapper/wrapper_codec.hpp"

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

dlms::wrapper::WrapperStreamDecoder MakeDecoder(
  const dlms::wrapper::WrapperCodecLimits& limits)
{
  dlms::wrapper::WrapperStreamDecoderOptions decoderOptions;
  decoderOptions.limits = limits;
  return dlms::wrapper::WrapperStreamDecoder(decoderOptions);
}

} // namespace

WrapperTcpProfileChannel::WrapperTcpProfileChannel(
  dlms::transport::IByteStream& stream,
  const ApduChannelOptions& options)
  : stream_(stream)
  , options_(options)
  , wrapperLimits_(MakeWrapperLimits(options))
  , decoder_(MakeDecoder(wrapperLimits_))
  , readBuffer_(options.scratchBufferSize == 0u ? 2048u : options.scratchBufferSize)
{
}

ProfileStatus WrapperTcpProfileChannel::Open()
{
  return MapTransportStatus(stream_.Open());
}

ProfileStatus WrapperTcpProfileChannel::Close()
{
  decoder_.Reset();
  pendingFrames_.clear();
  return MapTransportStatus(stream_.Close());
}

bool WrapperTcpProfileChannel::IsOpen() const
{
  return stream_.IsOpen();
}

ProfileStatus WrapperTcpProfileChannel::SendApdu(ProfileByteView apdu)
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
  return MapTransportStatus(stream_.WriteAll(data, wpdu.size()));
}

ProfileStatus WrapperTcpProfileChannel::ReceiveApdu(
  std::vector<std::uint8_t>& apdu)
{
  apdu.clear();

  while (pendingFrames_.empty()) {
    const ProfileStatus status = ReceiveNextFrame();
    if (status != ProfileStatus::Ok) {
      return status;
    }
  }

  try {
    apdu = pendingFrames_.front().data;
    pendingFrames_.erase(pendingFrames_.begin());
  } catch (...) {
    apdu.clear();
    return ProfileStatus::InternalError;
  }

  return ProfileStatus::Ok;
}

ProfileStatus WrapperTcpProfileChannel::ReceiveApdu(
  ProfileMutableBuffer output)
{
  if (!IsValidMutableBuffer(output)) {
    return ProfileStatus::InvalidArgument;
  }
  *output.writtenSize = 0u;

  while (pendingFrames_.empty()) {
    const ProfileStatus status = ReceiveNextFrame();
    if (status != ProfileStatus::Ok) {
      return status;
    }
  }

  return CopyFirstPendingFrame(output, true);
}

ProfileStatus WrapperTcpProfileChannel::ReceiveNextFrame()
{
  if (readBuffer_.empty()) {
    return ProfileStatus::InvalidArgument;
  }

  for (;;) {
    std::size_t bytesRead = 0u;
    const ProfileStatus readStatus =
      MapTransportStatus(stream_.ReadSome(&readBuffer_[0],
                                          readBuffer_.size(),
                                          bytesRead));
    if (readStatus != ProfileStatus::Ok) {
      return readStatus;
    }
    if (bytesRead == 0u) {
      return ProfileStatus::NeedMoreData;
    }

    std::vector<dlms::wrapper::WrapperFrameBuffer> frames;
    const ProfileStatus decodeStatus =
      MapWrapperStatus(decoder_.Push(&readBuffer_[0], bytesRead, frames));

    if (decodeStatus == ProfileStatus::Ok) {
      try {
        pendingFrames_.insert(pendingFrames_.end(), frames.begin(), frames.end());
      } catch (...) {
        decoder_.Reset();
        pendingFrames_.clear();
        return ProfileStatus::InternalError;
      }
      return ProfileStatus::Ok;
    }

    if (decodeStatus != ProfileStatus::NeedMoreData) {
      decoder_.Reset();
      return decodeStatus;
    }
  }
}

ProfileStatus WrapperTcpProfileChannel::CopyFirstPendingFrame(
  ProfileMutableBuffer output,
  bool consume)
{
  const std::vector<std::uint8_t>& apdu = pendingFrames_.front().data;
  if (output.size < apdu.size()) {
    return ProfileStatus::OutputBufferTooSmall;
  }

  if (!apdu.empty()) {
    std::copy(apdu.begin(), apdu.end(), output.data);
  }
  *output.writtenSize = apdu.size();

  if (consume) {
    pendingFrames_.erase(pendingFrames_.begin());
  }

  return ProfileStatus::Ok;
}

} // namespace profile
} // namespace dlms
