#pragma once

#include "dlms/profile/apdu_channel.hpp"
#include "dlms/transport/byte_stream.hpp"
#include "dlms/wrapper/wrapper_stream_decoder.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dlms {
namespace profile {

class WrapperTcpProfileChannel : public IApduChannel
{
public:
  WrapperTcpProfileChannel(
    dlms::transport::IByteStream& stream,
    const ApduChannelOptions& options);

  ProfileStatus Open();
  ProfileStatus Close();
  bool IsOpen() const;

  ProfileStatus SendApdu(ProfileByteView apdu);
  ProfileStatus ReceiveApdu(std::vector<std::uint8_t>& apdu);
  ProfileStatus ReceiveApdu(ProfileMutableBuffer output);

private:
  ProfileStatus ReceiveNextFrame();
  ProfileStatus CopyFirstPendingFrame(ProfileMutableBuffer output, bool consume);

  dlms::transport::IByteStream& stream_;
  ApduChannelOptions options_;
  dlms::wrapper::WrapperCodecLimits wrapperLimits_;
  dlms::wrapper::WrapperStreamDecoder decoder_;
  std::vector<std::uint8_t> readBuffer_;
  std::vector<dlms::wrapper::WrapperFrameBuffer> pendingFrames_;
};

} // namespace profile
} // namespace dlms

