#!/bin/sh
# Enforces the separation between navigation model, rendering and hardware.
# Run from firmware/host. Exits non-zero and says why on a violation.
#
#   core/  navigation data and formatting   -> may include only core/ and std headers
#   sim/   simulated data                   -> may include only core/ and std headers
#   ui/    rendering through Canvas         -> may include core/, ui/ and std headers
#   (hardware code lives in firmware/src and is the only place that may include
#    Arduino, Wire, or a display driver)

LIB=../lib/motonav/src
status=0

# Anything that ties code to a platform or a driver.
HW='Arduino\.h|Wire\.h|Adafruit|SPI\.h|WiFi|BLE|NimBLE|esp_|freertos'

check() { # dir, forbidden-regex, description
  hits=$(grep -rnE "^[[:space:]]*#[[:space:]]*include[[:space:]]*[<\"]($2)" "$LIB/$1" 2>/dev/null)
  if [ -n "$hits" ]; then
    echo "LAYERING VIOLATION: $1/ must not include $3"
    echo "$hits"
    status=1
  fi
}

for d in core sim ui; do
  check "$d" "$HW" "hardware or driver headers"
done
check core 'ui/|sim/' "ui/ or sim/"
check sim 'ui/' "ui/"

# Comments and strings can mention hardware; only #include lines are checked.
if [ "$status" -eq 0 ]; then echo "layering check: ok"; fi
exit $status
