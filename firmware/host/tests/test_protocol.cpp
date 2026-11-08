// BLE protocol v1: codec, receiver, status, log lines, and the path from received
// bytes to what the display shows. HOST-tested; no Bluetooth radio is involved.
#include <stdio.h>
#include <string.h>

#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "protocol/ble_log.h"
#include "protocol/crc16.h"
#include "protocol/frame.h"
#include "protocol/receiver.h"
#include "test_framework.h"
#include "ui/view.h"

using namespace motonav;

namespace {

typedef std::vector<uint8_t> Bytes;

// --- shared vectors (produced by the independent Python implementation) -------------------

struct Vector {
  std::string name;
  Bytes frame;
  std::string result;  // "ok" or an error name
  std::string fields;  // "seq=1 mode=0 ..." (ok only)
};

Bytes parseHex(const std::string& text) {
  Bytes out;
  if (text == "-") return out;
  for (size_t i = 0; i + 1 < text.size(); i += 2) {
    out.push_back(static_cast<uint8_t>(strtoul(text.substr(i, 2).c_str(), nullptr, 16)));
  }
  return out;
}

std::string trim(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

std::vector<Vector> loadVectors() {
  std::vector<Vector> out;
  std::ifstream in("../../protocol/vectors.txt");
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::vector<std::string> parts;
    size_t start = 0;
    for (int i = 0; i < 3; ++i) {
      size_t semi = line.find(" ; ", start);
      if (semi == std::string::npos) {
        parts.push_back(line.substr(start));
        start = std::string::npos;
        break;
      }
      parts.push_back(line.substr(start, semi - start));
      start = semi + 3;
    }
    if (start != std::string::npos) parts.push_back(line.substr(start));
    Vector v;
    v.name = trim(parts[0]);
    v.frame = parseHex(trim(parts[1]));
    v.result = trim(parts[2]);
    if (parts.size() > 3) v.fields = parts[3];
    out.push_back(v);
  }
  return out;
}

// "key=value" lookup in a fields string. `road=` runs to the end of the line.
std::string field(const std::string& fields, const std::string& key) {
  const std::string tag = key + "=";
  size_t at = fields.find(tag);
  // `road=` is last and may contain spaces; other keys end at the next space.
  if (at == std::string::npos) return "<missing>";
  size_t from = at + tag.size();
  if (key == "road") return fields.substr(from);
  size_t end = fields.find(' ', from);
  return fields.substr(from, end == std::string::npos ? std::string::npos : end - from);
}

std::string unescapeRoad(const std::string& text) {
  std::string out;
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\\' && i + 3 < text.size() && text[i + 1] == 'x') {
      out.push_back(static_cast<char>(strtoul(text.substr(i + 2, 2).c_str(), nullptr, 16)));
      i += 3;
    } else {
      out.push_back(text[i]);
    }
  }
  return out;
}

uint32_t numberOrUnknown(const std::string& s) {
  return s == "unknown" ? kUnknownU32 : static_cast<uint32_t>(strtoul(s.c_str(), nullptr, 10));
}

// --- helpers -------------------------------------------------------------------------------

NavUpdate makeUpdate(NavMode mode, Maneuver maneuver, uint32_t distance, const char* road,
                     uint8_t exitNumber = 0, uint32_t remaining = kUnknownU32,
                     uint32_t seconds = kUnknownU32) {
  NavUpdate u;
  u.nav.mode = mode;
  u.nav.maneuver = maneuver;
  u.nav.distanceToManeuverM = distance;
  u.nav.roundaboutExit = exitNumber;
  setRoadName(u.nav, road);
  u.extras.remainingDistanceM = remaining;
  u.extras.remainingSeconds = seconds;
  return u;
}

Bytes encode(const NavUpdate& u, uint8_t seq) {
  uint8_t buf[kMaxFrameBytes];
  const size_t n = encodeNavUpdate(u, seq, buf, sizeof(buf));
  return Bytes(buf, buf + n);
}

Bytes goodFrame(uint8_t seq = 1) {
  return encode(makeUpdate(NavMode::Navigating, Maneuver::TurnLeft, 450, "Main Street", 0, 12000, 1800), seq);
}

ProtocolError decodeBytes(const Bytes& b, uint8_t* seq = nullptr, NavUpdate* u = nullptr) {
  uint8_t s = 0;
  NavUpdate tmp;
  return decodeNavUpdate(b.data(), b.size(), seq ? seq : &s, u ? u : &tmp);
}

bool sameNav(const NavSnapshot& a, const NavSnapshot& b) {
  return a.mode == b.mode && a.maneuver == b.maneuver && a.distanceToManeuverM == b.distanceToManeuverM &&
         a.roundaboutExit == b.roundaboutExit && strcmp(a.roadName, b.roadName) == 0;
}

// Small deterministic pseudo-random generator, so a failure can be reproduced.
struct Lcg {
  uint32_t state;
  explicit Lcg(uint32_t seed) : state(seed) {}
  uint32_t next() {
    state = state * 1664525u + 1013904223u;
    return state >> 8;
  }
  uint8_t byte() { return static_cast<uint8_t>(next()); }
};

void setCrc(Bytes& frame) {
  const uint16_t crc = crc16CcittFalse(frame.data(), frame.size() - 2);
  frame[frame.size() - 2] = static_cast<uint8_t>(crc);
  frame[frame.size() - 1] = static_cast<uint8_t>(crc >> 8);
}

}  // namespace

// ------------------------------------- CRC ---------------------------------------------------

TEST(crc16_matches_published_check_value) {
  const char* text = "123456789";
  CHECK_INT(crc16CcittFalse(reinterpret_cast<const uint8_t*>(text), 9), 0x29B1);
  CHECK_INT(crc16CcittFalse(nullptr, 0), 0xFFFF);
  const uint8_t a[] = {0x00};
  CHECK_INT(crc16CcittFalse(a, 1), 0xE1F0);  // CRC-16/CCITT-FALSE of a single zero byte
}

TEST(crc16_can_be_chained) {
  const uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8};
  const uint16_t whole = crc16CcittFalse(data, 8);
  CHECK_INT(crc16CcittFalse(data + 3, 5, crc16CcittFalse(data, 3)), whole);
}

// ---------------------------- shared vectors (cross-implementation) --------------------------

TEST(shared_vectors_decode_exactly_as_the_independent_reference_says) {
  const std::vector<Vector> vectors = loadVectors();
  CHECK(vectors.size() >= 40);
  std::set<std::string> errorsSeen;
  int okCount = 0;
  for (const Vector& v : vectors) {
    uint8_t seq = 0;
    NavUpdate u;
    const ProtocolError error = decodeBytes(v.frame, &seq, &u);
    if (v.result != "ok") {
      if (v.result != protocolErrorName(error)) {
        printf("    vector %s: expected %s, firmware codec says %s\n", v.name.c_str(), v.result.c_str(),
               protocolErrorName(error));
      }
      CHECK_STR(protocolErrorName(error), v.result.c_str());
      errorsSeen.insert(v.result);
      continue;
    }
    ++okCount;
    CHECK(error == ProtocolError::None);
    CHECK_INT(seq, strtol(field(v.fields, "seq").c_str(), nullptr, 10));
    CHECK_INT(static_cast<int>(u.nav.mode), strtol(field(v.fields, "mode").c_str(), nullptr, 10));
    CHECK_INT(static_cast<int>(u.nav.maneuver), strtol(field(v.fields, "maneuver").c_str(), nullptr, 10));
    CHECK_INT(u.nav.roundaboutExit, strtol(field(v.fields, "exit").c_str(), nullptr, 10));
    CHECK_INT(u.nav.distanceToManeuverM, numberOrUnknown(field(v.fields, "dist")));
    CHECK_INT(u.extras.remainingDistanceM, numberOrUnknown(field(v.fields, "remaining")));
    CHECK_INT(u.extras.remainingSeconds, numberOrUnknown(field(v.fields, "seconds")));
    CHECK_STR(u.nav.roadName, unescapeRoad(field(v.fields, "road")).c_str());

    // The firmware's encoder must reproduce the reference's bytes exactly.
    const Bytes again = encode(u, seq);
    if (again != v.frame) printf("    vector %s: the C++ encoder produced different bytes\n", v.name.c_str());
    CHECK(again == v.frame);
  }
  CHECK(okCount >= 20);  // the file has 23 valid vectors; this catches a truncated file
  // Every error class must be covered by at least one vector.
  for (const char* e : {"TooShort", "BadVersion", "BadLength", "BadCrc", "UnknownType", "BadPayload"}) {
    CHECK(errorsSeen.count(e) == 1);
  }
}

// ------------------------------------ codec properties ---------------------------------------

TEST(codec_round_trips_every_maneuver_and_mode) {
  for (int mode = 0; mode <= 3; ++mode) {
    for (int man = 0; man < kManeuverCount; ++man) {
      const NavUpdate in = makeUpdate(static_cast<NavMode>(mode), static_cast<Maneuver>(man), 123, "Road", 2, 5000, 600);
      const Bytes frame = encode(in, 77);
      CHECK(frame.size() == 4 + 16 + 4 + 2);
      uint8_t seq = 0;
      NavUpdate out;
      CHECK(decodeBytes(frame, &seq, &out) == ProtocolError::None);
      CHECK_INT(seq, 77);
      CHECK(sameNav(in.nav, out.nav));
      CHECK_INT(out.extras.remainingDistanceM, 5000);
      CHECK_INT(out.extras.remainingSeconds, 600);
    }
  }
}

TEST(codec_size_limits) {
  // The longest legal frame is 4 + 16 + 31 + 2 = 53 bytes.
  const Bytes longest = encode(makeUpdate(NavMode::Navigating, Maneuver::Straight, 1, std::string(31, 'x').c_str()), 1);
  CHECK_INT(longest.size(), 53);
  CHECK(decodeBytes(longest) == ProtocolError::None);

  uint8_t small[10];
  CHECK_INT(encodeNavUpdate(NavUpdate(), 1, small, sizeof(small)), 0);  // buffer too small
  NavUpdate bad = makeUpdate(NavMode::Navigating, Maneuver::Straight, 1, "");
  bad.nav.roundaboutExit = 100;
  uint8_t buf[kMaxFrameBytes];
  CHECK_INT(encodeNavUpdate(bad, 1, buf, sizeof(buf)), 0);  // exit above 99 cannot be represented
}

TEST(every_single_bit_flip_is_detected) {
  for (uint8_t seq : {static_cast<uint8_t>(0), static_cast<uint8_t>(1), static_cast<uint8_t>(255)}) {
    const Bytes frame = goodFrame(seq);
    for (size_t byte = 0; byte < frame.size(); ++byte) {
      for (int bit = 0; bit < 8; ++bit) {
        Bytes damaged = frame;
        damaged[byte] = static_cast<uint8_t>(damaged[byte] ^ (1u << bit));
        if (decodeBytes(damaged) == ProtocolError::None) {
          printf("    flipping bit %d of byte %zu was not detected\n", bit, byte);
          CHECK(false);
        }
      }
    }
  }
  CHECK(true);
}

TEST(every_truncation_and_extension_is_rejected) {
  const Bytes frame = goodFrame();
  for (size_t n = 0; n < frame.size(); ++n) {
    Bytes cut(frame.begin(), frame.begin() + static_cast<long>(n));
    CHECK(decodeBytes(cut) != ProtocolError::None);
  }
  for (int extra = 1; extra <= 12; ++extra) {
    Bytes longer = frame;
    longer.insert(longer.end(), static_cast<size_t>(extra), 0);
    CHECK(decodeBytes(longer) != ProtocolError::None);
  }
}

TEST(validation_order_is_as_documented) {
  // Too short beats everything; too long is reported as a length problem.
  CHECK(decodeBytes(Bytes()) == ProtocolError::TooShort);
  CHECK(decodeBytes(Bytes(5, 0xFF)) == ProtocolError::TooShort);
  CHECK(decodeBytes(Bytes(65, 0)) == ProtocolError::BadLength);
  // A wrong version is reported before the length or CRC are looked at.
  Bytes v2 = goodFrame();
  v2[0] = 2;
  CHECK(decodeBytes(v2) == ProtocolError::BadVersion);
  // ...even when the length is wrong too (this is what distinguishes the two orders).
  Bytes versionAndLength = goodFrame();
  versionAndLength[0] = 2;
  versionAndLength[3] = static_cast<uint8_t>(versionAndLength[3] + 5);
  CHECK(decodeBytes(versionAndLength) == ProtocolError::BadVersion);
  // A wrong length is reported before the CRC.
  Bytes len = goodFrame();
  len[3] = static_cast<uint8_t>(len[3] + 1);
  CHECK(decodeBytes(len) == ProtocolError::BadLength);
  // The CRC is checked before the type: a damaged unknown-type frame is BadCrc.
  Bytes type = goodFrame();
  type[1] = 9;
  CHECK(decodeBytes(type) == ProtocolError::BadCrc);
  setCrc(type);
  CHECK(decodeBytes(type) == ProtocolError::UnknownType);
}

TEST(random_garbage_is_rejected_safely) {
  // ASan and UBSan watch this test: no read past the end, no crash, whatever the bytes are.
  // Random bytes are essentially never a valid frame (the structured fuzz below reaches the
  // accepting paths), but if one ever were, it must still re-encode to the same bytes.
  Lcg rng(12345);
  int accepted = 0;
  for (int i = 0; i < 300000; ++i) {
    Bytes data(rng.next() % 71);
    for (uint8_t& b : data) b = rng.byte();
    if (!data.empty() && (rng.next() & 3) == 0) data[0] = kProtocolVersion;  // reach deeper checks
    uint8_t seq = 0;
    NavUpdate u;
    if (decodeBytes(data, &seq, &u) == ProtocolError::None) {
      ++accepted;
      CHECK(encode(u, seq) == data);
    }
  }
  printf("    (%d of 300000 random buffers happened to be valid)\n", accepted);
  CHECK(true);
}

TEST(structured_fuzz_matches_an_independent_oracle) {
  // Frames with a correct version, length and CRC but random payload bytes: the decoder
  // must accept exactly those the field rules allow.
  Lcg rng(777);
  int accepted = 0, rejected = 0;
  for (int i = 0; i < 200000; ++i) {
    const size_t payloadLength = rng.next() % 56;
    Bytes frame(kFrameHeaderBytes + payloadLength + kFrameCrcBytes);
    frame[0] = kProtocolVersion;
    frame[1] = (rng.next() % 8 == 0) ? rng.byte() : kMsgNavUpdate;
    frame[2] = rng.byte();
    frame[3] = static_cast<uint8_t>(payloadLength);
    for (size_t k = 0; k < payloadLength; ++k) frame[4 + k] = rng.byte();
    if (payloadLength >= 16 && rng.next() % 3 != 0) {  // often make the road length consistent
      const size_t roadLength = payloadLength - 16;
      frame[4 + 15] = static_cast<uint8_t>(roadLength);
      if (rng.next() % 2) frame[4] = rng.byte() % 4;          // mode mostly valid
      if (rng.next() % 2) frame[5] = rng.byte() % 11;         // maneuver mostly valid
      if (rng.next() % 2) frame[6] = rng.byte() % 100;        // exit mostly valid
      if (rng.next() % 2) for (size_t k = 0; k < roadLength; ++k) frame[4 + 16 + k] = 'a' + rng.byte() % 26;
    }
    setCrc(frame);

    // The oracle, written from the specification and sharing no code with the decoder.
    ProtocolError expected = ProtocolError::None;
    if (frame[1] != kMsgNavUpdate) {
      expected = ProtocolError::UnknownType;
    } else if (payloadLength < 16) {
      expected = ProtocolError::BadPayload;
    } else {
      const size_t roadLength = frame[4 + 15];
      bool nul = false;
      for (size_t k = 0; k < roadLength && 16 + k < payloadLength; ++k) nul = nul || frame[4 + 16 + k] == 0;
      if (roadLength > 31 || 16 + roadLength != payloadLength || frame[4] > 3 || frame[5] > 10 || frame[6] > 99 || nul) {
        expected = ProtocolError::BadPayload;
      }
    }
    uint8_t seq = 0;
    NavUpdate decoded;
    const ProtocolError actual = decodeBytes(frame, &seq, &decoded);
    if (actual == ProtocolError::None && encode(decoded, seq) != frame) {
      printf("    iteration %d: an accepted frame did not re-encode to the same bytes\n", i);
      CHECK(false);
      return;
    }
    if (actual != expected) {
      printf("    iteration %d: oracle says %s, decoder says %s\n", i, protocolErrorName(expected), protocolErrorName(actual));
      CHECK(actual == expected);
      return;
    }
    (expected == ProtocolError::None ? accepted : rejected)++;
  }
  printf("    (%d accepted, %d rejected)\n", accepted, rejected);
  CHECK(accepted > 5000 && rejected > 5000);  // the fuzzer exercised both sides
}

// ------------------------------------ receiver -----------------------------------------------

TEST(receiver_accepts_valid_frames_and_updates_state) {
  Receiver r;
  CHECK(r.snapshot().mode == NavMode::Ready);  // nothing received yet
  const Bytes f = goodFrame(5);
  CHECK(r.receive(f.data(), f.size()) == RxResult::Accepted);
  CHECK(r.snapshot().maneuver == Maneuver::TurnLeft);
  CHECK_INT(r.snapshot().distanceToManeuverM, 450);
  CHECK_STR(r.snapshot().roadName, "Main Street");
  CHECK_INT(r.extras().remainingDistanceM, 12000);
  CHECK_INT(r.extras().remainingSeconds, 1800);
  CHECK_INT(r.stats().accepted, 1);
  CHECK_INT(r.stats().lastAcceptedSequence, 5);
  CHECK(r.lastCallError() == ProtocolError::None);
}

TEST(receiver_rejections_never_change_the_state) {
  Receiver r;
  const Bytes f = goodFrame(5);
  r.receive(f.data(), f.size());
  const NavSnapshot before = r.snapshot();
  const ReceiverStats statsBefore = r.stats();

  Bytes crc = f;
  crc[8] ^= 0x40;
  CHECK(r.receive(crc.data(), crc.size()) == RxResult::Rejected);
  CHECK(r.lastCallError() == ProtocolError::BadCrc);
  Bytes cut(f.begin(), f.end() - 2);
  CHECK(r.receive(cut.data(), cut.size()) == RxResult::Rejected);
  CHECK(r.receive(nullptr, 0) == RxResult::Rejected);
  CHECK(r.lastCallError() == ProtocolError::TooShort);

  CHECK(sameNav(before, r.snapshot()));
  CHECK_INT(r.stats().accepted, statsBefore.accepted);
  CHECK_INT(r.stats().rejected, 3);
}

TEST(receiver_sequence_rules) {
  Receiver r;
  auto send = [&](uint8_t seq, uint32_t distance) {
    const Bytes f = encode(makeUpdate(NavMode::Navigating, Maneuver::Straight, distance, ""), seq);
    return r.receive(f.data(), f.size());
  };

  CHECK(send(10, 1000) == RxResult::Accepted);       // the first frame is always accepted
  CHECK(send(11, 990) == RxResult::Accepted);        // next
  CHECK(send(11, 123) == RxResult::Duplicate);       // same sequence: ignored
  CHECK_INT(r.snapshot().distanceToManeuverM, 990);  // ...and did not change anything
  CHECK(send(15, 900) == RxResult::Accepted);        // lost frames in between are fine
  CHECK(send(14, 555) == RxResult::Stale);           // older: rejected
  CHECK_INT(r.snapshot().distanceToManeuverM, 900);
  CHECK(r.lastCallError() == ProtocolError::StaleSequence);
  CHECK_INT(r.stats().duplicates, 1);
  CHECK_INT(r.stats().rejected, 1);
  CHECK_INT(r.stats().accepted, 3);

  // The counter wraps: 255 -> 0 is "newer".
  Receiver w;
  auto sendW = [&](uint8_t seq) {
    const Bytes f = encode(makeUpdate(NavMode::Ready, Maneuver::Straight, 0, ""), seq);
    return w.receive(f.data(), f.size());
  };
  CHECK(sendW(254) == RxResult::Accepted);
  CHECK(sendW(255) == RxResult::Accepted);
  CHECK(sendW(0) == RxResult::Accepted);
  CHECK(sendW(1) == RxResult::Accepted);
  CHECK(sendW(0) == RxResult::Stale);
  // 127 ahead is still newer, 128 ahead is older.
  Receiver h;
  auto sendH = [&](uint8_t seq) {
    const Bytes f = encode(makeUpdate(NavMode::Ready, Maneuver::Straight, 0, ""), seq);
    return h.receive(f.data(), f.size());
  };
  CHECK(sendH(0) == RxResult::Accepted);
  CHECK(sendH(127) == RxResult::Accepted);
  CHECK(sendH(127 + 128) == RxResult::Stale);
}

TEST(receiver_resync_behaviour) {
  Receiver r;
  auto send = [&](uint8_t seq, uint32_t distance) {
    const Bytes f = encode(makeUpdate(NavMode::Navigating, Maneuver::Straight, distance, ""), seq);
    return r.receive(f.data(), f.size());
  };
  CHECK(send(50, 1) == RxResult::Accepted);
  // The phone restarts its counter. 50 -> 0 is 206 ahead, i.e. older.
  CHECK(send(0, 100) == RxResult::Stale);
  CHECK(send(1, 99) == RxResult::Stale);
  CHECK_INT(r.snapshot().distanceToManeuverM, 1);          // still the old data
  CHECK(send(2, 98) == RxResult::Accepted);                // third in a row: resync
  CHECK_INT(r.snapshot().distanceToManeuverM, 98);
  CHECK_INT(r.stats().resyncs, 1);
  CHECK(send(3, 97) == RxResult::Accepted);                // and it continues normally
  CHECK_INT(r.stats().rejected, 2);

  // A valid newer frame in between breaks the run of stale frames.
  Receiver s;
  auto sendS = [&](uint8_t seq) {
    const Bytes f = encode(makeUpdate(NavMode::Ready, Maneuver::Straight, 0, ""), seq);
    return s.receive(f.data(), f.size());
  };
  CHECK(sendS(50) == RxResult::Accepted);
  CHECK(sendS(0) == RxResult::Stale);
  CHECK(sendS(1) == RxResult::Stale);
  CHECK(sendS(51) == RxResult::Accepted);   // newer: resets the stale count
  CHECK(sendS(0) == RxResult::Stale);
  CHECK(sendS(1) == RxResult::Stale);       // only two in a row again: not yet resynced
  CHECK_INT(s.stats().resyncs, 0);
}

TEST(receiver_reset_forgets_everything) {
  Receiver r;
  const Bytes f = goodFrame(200);
  r.receive(f.data(), f.size());
  r.reset();
  CHECK(r.snapshot().mode == NavMode::Ready);
  CHECK_STR(r.snapshot().roadName, "");
  CHECK_INT(r.stats().accepted, 0);
  CHECK(!r.stats().anyAccepted);
  // After a reset the next frame is accepted whatever its sequence number.
  const Bytes g = goodFrame(3);
  CHECK(r.receive(g.data(), g.size()) == RxResult::Accepted);
}

TEST(receiver_counts_frames_dropped_before_parsing) {
  Receiver r;
  r.recordDropped(ProtocolError::QueueOverflow);
  r.recordDropped(ProtocolError::BadLength);
  CHECK_INT(r.stats().rejected, 2);
  CHECK(r.stats().lastError == ProtocolError::BadLength);
  CHECK_INT(r.stats().accepted, 0);
}

TEST(receiver_fuzz_never_corrupts_state) {
  Receiver r;
  const Bytes f = goodFrame(9);
  r.receive(f.data(), f.size());
  Lcg rng(4242);
  for (int i = 0; i < 100000; ++i) {
    Bytes data(rng.next() % 70);
    for (uint8_t& b : data) b = rng.byte();
    NavSnapshot before = r.snapshot();
    const RxResult result = r.receive(data.data(), data.size());
    if (result != RxResult::Accepted) {
      CHECK(sameNav(before, r.snapshot()));  // anything not accepted leaves the state alone
    }
    // Whatever the state is, it must always be displayable and safe.
    const NavSnapshot& now = r.snapshot();
    CHECK(static_cast<int>(now.mode) <= 3 && static_cast<int>(now.maneuver) <= 10);
    CHECK(strlen(now.roadName) <= kRoadNameMax);
  }
}

// --------------------------- status characteristic and log lines ------------------------------

TEST(status_bytes) {
  ReceiverStats s;
  s.accepted = 0x0102;
  s.rejected = 0x0304;
  s.lastAcceptedSequence = 9;
  s.anyAccepted = true;
  s.lastError = ProtocolError::BadCrc;
  uint8_t out[kStatusBytes];
  encodeStatus(s, out);
  const uint8_t expected[kStatusBytes] = {1, 9, 4, 1, 0x02, 0x01, 0x04, 0x03};
  CHECK(memcmp(out, expected, kStatusBytes) == 0);

  ReceiverStats wrapped;
  wrapped.accepted = 65537;  // counters wrap at 16 bits on the wire
  encodeStatus(wrapped, out);
  CHECK_INT(out[4], 1);
  CHECK_INT(out[5], 0);
  encodeStatus(ReceiverStats(), out);
  CHECK_INT(out[3], 0);  // nothing accepted yet
}

TEST(ble_log_lines) {
  char line[160];
  formatBleBootLine(line, sizeof(line));
  CHECK_STR(line, "MotoNav firmware - Phase 2 (BLE, simulated navigation messages)");
  formatBleLinkLine(line, sizeof(line), true, 0);
  CHECK_STR(line, "ble: connected");
  formatBleLinkLine(line, sizeof(line), false, 531);
  CHECK_STR(line, "ble: disconnected (reason 531)");
  formatBleRejectLine(line, sizeof(line), RxResult::Rejected, ProtocolError::BadCrc, 22, 0);
  CHECK_STR(line, "ble: rejected BadCrc (22 bytes)");
  formatBleRejectLine(line, sizeof(line), RxResult::Stale, ProtocolError::StaleSequence, 26, 3);
  CHECK_STR(line, "ble: ignored stale (seq 3)");
  formatBleRejectLine(line, sizeof(line), RxResult::Duplicate, ProtocolError::None, 26, 7);
  CHECK_STR(line, "ble: ignored duplicate (seq 7)");
  formatBleRejectLine(line, sizeof(line), RxResult::Accepted, ProtocolError::None, 26, 7);
  CHECK_STR(line, "");

  ReceiverStats s;
  s.accepted = 12;
  s.lastError = ProtocolError::BadVersion;
  formatBleStatsLine(line, sizeof(line), true, s, 2);
  CHECK_STR(line, "[ble] connected=1 accepted=12 rejected=0 duplicates=0 resyncs=0 overflow=2 last_error=BadVersion");
}

TEST(every_protocol_error_has_a_distinct_name) {
  std::set<std::string> names;
  for (int e = 0; e <= 8; ++e) names.insert(protocolErrorName(static_cast<ProtocolError>(e)));
  CHECK_INT(names.size(), 9);
  CHECK(names.count("Unknown") == 0);
}

// ------------------------- from received bytes to the display --------------------------------

TEST(received_frames_drive_the_screens) {
  Receiver r;
  char text[200];
  auto show = [&](bool connected) {
    describeView(buildView(deviceStateFrom(connected, r)), text, sizeof(text));
    return std::string(text);
  };

  // Not connected: the Disconnected screen, whatever the receiver holds.
  CHECK_STR(show(false).c_str(), "screen=Disconnected");
  // Connected, nothing received: Ready.
  CHECK_STR(show(true).c_str(), "screen=Ready");

  Bytes f = encode(makeUpdate(NavMode::Navigating, Maneuver::TurnLeft, 450, "Main Street"), 1);
  r.receive(f.data(), f.size());
  CHECK_STR(show(true).c_str(), "screen=Navigating icon=TurnLeft distance=\"450 m\" road=\"Main Street\"");

  f = encode(makeUpdate(NavMode::Navigating, Maneuver::Roundabout, 1200, "Ring Road", 2), 2);
  r.receive(f.data(), f.size());
  CHECK_STR(show(true).c_str(), "screen=Navigating icon=Roundabout exit=2 distance=\"1.2 km\" road=\"Ring Road\"");

  // Invalid frames do not change what is shown.
  const std::string shown = show(true);
  Bytes bad = f;
  bad[10] ^= 1;
  r.receive(bad.data(), bad.size());
  CHECK_STR(show(true).c_str(), shown.c_str());

  f = encode(makeUpdate(NavMode::Rerouting, Maneuver::Straight, kUnknownU32, ""), 3);
  r.receive(f.data(), f.size());
  CHECK_STR(show(true).c_str(), "screen=Rerouting");
  f = encode(makeUpdate(NavMode::Arrived, Maneuver::Arrived, 0, ""), 4);
  r.receive(f.data(), f.size());
  CHECK_STR(show(true).c_str(), "screen=Arrived");
  f = encode(makeUpdate(NavMode::Ready, Maneuver::Straight, kUnknownU32, ""), 5);  // navigation stopped
  r.receive(f.data(), f.size());
  CHECK_STR(show(true).c_str(), "screen=Ready");

  // Losing the connection shows Disconnected; after reset and reconnect, Ready again.
  CHECK_STR(show(false).c_str(), "screen=Disconnected");
  r.reset();
  CHECK_STR(show(true).c_str(), "screen=Ready");
}

TEST(a_road_name_in_a_frame_is_cleaned_for_the_display) {
  Receiver r;
  const Bytes f = encode(makeUpdate(NavMode::Navigating, Maneuver::Straight, 100, "Caf\xC3\xA9 Road"), 1);
  r.receive(f.data(), f.size());
  const ScreenView v = buildView(deviceStateFrom(true, r));
  CHECK_STR(v.road, "Caf? Road");  // the font cannot draw the accent
}

TEST(mtu_log_line_warns_when_frames_cannot_fit) {
  char line[160];
  formatBleMtuLine(line, sizeof(line), 247);
  CHECK_STR(line, "ble: MTU 247");
  formatBleMtuLine(line, sizeof(line), 56);  // 53-byte frame + 3 bytes of ATT overhead
  CHECK_STR(line, "ble: MTU 56");
  formatBleMtuLine(line, sizeof(line), 23);  // the default before negotiation
  CHECK(strstr(line, "too small") != nullptr);
  CHECK(strstr(line, "at least 56") != nullptr);
  // The constant really is the longest frame the encoder can make.
  Bytes longest = encode(makeUpdate(NavMode::Navigating, Maneuver::Straight, 1, std::string(31, 'x').c_str()), 1);
  CHECK_INT(longest.size(), kMaxNavFrameBytes);
}

TEST(accepted_log_line) {
  char line[260];
  formatBleAcceptedLine(line, sizeof(line), 3, "screen=Ready");
  CHECK_STR(line, "ble: accepted seq 3 -> expect: screen=Ready");
}
