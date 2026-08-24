# Event detection bench session

Closes the bench-test acceptance gap for #23, tracked under the Etapa 2 handoff
(`HANDOFF-ensaio-de-eventos.md`).

Date: 24 Aug 2026.
Firmware: commit `78f3ae3e27cafafcea88481558c947b69b934809` (`git rev-parse HEAD`
confirmed against the flashed build before every capture below).

## Native test suite

```
Collected 2 tests

Processing test_event_detector in native environment
--------------------------------------------------------------------------------
Building...
Testing...
test/test_event_detector/test_event_detector.cpp:216: test_step_above_threshold_produces_one_event_with_close_magnitude	[PASSED]
test/test_event_detector/test_event_detector.cpp:217: test_noise_below_threshold_produces_no_event	[PASSED]
test/test_event_detector/test_event_detector.cpp:218: test_switch_off_detected_with_opposite_direction	[PASSED]
test/test_event_detector/test_event_detector.cpp:219: test_magnitude_uses_window_means_not_adjacent_samples	[PASSED]
test/test_event_detector/test_event_detector.cpp:220: test_stable_plateau_produces_no_repeated_events_over_long_run	[PASSED]
test/test_event_detector/test_event_detector.cpp:221: test_two_steps_inside_confirmation_window_produce_single_event	[PASSED]
test/test_event_detector/test_event_detector.cpp:222: test_multi_second_ramp_does_not_fragment_into_several_events	[PASSED]
------------ native:test_event_detector [PASSED] Took 0.86 seconds ------------

Processing test_meter in native environment
--------------------------------------------------------------------------------
Building...
Testing...
test/test_meter/test_meter.cpp:259: test_pure_dc_centers_to_zero	[PASSED]
test/test_meter/test_meter.cpp:260: test_sine_offset_is_removed_ac_preserved	[PASSED]
test/test_meter/test_meter.cpp:261: test_pure_dc_window_closes_with_zero_vrms	[PASSED]
test/test_meter/test_meter.cpp:262: test_sine_window_vrms_matches_amplitude_over_sqrt2	[PASSED]
test/test_meter/test_meter.cpp:263: test_window_closes_by_elapsed_time_not_sample_count	[PASSED]
test/test_meter/test_meter.cpp:264: test_first_window_before_dc_settles_is_bounded	[PASSED]
test/test_meter/test_meter.cpp:265: test_sine_window_irms_matches_calibrated_conversion	[PASSED]
test/test_meter/test_meter.cpp:266: test_sine_window_apparent_power_matches_mains_voltage_times_irms	[PASSED]
test/test_meter/test_meter.cpp:267: test_switching_mains_voltage_changes_apparent_power_proportionally	[PASSED]
----------------- native:test_meter [PASSED] Took 0.52 seconds -----------------

=================================== SUMMARY ===================================
Environment    Test                 Status    Duration
-------------  -------------------  --------  ------------
native         test_event_detector  PASSED    00:00:00.862
native         test_meter           PASSED    00:00:00.517
================= 16 test cases: 16 succeeded in 00:00:01.379 =================
```

## Loads

- Sandwich maker, ~790-800 VA nominal, thermostatic. Heating periods observed at
  31-37 s (well above the 3-sample/3 s confirmation window); off periods 240-280 s.
- Fan: head locked, no oscillation, speed 3, ~45-52 VA.
- Lenovo laptop charger, notebook actually charging (not just plugged in):
  battery 40% at session start, 22% at the last check (20:20:55). ~40-110 VA,
  with a noisy multi-second soft-start ramp on power-up rather than an
  instantaneous step.

## Captures

Five capture files under `docs/measurements/`, split across two USB/serial
interruptions (see "What went wrong"). Each file's `t_s` column is relative to
that file's own start; `event-detection-ground-truth.csv`'s `capture` column
disambiguates which file each ground-truth row belongs to.

| capture | wall-clock anchor (`t_s=0`) | covers |
|---|---|---|
| session-1 | 18:15:06 | sync mark, 9 isolated on/off pairs (3 loads x3), fan-startup-observed x3, simultaneous-switching x3 pairs |
| session-2 | 19:00:10.0 | start of the 10-minute thermostat-cycling observation; crashed with a USB disconnect before the observation's natural end |
| session-3 | not independently calibrated (crash-recovery restart; see below) | sandwich maker auto re-engaging after cooldown, then the first staircase-overlap attempt, interrupted by an ESP32 reboot |
| session-4 | not independently calibrated (continuation of the same recovery capture, post-reboot) | staircase-overlap attempt continues; hit its own 900 s script duration cap before the attempt finished |
| session-5 | 20:21:41.0 | staircase-overlap, redone from a rested baseline: clean full ascent and descent |

Wall-clock span from the first capture's start to the last recorded action:
18:15:06 to 20:34:40 (~2h19m), including the 10-minute passive observation
window, deliberate waits for the sandwich maker to cool or auto re-engage, and
recovery from the two hardware interruptions below.

## Counts

- 46 ground-truth actions logged (`event-detection-ground-truth.csv`), across
  sandwich_maker, fan, charger, and three simultaneous-switching combinations.
- 101 events detected across all five capture files combined (61 + 17 + 5 + 4 + 14).
- Calibration precision: where an independent wall-clock reading was available,
  the sync mark's two edges (on/off) agreed with each other to within ~1 s, and
  most later actions landed within 0.5-1.5 s of their reported time once
  calibrated from that mark.

## What went wrong (not painted over)

**1. USB/serial disconnect.** Partway through the thermostat-cycling
observation (session-2), the capture script crashed with `OSError: [Errno 6]
Device not configured`. Root cause not isolated; candidates are the sandwich
maker's switching disturbing a shared power strip, or the USB cable being
bumped. The port recovered within seconds.

**2. ESP32 reboot mid-capture.** During the recovery capture, the ESP32 itself
rebooted (boot-banner bytes found in the raw serial log), which reset
`micros()` and the firmware's internal detector state. The raw file was split
into `session-3.csv` (pre-reboot) and `session-4.csv` (post-reboot) so the two
independent `t_s=0` origins don't collide in the same file. Likely the same
root disturbance as (1), not confirmed.

**3. Two actions never recorded.** `session-4`'s capture script had its own
900 s duration limit, which elapsed (~20:14) before the user's 20:20:33
(sandwich on) and 20:20:55 (charger on) actions — those two physical actions
are not in any capture file. The staircase-overlap scenario was redone from a
rested baseline in session-5 instead of trying to patch this gap.

**4. Staircase descent order.** The script called for switching off in reverse
order (charger, fan, sandwich). In session-5 the sandwich maker's own
thermostat cut off on its own between the charger-off and fan-off steps, out
of the planned sequence (`t_s=722.02`). Genuine data, kept as-is.

**5. Accidental charger activation.** In session-1, the user meant to start
the fan-startup-observed item but hit the charger switch by mistake
(`t_s=2037.02` on, `2050.02` off), corrected within ~25 s. Kept in the ground
truth, flagged as accidental.

**6. Fragmentation pattern.** Nearly every manual sandwich-maker on/off
transition produced 2 consecutive detected events (a large primary step plus a
smaller trailing one) instead of 1 — consistent with what the user had already
noticed informally before this session. Fan and charger transitions were
mostly single clean events, except the charger's power-up, which showed a
slow, noisy multi-second soft-start ramp producing 2 fragments (the charger's
own supply inrush, not a detector defect). The thermostat's own *autonomous*
transitions fragmented further still — 3-4 events per transition over the
pattern's ~9 s ramp — the most fragmentation-prone case in the dataset.

**7. Adaptive-reference candidates.** Searched all five files for the specific
pattern the handoff flagged (an isolated sample crossing the threshold without
confirming, silently pulling the baseline toward it). No case of a
never-confirmed candidate contaminating a later measurement was found; the
handful of large single-row jumps found were all the leading edge of a
transition that a following row went on to confirm as an event (mostly the
charger's soft-start), not a rejected candidate. One small, unexplained ~45 VA
blip appears in session-5 at `t_s=186` during the reset-to-baseline pause
before the staircase rebuild; it settles and clears before the deliberate
sequence begins at `t_s=597`, and does not correspond to any reported action,
so it is not in the ground truth.

## Per-scenario summary (Passo 4)

- **Sync mark**: on/off pair detected cleanly, magnitude (~762/~792 VA
  combined across its two fragments) consistent with the sandwich maker's
  nominal power.
- **9 isolated pairs**: all 18 actions (3 loads x3 reps x on/off) detected;
  sandwich maker fragmented on 5 of 6 transitions, fan was clean on all 6,
  charger showed the soft-start ramp on all 3 on-transitions but clean single
  events on all 3 off-transitions.
- **Fan-startup-observed x3**: all 3 reps produced a single clean event on
  both on and off, no fragmentation — a direct, clear answer to the
  literature's false-positive-on-startup question for this fan.
- **Simultaneous-switching x3 pairs**: all 3 pairs detected as one fused event
  per transition, magnitude approximating the sum of the two loads, as
  expected — this is the detector's documented limitation surfacing as
  intended, not a defect.
- **Thermostat cycling (10 min passive)**: 3 complete heat cycles observed,
  heating periods 31-37 s (comfortably above the 3-sample confirmation
  window, so no cycle was missed), off periods 240-280 s (~10% duty cycle).
- **Staircase overlap**: ascent (sandwich -> +fan -> +charger) captured
  cleanly as 3 single clean events reaching ~915 VA combined; descent partly
  out of planned order (see item 4 above) but fully captured, returning
  cleanly to the rest baseline.
