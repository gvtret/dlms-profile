#include "dlms/profile/hdlc_profile_channel.hpp"

#include "dlms/hdlc/hdlc_address.hpp"
#include "dlms/hdlc/hdlc_codec.hpp"
#include "dlms/hdlc/hdlc_control.hpp"
#include "dlms/hdlc/hdlc_segmentation.hpp"
#include "dlms/hdlc/hdlc_session.hpp"
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

dlms::hdlc::HdlcSessionRole ToHdlcSessionRole(HdlcProfileRole role)
{
  return role == HdlcProfileRole::Server
    ? dlms::hdlc::HdlcSessionRole::Server
    : dlms::hdlc::HdlcSessionRole::Client;
}

dlms::hdlc::HdlcSessionOptions MakeDefaultSessionOptions()
{
  dlms::hdlc::HdlcSessionOptions options;
  options.role = dlms::hdlc::HdlcSessionRole::Client;
  dlms::hdlc::DlmsHdlcAddress::MakeClientAddress(0x10u,
                                                 options.clientAddress);
  dlms::hdlc::DlmsHdlcAddress::MakeServerAddress(0x01u,
                                                 0x00u,
                                                 options.serverAddress);
  options.limits = dlms::hdlc::DefaultHdlcCodecLimits();
  options.negotiationLimits =
    dlms::hdlc::DefaultHdlcSessionNegotiationLimits();
  return options;
}

} // namespace

HdlcProfileChannel::HdlcProfileChannel(
  dlms::transport::IByteStream& stream,
  const ApduChannelOptions& options)
  : stream_(stream)
  , options_(options)
  , hdlcLimits_(dlms::hdlc::DefaultHdlcCodecLimits())
  , decoder_(MakeDecoder(hdlcLimits_))
  , reassembler_(hdlcLimits_)
  , session_(MakeDefaultSessionOptions())
  , readBuffer_(options.scratchBufferSize == 0u ? 2048u : options.scratchBufferSize)
{
  dlms::hdlc::HdlcSessionOptions sessionOptions;
  if (MakeHdlcSessionOptions(sessionOptions) == ProfileStatus::Ok) {
    session_ = dlms::hdlc::HdlcSession(sessionOptions);
    hdlcLimits_ = sessionOptions.limits;
    decoder_ = MakeDecoder(hdlcLimits_);
    reassembler_ = dlms::hdlc::HdlcReassembler(hdlcLimits_);
  }
}

ProfileStatus HdlcProfileChannel::Open()
{
  return MapTransportStatus(stream_.Open());
}

ProfileStatus HdlcProfileChannel::Close()
{
  decoder_.Reset();
  reassembler_.Reset();
  pendingHdlcFrames_.clear();
  pendingApdus_.clear();
  return MapTransportStatus(stream_.Close());
}

bool HdlcProfileChannel::IsOpen() const
{
  return stream_.IsOpen();
}

ProfileStatus HdlcProfileChannel::ConnectDataLink()
{
  if (!options_.hdlcUseSession) {
    return ProfileStatus::UnsupportedFeature;
  }
  if (!IsOpen()) {
    return ProfileStatus::NotOpen;
  }
  if (options_.hdlcRole != HdlcProfileRole::Client) {
    return ProfileStatus::UnsupportedFeature;
  }

  ProfileStatus status = EnsureSession();
  if (status != ProfileStatus::Ok) {
    return status;
  }

  std::vector<std::uint8_t> frameBytes;
  status = MapHdlcStatus(session_.BuildConnectRequest(frameBytes));
  if (status != ProfileStatus::Ok) {
    return status;
  }
  status = WriteFrameBytes(frameBytes);
  if (status != ProfileStatus::Ok) {
    return status;
  }

  return ReceiveSessionControlFrame();
}

ProfileStatus HdlcProfileChannel::AcceptDataLink()
{
  if (!options_.hdlcUseSession) {
    return ProfileStatus::UnsupportedFeature;
  }
  if (!IsOpen()) {
    return ProfileStatus::NotOpen;
  }
  if (options_.hdlcRole != HdlcProfileRole::Server) {
    return ProfileStatus::UnsupportedFeature;
  }

  ProfileStatus status = EnsureSession();
  if (status != ProfileStatus::Ok) {
    return status;
  }

  status = ReceiveSessionControlFrame();
  if (status != ProfileStatus::Ok) {
    return status;
  }

  std::vector<std::uint8_t> frameBytes;
  status = MapHdlcStatus(session_.BuildConnectResponse(frameBytes));
  if (status != ProfileStatus::Ok) {
    return status;
  }
  return WriteFrameBytes(frameBytes);
}

ProfileStatus HdlcProfileChannel::DisconnectDataLink()
{
  if (!options_.hdlcUseSession) {
    return ProfileStatus::UnsupportedFeature;
  }
  if (!IsOpen()) {
    return ProfileStatus::NotOpen;
  }

  std::vector<std::uint8_t> frameBytes;
  ProfileStatus status =
    MapHdlcStatus(session_.BuildDisconnectRequest(frameBytes));
  if (status != ProfileStatus::Ok) {
    return status;
  }
  status = WriteFrameBytes(frameBytes);
  if (status != ProfileStatus::Ok) {
    return status;
  }

  return ReceiveSessionControlFrame();
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

  if (options_.hdlcUseSession) {
    return SendSessionInformation(lpdu);
  }

  std::vector<std::uint8_t> frameBytes;
  status = BuildFrame(lpdu, frameBytes);
  if (status != ProfileStatus::Ok) {
    return status;
  }

  return WriteFrameBytes(frameBytes);
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

ProfileStatus HdlcProfileChannel::MakeHdlcSessionOptions(
  dlms::hdlc::HdlcSessionOptions& sessionOptions) const
{
  sessionOptions.role = ToHdlcSessionRole(options_.hdlcRole);
  sessionOptions.limits = dlms::hdlc::DefaultHdlcCodecLimits();

  ProfileStatus status =
    MapHdlcStatus(dlms::hdlc::DlmsHdlcAddress::MakeClientAddress(
      options_.hdlcClientAddress,
      sessionOptions.clientAddress));
  if (status != ProfileStatus::Ok) {
    return status;
  }

  status = MapHdlcStatus(dlms::hdlc::DlmsHdlcAddress::MakeServerAddress(
    options_.hdlcLogicalDeviceAddress,
    options_.hdlcPhysicalDeviceAddress,
    sessionOptions.serverAddress));
  if (status != ProfileStatus::Ok) {
    return status;
  }

  sessionOptions.negotiationLimits =
    dlms::hdlc::DefaultHdlcSessionNegotiationLimits();
  sessionOptions.negotiationLimits.maxInformationFieldLengthTransmit =
    options_.hdlcMaxInformationFieldLengthTransmit;
  sessionOptions.negotiationLimits.maxInformationFieldLengthReceive =
    options_.hdlcMaxInformationFieldLengthReceive;
  sessionOptions.negotiationLimits.windowSizeTransmit =
    options_.hdlcWindowSizeTransmit;
  sessionOptions.negotiationLimits.windowSizeReceive =
    options_.hdlcWindowSizeReceive;

  const std::size_t reassemblyLimit = options_.maximumApduSize == 0u
    ? sessionOptions.limits.maximumReassembledInformationSize
    : options_.maximumApduSize + 3u;
  if (sessionOptions.limits.maximumReassembledInformationSize <
      reassemblyLimit) {
    sessionOptions.limits.maximumReassembledInformationSize = reassemblyLimit;
  }

  return ProfileStatus::Ok;
}

ProfileStatus HdlcProfileChannel::EnsureSession()
{
  dlms::hdlc::HdlcSessionOptions sessionOptions;
  ProfileStatus status = MakeHdlcSessionOptions(sessionOptions);
  if (status != ProfileStatus::Ok) {
    return status;
  }
  if (session_.State() == dlms::hdlc::HdlcSessionState::Disconnected) {
    session_ = dlms::hdlc::HdlcSession(sessionOptions);
    hdlcLimits_ = sessionOptions.limits;
    decoder_ = MakeDecoder(hdlcLimits_);
    reassembler_ = dlms::hdlc::HdlcReassembler(hdlcLimits_);
    pendingHdlcFrames_.clear();
  }
  return ProfileStatus::Ok;
}

ProfileStatus HdlcProfileChannel::WriteFrameBytes(
  const std::vector<std::uint8_t>& frameBytes)
{
  const std::uint8_t* data = frameBytes.empty() ? 0 : &frameBytes[0];
  return MapTransportStatus(stream_.WriteAll(data, frameBytes.size()));
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

ProfileStatus HdlcProfileChannel::SendSessionInformation(
  const std::vector<std::uint8_t>& lpdu)
{
  if (session_.State() != dlms::hdlc::HdlcSessionState::Connected) {
    return ProfileStatus::NotOpen;
  }

  const dlms::hdlc::HdlcSessionNegotiationLimits& limits =
    session_.NegotiatedLimits();
  hdlcLimits_.maximumInformationFieldSize =
    limits.maxInformationFieldLengthTransmit;

  if (lpdu.size() <= limits.maxInformationFieldLengthTransmit) {
    std::vector<std::uint8_t> frameBytes;
    const ProfileStatus status =
      MapHdlcStatus(session_.BuildInformationFrame(
        lpdu.empty() ? 0 : &lpdu[0],
        lpdu.size(),
        true,
        frameBytes));
    if (status != ProfileStatus::Ok) {
      return status;
    }
    return WriteFrameBytes(frameBytes);
  }

  std::vector<std::uint8_t> baseBytes;
  ProfileStatus status =
    MapHdlcStatus(session_.BuildInformationFrame(0, 0u, true, baseBytes));
  if (status != ProfileStatus::Ok) {
    return status;
  }

  dlms::hdlc::HdlcFrameBuffer baseBuffer;
  status = MapHdlcStatus(dlms::hdlc::DecodeFrame(
    baseBytes.empty() ? 0 : &baseBytes[0],
    baseBytes.size(),
    hdlcLimits_,
    baseBuffer));
  if (status != ProfileStatus::Ok) {
    return status;
  }

  dlms::hdlc::HdlcFrame baseFrame;
  baseFrame.segmented = false;
  baseFrame.destination = baseBuffer.destination;
  baseFrame.source = baseBuffer.source;
  baseFrame.control = baseBuffer.control;
  baseFrame.informationData = 0;
  baseFrame.informationSize = 0u;

  dlms::hdlc::HdlcSegmentationOptions segmentationOptions;
  segmentationOptions.limits = hdlcLimits_;
  segmentationOptions.limits.maximumInformationFieldSize =
    limits.maxInformationFieldLengthTransmit;
  dlms::hdlc::HdlcSegmenter segmenter(segmentationOptions);
  std::vector<dlms::hdlc::HdlcFrameBuffer> frames;
  status = MapHdlcStatus(segmenter.SegmentInformation(
    baseFrame,
    lpdu.empty() ? 0 : &lpdu[0],
    lpdu.size(),
    frames));
  if (status != ProfileStatus::Ok) {
    return status;
  }

  for (std::size_t i = 0u; i < frames.size(); ++i) {
    dlms::hdlc::HdlcFrame frame;
    frame.segmented = frames[i].segmented;
    frame.destination = frames[i].destination;
    frame.source = frames[i].source;
    frame.control = frames[i].control;
    frame.informationData =
      frames[i].information.empty() ? 0 : &frames[i].information[0];
    frame.informationSize = frames[i].information.size();

    std::vector<std::uint8_t> frameBytes;
    status = MapHdlcStatus(dlms::hdlc::EncodeFrame(
      frame,
      hdlcLimits_,
      frameBytes));
    if (status != ProfileStatus::Ok) {
      return status;
    }
    status = WriteFrameBytes(frameBytes);
    if (status != ProfileStatus::Ok) {
      return status;
    }
  }

  return ProfileStatus::Ok;
}

ProfileStatus HdlcProfileChannel::SendSessionReceiveReady(bool pollFinal)
{
  std::vector<std::uint8_t> frameBytes;
  const ProfileStatus status =
    MapHdlcStatus(session_.BuildReceiveReadyFrame(pollFinal, frameBytes));
  if (status != ProfileStatus::Ok) {
    return status;
  }
  return WriteFrameBytes(frameBytes);
}

ProfileStatus HdlcProfileChannel::ReceiveDecodedFrame(
  dlms::hdlc::HdlcFrameBuffer& frame)
{
  if (!pendingHdlcFrames_.empty()) {
    frame = pendingHdlcFrames_.front();
    pendingHdlcFrames_.pop_front();
    return ProfileStatus::Ok;
  }
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
      if (frames.empty()) {
        return ProfileStatus::NeedMoreData;
      }
      for (std::size_t i = 1u; i < frames.size(); ++i) {
        pendingHdlcFrames_.push_back(frames[i]);
      }
      frame = frames[0];
      return ProfileStatus::Ok;
    }

    if (decodeStatus != ProfileStatus::NeedMoreData) {
      decoder_.Reset();
      return decodeStatus;
    }
  }
}

ProfileStatus HdlcProfileChannel::ReceiveSessionControlFrame()
{
  dlms::hdlc::HdlcFrameBuffer frame;
  ProfileStatus status = ReceiveDecodedFrame(frame);
  if (status != ProfileStatus::Ok) {
    return status;
  }
  status = MapHdlcStatus(session_.ReceiveFrame(frame));
  if (status != ProfileStatus::Ok) {
    return status;
  }
  return ProfileStatus::Ok;
}

ProfileStatus HdlcProfileChannel::ReceiveNextApdu()
{
  for (;;) {
    dlms::hdlc::HdlcFrameBuffer frame;
    const ProfileStatus receiveStatus = ReceiveDecodedFrame(frame);
    if (receiveStatus != ProfileStatus::Ok) {
      return receiveStatus;
    }

    const ProfileStatus frameStatus = DecodeFrameToPendingApdu(frame);
    if (frameStatus == ProfileStatus::NeedMoreData) {
      continue;
    }
    if (frameStatus != ProfileStatus::Ok) {
      return frameStatus;
    }
    if (!pendingApdus_.empty()) {
      return ProfileStatus::Ok;
    }
  }
}

ProfileStatus HdlcProfileChannel::DecodeFrameToPendingApdu(
  const dlms::hdlc::HdlcFrameBuffer& frame)
{
  if (!options_.hdlcUseSession) {
    if (frame.segmented) {
      return ProfileStatus::UnsupportedFeature;
    }
    return DecodeCompleteInformationToPendingApdu(frame);
  }

  dlms::hdlc::HdlcFrameBuffer completed;
  bool hasCompleted = false;
  ProfileStatus status =
    MapHdlcStatus(reassembler_.PushFrame(frame, completed, hasCompleted));
  if (status == ProfileStatus::NeedMoreData) {
    return ProfileStatus::NeedMoreData;
  }
  if (status != ProfileStatus::Ok) {
    reassembler_.Reset();
    return status;
  }

  status = MapHdlcStatus(session_.ReceiveFrame(completed));
  if (status != ProfileStatus::Ok) {
    return status;
  }

  if (!hasCompleted || completed.information.empty()) {
    return ProfileStatus::Ok;
  }

  status = DecodeCompleteInformationToPendingApdu(completed);
  if (status != ProfileStatus::Ok) {
    return status;
  }

  return SendSessionReceiveReady(true);
}

ProfileStatus HdlcProfileChannel::DecodeCompleteInformationToPendingApdu(
  const dlms::hdlc::HdlcFrameBuffer& frame)
{
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
