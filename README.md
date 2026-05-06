# dlms-profile

`dlms-profile` provides DLMS/COSEM APDU channels over lower profile
transports. It keeps APDU bytes opaque and delegates protocol framing to the
existing lower-layer libraries.

Supported channels:

- Wrapper over TCP byte streams.
- Wrapper over UDP datagrams.
- HDLC + LLC over byte streams.
- HDLC + LLC over byte streams with optional HDLC data-link session
  orchestration.

The library does not parse ACSE or xDLMS APDUs, does not manage association
state, and does not implement APDU-level retry, timeout, security, or COSEM
object logic. Optional HDLC session mode only retries data-link frames while
waiting for HDLC acknowledgements.

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

For the connection-oriented HDLC profile, enable session mode and establish the
data link before sending APDUs:

```cpp
dlms::profile::ApduChannelOptions options =
  dlms::profile::DefaultApduChannelOptions();
options.hdlcUseSession = true;
options.hdlcRole = dlms::profile::HdlcProfileRole::Client;
options.hdlcRetryCount = 3;
options.hdlcRetryDelayMilliseconds = 10;

dlms::profile::HdlcProfileChannel channel(stream, options);
channel.Open();
channel.ConnectDataLink();
channel.SendApdu(dlms::profile::ProfileByteView{apdu, apduSize});
```

In session mode `SendApdu()` waits for HDLC acknowledgement and retries the
last outbound frame on retryable receive statuses until the configured retry
limit is reached.

All examples pass APDU bytes as opaque payload. Use `dlms-apdu` above this
layer when ACSE or xDLMS parsing is required.

The C API supports both existing C++ lower-layer interface pointers and pure C
callback transports. Use the callback constructors when the lower byte stream
or datagram endpoint is owned by C code.
