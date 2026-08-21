# Determining the sensor's effective turns ratio

Capture date: 19 Aug 2026
Firmware commit: `cc2460661ffe33d6524d2d1915a2c11647c7f13a`

Multimeter: Fluke 117. True RMS. AC voltage accuracy figures below are the
commonly published Fluke 117 specification, **not confirmed against this
unit's own manual** — flagged here rather than presented as verified.

Rig: SCT-013-000 current transformer, 22 Ω burden resistor, ESP32 +
ADS1115 running the firmware above. `vrms` is the firmware's own RMS
reading of the burden voltage (`lib/meter/meter.cpp`), printed with six
decimal places. Every reading used for the final ratio comes from the
multimeter or from computation — no smart-plug readings are used anywhere
in this document.

Raw serial samples for every capture below, including the discarded ones,
are in `docs/measurements/turns-ratio.csv` (`included_in_mean` marks which
samples survived trimming).

## Turn-count verification and low-current comparison (soldering iron)

Before trusting a 10-turn winding as exactly 10 turns, its ratio to a plain
1-turn pass was checked directly: `vrms_at_N_turns / vrms_at_1_turn` equals
the turn count regardless of the sensor's true `ctRatio` or the burden
resistor's exact value, since both cancel out of that ratio. This doubles as
the 1-turn-vs-10-turn low-current comparison carried over from #15.

| point | turns | n | mean vrms (V) | stdev (rel.) |
|---|---|---|---|---|
| `10turn_initial` | 10 | 45 | 0.028785 | 0.39% |
| `1turn_confirmed` | 1 | 45 | 0.002888 | 0.35% |

Ratio: 0.028785 / 0.002888 = **9.97**, against a target of 10 — the winding
is correct within measurement noise. The 1-turn reading came out very
slightly *higher* than 1/10 of the 10-turn reading, not lower, so there is
no sign of the sensor under-reporting at this low a current (0.26 A).

**One discarded attempt along the way**: the first attempt at the 1-turn
capture read at the no-load noise floor (~0.00001 V) — traced to the iron's
mains socket not being fully seated. Reseated and recaptured; see
`1turn_socket_loose` in the CSV for the discarded run.

## ADC-vs-multimeter cross-check (10 turns, soldering iron)

With the winding confirmed, the burden voltage at 10 turns was also read
directly on the Fluke 117 (mV AC range) as a check on the ADS1115's
absolute accuracy, independent of turn count or `ctRatio`.

| source | value |
|---|---|
| firmware `vrms` (`10turn_confirmed`, n=45) | 0.028606 V |
| multimeter (mV AC) | 28.9 mV, 29.0 mV |

Agreement: firmware reads 0.028606 V against a multimeter mean of 0.02895 V
— a 1.2% difference, consistent with the multimeter's own accuracy spec at
this range. The ADC's burden-voltage reading is trustworthy.

**Two more discarded attempts along the way** (`10turn_disturbed_1`,
`10turn_disturbed_2` in the CSV): after the winding was confirmed, probing
the multimeter disturbed the burden resistor's connection, producing a
smooth ~10–12 s oscillation between roughly 26.4 mV and 30.2 mV (±4.5%
relative stdev, against ±0.4% on every clean capture). It persisted across
a recapture even without further handling, so it was a real marginal
connection, not incidental noise. A physical check of the resistor's leads
resolved it; `10turn_confirmed` above is the clean recapture after the fix.

## Method B: scope note

Method B's own independent `ratio_effective` — computed from `I_primary = N
× I_load` with `I_load` measured independently of any nameplate — was **not
produced**. Establishing `I_load` for the iron independently would have
required breaking one conductor of its cord to insert the multimeter in
series (ammeter mode); by decision, that step was skipped for this session.
What the 10-turn setup did produce: the turn-count verification, the
low-current comparison, and the ADC cross-check above. This is a deliberate
scope reduction, not an oversight — see "What this does not do" below.

## Method A: resistive load with measured resistance (sandwich maker)

Nameplate resistance, carried over from prior work
(`docs/measurements/voltage-and-calibration.md`), where it was cross-checked
against two independent smart-plug readings (70.51 Ω, 70.68 Ω) to 0.2%
agreement — that smart-plug cross-check is historical context only; no new
smart-plug reading was taken for this issue:

```
R = 220² / 686 = 70.5539 Ω
```

**Superseded below.** `docs/measurements/reference-load-resistance.md`
later measured this appliance's resistance directly — `R_hot = 70.8531 Ω`,
a 0.42% difference from the nameplate figure above, well inside that
measurement's own uncertainty. The ratio and factor below now use the
measured `R_hot`, not the nameplate value.

The sandwich maker is thermostatic, so per the bench-measurement protocol
each point required waiting for a full cutoff from a cold connection and
then capturing through the following on-cycle. Two consecutive on-cycles
were captured this way, back to back:

| point | n (trimmed) | mean vrms (V) | mains V~ | notes |
|---|---|---|---|---|
| `sandwich_cycle_1` | 28 | 0.036466 | not captured | ramp-in/cutoff trimmed; no mains reading paired |
| `sandwich_cycle_2` | 28 | 0.036420 | 236.75 V (avg of 236.7/236.8) | ramp-in/cutoff trimmed; used for the ratio below |

The two independent on-cycles agree to 0.13% on `vrms` (0.036466 V vs.
0.036420 V), which is the best available estimate of cycle-to-cycle
repeatability for this load. Both fall short of the 30-sample target — the
thermostat's on-time this session only sustained 28 clean samples per
cycle, and per the bench protocol that shortfall is reported rather than
padded across cycles.

`sandwich_cycle_2` is the one used for the ratio, since it is the point with
a mains voltage reading taken during the same plateau. Updated to use the
measured `R_hot = 70.8531 Ω` (`docs/measurements/reference-load-resistance.md`)
in place of the nameplate value:

```
I_primary  = V_mains / R_hot       = 236.75 / 70.8531   = 3.34142 A
I_secondary = V_burden / 22 Ω      = 0.036420 / 22       = 0.0016555 A
ratio_effective = I_primary / I_secondary                = 2018.4
```

(For reference, the nameplate-only figure was `ratio_effective ≈ 2027.0` —
the two agree to 0.42%, the same gap between the measured and nameplate
resistances.)

## Conclusion

**`ratio_effective ≈ 2018`**, from the single Method A point at 3.34 A.

- 0.9% from the currently configured `ctRatio = 2000`.
- 12.1% from the datasheet's alternative figure of 1800.

The measured ratio sits decisively closer to 2000: the gap to 1800 is an
order of magnitude larger than the total uncertainty on this figure (~1.7%,
see below — mains-voltage spec, the now-measured resistance, and firmware
repeatability). **2000 is the value supported by this measurement; 1800 is
not.**

This rests on one current point (3.34 A) rather than the two-method,
multi-current cross-check the issue originally called for — see below.

## Uncertainty budget

`ratio_effective = (V_mains / R) / (V_burden / 22 Ω)`, so the uncertainty of
the derived `calibrationFactor` is the combination of the terms feeding that
expression.

**Two terms cancel and do not contribute.** The burden resistor's tolerance
and the ADC's gain error appear both in the measurement that fixes the factor
and in the firmware computation that later applies it. Whatever their true
values, they divide out. This is why the result is tighter than a naive sum
of component tolerances would suggest.

**`R` is now bounded**, per `docs/measurements/reference-load-resistance.md`:
measured directly (`R_hot = 70.8531 Ω`, a 0.42% agreement with the
nameplate), not assumed from the nameplate alone.

| term | contribution | basis |
|---|---|---|
| `V_mains` | 1.13% | Fluke 117, ±(1.0% + 3 digits) at 236.75 V |
| `V_burden` (firmware) | 0.13% | cycle-to-cycle repeatability, two on-cycles |
| `R` of the sandwich maker | 1.23% | measured directly; see breakdown below |

Combined in quadrature: **~1.7%**. This is the total uncertainty on
`ratio_effective` and on `calibrationFactor` derived from it — no longer
bounded only below, as it was before this term was closed.

**Where the `R` term's 1.23% comes from** (full derivation in
`docs/measurements/reference-load-resistance.md`): the multimeter's own Ω
accuracy spec on the cold reading (1.18%, ±(0.9% + 2 digits) at 70.7 Ω),
combined with the cold/hot `vrms` ratio's sampling uncertainty (0.32%,
standard error of the mean — see that document's caveat on why this is
likely optimistic) and a small mains-drift correction term (0.10%).

This total (~1.7%) is higher than a since-superseded estimate in this
document that guessed "roughly 1.2%" before the measurement was taken — the
Ω-range spec turned out to be nearly as large a contributor as the
V-range spec, and the two add in quadrature rather than one dominating.
Still a hard bound, which was the point of closing this term.

**Historical note, since withdrawn.** An earlier draft of this section cited
agreement with two smart-plug readings (70.51 Ω, 70.68 Ω) as evidence that
the nameplate was accurate to 0.2%. That justification was withdrawn before
any smart-plug-based number was used in a real calibration: the smart plug
is the ground truth reserved for validating the system, and using it to
underwrite the calibration would make the validation partly
self-referential. The direct multimeter + firmware-ratio measurement in
`reference-load-resistance.md` replaces that withdrawn justification with a
traceable one.

**A caveat on the caveat:** the Fluke 117 accuracy figures above (both Ω and
V) are the commonly published specifications, not confirmed against this
unit's own manual.

## What this does not do

- **Does not set `ctRatio` or `calibrationFactor`.** Both are left
  unchanged in the firmware, together with the pending decision on
  `calibrationFactor` referenced in #10.
- **Does not include an independent Method B ratio point.** By decision
  (documented above), `I_load` for the small load was not measured
  independently, so the multi-turn setup did not produce its own
  `ratio_effective` — only the turn-count verification, the low-current
  comparison, and the ADC cross-check.
- **Tests only one current (3.34 A).** The issue suggested checking more
  than one point if time allowed; that did not happen this session. A
  second Method A point, or the skipped Method B current-in-series
  measurement, would strengthen this beyond a single data point.
- **Does not touch anything under `lib/`.**

## Acceptance criteria

- [ ] Effective ratio measured by method B, at 10 turns, with ≥ 30 firmware
      samples — **not met**; see "Method B: scope note" above.
- [x] Cross-check by method A recorded
- [x] 1-turn versus 10-turn comparison recorded, with its reading on
      low-current behaviour
- [x] `docs/measurements/turns-ratio.md` committed
- [x] No smart-plug readings used anywhere
- [x] `ctRatio` and `calibrationFactor` left unchanged
- [x] No changes under `lib/`
