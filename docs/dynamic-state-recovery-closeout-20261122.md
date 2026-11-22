# Dynamic receiver state recovery — integration closeout

**Date:** 2026-11-22  
**Repository:** `vk4tmz/ka9q-radio-forked`  
**Branch:** `feature/dynamic-state-recovery`  
**Validated implementation/test-harness commit:** `2a6db3bf8cb7b4ce2d981b9ccf8f5f084fea16ff`  
**Disposition:** Integration and testing complete at the approved offline/synthetic-SDR boundary. No further hardware testing is planned for this work.

## Scope completed

- Brought the Dynamic recovery implementation forward to upstream KA9Q `main` commit `095ab864c4c76a53dd15620452ed919ebfc45fad`.
- Reconciled the removed Doppler fields in upstream radio status, fixed the `pthread_setname` include, and retained upstream sweep-tuning parameters in checkpoint serialization.
- Preserved commandable receiver configuration using KA9Q's actual TLV encoder/decoder.
- Verified recovery avoids replacing static channels when SSRCs collide, rejects malformed checkpoints before replay, and preserves the checkpoint on an explicit recovery failure.
- Fast-forwarded `feature/dynamic-state-recovery` from its previous tip, without force-updating branch history.
- Added reproducible tests:
  - `tests/test_dynamic_state_parser.c`
  - `tests/test_dynamic_state_lifecycle.c` (mock codec)
  - `tests/test_dynamic_state_tlv_roundtrip.c` (real codec)
  - `tests/test_dynamic_state_real_restore.c` (real codec + checkpoint orchestration)
  - `tests/test_dynamic_state_process_restart.py` (real test RADIOD using a synthetic SDR frontend)

## Verification evidence — devlap200

The fresh Git archive in `/tmp/ka9q-dynamic-final.Gzkl1b` was built without installing anything:

| Validation | Observation |
| --- | --- |
| `radiod`, `rx888.so`, `tune` clean build | PASS |
| Parser malformed-record regression | PASS |
| Offline checkpoint lifecycle | PASS |
| Real KA9Q TLV round-trip | PASS (139-byte payload) |
| Real checkpoint -> restore with FM and Linear channels | PASS |
| Static SSRC collision and corrupt checkpoint tests | PASS |
| `sig_gen.so` build | PASS |
| Independent unprivileged network namespace | PASS |
| Two receiver create -> checkpoint -> synthetic-RADIOD restart -> restore | PASS |
| Post-restore RTP output for both test SSRCs | PASS |

Synthetic-SDR process test log directory on devlap200: `/tmp/ka9q-dynamic-process-6r5o0qn6`. The test used a separate Linux network namespace and a private checkpoint, created SSRCs 100001 and 100002 via real KA9Q control commands, restarted **only the test RADIOD process**, observed `restored=2`, found the original checkpoint unchanged, and received both SSRCs in resumed RTP output.

The deployed binary was not replaced or restarted. Its checksum, recorded after both the final offline suite and synthetic restart test, remained:

```text
12756b0608dd69c3f4767bc6ac6bb54e478f2a928ed91dbe3346a34d3b770470  /usr/local/sbin/radiod
```

## Acceptance boundary and exclusions

**Accepted:** code integration, clean build, offline parser/lifecycle/real-codec regressions, and isolated full RADIOD process restart using `sig_gen` with recovered RTP streams.

**Not performed / not required for this closeout:** rebooting production, replacing an installed RADIOD binary, changing systemd/tmux deployment, interrupting the VHF receivers, or testing crash recovery against physical RX888/RTL-SDR hardware. Do not describe those unperformed actions as validated.

The runtime-facing test harness stubs no codec or checkpoint logic; the separate offline test harnesses intentionally stub SDR- and radio-dependent operations. The synthetic test checks actual post-restore RTP but is not a physical-SDR acceptance test.

## Handover

This feature branch is left intact as a validated, documented checkpoint. No deployment action, further test request, or automatic merge into the repository default branch is implied by this closeout. Revisit physical SDR integration or production rollout **only on a separate explicit request**.
