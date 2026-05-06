#pragma once

#include "dlms/hdlc/hdlc_error.hpp"
#include "dlms/llc/llc_error.hpp"
#include "dlms/transport/transport_status.hpp"
#include "dlms/wrapper/wrapper_error.hpp"
#include "dlms/wrapper/wrapper_ports.hpp"

#include <cstddef>
#include <cstdint>

namespace dlms {
namespace profile {

enum class ProfileStatus
{
  Ok = 0,
  NeedMoreData = 1,
  OutputBufferTooSmall = 2,
  InvalidArgument = 3,
  NotOpen = 4,
  AlreadyOpen = 5,
  OpenFailed = 6,
  ReadFailed = 7,
  WriteFailed = 8,
  Timeout = 9,
  ConnectionClosed = 10,
  WouldBlock = 11,
  InvalidFrame = 12,
  InvalidLength = 13,
  InvalidAddress = 14,
  PayloadTooLarge = 15,
  UnsupportedFeature = 16,
  InternalError = 17
};

struct ProfileByteView
{
  const std::uint8_t* data;
  std::size_t size;
};

struct ProfileMutableBuffer
{
  std::uint8_t* data;
  std::size_t size;
  std::size_t* writtenSize;
};

enum class HdlcProfileDirection
{
  ClientToServer,
  ServerToClient
};

enum class HdlcProfileRole
{
  Client,
  Server
};

struct ApduChannelOptions
{
  std::uint16_t localWrapperPort;
  std::uint16_t remoteWrapperPort;

  std::uint8_t hdlcClientAddress;
  std::uint16_t hdlcLogicalDeviceAddress;
  std::uint16_t hdlcPhysicalDeviceAddress;
  HdlcProfileDirection hdlcDirection;
  HdlcProfileRole hdlcRole;
  bool hdlcUseSession;
  std::size_t hdlcMaxInformationFieldLengthTransmit;
  std::size_t hdlcMaxInformationFieldLengthReceive;
  std::uint8_t hdlcWindowSizeTransmit;
  std::uint8_t hdlcWindowSizeReceive;

  std::size_t maximumApduSize;
  std::size_t scratchBufferSize;
};

ApduChannelOptions DefaultApduChannelOptions();

bool IsValidByteView(ProfileByteView view);
bool IsValidMutableBuffer(ProfileMutableBuffer buffer);

ProfileStatus MapTransportStatus(dlms::transport::TransportStatus status);
ProfileStatus MapWrapperStatus(dlms::wrapper::WrapperStatus status);
ProfileStatus MapLlcStatus(dlms::llc::LlcStatus status);
ProfileStatus MapHdlcStatus(dlms::hdlc::HdlcStatus status);

} // namespace profile
} // namespace dlms
