# Profile C++ API

## Status Model

All public runtime functions return `ProfileStatus`. Public APIs must not throw
exceptions. Allocation failures in vector convenience paths are mapped to
`ProfileStatus::InternalError`.

## Buffer Types

`ProfileByteView` is a non-owning input view. `ProfileMutableBuffer` is a
caller-provided output buffer and written-size pair.

Null data pointers are accepted only when the matching size is zero.

## Channel Options

`ApduChannelOptions` contains:

- local and remote wrapper ports;
- HDLC client, logical server, and physical server addresses;
- HDLC client/server direction;
- maximum APDU receive size.

The default is public client to management logical device.

## Channel Interface

`IApduChannel` exposes:

```cpp
ProfileStatus Open();
ProfileStatus Close();
bool IsOpen() const;
ProfileStatus SendApdu(ProfileByteView apdu);
ProfileStatus ReceiveApdu(std::vector<std::uint8_t>& apdu);
ProfileStatus ReceiveApdu(ProfileMutableBuffer output);
```

## Concrete Channels

- `WrapperTcpProfileChannel` uses `IByteStream`, `EncodeWpdu`, and
  `WrapperStreamDecoder`.
- `WrapperUdpProfileChannel` uses `IDatagramTransport`, `EncodeWpdu`, and
  `DecodeWpdu`.
- `HdlcProfileChannel` uses `IByteStream`, standard DLMS LLC headers,
  `EncodeFrame`, and `HdlcStreamDecoder`.

The concrete channels do not include APDU codec headers.

## Examples

Wrapper/TCP:

```cpp
dlms::profile::ApduChannelOptions options =
  dlms::profile::DefaultApduChannelOptions();
dlms::profile::WrapperTcpProfileChannel channel(stream, options);

channel.Open();
channel.SendApdu(dlms::profile::ProfileByteView{apdu, apduSize});

std::vector<std::uint8_t> received;
channel.ReceiveApdu(received);
```

Wrapper/UDP:

```cpp
dlms::profile::WrapperUdpProfileChannel channel(
  datagram,
  dlms::profile::DefaultApduChannelOptions());
channel.SendApdu(dlms::profile::ProfileByteView{apdu, apduSize});
```

HDLC/LLC:

```cpp
dlms::profile::ApduChannelOptions options =
  dlms::profile::DefaultApduChannelOptions();
options.hdlcDirection = dlms::profile::HdlcProfileDirection::ClientToServer;

dlms::profile::HdlcProfileChannel channel(stream, options);
channel.SendApdu(dlms::profile::ProfileByteView{apdu, apduSize});
```

Caller-provided receive buffer:

```cpp
std::uint8_t output[1024];
std::size_t written = 0;
dlms::profile::ProfileMutableBuffer buffer;
buffer.data = output;
buffer.size = sizeof(output);
buffer.writtenSize = &written;

const dlms::profile::ProfileStatus status = channel.ReceiveApdu(buffer);
```
