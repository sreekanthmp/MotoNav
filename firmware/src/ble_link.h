// The Bluetooth LE side of Phase 2, built on NimBLE-Arduino.
//
// This file is deliberately thin: it advertises, accepts one phone, copies each write into
// a queue, and publishes a status value. Every decision about what a write MEANS is made by
// the host-tested Receiver (lib/motonav_protocol), on the main task.
//
// STATUS: compile-checked only. It has never run on a real board, and no Bluetooth radio
// was available to test it. See TESTING.md.
//
// Threading: NimBLE calls the callbacks on its own task. They only copy bytes into a
// FreeRTOS queue and bump atomic counters; the main task does everything else.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace motonav {

class BleLink {
 public:
  // Starts the stack, the MotoNav GATT service and advertising. False if it could not start.
  bool begin();

  bool connected() const;
  uint16_t mtu() const;  // current ATT MTU (23 until the phone negotiates a larger one)

  // True once after a phone connects / disconnects. `reason` is the BLE disconnect reason.
  bool takeConnected();
  bool takeDisconnected(int* reason);

  // Next received write, copied into `out`. False if there is none.
  bool popFrame(uint8_t* out, size_t capacity, size_t* length);

  // Drops anything still queued (call on connect and disconnect so frames from one
  // connection are never read as belonging to the next).
  void flush();

  uint32_t overflowCount() const;  // writes lost because the queue was full
  uint32_t oversizeCount() const;  // writes longer than the protocol allows, dropped unread

  // Sets the STATUS characteristic and notifies the phone if it subscribed.
  void publishStatus(const uint8_t* status, size_t length);
};

}  // namespace motonav
