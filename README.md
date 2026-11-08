# MotoNav

A compact motorcycle navigation display. A small screen on the handlebar shows
simplified turn-by-turn instructions. The phone does all the navigation work
(GPS, routing, instructions) and sends the result to the device over Bluetooth
Low Energy (BLE).

The long-term goal is a commercial product. The current goal is a working
prototype built in small, reviewable stages.

## Status

Status vocabulary: **Implemented**, **Tested**, **Partially implemented**,
**Planned**, **Blocked**.

| Area | Status | Notes |
|---|---|---|
| Phase 0 — discovery and planning | Implemented (documents only) | Awaiting review. See [DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md). |
| ESP32 firmware | Planned | Nothing written yet. Phase 1. |
| BLE protocol | Planned | Phase 2. |
| Android app | Planned | Phase 3. |
| Google Navigation SDK integration | Planned | Phase 4. Commercial-terms questions are open, see [DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md#technical-risks-and-unknowns). |
| Hardware identification | Blocked | Needs input from the person with the board. See [HARDWARE.md](HARDWARE.md). |

**Nothing in this repository has been built or run yet.** There is no firmware,
no Android code, and no test results.

## Documents

| Document | Purpose |
|---|---|
| [ARCHITECTURE.md](ARCHITECTURE.md) | System design, verified Google SDK facts, proposed repository layout |
| [HARDWARE.md](HARDWARE.md) | Hardware assumptions, identification steps, prototype wiring |
| [DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) | Phases, risks, Phase 1 plan and acceptance criteria |
| [DECISIONS.md](DECISIONS.md) | Technical decisions and their status |

Created when their phase starts (so they never describe work that does not
exist): `BLUETOOTH_PROTOCOL.md` (Phase 2), `TESTING.md` (Phase 1, with the
first real test results).
