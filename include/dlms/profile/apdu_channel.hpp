#pragma once

#include "dlms/profile/profile_types.hpp"

#include <cstdint>
#include <vector>

namespace dlms {
namespace profile {

class IApduChannel
{
public:
  virtual ~IApduChannel() {}

  virtual ProfileStatus Open() = 0;
  virtual ProfileStatus Close() = 0;
  virtual bool IsOpen() const = 0;

  virtual ProfileStatus SendApdu(ProfileByteView apdu) = 0;
  virtual ProfileStatus ReceiveApdu(std::vector<std::uint8_t>& apdu) = 0;
  virtual ProfileStatus ReceiveApdu(ProfileMutableBuffer output) = 0;
};

} // namespace profile
} // namespace dlms

