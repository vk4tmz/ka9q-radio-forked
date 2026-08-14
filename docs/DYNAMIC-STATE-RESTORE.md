# Native dynamic receiver state restore for `radiod`

Status: initial native implementation completed for local build/live validation (VK4TMZ, 2026-08-14).

## Goal

Make a `radiod` crash/restart restore the dynamic receivers that were successfully created or updated through KA9Q control, without moving receiver ownership back into the static radiod configuration and without requiring every decoder application to perform restart reconciliation.

Applications continue to own normal receiver intent. `radiod` remembers the effective dynamic receiver state it was successfully asked to maintain. A supervisor decides whether a startup is a fresh/manual start or a crash-recovery start.

## Startup contract

Normal startup remains a fresh start:

```text
radiod <config>
    -> load static configuration
    -> clear/ignore any previous dynamic checkpoint
    -> accept normal external status/control traffic
```

Crash recovery is explicit and opt-in through the implemented restore command-line flag:

```text
radiod --restore-dynamic-state <config>
    -> load static configuration and initialise frontend
    -> enter RESTORING_DYNAMIC_STATE
    -> gate external command/status processing
    -> restore checkpointed dynamic receivers only
    -> verify internal receiver creation
    -> leave restore gate
    -> accept normal external traffic
```

A stale checkpoint must never resurrect merely because radiod is started manually. The supervisor uses restore mode only after an unexpected radiod exit.

The implemented state-file option is:

```text
--dynamic-state-file PATH
```

When omitted, the default is:

```text
$HOME/.local/state/ka9q-radio/radiod-dynamic-state.chk
```

### Fail-closed restore semantics

An explicit `--restore-dynamic-state` request is a contract, not a best-effort hint. If the checkpoint is missing, unreadable, corrupt or has an unsupported format/version, startup aborts with a non-zero exit status. Radiod does **not** silently fall back to a fresh start and does **not** overwrite or clear the failed checkpoint.

A normal startup without `--restore-dynamic-state` is the deliberate fresh-start path and atomically publishes a fresh dynamic checkpoint after static configuration is loaded.

## Restore gate

During `RESTORING_DYNAMIC_STATE`, external KA9Q command packets received on the radio control/status socket are dropped rather than queued.

Dropping is deliberate:

- queued status requests could describe an obsolete startup view;
- queued retune/create commands could race checkpoint replay;
- deterministic restore is safer than replaying requests that arrived while state was incomplete.

The implementation should count dropped packets for diagnostics. Once restore is complete, normal command/status processing resumes.

If restore fails, radiod exits while the gate is still closed. This is intentionally fail-closed: an explicit restore must never become a silent fresh startup with missing dynamic receivers.

The current source uses SSRC `0xffffffff` for the global all-channel status request. SSRC `0` is reserved for the dynamic channel template. Both are handled centrally by `radio_status()` in `src/radio_status.c`.

## Static versus dynamic provenance

Only receivers whose lifecycle originates from external dynamic control are checkpointed.

Static receivers created from the configuration file are never checkpointed because normal `loadconfig()` startup recreates them.

Add explicit receiver provenance to `chan_t`, conceptually:

```c
enum channel_origin {
    CHANNEL_ORIGIN_STATIC_CONFIG,
    CHANNEL_ORIGIN_DYNAMIC_CONTROL,
    CHANNEL_ORIGIN_DYNAMIC_RESTORE,
};
```

The static configuration path in `src/radio.c` marks channels `STATIC_CONFIG`. The dynamic control path in `radio_status()` marks newly created channels `DYNAMIC_CONTROL`. Checkpoint replay creates channels as `DYNAMIC_RESTORE`; after successful restore they continue to be treated as persistent dynamic channels for subsequent updates.

## What is persisted

The checkpoint represents the effective current state of dynamic receivers that should exist if radiod crashed at that instant.

It is not an append-only history. For each dynamic SSRC:

```text
CREATE / first successful command -> add checkpoint entry
RETUNE / UPDATE                  -> replace checkpoint entry
NORMAL DYNAMIC CHANNEL REMOVAL   -> remove checkpoint entry
PROCESS CRASH                    -> checkpoint remains intact
```

Persist enough state to reproduce the receiver from the existing `chan_t`/status encoding: SSRC, demod/preset, RF frequency, output sample rate/channels/encoding/destination/TTL, filter parameters, gain/AGC related state, lifetime and other command-controlled fields needed for equivalent recreation.

Prefer reusing the existing radio status/command encoding rather than inventing a second independent receiver schema where practical.

## Authoritative checkpoint point

Do not checkpoint merely when a UDP command packet is received or queued.

External commands are queued by `radio_status()` and are later applied by the demodulator thread through `decode_radio_commands()`. Persistence should occur only after the command has actually been applied successfully to the channel state. This ensures the checkpoint records effective state rather than requested state.

Because `decode_radio_commands()` is called by several demodulator implementations, the persistence hook should be centralised around the common command-application path/API rather than duplicated per demodulator.

## Normal removal and lifetime expiry

Dynamic channels may disappear normally, including lifetime expiry. `close_chan()` is the central final teardown path and is therefore a candidate checkpoint-removal hook for dynamic channels.

A process crash will not execute normal teardown reliably; this is desired because the last checkpoint must survive the crash. Intentional/manual startup without the restore flag clears/ignores it.

## Checkpoint durability

State must survive abrupt process termination without leaving a corrupt file.

Initial implementation should use an in-memory dynamic-state map plus atomic checkpoint replacement:

```text
update effective in-memory dynamic state
mark checkpoint dirty
write temporary checkpoint
flush/close
atomic rename over current checkpoint
```

Rapid scanner retunes must not force an `fsync` on every hop. Coalesce writes (for example a few hundred milliseconds) while ensuring creates/deletes and shutdown-safe points can request an immediate flush. Keep a previous known-good checkpoint generation if inexpensive.

The initial implementation uses a user-writable host-local default under `$HOME/.local/state/ka9q-radio/`, with `--dynamic-state-file PATH` available for a service-managed `/var/lib/ka9q-radio/` deployment later.

## Scanner behaviour

No special application-aware scanner policy is required in native radiod persistence.

If an LMS scanner SSRC was tuned to frequency A at the last committed checkpoint, restore recreates it at A. The already-running scanner application will perform its next normal retune and immediately resume ownership. Restoring the last valid state is preferable to leaving the SSRC missing.

Coalesced checkpoint writes keep high-rate scanner hopping from producing unnecessary disk I/O.

## WEFAX behaviour

An active WEFAX receiver is a normal dynamic receiver while a capture is running. If radiod crashes mid-capture, its current receiver remains in the checkpoint and is restored. Missing RF samples during the outage cannot be recovered, but the existing decoder can resume receiving once RTP returns.

If WEFAX had already ended and its receiver had normally expired/been removed, the checkpoint-removal path prevents it from being resurrected.

## Supervisor relationship

The radiod supervisor remains separate from native dynamic-state persistence.

On intentional SIGINT/SIGTERM/manual stop:

```text
supervisor marks intentional stop
radiod terminates
supervisor exits
no automatic restart
```

On unexpected exit:

```text
record crash telemetry
short backoff (~1 second initially)
restart radiod with --restore-dynamic-state
record restore result / restart generation
```

Crash-loop backoff may increase after repeated rapid failures. The old five-second delay existed mainly to allow a second Ctrl-C; explicit signal handling means normal crash recovery can be faster.

## Dashboard telemetry planned

Keep crash/restart telemetry distinct from dynamic-state restore telemetry so the dashboard can show both process stability and recovery quality.

Desired supervisor metrics include:

- crashes per day/week;
- manual restarts per day/week;
- automatic restarts;
- uptime before each unexpected exit;
- last exit code/signal;
- crash-loop state.

Desired radiod restore metrics include:

- restore requested/not requested;
- checkpoint age/generation;
- dynamic receivers in checkpoint;
- receivers restored;
- restore failures;
- restore duration;
- external command packets dropped while restore was gated.

## Initial source review findings

The current source is well-shaped for this feature:

1. `radio_status()` in `src/radio_status.c` is the single external control/status command receive loop. It handles the global `0xffffffff` status request and specific-SSRC dynamic control. This is the natural external restore gate.
2. Dynamic receiver creation occurs there through `lookup_or_create_chan(ssrc, &Template)`.
3. Static config receivers use the same allocator from the config path in `src/radio.c`, so an explicit origin field is needed to distinguish provenance.
4. External commands are queued in `radio_status()` and applied later through the shared `decode_radio_commands()` function from the demodulator loops. Persistence should happen after successful command application, not at receive time.
5. `close_chan()` is the central final teardown path and is a suitable place to remove normally terminated dynamic receivers from the checkpoint.
6. Startup currently calls `loadconfig()` before entering the main sleep/CPU loop. Restore should be inserted after static configuration/frontend/channel setup is complete but before the external command gate is opened.
7. Existing `closedown()` ultimately uses `_exit()`. Crash safety therefore must rely on already-persisted atomic checkpoints, not on a final graceful write during process termination.

## Safety invariants

These are implementation/test requirements:

1. Static config channels are never written to the dynamic checkpoint.
2. Normal startup never restores stale dynamic state unless the explicit restore flag is present.
3. Restore mode gates external command/status packets until replay completes.
4. Gated packets are dropped, not queued.
5. A received-but-not-applied command is never persisted as effective state.
6. A dynamic receiver normally removed/expired is removed from the checkpoint.
7. A process crash leaves the last known-good checkpoint intact.
8. Missing/corrupt/incompatible state during explicit restore aborts startup non-zero; the checkpoint is left untouched and the external gate never opens.
9. Partial replay failure also aborts startup rather than exposing a half-restored radio as healthy.
10. Checkpoint replacement is atomic; a truncated/corrupt temporary write never replaces the previous valid generation.
11. Restore does not duplicate static receivers already recreated from config.

## Initial implementation notes

The first native implementation adds:

- `src/dynamic_state.c`: coalesced atomic checkpoint writer and restore parser/replay;
- `chan_t.origin`: static/dynamic-control/dynamic-restore provenance;
- `encode_radio_command_state()`: a compact TLV snapshot containing commandable/effective state and intentionally reusing `decode_radio_commands()` for replay;
- `--restore-dynamic-state`;
- `--dynamic-state-file PATH`;
- restore gating in `radio_status()`;
- checkpoint dirtying after successful `decode_radio_commands()` application;
- checkpoint removal after normal dynamic `close_chan()` teardown.

Checkpoint writes are coalesced by approximately 250 ms so rapid LMS scanner retunes do not force an `fsync` per hop. The on-disk payload is an internal/versioned radiod checkpoint format and should not be treated as a public API.

### Validation sequence before supervisor integration

1. Build/install the patched radiod on the remote RX888 host.
2. Start radiod normally (fresh mode).
3. Start the normal dynamic applications once so the checkpoint is populated.
4. Inspect the checkpoint timestamp/contents and confirm it changes as channels are created/retuned.
5. Stop/kill radiod unexpectedly without restarting applications.
6. Restart with `--restore-dynamic-state` and confirm dynamic SSRCs immediately return.
7. Confirm HFDL/JS8/DSC/HF-APRS resume and LMS continues hopping; test an active WEFAX capture when convenient.
8. Test a missing checkpoint and a deliberately invalid checkpoint: both restore starts must fail non-zero without clearing the file.
9. Only after these tests are stable, integrate the crash supervisor and dashboard telemetry.

## `pcmrecord --stdout` continuity across radiod restart

Dynamic receiver restoration alone is not sufficient if a long-running consumer exits when the RTP sender restarts. `pcmrecord --stdout` historically keyed a session by SSRC, payload type, sender address, and sender UDP source port. A restarted radiod can restore the same SSRC while using a new source port. The old session could then remain idle and, after the default 20-second timeout, terminate the whole single-shot stdout process even though the replacement stream had resumed.

For stdout/cat mode, the selected SSRC is now treated as the logical stream identity. When status for that SSRC arrives from a new sender tuple, `pcmrecord` rebinds the existing stdout session, clears stale RTP/resequencing state, and keeps stdout open. File-recording modes retain the existing tuple-based session behaviour.
