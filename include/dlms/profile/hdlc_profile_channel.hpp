#pragma once

#include "dlms/hdlc/hdlc_segmentation.hpp"
#include "dlms/hdlc/hdlc_session.hpp"
#include "dlms/hdlc/hdlc_stream_decoder.hpp"
#include "dlms/profile/apdu_channel.hpp"
#include "dlms/transport/byte_stream.hpp"
#include "dlms/transport/timer_scheduler.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace dlms {
namespace profile {

class HdlcProfileChannel : public IApduChannel
{
public:
  HdlcProfileChannel(
    dlms::transport::IByteStream& stream,
    const ApduChannelOptions& options);
  HdlcProfileChannel(
    dlms::transport::IByteStream& stream,
    dlms::transport::ITimerScheduler& timer,
    const ApduChannelOptions& options);

  ProfileStatus Open();
  ProfileStatus Close();
  bool IsOpen() const;

  ProfileStatus ConnectDataLink();
  ProfileStatus AcceptDataLink();
  ProfileStatus DisconnectDataLink();

  ProfileStatus SendApdu(ProfileByteView apdu);
  ProfileStatus ReceiveApdu(std::vector<std::uint8_t>& apdu);
  ProfileStatus ReceiveApdu(ProfileMutableBuffer output);

private:
  ProfileStatus MakeHdlcSessionOptions(
    dlms::hdlc::HdlcSessionOptions& sessionOptions) const;
  ProfileStatus EnsureSession();
  ProfileStatus WriteFrameBytes(const std::vector<std::uint8_t>& frameBytes);
  ProfileStatus BuildSessionInformationFrame(
    const std::uint8_t* information,
    std::size_t informationSize,
    bool segmented,
    bool pollFinal,
    std::vector<std::uint8_t>& frameBytes);
  ProfileStatus SleepBeforeRetry();
  ProfileStatus ReceiveSessionControlFrameWithRetry(
    const std::vector<std::uint8_t>& retryFrameBytes);
  ProfileStatus BuildFrame(
    const std::vector<std::uint8_t>& lpdu,
    std::vector<std::uint8_t>& frameBytes) const;
  ProfileStatus SendSessionInformation(
    const std::vector<std::uint8_t>& lpdu);
  ProfileStatus SendSessionReceiveReady(bool pollFinal);
  ProfileStatus ReceiveDecodedFrame(dlms::hdlc::HdlcFrameBuffer& frame);
  ProfileStatus ReceiveSessionControlFrame();
  ProfileStatus ReceiveNextApdu();
  ProfileStatus DecodeFrameToPendingApdu(
    const dlms::hdlc::HdlcFrameBuffer& frame);
  ProfileStatus DecodeCompleteInformationToPendingApdu(
    const dlms::hdlc::HdlcFrameBuffer& frame);
  ProfileStatus CopyFirstPendingApdu(ProfileMutableBuffer output, bool consume);

  dlms::transport::IByteStream& stream_;
  dlms::transport::TimerScheduler defaultTimer_;
  dlms::transport::ITimerScheduler* timer_;
  ApduChannelOptions options_;
  dlms::hdlc::HdlcCodecLimits hdlcLimits_;
  dlms::hdlc::HdlcStreamDecoder decoder_;
  dlms::hdlc::HdlcReassembler reassembler_;
  dlms::hdlc::HdlcSession session_;
  std::vector<std::uint8_t> readBuffer_;
  std::deque<dlms::hdlc::HdlcFrameBuffer> pendingHdlcFrames_;
  std::vector<std::vector<std::uint8_t> > pendingApdus_;
};

} // namespace profile
} // namespace dlms
