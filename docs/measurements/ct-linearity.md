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
  at 37.00, 37.15 and 36.28 mV mean, consistent with the 36.545 mV reported
  here to within ~2%, so the short plateau does not look like an outlier.

## Result

| load | nominal | i_approx | % of CT rating | `V_burden` multimeter | `V_burden` firmware | samples | deviation | k_required |
|---|---|---|---|---|---|---|---|---|
| fan | 50 W | 0.19 A | 0.19% | 2.85 mV | 2.123 mV | 46 | -25.5% | 1.3424 |
| soldering iron | 60 W | 0.26 A | 0.26% | 3.45 mV | 2.899 mV | 46 | -16.0% | 1.1901 |
| laptop charger | 65 W | 0.47 A | 0.47% | 5.75 mV | 4.618 mV | 45 | -19.7% | 1.2451 |
| sandwich maker | 686 W | 3.18 A | 3.18% | 37.1 mV | 36.545 mV | 27 | -1.5% | 1.0152 |

## Validity of each point

Only one of the four points is usable as a reference. The Fluke 117 specifies
AC accuracy from 1% to 100% of range, i.e. at or above **6.0 mV** on its
600 mV mV-AC range. Applying its ±(1.0% + 3 counts) figure:

| load | `V_burden` mm | ±accuracy | inside specified range? |
|---|---|---|---|
| fan | 2.85 mV | ±0.33 mV | no |
| soldering iron | 3.45 mV | ±0.33 mV | no |
| laptop charger | 5.75 mV | ±0.36 mV | no |
| sandwich maker | 37.1 mV | ±0.67 mV | **yes** |

The sandwich maker is the only measurement taken within the meter's specified
range, and it is also the only point where the two instruments agree — to
1.5%, inside the meter's own ±1.8% at that reading. The `k_required` values
of the other three carry no weight.

## Reading

The `k_required` column does **not** measure a property of the current
transformer, and cannot. Three independent arguments:

**1. The comparison is structurally blind to CT error.** The multimeter and
the firmware measure the *same physical node* — the voltage across the 22 Ω
burden. Any ratio error of the transformer is already present in that voltage
and therefore affects both instruments identically. It cannot, by
construction, produce a disagreement *between* them. Whatever the CT does at
low current, it cancels out of this comparison.

**2. The multimeter's readings are physically impossible.** Converting each
reading into an apparent turns ratio, the multimeter implies 1467:1 at the
fan — *below* the nominal ratio. In a passive current transformer the
magnetising current can only divert secondary current, which can only make
the apparent ratio *higher* than nominal, never lower. The firmware's
readings imply 1969:1, which is consistent.

**3. The disagreement tracks the meter's specified range, not the current.**
As tabulated above, the three points where the instruments disagree are
exactly the three taken below the meter's 6.0 mV floor, and the one point
inside its specified range is the one where they agree.

The two instruments also do not measure the same quantity: the Fluke's mV AC
range is DC-coupled, while the firmware removes the running mean before
computing RMS; and their bandwidths differ (1 kHz specified for the meter
against a 430 Hz Nyquist limit for the ADC at 860 SPS). Both differences
would appear as a roughly constant additive offset rather than a
proportional error — which is the signature actually observed: the gap is
0.55-0.73 mV in three of the four points, *including* the largest load,
instead of growing with current.

**Conclusion: the discrepancy is a measurement artifact of probing millivolt
signals beside an energised conductor, not a property of the transformer.**
The four test loads remain usable.

What this does *not* establish is the chain's absolute accuracy against the
true primary current. That question stays open, and no standard helps: IEC
61869-2 specifies down to 1% of rated current and IEEE C57.13 down to 10%,
while these loads sit between 0.19% and 3.18% of the sensor's 100 A rating.
The correct statement is that **no accuracy class covers this operating
region**, not that the sensor is out of class.

## What this does not do

This does not set `calibrationFactor`. That decision follows separately, per
#10 and #15, together with how the residual error is declared in the thesis.
