# ADR 0001: Keep Visual Wake Direct Without Local VAD Gating

Status: Accepted

Date: 2026-06-19

## Context

The current Grind Buddy runtime chain is:

```text
K230 face pose -> ESP32-S3 visual wake gate -> Xiaozhi listening session -> local backend
```

The ESP32 controller still contains host-testable local voice detection hooks,
but the active bench path starts the Xiaozhi wake flow directly after stable
visual permission. The 2026-06-17 and 2026-06-19 bench runs verified that this
path opens listening, reaches the local backend, and silently stops listening
when gaze or presence permission is revoked.

Reintroducing a separate local VAD gate after visual wake would add another
permission check between stable gaze and Xiaozhi listening.

## Decision

Do not reintroduce local VAD gating for the current product slice.

Stable visual wake remains the single local gate before Xiaozhi listening:

- K230 confirms face pose.
- ESP32 applies gaze hysteresis and wake cooldown.
- Xiaozhi receives wake directly when visual permission is stable.
- `gaze.leave`, `presence.leave`, or K230 timeout revokes voice permission and
  silently stops listening when needed.

The existing local voice detection controller hooks may stay in code as dormant
testable extension points, but they are not part of the current runtime path.

## Consequences

Benefits:

- Lower wake latency after stable frontal gaze.
- Fewer moving parts in the already-verified K230 -> ESP32 -> backend chain.
- Avoids double-gating audio before Xiaozhi's own listening and backend VAD
  behavior.
- Keeps Phase 3 focused on measured visual stability and threshold tuning.

Trade-offs:

- A stable visual false positive can still start Xiaozhi listening.
- The next control lever for false starts is visual threshold tuning, not local
  audio gating.

## Revisit Triggers

Reconsider local VAD gating only if real bench logs show one of these patterns:

- stable frontal gaze repeatedly wakes Xiaozhi while the user is silent;
- background speech causes unacceptable false starts after visual wake;
- product behavior needs a separate "seen but not listening yet" state.

Until then, tuning work should use recorded K230 pose and ESP32 event logs
before changing face or gaze thresholds.
