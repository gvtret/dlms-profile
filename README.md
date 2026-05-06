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

