# Profile Test Plan

## Unit Tests

- Status mapping from transport, wrapper, LLC, and HDLC statuses.
- Invalid pointer and zero-size argument handling.
- Wrapper/TCP send writes one encoded WPDU.
- Wrapper/TCP receive returns APDU after split stream chunks.
- Wrapper/UDP send writes one datagram.
- Wrapper/UDP receive decodes one full WPDU datagram.
- Wrapper/UDP receive rejects truncated or malformed datagrams.
- HDLC/LLC send writes an HDLC frame carrying standard LLC APDU payload.
- HDLC/LLC receive decodes HDLC frame and LLC LPDU into APDU bytes.
- Caller-provided receive buffers report `OutputBufferTooSmall`.
- C header compiles as C.

## Root Integration Tests

Root tests should prove cross-repository contracts only:

- APDU GET request survives Wrapper/TCP profile channel.
- APDU GET request survives Wrapper/UDP profile channel.
- APDU AARQ or GET request survives HDLC/LLC profile channel.
- APDU decoding is used only at the assertion boundary.

## Commands

```text
cmake -S E:\work\dlms -B E:\work\dlms\build-codex
cmake --build E:\work\dlms\build-codex
ctest --test-dir E:\work\dlms\build-codex
```

