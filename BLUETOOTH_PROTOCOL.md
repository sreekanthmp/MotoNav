# MotoNav Bluetooth LE protocol, version 1

**Status.** The firmware side is **implemented** and **host-tested**: the frame codec, the
receiver and its sequence rules, and the path from received bytes to what the display shows
(77 C++ and 93 Python tests, including fuzzing and a cross-check against an independent
implementation). The Bluetooth radio code is **compile-checked only**. **It has never run over a
real Bluetooth link**, because no radio hardware was available. The phone side is a test sender
for Android (`android/`, Phase 3): its protocol code is **host-tested** against the same vectors
(138 Kotlin tests); its Bluetooth adapter is **compile-checked only** and has never run on a phone.
The real navigation app is not implemented (Phases 4 and 5). Where this document says what the firmware "does", that is what the
tested logic does; how a real phone and the real radio behave is unknown until the device tests
in [TESTING.md](TESTING.md) sections 5 and 6 are run.

## 1. Roles

| | Role |
|---|---|
| Phone (Android app, later) | BLE **central**, GATT **client**. Does all navigation. Sends messages. |
| MotoNav (ESP32) | BLE **peripheral**, GATT **server**. Receives messages, shows them, reports status. |

Messages flow **phone to device**. The device sends back only a small status value.

## 2. GATT layout

Device name: `MotoNav`. It advertises the service UUID, so a phone can scan with a filter on it.

| Item | UUID | Properties | Direction | Value |
|---|---|---|---|---|
| **Service** | `0ce668c8-034a-476c-9c79-4ed464204496` | | | |
| **NAV_DATA** characteristic | `6ae1b797-992f-409f-80b9-9eea3593fc4b` | Write, Write Without Response | phone to device | one frame (section 4), 6 to 53 bytes |
| **STATUS** characteristic | `664be4dd-4733-45a1-9820-496f5923a0ba` | Read, Notify | device to phone | 8 bytes (section 8) |

These UUIDs were generated randomly for this project; they are not derived from anything. Do not
change them once phones are using them.

## 3. Connection rules

- **One phone at a time** (the firmware is built with a single BLE connection). A second phone
  cannot connect while one is connected.
- **MTU.** The longest frame is 53 bytes, so the phone must negotiate an ATT MTU of **at least
  56** (53 plus 3 bytes of ATT overhead) before sending. The firmware asks for 247 but cannot
  force the phone to; on Android call `requestMtu(247)` and wait for its callback. At the default
  MTU of 23 only 20 bytes fit in a write, and longer writes are not guaranteed to arrive intact.
  A truncated frame is simply rejected (`BadLength`), so this fails safely. The firmware prints
  the negotiated MTU, and warns if it is too small.
- **Write type.** Use Write (with response) for navigation updates: at about one update a
  second the acknowledgement costs nothing and reports lost writes. Write Without Response is
  also accepted.
- **Rate.** About one update a second is expected (Google's turn-by-turn feed delivers about one
  a second). The firmware queues up to 8 writes; beyond that, writes are dropped and counted as
  `QueueOverflow`.
- **Sequence numbers are per connection.** They restart with every new connection.
- **On connect:** the firmware discards its navigation state and anything still queued, and shows
  the Ready screen. A frame written in the instant of connecting may be discarded, so send the
  current state again after the MTU exchange.
- **On disconnect:** the firmware shows the Disconnected screen, discards its state, and starts
  advertising again.
- **No pairing or encryption** in this version. See section 11.

## 4. Frame format

All multi-byte numbers are **little-endian**.

| Offset | Size | Field | Value |
|---|---|---|---|
| 0 | 1 | version | `1` |
| 1 | 1 | type | `0x01` = NAV_UPDATE (the only type so far) |
| 2 | 1 | sequence | 0 to 255, wraps around |
| 3 | 1 | length | number of payload bytes |
| 4 | *length* | payload | see section 5 |
| 4+*length* | 2 | CRC | CRC-16/CCITT-FALSE over every byte before it |

**CRC-16/CCITT-FALSE:** polynomial `0x1021`, initial value `0xFFFF`, no bit reflection, no final
XOR. The CRC of the ASCII text `123456789` is `0x29B1`.

A frame is at most 64 bytes; anything longer is rejected without being read.

## 5. NAV_UPDATE payload (type `0x01`)

The payload is 16 bytes plus the road name, so *length* = 16 + road-name length.

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 1 | mode | `0` Ready, `1` Navigating, `2` Rerouting, `3` Arrived |
| 1 | 1 | maneuver | next maneuver, `0` to `10` (table below) |
| 2 | 1 | roundabout exit | exit number, `0` = unknown, at most `99`. The display draws exits 1 to 9. |
| 3 | 4 | distance to maneuver | metres. `0xFFFFFFFF` = unknown |
| 7 | 4 | remaining distance | metres to the destination. `0xFFFFFFFF` = unknown |
| 11 | 4 | remaining time | seconds to the destination. `0xFFFFFFFF` = unknown |
| 15 | 1 | road-name length | 0 to 31 |
| 16 | *road length* | road name | UTF-8 text, **no NUL bytes**, not NUL-terminated |

**Maneuver values** (these are the numeric values of the firmware's `Maneuver` enum; a compile-time
check stops the two from drifting apart):

| Value | Maneuver | Value | Maneuver |
|---|---|---|---|
| 0 | Straight | 6 | Sharp right |
| 1 | Turn left | 7 | U-turn left |
| 2 | Turn right | 8 | U-turn right |
| 3 | Slight left | 9 | Roundabout |
| 4 | Slight right | 10 | Arrived |
| 5 | Sharp left | | |

**Notes**

- *Navigation stopped* is sent as mode `0` (Ready). *Rerouting* is mode `2` and its maneuver is
  ignored. *Arrived* is mode `3`.
- Remaining distance and remaining time are carried and stored, but **the display does not draw
  them yet**. The phone converts remaining time to a clock ETA itself if one is ever wanted,
  because the ESP32 has no clock.
- U-turns carry a side, because the Google SDK distinguishes clockwise from counterclockwise and
  they mirror between left- and right-hand traffic. Which Google constant maps to left or right in
  each driving side has to be verified on a real route (Phase 4).
- Road names that the display's font cannot draw (anything outside printable ASCII) are shown as
  `?`, or hidden if nothing readable is left. The bytes are carried as sent.
- The road name must not contain a NUL byte, because it would silently cut the name short.

### Worked example

TURN LEFT in 450 m on "Main Street", 12 km and 30 minutes left, sequence 2:

```text
01 01 02 1B   01 01 00   C2 01 00 00   E0 2E 00 00   08 07 00 00   0B   4D 61 69 6E 20 53 74 72 65 65 74   FF 56
```

| Bytes | Meaning |
|---|---|
| `01` | version 1 |
| `01` | type NAV_UPDATE |
| `02` | sequence 2 |
| `1B` | payload length 27 (16 + 11) |
| `01` | mode: Navigating |
| `01` | maneuver: Turn left |
| `00` | roundabout exit: unknown / not applicable |
| `C2 01 00 00` | distance to maneuver: `0x000001C2` = 450 m |
| `E0 2E 00 00` | remaining distance: `0x00002EE0` = 12,000 m |
| `08 07 00 00` | remaining time: `0x00000708` = 1,800 s |
| `0B` | road-name length 11 |
| `4D 61 69 6E 20 53 74 72 65 65 74` | "Main Street" |
| `FF 56` | CRC = `0x56FF` |

## 6. Validation

A frame is checked in this order, and the first failure is the result. Nothing in the firmware's
state changes unless every check passes.

| # | Check | Error |
|---|---|---|
| 1 | at least 6 bytes | `TooShort` (1) |
| 2 | at most 64 bytes | `BadLength` (3) |
| 3 | version equals 1 | `BadVersion` (2) |
| 4 | 6 + *length* equals the frame size (catches truncation and trailing bytes) | `BadLength` (3) |
| 5 | CRC matches | `BadCrc` (4) |
| 6 | type equals `0x01` | `UnknownType` (5) |
| 7 | payload is at least 16 bytes; road-name length is at most 31 and matches the payload; mode at most 3; maneuver at most 10; exit at most 99; no NUL in the road name | `BadPayload` (6) |

The CRC is checked **before** the contents are interpreted, so a damaged frame is never partly
believed. Two further results come from the sequence rules and the device:

| Code | Name | Meaning |
|---|---|---|
| 7 | `StaleSequence` | a valid frame older than one already accepted |
| 8 | `QueueOverflow` | dropped before parsing because the receive queue was full |

A write longer than 64 bytes is counted as `BadLength`.

## 7. Sequence rules

After a connect (or reset) the **first valid frame is always accepted**. After that, comparing the
new sequence with the last accepted one, modulo 256 (so 255 followed by 0 counts as newer):

| Frame is | Result |
|---|---|
| the same sequence | **Duplicate**: ignored, nothing changes, counted |
| 1 to 127 ahead | **Accepted**: newer. Skipped numbers (lost frames) are fine. |
| 128 to 255 ahead (that is, older) | **Stale**: rejected, counted |
| the **third** stale frame in a row | **Accepted as a resync**: the phone restarted its counter. Its sequence becomes the new baseline. |

A newer or duplicate valid frame in between resets the run of stale frames. Invalid frames neither
count nor reset it.

The phone should simply increase the sequence by 1 for every frame it sends on a connection.

## 8. STATUS characteristic

Eight bytes, little-endian. Readable at any time. The firmware also sends a notification after
every write it handles (and when a connection starts), if the phone has enabled notifications.

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | protocol version supported (`1`) |
| 1 | 1 | sequence of the last accepted frame |
| 2 | 1 | last error: the code of the most recent rejected frame (section 6), `0` if none |
| 3 | 1 | flags; bit 0 set once at least one frame has been accepted |
| 4 | 2 | frames accepted (wraps at 65,536) |
| 6 | 2 | frames rejected (wraps at 65,536), including stale frames and frames dropped unread |

This is how a phone, or a tester using a generic BLE app, sees whether a message was accepted.

## 9. Versioning

- The version byte is `1`. A frame with any other version is rejected (`BadVersion`) before
  anything else is looked at.
- Any change to the meaning or layout of an existing field needs a new version number.
- A new kind of message gets a new type value. Types `0x02` to `0x7F` are reserved for future use;
  today any type other than `0x01` is rejected (`UnknownType`).
- Not implemented yet, by design: a heartbeat message and stale-data detection (Phase 5), and any
  device-to-phone message other than STATUS.

## 10. Test vectors and tools

- [`protocol/vectors.txt`](protocol/vectors.txt): 45 frames (23 valid, 22 invalid, covering every
  error class), each with the exact result a receiver must give. Generated by
  [`protocol/frame_tool.py`](protocol/frame_tool.py).
- `frame_tool.py` is an **independent implementation** of this format in Python (standard library
  only). The firmware's C++ codec and the tool are checked against the same vectors and against each
  other on 3,000 random frames, so a disagreement is caught by the tests. **The Android app must
  pass the same vectors.** (The Kotlin code in `android/protocol` does: it decodes all 45 vectors as stated and re-encodes the 23 valid ones byte for byte.)
- [`protocol/scenarios.txt`](protocol/scenarios.txt): the 12 simulated navigation messages and 5
  deliberately wrong ones, as the Python tool makes them. The Android code builds the same messages
  independently and a test compares them byte for byte.
- `python protocol/frame_tool.py scenarios` prints ready-to-send messages for testing with a
  generic BLE app (see [TESTING.md](TESTING.md) section 5).
- `python protocol/frame_tool.py encode ...` and `decode ...` build and check single frames.

## 11. Security and known limitations

- **No pairing, bonding or encryption.** Anyone within radio range can connect and write
  navigation data, and can read the status. For a motorcycle accessory this is a real concern for a
  commercial product, not just a prototype detail. It is not addressed in Phase 2.
- **No stale-data detection.** If the phone stops sending, the display keeps showing the last
  state. A timeout and a heartbeat belong to Phase 5.
- **One phone at a time**, and the MTU must be negotiated by the phone (section 3).
- **The radio code is untested on hardware**: advertising, connecting, MTU negotiation, write
  callbacks, notifications, reconnecting after a drop, and the interplay with the display and the
  serial port have never run.
- Android BLE behaves differently between phone makers (background limits, connection
  parameters). That is a Phase 3 and Phase 5 concern.
