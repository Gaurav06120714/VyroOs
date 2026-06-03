# Vyro Compositor IPC Protocol (v1)

Apps talk to `vyro-compositor` over a UNIX **seqpacket** socket at
`/run/vyro/compositor.sock`. Every message is one packet: a 16-byte
header followed by an optional payload, with optional `SCM_RIGHTS` fd
passing.

## Header (16 bytes, little-endian)

| Offset | Size | Field    | Notes |
|--------|------|----------|-------|
| 0      | 4    | magic    | `0x56594F50` ("VYOP") |
| 4      | 2    | op       | opcode (see below) |
| 6      | 2    | flags    | bit 0 = fd attached via `SCM_RIGHTS` |
| 8      | 4    | length   | payload length in bytes |
| 12     | 4    | serial   | request serial; reply echoes it |

## Opcodes

| Opcode | Direction | Payload | Meaning |
|--------|-----------|---------|---------|
| `0x0001` HELLO          | C→S | `vyro_msg_hello_t` | handshake |
| `0x0010` WINDOW_CREATE  | C→S | `vyro_msg_window_create_t` | create a window |
| `0x0011` WINDOW_DESTROY | C→S | `vyro_msg_window_id_t` | destroy |
| `0x0012` WINDOW_PRESENT | C→S | `vyro_msg_present_t` + memfd via SCM_RIGHTS | flip the backbuffer |
| `0x0013` WINDOW_TITLE   | C→S | `vyro_msg_window_id_t` + utf8 | change title |
| `0x8001` HELLO_OK       | S→C | `vyro_msg_hello_ok_t` | handshake reply |
| `0x8010` WINDOW_OK      | S→C | `vyro_msg_window_ok_t` | window id assigned |
| `0x8100` EVENT          | S→C | `vyro_event_t` | input / lifecycle event |
| `0x8FFF` ERROR          | S→C | `vyro_msg_error_t` | error response |

## Handshake

1. Client connects to `/run/vyro/compositor.sock`.
2. Client sends `HELLO { protocol_version: 1, client_pid }`.
3. Server replies `HELLO_OK { protocol_version, session_id, screen_w, screen_h }`.
4. If protocol_version mismatch, server replies `ERROR { code: EPROTONOSUPPORT }` and closes.

## Framebuffer model

Clients allocate a `memfd` sized `stride * height`, draw into it (mmap),
and send `WINDOW_PRESENT` with the memfd attached via `SCM_RIGHTS`. The
compositor mmaps the memfd, blits into the screen buffer, and closes its
copy of the fd. The client may reuse the same memfd for the next frame.

Pixel format is `BGRX8888` (Vyro's standard) — same as the microkernel
libvyro. The first phase is single-buffered; double-buffering with a fd
swap lands in vB.0.3.

## Why seqpacket

Message framing is guaranteed (no partial reads), and `SCM_RIGHTS` works
the same as on stream sockets. Compared to stream + length-prefixed
framing, the client/server code is simpler and we get one syscall per
message instead of two.

## Versioning

Bump `VYRO_PROTO_VERSION` on any field-layout change. The compositor
refuses connections from a higher version client. Backwards-compatible
extensions (new opcodes, new high-bit flags) are allowed without a bump.
