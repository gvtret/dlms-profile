# dlms-profile

`dlms-profile` provides DLMS/COSEM APDU channels over lower profile
transports. It keeps APDU bytes opaque and delegates protocol framing to the
existing lower-layer libraries.

Supported v1 channels:

- Wrapper over TCP byte streams.
- Wrapper over UDP datagrams.
- HDLC + LLC over byte streams.

The library does not parse ACSE or xDLMS APDUs, does not manage association
state, and does not implement retry, timeout, security, or COSEM object logic.

## Build

```text
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

When built inside the root integration workspace, `dlms-profile` links against
`dlms-transport`, `dlms-wrapper`, `dlms-hdlc`, and `dlms-llc`.

## Wrapper/TCP Example

```cpp
#include "dlms/profile/wrapper_tcp_profile_channel.hpp"
#include "dlms/transport/tcp_stream_transport.hpp"

dlms::profile::ApduChannelOptions options =
  dlms::profile::DefaultApduChannelOptions();

dlms::transport::TcpStreamTransport stream(tcpOptions);
dlms::profile::WrapperTcpProfileChannel channel(stream, options);

channel.Open();
channel.SendApdu(dlms::profile::ProfileByteView{apdu, apduSize});

std::vector<std::uint8_t> receivedApdu;
channel.ReceiveApdu(receivedApdu);
channel.Close();
```

## Wrapper/UDP Example

```cpp
#include "dlms/profile/wrapper_udp_profile_channel.hpp"
#include "dlms/transport/udp_transport.hpp"

dlms::transport::UdpTransport datagram(udpOptions);
dlms::profile::WrapperUdpProfileChannel channel(
  datagram,
  dlms::profile::DefaultApduChannelOptions());
```

## HDLC/LLC Example

```cpp
#include "dlms/profile/hdlc_profile_channel.hpp"
#include "dlms/transport/serial_transport.hpp"

dlms::profile::ApduChannelOptions options =
  dlms::profile::DefaultApduChannelOptions();
options.hdlcClientAddress = 0x10;
options.hdlcLogicalDeviceAddress = 0x01;

dlms::transport::SerialTransport serial(serialOptions);
dlms::profile::HdlcProfileChannel channel(serial, options);
```

All examples pass APDU bytes as opaque payload. Use `dlms-apdu` above this
layer when ACSE or xDLMS parsing is required.
