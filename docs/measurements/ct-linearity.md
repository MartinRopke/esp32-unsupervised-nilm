# CT linearity across the load range

Follow-up to #10, tracked in #15.

Capture date: 18 Aug 2026
Firmware: base commit `1741336`, with the `vrms` serial print widened from 4 to
6 decimal places for this capture (the only firmware change in scope for
#15). Not committed separately before capture; committed alongside this
writeup.

Multimeter: Fluke 117 (True RMS), mV AC mode. This mode has a single fixed
600.0 mV range with 0.1 mV resolution; accuracy is ±(1.0% of reading + 3
counts) at 45-500 Hz, which covers the 50/60 Hz mains signal measured here.
Probed directly across the burden resistor's two terminals for all four
points.

## Method

Each load was run to steady state and probed on the multimeter while the
firmware's `vrms` was captured live from the serial stream (`>vrms:` lines,
one per 1 s RMS window). `deviation_pct` is the firmware reading relative to
the multimeter reading; `k_required = v_burden_mm / v_burden_fw` is the
multiplier that would bring the firmware's burden-voltage reading in line
with the multimeter reference at that point.

- **Fan** (head locked, speed 3): steady within seconds of power-on, no
  special wait.
- **Soldering iron**: flat across a full 60 s check with no visible ramp (no
  thermostat).
- **Laptop charger** (battery not full): baseline ~4.3 mV with periodic
  bursts to ~5.5-5.8 mV every ~6 s, consistent with the charging IC's duty
  cycle. The 45 s / 45-sample capture window averages over this.
- **Sandwich maker** (thermostatic): measured only after the thermostat's
  first off/on cycle completed from a cold connection, per the original
  methodology in #8/#10. The appliance's "on" duty cycle at steady state
  turned out to be short (~30-60 s), so a single continuous plateau topped
  out at 27 clean samples (34 raw samples minus the leading/trailing samples
  where the 1 s RMS window straddles the on/off transition) rather than the
  full 30-sample target reached by the other three points. Three other
  on-phases captured during setup, not used for the reported value, landed
  at 37.00, 37.15 and 36.28 mV mean -- consistent with the 36.545 mV reported
  here to within ~2%, so the short plateau does not look like an outlier.

## Result

| load | nominal | i_approx | % of CT rating | `V_burden` multimeter | `V_burden` firmware | samples | deviation | k_required |
|---|---|---|---|---|---|---|---|---|
| fan | 50 W | 0.19 A | 0.19% | 2.85 mV | 2.123 mV | 46 | -25.5% | 1.3424 |
| soldering iron | 60 W | 0.26 A | 0.26% | 3.45 mV | 2.899 mV | 46 | -16.0% | 1.1901 |
| laptop charger | 65 W | 0.47 A | 0.47% | 5.75 mV | 4.618 mV | 45 | -19.7% | 1.2451 |
| sandwich maker | 686 W | 3.18 A | 3.18% | 37.1 mV | 36.545 mV | 27 | -1.5% | 1.0152 |

## Reading

The required correction is **not strictly monotonic** in the central
estimates: `k_required` runs 1.3424 (fan) -> 1.1901 (iron) -> 1.2451
(charger) -> 1.0152 (sandwich maker), and iron's value dips below charger's
even though iron draws less current.

That dip does not survive the multimeter's own resolution floor. The Fluke
117's mV AC accuracy is ±(1.0% + 3 counts) = ±(1.0% + 0.3 mV), which at these
signal levels is large relative to the signal itself:

| load | `V_burden` mm | ±accuracy | range | `k_required` range |
|---|---|---|---|---|
| fan | 2.85 mV | ±0.33 mV | 2.52-3.18 mV | 1.19-1.50 |
| soldering iron | 3.45 mV | ±0.33 mV | 3.12-3.78 mV | 1.07-1.31 |
| laptop charger | 5.75 mV | ±0.36 mV | 5.39-6.11 mV | 1.17-1.32 |
| sandwich maker | 37.1 mV | ±0.67 mV | 36.43-37.77 mV | 1.00-1.03 |

The fan, iron and charger `k_required` ranges all overlap each other
substantially -- the multimeter cannot resolve a reliable ordering among
these three points at these signal levels. What it can resolve clearly is
the break between that low-current cluster (all under 0.5% of the CT's 100 A
rating, `k_required` roughly 1.19-1.34 at the central estimates) and the
sandwich maker (3.18% of rating, `k_required` 1.02, with a range that does
not overlap any of the other three).

That break is the answer to what #15 set out to check: the correction needed
is clearly load-dependent, is much larger below ~0.5% of the CT's rated
current than at 3.18% of rating, and the disagreement between the original
iron and sandwich-maker calibration points in #10 is not explained by the
multimeter's noise floor alone (voltage-and-calibration.md previously
concluded that). It is consistent with the CT's magnetising current
becoming non-negligible at these small fractions of its rating -- exactly
the mechanism #15 set out to test. The finer question of whether the
correction varies smoothly *within* the low-current cluster, or has some
other shape below 0.5% of rating, is not resolvable with this multimeter and
is out of scope here.

## What this does not do

This does not set `calibrationFactor`. That decision follows separately, per
#10 and #15, together with how the residual error is declared in the thesis.
