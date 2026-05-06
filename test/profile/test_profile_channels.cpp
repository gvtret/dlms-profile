#include "dlms/profile/hdlc_profile_channel.hpp"
#include "dlms/profile/profile_types.hpp"
#include "dlms/profile/wrapper_tcp_profile_channel.hpp"
#include "dlms/profile/wrapper_udp_profile_channel.hpp"

#include "dlms/hdlc/hdlc_codec.hpp"
#include "dlms/llc/llc_codec.hpp"
#include "dlms/transport/fake_transport.hpp"
#include "dlms/wrapper/wrapper_codec.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace {

using dlms::profile::ApduChannelOptions;
using dlms::profile::DefaultApduChannelOptions;
using dlms::profile::HdlcProfileChannel;
using dlms::profile::ProfileByteView;
using dlms::profile::ProfileMutableBuffer;
using dlms::profile::ProfileStatus;
using dlms::profile::WrapperTcpProfileChannel;
using dlms::profile::WrapperUdpProfileChannel;
using dlms::transport::FakeByteStream;
using dlms::transport::FakeDatagramTransport;
using dlms::transport::TransportStatus;

std::vector<std::uint8_t> Bytes(
  const std::uint8_t* data,
  std::size_t size)
{
  return std::vector<std::uint8_t>(data, data + size);
}

ProfileByteView View(const std::vector<std::uint8_t>& bytes)
{
  ProfileByteView view;
  view.data = bytes.empty() ? 0 : &bytes[0];
  view.size = bytes.size();
  return view;
}

std::vector<std::uint8_t> EncodeWpdu(const std::vector<std::uint8_t>& apdu)
{
  dlms::wrapper::WrapperFrame frame;
  frame.sourcePort = dlms::wrapper::kPublicClient;
  frame.destinationPort = dlms::wrapper::kManagementLogicalDevice;
  frame.data = apdu.empty() ? 0 : &apdu[0];
  frame.dataSize = apdu.size();

  std::vector<std::uint8_t> wpdu;
  EXPECT_EQ(dlms::wrapper::WrapperStatus::Ok,
            dlms::wrapper::EncodeWpdu(frame,
                                      dlms::wrapper::DefaultWrapperCodecLimits(),
                                      wpdu));
  return wpdu;
}

TEST(ProfileTypes, MapsLowerLayerStatuses)
{
  EXPECT_EQ(ProfileStatus::WouldBlock,
            dlms::profile::MapTransportStatus(TransportStatus::WouldBlock));
  EXPECT_EQ(ProfileStatus::InvalidLength,
            dlms::profile::MapWrapperStatus(
              dlms::wrapper::WrapperStatus::InvalidLength));
  EXPECT_EQ(ProfileStatus::InvalidFrame,
            dlms::profile::MapLlcStatus(dlms::llc::LlcStatus::InvalidHeader));
  EXPECT_EQ(ProfileStatus::InvalidAddress,
            dlms::profile::MapHdlcStatus(
              dlms::hdlc::HdlcStatus::InvalidAddress));
}

TEST(WrapperTcpProfileChannelTest, SendApduWritesOneWpdu)
{
  FakeByteStream stream;
  WrapperTcpProfileChannel channel(stream, DefaultApduChannelOptions());
  const std::uint8_t rawApdu[] = {0xc0, 0x01, 0x81, 0x00};
  const std::vector<std::uint8_t> apdu = Bytes(rawApdu, sizeof(rawApdu));

  ASSERT_EQ(ProfileStatus::Ok, channel.Open());
  EXPECT_EQ(ProfileStatus::Ok, channel.SendApdu(View(apdu)));

  ASSERT_EQ(1u, stream.Writes().size());
  dlms::wrapper::WrapperFrameBuffer decoded;
  ASSERT_EQ(dlms::wrapper::WrapperStatus::Ok,
            dlms::wrapper::DecodeWpdu(&stream.Writes()[0][0],
                                      stream.Writes()[0].size(),
                                      dlms::wrapper::DefaultWrapperCodecLimits(),
                                      decoded));
  EXPECT_EQ(apdu, decoded.data);
}

TEST(WrapperTcpProfileChannelTest, ReceiveApduHandlesSplitWpdu)
{
  FakeByteStream stream;
  WrapperTcpProfileChannel channel(stream, DefaultApduChannelOptions());
  const std::uint8_t rawApdu[] = {0xc0, 0x01, 0x7e, 0x00};
  const std::vector<std::uint8_t> apdu = Bytes(rawApdu, sizeof(rawApdu));
  const std::vector<std::uint8_t> wpdu = EncodeWpdu(apdu);

  ASSERT_EQ(ProfileStatus::Ok, channel.Open());
  stream.ScriptRead(std::vector<std::uint8_t>(wpdu.begin(), wpdu.begin() + 8));
  stream.ScriptRead(std::vector<std::uint8_t>(wpdu.begin() + 8, wpdu.end()));

  std::vector<std::uint8_t> received;
  ASSERT_EQ(ProfileStatus::Ok, channel.ReceiveApdu(received));
  EXPECT_EQ(apdu, received);
}

TEST(WrapperTcpProfileChannelTest, SmallReceiveBufferDoesNotConsumeApdu)
{
  FakeByteStream stream;
  WrapperTcpProfileChannel channel(stream, DefaultApduChannelOptions());
  const std::uint8_t rawApdu[] = {0xc0, 0x01, 0x02};
  const std::vector<std::uint8_t> apdu = Bytes(rawApdu, sizeof(rawApdu));
  const std::vector<std::uint8_t> wpdu = EncodeWpdu(apdu);

  ASSERT_EQ(ProfileStatus::Ok, channel.Open());
  stream.ScriptRead(wpdu);

  std::uint8_t small[2] = {};
  std::size_t written = 99u;
  ProfileMutableBuffer smallBuffer;
  smallBuffer.data = small;
  smallBuffer.size = sizeof(small);
  smallBuffer.writtenSize = &written;
  EXPECT_EQ(ProfileStatus::OutputBufferTooSmall,
            channel.ReceiveApdu(smallBuffer));
  EXPECT_EQ(0u, written);

  std::uint8_t output[3] = {};
  ProfileMutableBuffer outputBuffer;
  outputBuffer.data = output;
  outputBuffer.size = sizeof(output);
  outputBuffer.writtenSize = &written;
  ASSERT_EQ(ProfileStatus::Ok, channel.ReceiveApdu(outputBuffer));
  EXPECT_EQ(3u, written);
  EXPECT_EQ(apdu, Bytes(output, written));
}

TEST(WrapperUdpProfileChannelTest, SendApduWritesOneDatagram)
{
  FakeDatagramTransport datagram;
  WrapperUdpProfileChannel channel(datagram, DefaultApduChannelOptions());
  const std::uint8_t rawApdu[] = {0xc1, 0x02};
  const std::vector<std::uint8_t> apdu = Bytes(rawApdu, sizeof(rawApdu));

  ASSERT_EQ(ProfileStatus::Ok, channel.Open());
  EXPECT_EQ(ProfileStatus::Ok, channel.SendApdu(View(apdu)));

  ASSERT_EQ(1u, datagram.SentDatagrams().size());
  dlms::wrapper::WrapperFrameBuffer decoded;
  ASSERT_EQ(dlms::wrapper::WrapperStatus::Ok,
            dlms::wrapper::DecodeWpdu(&datagram.SentDatagrams()[0][0],
                                      datagram.SentDatagrams()[0].size(),
                                      dlms::wrapper::DefaultWrapperCodecLimits(),
                                      decoded));
  EXPECT_EQ(apdu, decoded.data);
}

TEST(WrapperUdpProfileChannelTest, ReceiveApduDecodesFullDatagram)
{
  FakeDatagramTransport datagram;
  WrapperUdpProfileChannel channel(datagram, DefaultApduChannelOptions());
  const std::uint8_t rawApdu[] = {0xc4, 0x01, 0x81};
  const std::vector<std::uint8_t> apdu = Bytes(rawApdu, sizeof(rawApdu));

  ASSERT_EQ(ProfileStatus::Ok, channel.Open());
  datagram.ScriptReceive(EncodeWpdu(apdu));

  std::vector<std::uint8_t> received;
  ASSERT_EQ(ProfileStatus::Ok, channel.ReceiveApdu(received));
  EXPECT_EQ(apdu, received);
}

TEST(WrapperUdpProfileChannelTest, RejectsTruncatedDatagram)
{
  FakeDatagramTransport datagram;
  WrapperUdpProfileChannel channel(datagram, DefaultApduChannelOptions());
  const std::uint8_t truncated[] = {
    0x00, 0x01,
    0x00, 0x10,
    0x00, 0x01,
    0x00, 0x02,
    0xc0
  };

  ASSERT_EQ(ProfileStatus::Ok, channel.Open());
  datagram.ScriptReceive(Bytes(truncated, sizeof(truncated)));

  std::vector<std::uint8_t> received;
  EXPECT_EQ(ProfileStatus::InvalidLength, channel.ReceiveApdu(received));
}

TEST(HdlcProfileChannelTest, SendApduWritesHdlcFrameWithLlcPayload)
{
  FakeByteStream stream;
  HdlcProfileChannel channel(stream, DefaultApduChannelOptions());
  const std::uint8_t rawApdu[] = {0x60, 0x01, 0x02};
  const std::vector<std::uint8_t> apdu = Bytes(rawApdu, sizeof(rawApdu));

  ASSERT_EQ(ProfileStatus::Ok, channel.Open());
  ASSERT_EQ(ProfileStatus::Ok, channel.SendApdu(View(apdu)));

  ASSERT_EQ(1u, stream.Writes().size());
  dlms::hdlc::HdlcFrameBuffer frame;
  ASSERT_EQ(dlms::hdlc::HdlcStatus::Ok,
            dlms::hdlc::DecodeFrame(&stream.Writes()[0][0],
                                    stream.Writes()[0].size(),
                                    dlms::hdlc::DefaultHdlcCodecLimits(),
                                    frame));

  dlms::llc::LlcLpduBuffer lpdu;
  ASSERT_EQ(dlms::llc::LlcStatus::Ok,
            dlms::llc::DecodeLpdu(&frame.information[0],
                                  frame.information.size(),
                                  false,
                                  lpdu));
  EXPECT_EQ(apdu, lpdu.lsdu);
}

TEST(HdlcProfileChannelTest, ReceiveApduDecodesHdlcAndLlc)
{
  FakeByteStream stream;
  HdlcProfileChannel channel(stream, DefaultApduChannelOptions());
  const std::uint8_t rawApdu[] = {0xc0, 0x01, 0x81, 0x00};
  const std::vector<std::uint8_t> apdu = Bytes(rawApdu, sizeof(rawApdu));

  ASSERT_EQ(ProfileStatus::Ok, channel.Open());
  ASSERT_EQ(ProfileStatus::Ok, channel.SendApdu(View(apdu)));
  ASSERT_EQ(1u, stream.Writes().size());
  stream.ScriptRead(stream.Writes()[0]);

  std::vector<std::uint8_t> received;
  ASSERT_EQ(ProfileStatus::Ok, channel.ReceiveApdu(received));
  EXPECT_EQ(apdu, received);
}

TEST(ProfileChannels, InvalidArgumentsAreRejected)
{
  FakeByteStream stream;
  WrapperTcpProfileChannel channel(stream, DefaultApduChannelOptions());

  ProfileByteView invalidView;
  invalidView.data = 0;
  invalidView.size = 1u;
  EXPECT_EQ(ProfileStatus::InvalidArgument, channel.SendApdu(invalidView));

  ProfileMutableBuffer invalidBuffer;
  invalidBuffer.data = 0;
  invalidBuffer.size = 1u;
  invalidBuffer.writtenSize = 0;
  EXPECT_EQ(ProfileStatus::InvalidArgument, channel.ReceiveApdu(invalidBuffer));
}

} // namespace

