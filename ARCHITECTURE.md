# Architecture

**Status: Proposed. Nothing described here is implemented yet.** Facts marked
**Verified** were read directly from Google's documentation on 2026-11-08 (the
pages were last updated 2026-11-03). Everything else is a proposal or an
assumption.

## 1. System overview

```
  Google Navigation SDK                 BLE GATT                 ESP32 firmware
 ┌─────────────────────────┐        ┌──────────────┐        ┌──────────────────────┐
 │ Android app (Kotlin)    │        │ versioned    │        │ BLE receive + validate│
 │  - routing, GPS, voice  │ ─────► │ messages     │ ─────► │  -> NavSnapshot       │
 │  - TurnByTurn feed      │        │ (phone ->    │        │  -> screen renderers  │
 │  - maps feed to protocol│        │  device)     │        │  -> display driver    │
 └─────────────────────────┘        └──────────────┘        └──────────────────────┘
        does all navigation                                   does no navigation
```

The phone is the only navigation processor. The ESP32 never routes, never
reads GPS, and never needs the internet.

### Boundaries (kept deliberately thin)

1. **Navigation source -> `NavSnapshot`.** On Android, the TurnByTurn feed is
   converted into a small, transport-independent `NavSnapshot`. Nothing past
   this point knows about Google types.
2. **`NavSnapshot` -> BLE message.** Encoding and decoding live in one module
   per side. The navigation code never touches GATT. The BLE code never looks
   inside a `NavSnapshot`.
3. **`NavSnapshot` -> pixels (firmware).** Screen renderers take a snapshot and
   draw to a display object. They contain no BLE or state-machine logic.
   Swapping the SSD1306 for a circular AMOLED means changing the display
   driver and the screen layouts, not the navigation logic.

No further layers are proposed. In particular: no dependency-injection
framework, no plugin system, no message bus.

## 2. Google Navigation SDK facts (Verified)

Sources: [TBT feed guide](https://developers.google.com/maps/documentation/navigation/android-sdk/tbt-feed),
[Maneuver](https://developers.google.com/maps/documentation/navigation/android-sdk/reference/com/google/android/libraries/mapsplatform/turnbyturn/model/Maneuver),
[NavInfo](https://developers.google.com/maps/documentation/navigation/android-sdk/reference/com/google/android/libraries/mapsplatform/turnbyturn/model/NavInfo),
[StepInfo](https://developers.google.com/maps/documentation/navigation/android-sdk/reference/com/google/android/libraries/mapsplatform/turnbyturn/model/StepInfo),
[NavState](https://developers.google.com/maps/documentation/navigation/android-sdk/reference/com/google/android/libraries/mapsplatform/turnbyturn/model/NavState).

### Delivery

- The feed is intended for "devices not designed for map-based navigation
  guidance" and "small screen displays where a full Android stack is not
  available", with two-wheeled vehicles named as an example. This matches the
  MotoNav use case.
- The app registers a service with
  `navigator.registerServiceForNavUpdates(packageName, serviceClassName, numNextStepsToPreview)`
  (an overload taking `NavigationUpdatesOptions` also exists). The SDK binds to
  the service and sends updates through an Android `Messenger`.
- Messages are identified by `Message.what == TurnByTurnManager.MSG_NAV_INFO`.
  `TurnByTurnManager.readNavInfoFromBundle(...)` turns the bundle into a
  `NavInfo`. Message codes 7200-7299 are reserved by Google.
- Update rate: "about once a second during navigation".
- The service "can live in a separate background process", starts when
  registered and stops when unregistered. The guide suggests considering a
  foreground service.
- Unregister with `navigator.unregisterServiceForNavUpdates()`.

### Data available

| Need | Source | Type / unit |
|---|---|---|
| Navigation state | `NavInfo.getNavState()` | `ENROUTE`=1, `REROUTING`=2, `STOPPED`=3, `UNKNOWN`=0 |
| Next maneuver | `NavInfo.getCurrentStep().getManeuver()` | int, 66 constants (0-65), see below |
| Distance to maneuver | `NavInfo.getDistanceToCurrentStepMeters()` | `Integer`, meters, nullable |
| Time to maneuver | `NavInfo.getTimeToCurrentStepSeconds()` | `Integer`, seconds |
| Remaining distance | `NavInfo.getDistanceToFinalDestinationMeters()` | `Integer`, meters |
| Remaining time | `NavInfo.getTimeToFinalDestinationSeconds()` | `Integer`, seconds |
| Road name | `StepInfo.getFullRoadName()`, `getSimpleRoadName()` | `String` |
| Instruction text | `StepInfo.getFullInstructionText()` | `String` |
| Roundabout exit | `StepInfo.getRoundaboutTurnNumber()` | `Integer` |
| Exit number | `StepInfo.getExitNumber()` | `String` |
| Route changed | `NavInfo.getRouteChanged()` | `boolean` |
| Next steps | `NavInfo.getRemainingSteps()` | `StepInfo[]` |
| Driving side | `StepInfo.getDrivingSide()` | int |

Notes:

- `getDistanceToDestinationMeters()` and `getTimeToDestinationSeconds()` are
  **deprecated**. Use the `...FinalDestination...` versions.
- **There is no ETA clock time.** The SDK gives remaining seconds. The phone
  must compute any clock ETA, because the ESP32 has no time source.
- In `REROUTING` the upcoming step is **not available**. In `STOPPED` the
  display should clear.
- There is **no `ARRIVED` nav state.** Arrival is a *maneuver*
  (`DESTINATION`=2, `DESTINATION_LEFT`=3, `DESTINATION_RIGHT`=4). How and when
  the arrival event itself is signalled is **not yet verified** (to check in
  Phase 4).
- Icon bitmaps are optional and off by default. MotoNav draws its own icons, so
  they are not needed.
- `Maneuver`, `NavInfo`, `StepInfo` and `NavState` are marked **beta / preview
  feature, "subject to change without backward compatibility"**.
- The `Maneuver` page and the TBT guide disagree on clockwise/counterclockwise
  wording for left-hand-traffic countries (the guide's sentence says
  "clockwise" for both sides). Real behaviour must be tested, not assumed.

### Travel mode, billing and requirements

- `RoutingOptions.TravelMode.TWO_WHEELER` exists ("a motorcycle or scooter").
  Default is `DRIVING`. Other modes: `CYCLING`, `WALKING`, `TAXI` (experimental).
- Billing: pay-as-you-go, SKU "Navigation Request", charged **per destination
  in a route request**, first 1,000 free per month. No extra charge for
  starting guidance, traffic reroutes or off-route reroutes. Billing must be
  enabled on the Cloud project, and an API key is required. The price per
  1,000 is on a separate pricing-list page that has **not** been read. See the
  [usage and billing page](https://developers.google.com/maps/documentation/navigation/android-sdk/pricing).
- Latest release at time of writing: **8.1.0 (2026-11-04)**. Release 8.0.0
  raised `targetSdk` to API 37 and requires Android Gradle Plugin 9.0.1 and
  Gradle 9.1.0. The setup page still lists "7.0.0 and later" requirements
  (target API 36, minimum API 24), so the setup page lags the release notes.
  The version is pinned in Phase 3/4 and its requirements read at that time.
- The Navigation SDK **replaces** the Maps SDK. They cannot be used in the same
  app.
- Google Play requires a Location Purpose Declaration (select "Navigation")
  for apps built on 8.0.0 or later.

## 3. Proposed `NavSnapshot` (transport-independent)

| Field | Type | Source |
|---|---|---|
| `state` | enum: `READY`, `NAVIGATING`, `REROUTING`, `ARRIVED` | `NavState` plus arrival |
| `maneuver` | enum, MotoNav set below | `StepInfo.getManeuver()` mapped |
| `distanceToManeuverM` | uint, or "unknown" | `getDistanceToCurrentStepMeters()` |
| `roadName` | string, bounded length | `getSimpleRoadName()` or `getFullRoadName()` |
| `remainingDistanceM` | uint | `getDistanceToFinalDestinationMeters()` |
| `remainingSeconds` or ETA minute-of-day | uint | phone-computed |
| `roundaboutExit` | uint, 0 if none | `getRoundaboutTurnNumber()` |

`DISCONNECTED` is **not** a snapshot field. It is a firmware condition derived
from the BLE link state.

### Proposed maneuver mapping (SDK -> MotoNav display set)

The requested display set has 11 entries. The SDK has 66 maneuver constants, so
a many-to-one mapping is needed.

| MotoNav | SDK constants (Verified names) |
|---|---|
| `STRAIGHT` | `STRAIGHT`, `DEPART`, `NAME_CHANGE`, `MERGE_*`, `ON_RAMP_UNSPECIFIED`, `OFF_RAMP_UNSPECIFIED`, `FERRY_*`, `UNKNOWN` |
| `TURN_LEFT` | `TURN_LEFT`, `ON_RAMP_LEFT`, `OFF_RAMP_LEFT` |
| `TURN_RIGHT` | `TURN_RIGHT`, `ON_RAMP_RIGHT`, `OFF_RAMP_RIGHT` |
| `SLIGHT_LEFT` | `TURN_SLIGHT_LEFT`, `TURN_KEEP_LEFT`, `FORK_LEFT`, `*_RAMP_SLIGHT_LEFT`, `*_RAMP_KEEP_LEFT` |
| `SLIGHT_RIGHT` | `TURN_SLIGHT_RIGHT`, `TURN_KEEP_RIGHT`, `FORK_RIGHT`, `*_RAMP_SLIGHT_RIGHT`, `*_RAMP_KEEP_RIGHT` |
| `SHARP_LEFT` | `TURN_SHARP_LEFT`, `*_RAMP_SHARP_LEFT` |
| `SHARP_RIGHT` | `TURN_SHARP_RIGHT`, `*_RAMP_SHARP_RIGHT` |
| `U_TURN` | `TURN_U_TURN_*`, `*_RAMP_U_TURN_*` |
| `ROUNDABOUT` | all 20 `ROUNDABOUT_*` constants (values 43-62) |
| `ARRIVED` | `DESTINATION`, `DESTINATION_LEFT`, `DESTINATION_RIGHT` |
| `REROUTING` | not a maneuver: comes from `NavState.REROUTING` |

Gaps to decide before the protocol is frozen (Phase 2):

1. `ROUNDABOUT` should probably carry the **exit number**. A roundabout with
   no exit number is much less useful.
2. `U_TURN` needs a side (left or right). Either two values or a flag.
3. Whether "keep left/right" and "fork" are acceptable as slight turns on the
   display, or deserve their own arrows.

## 4. Proposed repository structure

```
MotoNav/
├── README.md  ARCHITECTURE.md  HARDWARE.md  DEVELOPMENT_PLAN.md  DECISIONS.md
├── firmware/                    PlatformIO project (Phase 1)
│   ├── platformio.ini           env for the board + a native (host) env for tests
│   ├── src/
│   │   ├── main.cpp             wiring only: init, loop
│   │   ├── nav_model.h          NavSnapshot, enums. No display or BLE includes.
│   │   ├── distance_format.*    pure logic, unit tested
│   │   ├── sim_source.*         Phase 1 simulated navigation data
│   │   ├── ui/                  screens, icon drawing, display setup
│   │   └── ble/                 Phase 2
│   └── test/                    host-side unit and rendering tests
├── android/                     Phase 3 and later
└── protocol/                    Phase 2: message layout test vectors shared by both sides
```

`protocol/` holds known-good encoded messages (hex) that both the firmware
tests and the Android tests must decode identically. It is the cheapest
protection against the two sides drifting apart.

## 5. Policy constraints that shape the design

From the [Navigation SDK policies page](https://developers.google.com/maps/documentation/navigation/android-sdk/policies)
(Verified, last updated 2026-11-03):

- Allowed for commercial applications. Heavy or large vehicle routing needs
  Google's written consent. Motorcycles are not in the disallowed list on
  that page.
- **Do not persist road names or speed limits** beyond the user session. Doing
  so is "scraping". Design consequence: road names live in RAM only on the
  ESP32 and are never written to flash, and the Android app must not write them
  to logs, analytics or storage.
- Apps must show the Navigation SDK terms of use on first use, and tell users
  to exercise independent judgment.
- Overlays and customisations must not hide upcoming turns or distract the
  driver, and animations during active navigation are called out as unsafe.
  The "no unnecessary animations" display rule is therefore a policy match as
  well as a readability choice.
- Google Maps attribution is required when displaying Maps Platform content.
  Whether and how this applies to a separate hardware display is **not
  answered** by the page. See the open questions in
  [DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md).
