#!/bin/sh
# Enforces the separation between navigation model, rendering and hardware.
# Run from firmware/host. Exits non-zero and says why on a violation.
#
#   core/  navigation data and formatting   -> may include only core/ and std headers
#   sim/   simulated data                   -> may include only core/ and std headers
#   diag/  verdict logic for diagnostics    -> std headers only (it reads no hardware)
#   ui/    rendering through Canvas         -> may include core/, ui/ and std headers
#   protocol/ (lib/motonav_protocol) BLE frame format and receiver -> core/ and std headers only
#             (it must not know about Bluetooth; the motonav library must not know about it)
#   (hardware code lives in firmware/src and is the only place that may include
#    Arduino, Wire, or a display driver)

LIB=../lib/motonav/src
LIBP=../lib/motonav_protocol/src
status=0

# Anything that ties code to a platform or a driver.
HW='Arduino\.h|Wire\.h|Adafruit|SPI\.h|WiFi|BLE|NimBLE|esp_|freertos'

check() { # dir, forbidden-regex, description, [library root]
  hits=$(grep -rnE "^[[:space:]]*#[[:space:]]*include[[:space:]]*[<\"]($2)" "${4:-$LIB}/$1" 2>/dev/null)
  if [ -n "$hits" ]; then
    echo "LAYERING VIOLATION: $1/ must not include $3"
    echo "$hits"
    status=1
  fi
}

for d in core sim ui diag; do
  check "$d" "$HW" "hardware or driver headers"
done
check core 'ui/|sim/|diag/|protocol/' "ui/, sim/, diag/ or protocol/"
check sim 'ui/|diag/|protocol/' "ui/, diag/ or protocol/"
check ui 'protocol/' "protocol/"
check diag 'core/|ui/|sim/|protocol/' "core/, ui/, sim/ or protocol/"
check protocol "$HW" "hardware, driver or Bluetooth headers" "$LIBP"
check protocol 'ui/|sim/|diag/' "ui/, sim/ or diag/" "$LIBP"

# Comments and strings can mention hardware; only #include lines are checked.
if [ "$status" -eq 0 ]; then echo "layering check: ok"; fi
exit $status
