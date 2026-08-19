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

**Only Point 2 is usable.** The Fluke 117 specifies AC accuracy from 1% to
100% of range, i.e. at or above 6.0 mV on its 600 mV mV-AC range. The
soldering iron's 3.45 mV falls below that floor; the sandwich maker's
37.1 mV does not. The `k_new` of 1.19 from Point 1 carries no weight, and the
apparent disagreement between the two points is a property of the meter, not
of the sensor.

That the transformer cannot be the cause follows from three independent
arguments, developed at length in `PESQUISA-sct013-baixa-corrente.md`
(outside the repository) and in `ct-linearity.md`.

The decisive one is structural: the multimeter and the firmware measure
the *same physical node*, the voltage across the 22 Ω burden. Any ratio error
of the transformer is already contained in that voltage and affects both
instruments identically, so it cannot produce a disagreement between them.

Two further checks point the same way. Converting the readings into an
apparent turns ratio, the multimeter implies 1467:1 at the fan, *below* the
nominal ratio — impossible for a passive transformer, whose magnetising
current can only raise the apparent ratio, never lower it. And the Fluke 117
specifies AC accuracy only from 1% to 100% of range, i.e. at or above 6.0 mV
on the 600 mV mV-AC range: three of the four points sit below that floor, and
the only point inside it (the sandwich maker, 37.1 mV) is the only one where
the instruments agree.

The observed gap is roughly constant in absolute terms (0.55-0.73 mV across
loads differing seventeen-fold in current) rather than proportional, which is
the signature of an instrumentation offset — consistent with the meter's mV
AC range being DC-coupled while the firmware removes the mean, and with the
two differing in bandwidth.

**The discrepancy is a measurement artifact.** `k_new = 1.0639`, from the
sandwich maker, is the only value supported by a measurement taken inside the
meter's specified range.

**Firmware change pending**: `calibrationFactor` is still `1.0212` in
`main.cpp` as of this commit. Setting it follows from the shape of the #15
curve and is tracked separately, together with how the residual error is
declared in the thesis — see #15's "What this does not do".

## Uncertainty

The dominant term is the multimeter's specified range. Only the sandwich
maker point (37.1 mV) was taken inside the Fluke 117's specified AC range;
the other three fall below its 6.0 mV floor and cannot support a calibration
figure. On that single valid point the two instruments agree to 1.5%, within
the meter's own ±1.8% at that reading.

Two uncertainties remain open and are larger than anything above:

- **Turns ratio.** The sensor's datasheet is internally inconsistent, giving
  a turns ratio of 1:1800 alongside nominal values of 100 A : 50 mA, which
  imply 2000:1. The firmware uses 2000. If the effective ratio is 1800, every
  reported current carries an 11% systematic error. This is not visible in
  any burden-voltage comparison, because the ratio is applied afterwards.
- **Operating region.** All four loads draw between 0.19% and 3.18% of the
  sensor's 100 A rating. IEC 61869-2 specifies accuracy down to 1% of rated
  current and IEEE C57.13 down to 10%; no accuracy class covers most of this
  range. The correct statement for the write-up is that no class applies
  here, not that the sensor is out of class.

## Note on the burden value

The 22 Ω burden exceeds the 10 Ω maximum given in the sensor's datasheet.
This is recorded for completeness, not as a limitation: at the currents used
here the constraint does not bind.

That maximum exists to keep the core out of saturation, and saturation
depends on the voltage the secondary must develop, which scales with primary
current. At the sensor's rated 100 A, a 22 Ω burden would demand 1.1 V from
the secondary — the regime the limit is written for. At the largest load in
this work (3.4 A) the secondary develops 37 mV, thirty times less. The
operating point is far below where saturation becomes a consideration.

22 Ω is also the value used by the OpenEnergyMonitor reference design, and
the value at which that project's published characterisation of this sensor
was performed; it found waveform distortion from saturation negligible for
normal use.

The documented cost of a higher burden is increased phase error, more
pronounced at low current. That does not affect this work: the system
computes apparent power, a magnitude, and never derives active power from a
voltage and current pair, so phase error does not enter any reported
quantity.
