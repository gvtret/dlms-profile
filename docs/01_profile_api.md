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

