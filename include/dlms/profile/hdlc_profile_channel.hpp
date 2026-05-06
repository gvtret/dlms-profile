#pragma once

#include "dlms/hdlc/hdlc_stream_decoder.hpp"
#include "dlms/profile/apdu_channel.hpp"
#include "dlms/transport/byte_stream.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dlms {
namespace profile {

class HdlcProfileChannel : public IApduChannel
{
public:
  HdlcProfileChannel(
    dlms::transport::IByteStream& stream,
    const ApduChannelOptions& options);

  ProfileStatus Open();
  ProfileStatus Close();
  bool IsOpen() const;

  ProfileStatus SendApdu(ProfileByteView apdu);
  ProfileStatus ReceiveApdu(std::vector<std::uint8_t>& apdu);
  ProfileStatus ReceiveApdu(ProfileMutableBuffer output);

private:
  ProfileStatus BuildFrame(
    const std::vector<std::uint8_t>& lpdu,
    std::vector<std::uint8_t>& frameBytes) const;
  ProfileStatus ReceiveNextApdu();
  ProfileStatus DecodeFrameToPendingApdu(
    const dlms::hdlc::HdlcFrameBuffer& frame);
  ProfileStatus CopyFirstPendingApdu(ProfileMutableBuffer output, bool consume);

  dlms::transport::IByteStream& stream_;
  ApduChannelOptions options_;
  dlms::hdlc::HdlcCodecLimits hdlcLimits_;
  dlms::hdlc::HdlcStreamDecoder decoder_;
  std::vector<std::uint8_t> readBuffer_;
  std::vector<std::vector<std::uint8_t> > pendingApdus_;
};

} // namespace profile
} // namespace dlms

