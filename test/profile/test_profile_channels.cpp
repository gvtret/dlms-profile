#include "dlms/profile/hdlc_profile_channel.hpp"
#include "dlms/profile/profile_types.hpp"
#include "dlms/profile/wrapper_tcp_profile_channel.hpp"
#include "dlms/profile/wrapper_udp_profile_channel.hpp"

#include "dlms/hdlc/hdlc_codec.hpp"
#include "dlms/hdlc/hdlc_segmentation.hpp"
#include "dlms/hdlc/hdlc_session.hpp"
#include "dlms/llc/llc_codec.hpp"
#include "dlms/transport/fake_transport.hpp"
#include "dlms/transport/byte_stream.hpp"
#include "dlms/wrapper/wrapper_codec.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

#include <gtest/gtest.h>

namespace {

using dlms::profile::ApduChannelOptions;
using dlms::profile::DefaultApduChannelOptions;
using dlms::profile::HdlcProfileRole;
using dlms::profile::HdlcProfileChannel;
using dlms::profile::ProfileByteView;
using dlms::profile::ProfileMutableBuffer;
using dlms::profile::ProfileStatus;
using dlms::profile::IWrapperTcpTraceSink;
using dlms::profile::WrapperTcpTraceDirection;
using dlms::profile::WrapperTcpTraceEvent;
using dlms::profile::WrapperTcpTraceKind;
using dlms::profile::WrapperTcpProfileChannel;
using dlms::profile::WrapperUdpProfileChannel;
using dlms::transport::FakeByteStream;
using dlms::transport::FakeDatagramTransport;
using dlms::transport::IByteStream;
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

dlms::hdlc::HdlcSessionOptions MakeSessionOptions(
  dlms::hdlc::HdlcSessionRole role,
  std::size_t maxInfo)
{
  dlms::hdlc::HdlcSessionOptions options;
  options.role = role;
  EXPECT_EQ(dlms::hdlc::HdlcStatus::Ok,
            dlms::hdlc::DlmsHdlcAddress::MakeClientAddress(
              0x10u,
              options.clientAddress));
  EXPECT_EQ(dlms::hdlc::HdlcStatus::Ok,
            dlms::hdlc::DlmsHdlcAddress::MakeServerAddress(
              0x01u,
              0x00u,
              options.serverAddress));
  options.limits = dlms::hdlc::DefaultHdlcCodecLimits();
  options.limits.maximumReassembledInformationSize = 65538u;
  options.negotiationLimits =
    dlms::hdlc::DefaultHdlcSessionNegotiationLimits();
  options.negotiationLimits.maxInformationFieldLengthTransmit = maxInfo;
  options.negotiationLimits.maxInformationFieldLengthReceive = maxInfo;
  return options;
}

dlms::hdlc::HdlcStreamDecoderOptions MakeStreamDecoderOptions()
{
  dlms::hdlc::HdlcStreamDecoderOptions options;
  options.limits = dlms::hdlc::DefaultHdlcCodecLimits();
  options.noisePolicy =
    dlms::hdlc::HdlcNoisePolicy::IgnoreUntilOpeningFlag;
  return options;
}

class HdlcAutoPeerStream : public IByteStream
{
public:
  explicit HdlcAutoPeerStream(std::size_t maxInfo)
    : open_(false)
    , autoRespond_(true)
    , server_(MakeSessionOptions(dlms::hdlc::HdlcSessionRole::Server, maxInfo))
    , decoder_(MakeStreamDecoderOptions())
  {
  }

  TransportStatus Open()
  {
    open_ = true;
    return TransportStatus::Ok;
  }

  TransportStatus Close()
  {
    open_ = false;
    return TransportStatus::Ok;
  }

  bool IsOpen() const
  {
    return open_;
  }

  TransportStatus ReadSome(
    std::uint8_t* output,
    std::size_t outputSize,
    std::size_t& bytesRead)
  {
    bytesRead = 0u;
    if (!open_) {
      return TransportStatus::NotOpen;
    }
    if (readStatuses_.empty() == false) {
      const TransportStatus status = readStatuses_.front();
      readStatuses_.pop_front();
      return status;
    }
    if (reads_.empty()) {
      return TransportStatus::WouldBlock;
    }
    std::vector<std::uint8_t>& chunk = reads_.front();
    if (outputSize < chunk.size()) {
      return TransportStatus::OutputBufferTooSmall;
    }
    std::copy(chunk.begin(), chunk.end(), output);
    bytesRead = chunk.size();
    reads_.pop_front();
    return TransportStatus::Ok;
  }

  TransportStatus WriteAll(const std::uint8_t* input, std::size_t inputSize)
  {
    if (!open_) {
      return TransportStatus::NotOpen;
    }
    writes_.push_back(inputSize == 0u
      ? std::vector<std::uint8_t>()
      : std::vector<std::uint8_t>(input, input + inputSize));
    if (!autoRespond_) {
      return TransportStatus::Ok;
    }

    std::vector<dlms::hdlc::HdlcFrameBuffer> frames;
    if (decoder_.Push(input, inputSize, frames) != dlms::hdlc::HdlcStatus::Ok) {
      return TransportStatus::Ok;
    }
    for (std::size_t i = 0u; i < frames.size(); ++i) {
      const dlms::hdlc::HdlcFrameKind kind =
        frames[i].control.FrameKind();
      if (server_.ReceiveFrame(frames[i]) != dlms::hdlc::HdlcStatus::Ok) {
        continue;
      }
      std::vector<std::uint8_t> response;
      if (kind == dlms::hdlc::HdlcFrameKind::Unnumbered) {
        if (server_.BuildConnectResponse(response) == dlms::hdlc::HdlcStatus::Ok) {
          reads_.push_back(response);
        }
      } else if (kind == dlms::hdlc::HdlcFrameKind::Information) {
        if (server_.BuildReceiveReadyFrame(true, response) ==
            dlms::hdlc::HdlcStatus::Ok) {
          reads_.push_back(response);
        }
      }
    }
    return TransportStatus::Ok;
  }

  void ScriptRead(const std::vector<std::uint8_t>& bytes)
  {
    reads_.push_back(bytes);
  }

  void ScriptReadStatus(TransportStatus status)
  {
    readStatuses_.push_back(status);
  }

  void SetAutoRespond(bool autoRespond)
  {
    autoRespond_ = autoRespond;
  }

  const std::vector<std::vector<std::uint8_t> >& Writes() const
  {
    return writes_;
  }

private:
  bool open_;
  bool autoRespond_;
  dlms::hdlc::HdlcSession server_;
  dlms::hdlc::HdlcStreamDecoder decoder_;
  std::deque<TransportStatus> readStatuses_;
  std::deque<std::vector<std::uint8_t> > reads_;
  std::vector<std::vector<std::uint8_t> > writes_;
};

class RecordingWrapperTcpTraceSink : public IWrapperTcpTraceSink
{
public:
  void OnWrapperTcpTrace(const WrapperTcpTraceEvent& event)
  {
    events.push_back(event);
  }

  std::vector<WrapperTcpTraceEvent> events;
};

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

TEST(ProfileTypes, DefaultOptionsDisableWrapperTcpTrace)
{
  const ApduChannelOptions options = DefaultApduChannelOptions();
  EXPECT_EQ(0, options.wrapperTcpTraceSink);
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

TEST(WrapperTcpProfileChannelTest, SendApduEmitsOutboundTrace)
{
  FakeByteStream stream;
  RecordingWrapperTcpTraceSink trace;
  ApduChannelOptions options = DefaultApduChannelOptions();
  options.wrapperTcpTraceSink = &trace;
  WrapperTcpProfileChannel channel(stream, options);
  const std::uint8_t rawApdu[] = {0x60, 0x01, 0x00};
  const std::vector<std::uint8_t> apdu = Bytes(rawApdu, sizeof(rawApdu));

  ASSERT_EQ(ProfileStatus::Ok, channel.Open());
  ASSERT_EQ(ProfileStatus::Ok, channel.SendApdu(View(apdu)));

  ASSERT_EQ(1u, trace.events.size());
  EXPECT_EQ(WrapperTcpTraceKind::EncodedFrame, trace.events[0].kind);
  EXPECT_EQ(WrapperTcpTraceDirection::Outbound, trace.events[0].direction);
  EXPECT_EQ(ProfileStatus::Ok, trace.events[0].status);
  EXPECT_EQ(options.localWrapperPort, trace.events[0].sourcePort);
  EXPECT_EQ(options.remoteWrapperPort, trace.events[0].destinationPort);
  EXPECT_EQ(apdu.size(), trace.events[0].apduSize);
  EXPECT_EQ(stream.Writes()[0].size(), trace.events[0].encodedSize);
  EXPECT_EQ(stream.Writes()[0].size(), trace.events[0].byteSize);
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

TEST(WrapperTcpProfileChannelTest, ReceiveApduEmitsInboundDecodedTrace)
{
  FakeByteStream stream;
  RecordingWrapperTcpTraceSink trace;
  ApduChannelOptions options = DefaultApduChannelOptions();
  options.wrapperTcpTraceSink = &trace;
  WrapperTcpProfileChannel channel(stream, options);
  const std::uint8_t rawApdu[] = {0x61, 0x01, 0x00};
  const std::vector<std::uint8_t> apdu = Bytes(rawApdu, sizeof(rawApdu));

  ASSERT_EQ(ProfileStatus::Ok, channel.Open());
  stream.ScriptRead(EncodeWpdu(apdu));

  std::vector<std::uint8_t> received;
  ASSERT_EQ(ProfileStatus::Ok, channel.ReceiveApdu(received));

  ASSERT_EQ(1u, trace.events.size());
  EXPECT_EQ(WrapperTcpTraceKind::DecodedFrame, trace.events[0].kind);
  EXPECT_EQ(WrapperTcpTraceDirection::Inbound, trace.events[0].direction);
  EXPECT_EQ(ProfileStatus::Ok, trace.events[0].status);
  EXPECT_EQ(dlms::wrapper::kPublicClient, trace.events[0].sourcePort);
  EXPECT_EQ(dlms::wrapper::kManagementLogicalDevice,
            trace.events[0].destinationPort);
  EXPECT_EQ(apdu.size(), trace.events[0].apduSize);
  EXPECT_EQ(apdu.size(), trace.events[0].byteSize);
}

TEST(WrapperTcpProfileChannelTest, ReceiveApduEmitsReadFailureTrace)
{
  FakeByteStream stream;
  RecordingWrapperTcpTraceSink trace;
  ApduChannelOptions options = DefaultApduChannelOptions();
  options.wrapperTcpTraceSink = &trace;
  WrapperTcpProfileChannel channel(stream, options);

  ASSERT_EQ(ProfileStatus::Ok, channel.Open());
  stream.ScriptNextReadStatus(TransportStatus::ConnectionClosed);

  std::vector<std::uint8_t> received;
  EXPECT_EQ(ProfileStatus::ConnectionClosed, channel.ReceiveApdu(received));

  ASSERT_EQ(1u, trace.events.size());
  EXPECT_EQ(WrapperTcpTraceKind::ReadStatus, trace.events[0].kind);
  EXPECT_EQ(WrapperTcpTraceDirection::Inbound, trace.events[0].direction);
  EXPECT_EQ(ProfileStatus::ConnectionClosed, trace.events[0].status);
}

TEST(WrapperTcpProfileChannelTest, ReceiveApduEmitsDecodeFailureTrace)
{
  FakeByteStream stream;
  RecordingWrapperTcpTraceSink trace;
  ApduChannelOptions options = DefaultApduChannelOptions();
  options.wrapperTcpTraceSink = &trace;
  WrapperTcpProfileChannel channel(stream, options);
  const std::uint8_t invalid[] = {
    0x00, 0x02,
    0x00, 0x10,
    0x00, 0x01,
    0x00, 0x00
  };

  ASSERT_EQ(ProfileStatus::Ok, channel.Open());
  stream.ScriptRead(Bytes(invalid, sizeof(invalid)));

  std::vector<std::uint8_t> received;
  EXPECT_EQ(ProfileStatus::InvalidFrame, channel.ReceiveApdu(received));

  ASSERT_EQ(1u, trace.events.size());
  EXPECT_EQ(WrapperTcpTraceKind::DecodeStatus, trace.events[0].kind);
  EXPECT_EQ(WrapperTcpTraceDirection::Inbound, trace.events[0].direction);
  EXPECT_EQ(ProfileStatus::InvalidFrame, trace.events[0].status);
  EXPECT_EQ(sizeof(invalid), trace.events[0].byteSize);
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

TEST(HdlcProfileChannelTest, ConnectDataLinkPerformsSnrmUaHandshake)
{
  HdlcAutoPeerStream stream(128u);
  ApduChannelOptions options = DefaultApduChannelOptions();
  options.hdlcUseSession = true;
  options.hdlcRole = HdlcProfileRole::Client;
  HdlcProfileChannel channel(stream, options);

  ASSERT_EQ(ProfileStatus::Ok, channel.Open());
  EXPECT_EQ(ProfileStatus::Ok, channel.ConnectDataLink());
  ASSERT_EQ(1u, stream.Writes().size());

  dlms::hdlc::HdlcFrameBuffer frame;
  ASSERT_EQ(dlms::hdlc::HdlcStatus::Ok,
            dlms::hdlc::DecodeFrame(&stream.Writes()[0][0],
                                    stream.Writes()[0].size(),
                                    dlms::hdlc::DefaultHdlcCodecLimits(),
                                    frame));
  EXPECT_EQ(dlms::hdlc::HdlcFrameKind::Unnumbered,
            frame.control.FrameKind());
}

TEST(HdlcProfileChannelTest, SessionModeSendsIFrameAndConsumesRr)
{
  HdlcAutoPeerStream stream(128u);
  ApduChannelOptions options = DefaultApduChannelOptions();
  options.hdlcUseSession = true;
  options.hdlcRole = HdlcProfileRole::Client;
  HdlcProfileChannel channel(stream, options);
  const std::uint8_t rawApdu[] = {0xc0, 0x01, 0x81, 0x00};
  const std::vector<std::uint8_t> apdu = Bytes(rawApdu, sizeof(rawApdu));

  ASSERT_EQ(ProfileStatus::Ok, channel.Open());
  ASSERT_EQ(ProfileStatus::Ok, channel.ConnectDataLink());
  ASSERT_EQ(ProfileStatus::Ok, channel.SendApdu(View(apdu)));

  ASSERT_EQ(2u, stream.Writes().size());
  dlms::hdlc::HdlcFrameBuffer frame;
  ASSERT_EQ(dlms::hdlc::HdlcStatus::Ok,
            dlms::hdlc::DecodeFrame(&stream.Writes()[1][0],
                                    stream.Writes()[1].size(),
                                    dlms::hdlc::DefaultHdlcCodecLimits(),
                                    frame));
  EXPECT_EQ(dlms::hdlc::HdlcFrameKind::Information,
            frame.control.FrameKind());
}

TEST(HdlcProfileChannelTest, ConnectDataLinkRetriesSnrmAfterWouldBlock)
{
  HdlcAutoPeerStream stream(128u);
  ApduChannelOptions options = DefaultApduChannelOptions();
  options.hdlcUseSession = true;
  options.hdlcRole = HdlcProfileRole::Client;
  options.hdlcRetryCount = 1u;
  options.hdlcRetryDelayMilliseconds = 0u;
  HdlcProfileChannel channel(stream, options);

  ASSERT_EQ(ProfileStatus::Ok, channel.Open());
  stream.ScriptReadStatus(TransportStatus::WouldBlock);
  EXPECT_EQ(ProfileStatus::Ok, channel.ConnectDataLink());
  EXPECT_EQ(2u, stream.Writes().size());
}

TEST(HdlcProfileChannelTest, SessionModeRetransmitsIFrameAfterWouldBlock)
{
  HdlcAutoPeerStream stream(128u);
  ApduChannelOptions options = DefaultApduChannelOptions();
  options.hdlcUseSession = true;
  options.hdlcRole = HdlcProfileRole::Client;
  options.hdlcRetryCount = 1u;
  options.hdlcRetryDelayMilliseconds = 0u;
  HdlcProfileChannel channel(stream, options);
  const std::uint8_t rawApdu[] = {0xc0, 0x01, 0x81, 0x00};
  const std::vector<std::uint8_t> apdu = Bytes(rawApdu, sizeof(rawApdu));

  ASSERT_EQ(ProfileStatus::Ok, channel.Open());
  ASSERT_EQ(ProfileStatus::Ok, channel.ConnectDataLink());
  stream.ScriptReadStatus(TransportStatus::WouldBlock);
  ASSERT_EQ(ProfileStatus::Ok, channel.SendApdu(View(apdu)));

  ASSERT_EQ(3u, stream.Writes().size());
  EXPECT_EQ(stream.Writes()[1], stream.Writes()[2]);
}

TEST(HdlcProfileChannelTest, SessionModeReassemblesSegmentedInformation)
{
  FakeByteStream stream;
  ApduChannelOptions options = DefaultApduChannelOptions();
  options.hdlcUseSession = true;
  options.hdlcRole = HdlcProfileRole::Server;
  options.hdlcDirection = dlms::profile::HdlcProfileDirection::ClientToServer;
  options.hdlcMaxInformationFieldLengthTransmit = 32u;
  options.hdlcMaxInformationFieldLengthReceive = 32u;
  HdlcProfileChannel channel(stream, options);

  dlms::hdlc::HdlcSession client(
    MakeSessionOptions(dlms::hdlc::HdlcSessionRole::Client, 32u));
  std::vector<std::uint8_t> bytes;
  ASSERT_EQ(dlms::hdlc::HdlcStatus::Ok, client.BuildConnectRequest(bytes));

  ASSERT_EQ(ProfileStatus::Ok, channel.Open());
  stream.ScriptRead(bytes);
  ASSERT_EQ(ProfileStatus::Ok, channel.AcceptDataLink());
  ASSERT_EQ(1u, stream.Writes().size());

  dlms::hdlc::HdlcFrameBuffer ua;
  ASSERT_EQ(dlms::hdlc::HdlcStatus::Ok,
            dlms::hdlc::DecodeFrame(&stream.Writes()[0][0],
                                    stream.Writes()[0].size(),
                                    dlms::hdlc::DefaultHdlcCodecLimits(),
                                    ua));
  ASSERT_EQ(dlms::hdlc::HdlcStatus::Ok, client.ReceiveFrame(ua));

  const std::uint8_t rawApdu[] = {
    0xc0, 0x01, 0x81, 0x00, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e,
    0x1f, 0x20, 0x21, 0x22, 0x23
  };
  const std::vector<std::uint8_t> apdu = Bytes(rawApdu, sizeof(rawApdu));
  std::vector<std::uint8_t> lpdu;
  ASSERT_EQ(dlms::llc::LlcStatus::Ok,
            dlms::llc::EncodeLpdu(
              dlms::llc::MakeLlcHeader(dlms::llc::LlcDirection::ClientToServer),
              &apdu[0],
              apdu.size(),
              lpdu));

  std::vector<std::uint8_t> baseBytes;
  ASSERT_EQ(dlms::hdlc::HdlcStatus::Ok,
            client.BuildInformationFrame(0, 0u, true, baseBytes));
  dlms::hdlc::HdlcFrameBuffer baseBuffer;
  ASSERT_EQ(dlms::hdlc::HdlcStatus::Ok,
            dlms::hdlc::DecodeFrame(&baseBytes[0],
                                    baseBytes.size(),
                                    dlms::hdlc::DefaultHdlcCodecLimits(),
                                    baseBuffer));
  dlms::hdlc::HdlcFrame baseFrame;
  baseFrame.segmented = false;
  baseFrame.destination = baseBuffer.destination;
  baseFrame.source = baseBuffer.source;
  baseFrame.control = baseBuffer.control;
  baseFrame.informationData = 0;
  baseFrame.informationSize = 0u;

  dlms::hdlc::HdlcSegmentationOptions segmentationOptions;
  segmentationOptions.limits = dlms::hdlc::DefaultHdlcCodecLimits();
  segmentationOptions.limits.maximumInformationFieldSize = 32u;
  dlms::hdlc::HdlcSegmenter segmenter(segmentationOptions);
  std::vector<dlms::hdlc::HdlcFrameBuffer> frames;
  ASSERT_EQ(dlms::hdlc::HdlcStatus::Ok,
            segmenter.SegmentInformation(baseFrame,
                                         &lpdu[0],
                                         lpdu.size(),
                                         frames));
  ASSERT_GT(frames.size(), 1u);

  for (std::size_t i = 0u; i < frames.size(); ++i) {
    dlms::hdlc::HdlcFrame frame;
    frame.segmented = frames[i].segmented;
    frame.destination = frames[i].destination;
    frame.source = frames[i].source;
    frame.control = frames[i].control;
    frame.informationData = &frames[i].information[0];
    frame.informationSize = frames[i].information.size();
    std::vector<std::uint8_t> encoded;
    ASSERT_EQ(dlms::hdlc::HdlcStatus::Ok,
              dlms::hdlc::EncodeFrame(frame,
                                      dlms::hdlc::DefaultHdlcCodecLimits(),
                                      encoded));
    stream.ScriptRead(encoded);
  }

  std::vector<std::uint8_t> received;
  ASSERT_EQ(ProfileStatus::Ok, channel.ReceiveApdu(received));
  EXPECT_EQ(apdu, received);
  ASSERT_EQ(2u, stream.Writes().size());
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
