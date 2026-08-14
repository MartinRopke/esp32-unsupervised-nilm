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
