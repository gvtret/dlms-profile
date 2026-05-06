#include "dlms/profile/profile_types.hpp"

namespace dlms {
namespace profile {

ApduChannelOptions DefaultApduChannelOptions()
{
  ApduChannelOptions options;
  options.localWrapperPort = dlms::wrapper::kPublicClient;
  options.remoteWrapperPort = dlms::wrapper::kManagementLogicalDevice;
  options.hdlcClientAddress = 0x10u;
  options.hdlcLogicalDeviceAddress = 0x01u;
  options.hdlcPhysicalDeviceAddress = 0x00u;
  options.hdlcDirection = HdlcProfileDirection::ClientToServer;
  options.hdlcRole = HdlcProfileRole::Client;
  options.hdlcUseSession = false;
  options.hdlcMaxInformationFieldLengthTransmit = 0u;
  options.hdlcMaxInformationFieldLengthReceive = 0u;
  options.hdlcWindowSizeTransmit = 0u;
  options.hdlcWindowSizeReceive = 0u;
  options.maximumApduSize = 65535u;
  options.scratchBufferSize = 2048u;
  return options;
}

bool IsValidByteView(ProfileByteView view)
{
  return view.data != 0 || view.size == 0u;
}

bool IsValidMutableBuffer(ProfileMutableBuffer buffer)
{
  if (buffer.writtenSize == 0) {
    return false;
  }
  return buffer.data != 0 || buffer.size == 0u;
}

ProfileStatus MapTransportStatus(dlms::transport::TransportStatus status)
{
  using dlms::transport::TransportStatus;

  switch (status) {
  case TransportStatus::Ok:
    return ProfileStatus::Ok;
  case TransportStatus::InvalidArgument:
    return ProfileStatus::InvalidArgument;
  case TransportStatus::NotOpen:
    return ProfileStatus::NotOpen;
  case TransportStatus::AlreadyOpen:
    return ProfileStatus::AlreadyOpen;
  case TransportStatus::OpenFailed:
    return ProfileStatus::OpenFailed;
  case TransportStatus::ReadFailed:
    return ProfileStatus::ReadFailed;
  case TransportStatus::WriteFailed:
    return ProfileStatus::WriteFailed;
  case TransportStatus::Timeout:
    return ProfileStatus::Timeout;
  case TransportStatus::ConnectionClosed:
    return ProfileStatus::ConnectionClosed;
  case TransportStatus::WouldBlock:
    return ProfileStatus::WouldBlock;
  case TransportStatus::OutputBufferTooSmall:
    return ProfileStatus::OutputBufferTooSmall;
  case TransportStatus::UnsupportedFeature:
    return ProfileStatus::UnsupportedFeature;
  case TransportStatus::InternalError:
    return ProfileStatus::InternalError;
  }

  return ProfileStatus::InternalError;
}

ProfileStatus MapWrapperStatus(dlms::wrapper::WrapperStatus status)
{
  using dlms::wrapper::WrapperStatus;

  switch (status) {
  case WrapperStatus::Ok:
    return ProfileStatus::Ok;
  case WrapperStatus::NeedMoreData:
    return ProfileStatus::NeedMoreData;
  case WrapperStatus::OutputBufferTooSmall:
    return ProfileStatus::OutputBufferTooSmall;
  case WrapperStatus::InvalidArgument:
    return ProfileStatus::InvalidArgument;
  case WrapperStatus::InvalidVersion:
  case WrapperStatus::InvalidSourcePort:
  case WrapperStatus::InvalidDestinationPort:
    return ProfileStatus::InvalidFrame;
  case WrapperStatus::InvalidLength:
    return ProfileStatus::InvalidLength;
  case WrapperStatus::DataTooLarge:
  case WrapperStatus::FrameTooLarge:
    return ProfileStatus::PayloadTooLarge;
  case WrapperStatus::UnsupportedFeature:
    return ProfileStatus::UnsupportedFeature;
  case WrapperStatus::InternalError:
    return ProfileStatus::InternalError;
  }

  return ProfileStatus::InternalError;
}

ProfileStatus MapLlcStatus(dlms::llc::LlcStatus status)
{
  using dlms::llc::LlcStatus;

  switch (status) {
  case LlcStatus::Ok:
    return ProfileStatus::Ok;
  case LlcStatus::NeedMoreData:
    return ProfileStatus::NeedMoreData;
  case LlcStatus::OutputBufferTooSmall:
    return ProfileStatus::OutputBufferTooSmall;
  case LlcStatus::InvalidArgument:
    return ProfileStatus::InvalidArgument;
  case LlcStatus::InvalidHeader:
  case LlcStatus::InvalidDsap:
  case LlcStatus::InvalidSsap:
  case LlcStatus::InvalidControl:
    return ProfileStatus::InvalidFrame;
  case LlcStatus::InvalidLpduLength:
    return ProfileStatus::InvalidLength;
  case LlcStatus::LsduTooLarge:
    return ProfileStatus::PayloadTooLarge;
  case LlcStatus::BroadcastEncodeForbidden:
  case LlcStatus::UnsupportedAddress:
  case LlcStatus::UnsupportedControl:
  case LlcStatus::UnsupportedFeature:
    return ProfileStatus::UnsupportedFeature;
  case LlcStatus::InternalError:
    return ProfileStatus::InternalError;
  }

  return ProfileStatus::InternalError;
}

ProfileStatus MapHdlcStatus(dlms::hdlc::HdlcStatus status)
{
  using dlms::hdlc::HdlcStatus;

  switch (status) {
  case HdlcStatus::Ok:
    return ProfileStatus::Ok;
  case HdlcStatus::NeedMoreData:
  case HdlcStatus::SegmentationIncomplete:
    return ProfileStatus::NeedMoreData;
  case HdlcStatus::OutputBufferTooSmall:
    return ProfileStatus::OutputBufferTooSmall;
  case HdlcStatus::InvalidArgument:
    return ProfileStatus::InvalidArgument;
  case HdlcStatus::InvalidFlag:
  case HdlcStatus::InvalidFrameFormat:
  case HdlcStatus::InvalidFrameType:
  case HdlcStatus::InvalidControlField:
  case HdlcStatus::InvalidHeaderChecksum:
  case HdlcStatus::InvalidFrameChecksum:
  case HdlcStatus::SegmentationError:
    return ProfileStatus::InvalidFrame;
  case HdlcStatus::InvalidFrameLength:
    return ProfileStatus::InvalidLength;
  case HdlcStatus::InvalidAddress:
    return ProfileStatus::InvalidAddress;
  case HdlcStatus::FrameTooLarge:
  case HdlcStatus::InformationFieldTooLarge:
  case HdlcStatus::SegmentationOverflow:
    return ProfileStatus::PayloadTooLarge;
  case HdlcStatus::UnsupportedFrame:
  case HdlcStatus::UnsupportedAddress:
  case HdlcStatus::UnsupportedFeature:
    return ProfileStatus::UnsupportedFeature;
  case HdlcStatus::InternalError:
    return ProfileStatus::InternalError;
  }

  return ProfileStatus::InternalError;
}

} // namespace profile
} // namespace dlms
