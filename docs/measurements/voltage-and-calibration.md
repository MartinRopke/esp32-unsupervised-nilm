# Mains voltage under load and calibration

Capture date: 14 Aug 2026
Firmware commit: `14df091249762fc00983513af482fcd9083cd882`

Multimeter: Exbom MD-180L. **Not True RMS** — sensitivity calibrated for a
sinusoidal waveform, per the manufacturer's specification. Acceptable here
because the mains waveform is approximately sinusoidal; the residual error is
on the order of 1-2%, smaller than the spread between calibration methods
reported below.

Rig: sandwich maker on the sacrificial power strip, current transformer on the
exposed conductor, ESP32 running the firmware above. Every voltage was measured
with the multimeter, never with the smart plug, which is reserved as ground
truth for validation.

## No load versus under load

| moment | measured V~ |
|---|---|
| before switching the sandwich maker on | 242.0 V |
| with the sandwich maker conducting | 240.0 V |

Difference: 2.0 V at 3.06 A, implying about 0.65 Ω in the path. Not
significant, and below the threshold that would suggest contact resistance at
the sacrificial strip's splice.

## Simultaneous ESP32 reading

The sandwich maker was allowed to reach thermal steady state: the element had
already cycled off and back on before any reading used for calibration. Two
distinct conduction plateaus were captured to check consistency, each taken as
the mean of `power` samples above 500 VA within the plateau, excluding the
switch-on ramp.

| plateau | samples | V~ (multimeter) | vrms (V) | irms (A) | power (VA) |
|---|---|---|---|---|---|
| 1 | 30 | 240.0 | 0.0364 | 3.0639 | 674.05 |
| 2 | 26 | 240.0 | 0.0369 | 3.1129 | 684.85 |

The two plateaus agree within 1.6% on `irms`.

## Computing the new factor

```
R          = 220² / 686              = 70.5539 Ω
I_expected = V / R                   = 240.0 / 70.5539 = 3.4017 A
```

`I_uncalibrated` is the reported `irms` divided by the current factor, 0.9271.

| plateau | I_uncalibrated (A) | k_new | error of the current factor |
|---|---|---|---|
| 1 | 3.3048 | 1.0293 | +11.02% |
| 2 | 3.3577 | 1.0131 | +9.28% |
| mean | — | **1.0212** | **+10.15%** |

## Conclusion

`k_new` lands around **1.02**, close to unity. The current factor, 0.9271, was
obtained by assuming 220 V on a network that actually runs near 240 V under
load, which is what the measured error confirms.

**No firmware change was applied here.** Adopting `k_new` is tracked separately.

## Uncertainty

The dominant uncertainty is not the multimeter but the disagreement between
estimation methods: this measurement gives 1.021, while cross-checking the fan
against the smart plug gives 0.980 — a spread of about 4%.

This calibration also depends on the sandwich maker's nameplate (686 W at
220 V) to derive the element resistance, so it inherits the nameplate's
tolerance. Measuring the burden voltage directly with a multimeter that has a
millivolt AC range removes that dependency and would reduce the dominant term;
that path remains open in #10.

Because the factor is a single multiplier and `vrms` is recorded in every log,
data captured before any change can be reprocessed rather than re-measured.

## Direct burden-voltage calibration (issue #10)

Capture date: 18 Aug 2026
Firmware commit: `1741336` (differential `GAIN_SIXTEEN` chain, `calibrationFactor = 1.0212`)

Multimeter: Fluke 117 (True RMS), mV AC mode — fixed 600.0 mV range, 0.1 mV
resolution, accuracy ±(1.0% of reading + 3 counts) at 45-500 Hz. Probed
directly across the burden resistor's two terminals (the same A0/A1 nodes the
ADC differences), removing the nameplate-power dependency of the calibration
above per the methodology in #10.

Rig: same installation as above. Two resistive loads (PF ≈ 1) read at thermal
steady state via the live serial stream (`vrms`/`irms`/`power` at 1 Hz). The
sandwich maker's reading was taken only after its thermostat had cycled off
and back on once, following the same precaution as the original #8
methodology; the soldering iron has no thermostat and was read after its
initial ramp settled.

### Point 1 — soldering iron (60 W nameplate, no thermostat)

| quantity | value |
|---|---|
| `V_burden` (multimeter) | 3.4-3.5 mV, mean 3.45 mV |
| firmware `vrms` (back-calculated, n=41 samples, ~40 s plateau) | ~2.90 mV |
| firmware `irms` (reported, calibrated) | 0.2690 A |

```
I_ref          = V_burden / 22 Ω × 2000            = 0.3136 A
I_uncalibrated = irms_reported / 1.0212            = 0.2634 A
k_new          = I_ref / I_uncalibrated            = 1.19
```

### Point 2 — sandwich maker (686 W nameplate, thermostatic)

| quantity | value |
|---|---|
| `V_burden` (multimeter) | 37.2-37.3 mV, mean 37.25 mV |
| firmware `vrms` (reported, n=24 samples — cycle cut out mid-collection) | 35.0 mV |
| firmware `irms` (reported, calibrated) | 3.2508 A |

```
I_ref          = V_burden / 22 Ω × 2000            = 3.386 A
I_uncalibrated = irms_reported / 1.0212            = 3.183 A
k_new          = I_ref / I_uncalibrated            = 1.0639
```

## Conclusion

The two points disagree on `k_new` (1.19 vs. 1.0639) by more than measurement
noise alone should explain. The original reading here attributed that entirely
to the iron's `V_burden` sitting near the multimeter's resolution floor,
compounded by the heating element's resistance rising at operating
temperature (the #8 explanation). That reading is **superseded** by the
four-point characterisation in #15 (`docs/measurements/ct-linearity.md`):
measuring two more loads in the same low-current region (fan, laptop
charger — both, like the iron, under 0.5% of the CT's 100 A rating) showed
the same large correction the iron needed (`k_required` 1.19-1.34), while the
sandwich maker (3.18% of rating) needed a much smaller one (1.02) that does
not overlap the other three even accounting for the multimeter's resolution
floor.

That pattern — a large, roughly consistent correction across every load
under ~0.5% of the CT's rated current, and a much smaller one at 3.18% of
rating — is consistent with the CT's magnetising current becoming
non-negligible at small fractions of its rating, not solely with multimeter
noise. The multimeter's resolution floor is still real and does limit how
finely the three low-current points can be distinguished from each other
(see #15's uncertainty analysis), but it does not explain why *all three* of
them disagree with the sandwich maker in the same direction and by a similar
amount.

**Firmware change pending**: `calibrationFactor` is still `1.0212` in
`main.cpp` as of this commit. Setting it follows from the shape of the #15
curve and is tracked separately, together with how the residual error is
declared in the thesis — see #15's "What this does not do".

## Uncertainty

The dominant term is no longer attributed solely to the multimeter's
resolution floor at one point (the iron). #15 shows the correction needed is
load-dependent across the CT's low-current range, so a single factor tuned
against any one load — including the sandwich maker point used here — will
under-correct the smaller loads that are also test loads in this work (fan,
laptop charger). See #15 for the full four-point picture and its own
uncertainty analysis.
