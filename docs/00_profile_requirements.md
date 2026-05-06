# Profile Layer Requirements

## Scope

`dlms-profile` turns DLMS/COSEM lower profile transports into APDU channels.
The profile layer accepts caller-owned APDU bytes and returns received APDU
bytes without inspecting the APDU content.

## In Scope

- Wrapper/TCP APDU send and receive.
- Wrapper/UDP APDU send and receive.
- HDLC + LLC APDU send and receive.
- Optional HDLC data-link session connect/disconnect, I-frame sequence
  validation, RR generation, and segmented Information reassembly.
- HDLC session retry policy for SNRM/DISC/I-frame acknowledgement timeouts or
  would-block reads, bounded by caller options.
- Window-aware HDLC session send so long APDUs are split into separately
  sequenced I-frames and acknowledged before the send completes.
- Status-code-only public runtime API.
- C++11 API.
- Stable C ABI over opaque channel handles.
- GoogleTest unit coverage and root integration tests.

## Out Of Scope

- ACSE or xDLMS APDU parsing.
- Association state machine.
- Security, ciphering, authentication, or key handling.
- APDU request correlation or invoke-id handling.
- COSEM object model and access decisions.
- Transport endpoint ownership beyond calling the supplied transport interface.

## Documentation Basis

`doc-rag-remote` confirms these profile boundaries:

- Wrapper transports an APDU in the WPDU `Data` field and carries `Data_Length`.
- TCP is streaming, so the wrapper length field is needed to detect APDU
  boundaries.
- UDP sends one complete WPDU in one datagram.
- HDLC based LLC transports ACSE and xDLMS APDUs transparently for the
  application layer.
- The connection-oriented HDLC profile provides DL-CONNECT services and
  transparent reliable/segmented DL-DATA service to the application layer.

## Success Criteria

- APDU bytes are byte-identical after crossing a profile channel.
- TCP receive handles split WPDU input.
- UDP receive rejects malformed or truncated WPDU datagrams.
- HDLC/LLC receive returns the LLC LSDU as APDU bytes.
- HDLC session mode performs SNRM/UA before APDU transfer and reassembles
  segmented HDLC Information before LLC decode.
- HDLC session send completes only after the peer acknowledges the emitted
  I-frame sequence or an error/retry limit is reached.
- Runtime API paths do not throw exceptions.
