#include "dlms/profile/hdlc_profile_channel.hpp"

#include "dlms/hdlc/hdlc_address.hpp"
#include "dlms/hdlc/hdlc_codec.hpp"
#include "dlms/hdlc/hdlc_control.hpp"
#include "dlms/llc/llc_codec.hpp"
#include "dlms/llc/llc_header.hpp"

#include <algorithm>

namespace dlms {
namespace profile {

namespace {

dlms::hdlc::HdlcStreamDecoder MakeDecoder(
  const dlms::hdlc::HdlcCodecLimits& limits)
{
  dlms::hdlc::HdlcStreamDecoderOptions decoderOptions;
  decoderOptions.limits = limits;
  decoderOptions.noisePolicy =
    dlms::hdlc::HdlcNoisePolicy::IgnoreUntilOpeningFlag;
  return dlms::hdlc::HdlcStreamDecoder(decoderOptions);
}

dlms::llc::LlcDirection ToLlcDirection(HdlcProfileDirection direction)
{
  return direction == HdlcProfileDirection::ServerToClient
    ? dlms::llc::LlcDirection::ServerToClient
    : dlms::llc::LlcDirection::ClientToServer;
}

} // namespace

HdlcProfileChannel::HdlcProfileChannel(
  dlms::transport::IByteStream& stream,
  const ApduChannelOptions& options)
  : stream_(stream)
  , options_(options)
  , hdlcLimits_(dlms::hdlc::DefaultHdlcCodecLimits())
  , decoder_(MakeDecoder(hdlcLimits_))
  , readBuffer_(options.scratchBufferSize == 0u ? 2048u : options.scratchBufferSize)
{
}

ProfileStatus HdlcProfileChannel::Open()
{
  return MapTransportStatus(stream_.Open());
}

ProfileStatus HdlcProfileChannel::Close()
{
  decoder_.Reset();
  pendingApdus_.clear();
  return MapTransportStatus(stream_.Close());
}

bool HdlcProfileChannel::IsOpen() const
{
  return stream_.IsOpen();
}

ProfileStatus HdlcProfileChannel::SendApdu(ProfileByteView apdu)
{
  if (!IsValidByteView(apdu)) {
    return ProfileStatus::InvalidArgument;
  }
  if (options_.maximumApduSize != 0u && apdu.size > options_.maximumApduSize) {
    return ProfileStatus::PayloadTooLarge;
  }

  std::vector<std::uint8_t> lpdu;
  const dlms::llc::LlcHeader header =
    dlms::llc::MakeLlcHeader(ToLlcDirection(options_.hdlcDirection));
  ProfileStatus status =
    MapLlcStatus(dlms::llc::EncodeLpdu(header, apdu.data, apdu.size, lpdu));
  if (status != ProfileStatus::Ok) {
    return status;
  }

  std::vector<std::uint8_t> frameBytes;
  status = BuildFrame(lpdu, frameBytes);
  if (status != ProfileStatus::Ok) {
    return status;
  }

  const std::uint8_t* data = frameBytes.empty() ? 0 : &frameBytes[0];
  return MapTransportStatus(stream_.WriteAll(data, frameBytes.size()));
}

ProfileStatus HdlcProfileChannel::ReceiveApdu(
  std::vector<std::uint8_t>& apdu)
{
  apdu.clear();

  while (pendingApdus_.empty()) {
    const ProfileStatus status = ReceiveNextApdu();
    if (status != ProfileStatus::Ok) {
      return status;
    }
  }

  try {
    apdu = pendingApdus_.front();
    pendingApdus_.erase(pendingApdus_.begin());
  } catch (...) {
    apdu.clear();
    return ProfileStatus::InternalError;
  }

  return ProfileStatus::Ok;
}

ProfileStatus HdlcProfileChannel::ReceiveApdu(ProfileMutableBuffer output)
{
  if (!IsValidMutableBuffer(output)) {
    return ProfileStatus::InvalidArgument;
  }
  *output.writtenSize = 0u;

  while (pendingApdus_.empty()) {
    const ProfileStatus status = ReceiveNextApdu();
    if (status != ProfileStatus::Ok) {
      return status;
    }
  }

  return CopyFirstPendingApdu(output, true);
}

ProfileStatus HdlcProfileChannel::BuildFrame(
  const std::vector<std::uint8_t>& lpdu,
  std::vector<std::uint8_t>& frameBytes) const
{
  dlms::hdlc::HdlcAddress clientAddress;
  dlms::hdlc::HdlcAddress serverAddress;
  ProfileStatus status =
    MapHdlcStatus(dlms::hdlc::DlmsHdlcAddress::MakeClientAddress(
      options_.hdlcClientAddress,
      clientAddress));
  if (status != ProfileStatus::Ok) {
    return status;
  }

  status = MapHdlcStatus(dlms::hdlc::DlmsHdlcAddress::MakeServerAddress(
    options_.hdlcLogicalDeviceAddress,
    options_.hdlcPhysicalDeviceAddress,
    serverAddress));
  if (status != ProfileStatus::Ok) {
    return status;
  }

  dlms::hdlc::HdlcFrame frame;
  frame.segmented = false;
  if (options_.hdlcDirection == HdlcProfileDirection::ServerToClient) {
    frame.destination = clientAddress;
    frame.source = serverAddress;
  } else {
    frame.destination = serverAddress;
    frame.source = clientAddress;
  }

  status = MapHdlcStatus(dlms::hdlc::HdlcControl::Decode(0x10u, frame.control));
  if (status != ProfileStatus::Ok) {
    return status;
  }

  frame.informationData = lpdu.empty() ? 0 : &lpdu[0];
  frame.informationSize = lpdu.size();
  return MapHdlcStatus(dlms::hdlc::EncodeFrame(frame, hdlcLimits_, frameBytes));
}

ProfileStatus HdlcProfileChannel::ReceiveNextApdu()
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

    std::vector<dlms::hdlc::HdlcFrameBuffer> frames;
    const ProfileStatus decodeStatus =
      MapHdlcStatus(decoder_.Push(&readBuffer_[0], bytesRead, frames));
    if (decodeStatus == ProfileStatus::Ok) {
      for (std::size_t i = 0u; i < frames.size(); ++i) {
        const ProfileStatus frameStatus = DecodeFrameToPendingApdu(frames[i]);
        if (frameStatus != ProfileStatus::Ok) {
          return frameStatus;
        }
      }
      return ProfileStatus::Ok;
    }

    if (decodeStatus != ProfileStatus::NeedMoreData) {
      decoder_.Reset();
      return decodeStatus;
    }
  }
}

ProfileStatus HdlcProfileChannel::DecodeFrameToPendingApdu(
  const dlms::hdlc::HdlcFrameBuffer& frame)
{
  if (frame.segmented) {
    return ProfileStatus::UnsupportedFeature;
  }

  dlms::llc::LlcLpduBuffer lpdu;
  const std::uint8_t* information =
    frame.information.empty() ? 0 : &frame.information[0];
  const ProfileStatus status =
    MapLlcStatus(dlms::llc::DecodeLpdu(information,
                                       frame.information.size(),
                                       false,
                                       lpdu));
  if (status != ProfileStatus::Ok) {
    return status;
  }

  if (options_.maximumApduSize != 0u &&
      lpdu.lsdu.size() > options_.maximumApduSize) {
    return ProfileStatus::PayloadTooLarge;
  }

  try {
    pendingApdus_.push_back(lpdu.lsdu);
  } catch (...) {
    pendingApdus_.clear();
    return ProfileStatus::InternalError;
  }

  return ProfileStatus::Ok;
}

ProfileStatus HdlcProfileChannel::CopyFirstPendingApdu(
  ProfileMutableBuffer output,
  bool consume)
{
  const std::vector<std::uint8_t>& apdu = pendingApdus_.front();
  if (output.size < apdu.size()) {
    return ProfileStatus::OutputBufferTooSmall;
  }

  if (!apdu.empty()) {
    std::copy(apdu.begin(), apdu.end(), output.data);
  }
  *output.writtenSize = apdu.size();

  if (consume) {
    pendingApdus_.erase(pendingApdus_.begin());
  }

  return ProfileStatus::Ok;
}

} // namespace profile
} // namespace dlms

