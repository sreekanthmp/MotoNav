#include "ble_link.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <string.h>

#include <atomic>

#include "protocol/frame.h"
#include "protocol/receiver.h"

// One phone at a time: two phones writing navigation data would fight over the display.
#if !defined(CONFIG_BT_NIMBLE_MAX_CONNECTIONS) || CONFIG_BT_NIMBLE_MAX_CONNECTIONS != 1
#error "motonav_ble must be built with -DCONFIG_BT_NIMBLE_MAX_CONNECTIONS=1 (see platformio.ini)"
#endif

namespace motonav {

namespace {

// Fixed 128-bit identifiers of the MotoNav service. Documented in BLUETOOTH_PROTOCOL.md.
const char* const kServiceUuid = "0ce668c8-034a-476c-9c79-4ed464204496";
const char* const kNavDataUuid = "6ae1b797-992f-409f-80b9-9eea3593fc4b";  // phone -> device, write
const char* const kStatusUuid = "664be4dd-4733-45a1-9820-496f5923a0ba";   // device -> phone, read/notify

constexpr int kQueueDepth = 8;
constexpr uint16_t kPreferredMtu = 247;

struct QueuedWrite {
  uint8_t length;
  uint8_t data[kMaxFrameBytes];
};

QueueHandle_t queue = nullptr;
NimBLECharacteristic* statusCharacteristic = nullptr;

std::atomic<bool> isConnected{false};
std::atomic<uint16_t> currentMtu{23};
std::atomic<uint32_t> connectEvents{0};
std::atomic<uint32_t> disconnectEvents{0};
std::atomic<int> lastDisconnectReason{0};
std::atomic<uint32_t> overflows{0};
std::atomic<uint32_t> oversizes{0};

// Main task only.
uint32_t seenConnectEvents = 0;
uint32_t seenDisconnectEvents = 0;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo& connInfo) override {
    currentMtu = connInfo.getMTU();
    isConnected = true;
    ++connectEvents;
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override {
    isConnected = false;
    currentMtu = 23;
    lastDisconnectReason = reason;
    ++disconnectEvents;
  }
  void onMTUChange(uint16_t mtu, NimBLEConnInfo&) override { currentMtu = mtu; }
};

class NavDataCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
    const NimBLEAttValue value = characteristic->getValue();
    if (value.size() > kMaxFrameBytes) {
      ++oversizes;
      return;
    }
    QueuedWrite item;
    item.length = static_cast<uint8_t>(value.size());
    memcpy(item.data, value.data(), value.size());
    if (xQueueSend(queue, &item, 0) != pdTRUE) ++overflows;
  }
};

ServerCallbacks serverCallbacks;
NavDataCallbacks navDataCallbacks;

}  // namespace

bool BleLink::begin() {
  queue = xQueueCreate(kQueueDepth, sizeof(QueuedWrite));
  if (queue == nullptr) return false;

  if (!NimBLEDevice::init("MotoNav")) return false;
  // Our preferred maximum. The phone still has to ask for it (Android: requestMtu).
  NimBLEDevice::setMTU(kPreferredMtu);

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(&serverCallbacks, false);
  server->advertiseOnDisconnect(true);  // a phone that drops can reconnect

  NimBLEService* service = server->createService(kServiceUuid);
  NimBLECharacteristic* navData = service->createCharacteristic(
      kNavDataUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  navData->setCallbacks(&navDataCallbacks);

  statusCharacteristic = service->createCharacteristic(
      kStatusUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  uint8_t initial[kStatusBytes];
  encodeStatus(ReceiverStats(), initial);
  statusCharacteristic->setValue(initial, kStatusBytes);

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->setName("MotoNav");
  advertising->addServiceUUID(service->getUUID());
  return advertising->start();
}

bool BleLink::connected() const { return isConnected.load(); }
uint16_t BleLink::mtu() const { return currentMtu.load(); }

bool BleLink::takeConnected() {
  const uint32_t now = connectEvents.load();
  if (now == seenConnectEvents) return false;
  seenConnectEvents = now;
  return true;
}

bool BleLink::takeDisconnected(int* reason) {
  const uint32_t now = disconnectEvents.load();
  if (now == seenDisconnectEvents) return false;
  seenDisconnectEvents = now;
  *reason = lastDisconnectReason.load();
  return true;
}

bool BleLink::popFrame(uint8_t* out, size_t capacity, size_t* length) {
  QueuedWrite item;
  if (queue == nullptr || xQueueReceive(queue, &item, 0) != pdTRUE) return false;
  if (item.length > capacity) return false;  // cannot happen: capacity >= kMaxFrameBytes
  memcpy(out, item.data, item.length);
  *length = item.length;
  return true;
}

void BleLink::flush() {
  if (queue != nullptr) xQueueReset(queue);
}

uint32_t BleLink::overflowCount() const { return overflows.load(); }
uint32_t BleLink::oversizeCount() const { return oversizes.load(); }

void BleLink::publishStatus(const uint8_t* status, size_t length) {
  if (statusCharacteristic == nullptr) return;
  statusCharacteristic->setValue(status, length);
  if (isConnected.load()) statusCharacteristic->notify();
}

}  // namespace motonav
