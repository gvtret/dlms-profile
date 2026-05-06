#pragma once

#include "dlms/profile/apdu_channel.hpp"
#include "dlms/transport/datagram_transport.hpp"
#include "dlms/wrapper/wrapper_codec.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dlms {
namespace profile {

class WrapperUdpProfileChannel : public IApduChannel
{
public:
  WrapperUdpProfileChannel(
    dlms::transport::IDatagramTransport& datagram,
    const ApduChannelOptions& options);

  ProfileStatus Open();
  ProfileStatus Close();
  bool IsOpen() const;

  ProfileStatus SendApdu(ProfileByteView apdu);
  ProfileStatus ReceiveApdu(std::vector<std::uint8_t>& apdu);
  ProfileStatus ReceiveApdu(ProfileMutableBuffer output);

private:
  ProfileStatus ReceiveFrame(dlms::wrapper::WrapperFrameBuffer& frame);
  ProfileStatus CopyFrameData(
    const dlms::wrapper::WrapperFrameBuffer& frame,
    ProfileMutableBuffer output);

  dlms::transport::IDatagramTransport& datagram_;
  ApduChannelOptions options_;
  dlms::wrapper::WrapperCodecLimits wrapperLimits_;
  std::vector<std::uint8_t> readBuffer_;
};

} // namespace profile
} // namespace dlms

