# Measuring the reference load's resistance directly

Capture date: 20 Aug 2026
Firmware commit: `99b25dce846c7c4633dc4e0bcc59da2e3b92b2cb`

Multimeter: Fluke 117. True RMS. Ω accuracy figure below is the commonly
published Fluke 117 600 Ω-range specification, ±(0.9% + 2 digits), **not
confirmed against this unit's own manual** — same caveat already carried for
the AC-voltage spec in `turns-ratio.md`.

Rig: sandwich maker through the SCT-013-000 current transformer / 22 Ω
burden resistor rig, ESP32 + ADS1115 running the firmware above. `vrms` is
the firmware's own RMS reading of the burden voltage, printed with six
decimal places. No smart-plug readings are used anywhere in this document.

Raw serial samples for both capture attempts, including the discarded one,
are in `docs/measurements/reference-load-resistance.csv`
(`included_in_mean` marks which samples survived trimming).

## Step 1: cold resistance on the multimeter

Appliance unplugged and cold (last used well over 30 minutes prior). Ambient
temperature: ~24 °C. Indicator lamps: **present, two** (red and green),
wired across the same two plug pins being measured.

Lead nulling: an initial few exploratory touches (0.3, 0.4, 0.1, 0.2 Ω)
showed the null value bounces with contact pressure, so a deliberate set of
three touches was taken and used as the baseline:

| null touch | reading |
|---|---|
| 1 | 0.3 Ω |
| 2 | 0.3 Ω |
| 3 | 0.3 Ω |

**Null baseline: 0.3 Ω.**

Three readings across the plug pins, probes fully lifted and reseated
between each:

| reading | raw | null-corrected |
|---|---|---|
| 1 | 71.0 Ω | 70.7 Ω |
| 2 | 71.0 Ω | 70.7 Ω |
| 3 | 71.0 Ω | 70.7 Ω |

**R_cold = 70.7 Ω.**

This is 4.8% above the issue's orientation estimate of ~67.5 Ω (nameplate
Ω divided by an assumed 4.6% cold-to-hot rise). As the step 2 result below
shows, that assumed 4.6% rise does not hold for this element — the
discrepancy resolves itself rather than indicating a bad reading.

## Step 2: cold-to-hot `vrms` ratio from the firmware

### Discarded attempt: connection artifact

The first capture attempt showed a smooth oscillation in `vrms`, period
roughly 5–6 s, sample-to-sample relative stdev **~4.4–4.8%** across the
cold window, the full first heating ramp, and the hot plateau alike —
essentially identical in signature to the "disturbed" captures documented in
`turns-ratio.md` (±4.5% relative stdev there, traced at the time to a loose
lead at the burden resistor, against ±0.35–0.4% on every clean capture in
this project).

The rig was power-cycled and its connections physically inspected before a
second attempt (the USB-serial port also briefly vanished from the system
during this, consistent with something in the rig having been physically
disturbed). A quick 25 s live check with a fan as a steady, non-thermostatic
load confirmed the fix: relative stdev dropped to 1.01%. **What specifically
was reseated is not established** — a plausible but unconfirmed hypothesis
raised during the session was a bad socket adapter; it was not isolated
against the alternative (the burden/CT connection, the culprit in the prior
documented case) before moving on. This is a genuine gap, noted rather than
resolved — see "What this does not do."

The discarded attempt's samples are in the CSV (`attempt=1`,
`included_in_mean=false`) but not used for any figure below.

### Accepted capture

Thermostatic load, so per the bench-measurement protocol: captured from a
cold switch-on, through the first cutoff, and into the following on-phase.

| phase | n (trimmed) | mean vrms (V) | rel. stdev | mains V~ | notes |
|---|---|---|---|---|---|
| cold | 8 | 0.036554 | 0.79% | 236.25 V (avg of 236.3/236.2) | first sample after switch-on excluded (partial RMS window); remaining 8 span ~1–8 s post switch-on |
| hot | 31 | 0.036421 | 0.91% | 235.9 V (avg of 236.0/235.8/235.9) | on-transition ramp-in (3 samples) and the following cutoff (1 sample) trimmed from 35 raw hot-phase samples |

A steady-state sanity check on the untrimmed first-cycle heating phase
(t = 32–147 s, n = 115) gives 0.86% relative stdev — consistent with the
cold and hot windows above, confirming the oscillation seen in the discarded
attempt is gone throughout, not just improved at the edges.

Mains voltage drifted slightly between the two readings (236.25 V → 235.9 V,
−0.15%), corrected for below rather than ignored.

```
R_hot = R_cold × (vrms_cold / vrms_hot) × (V_mains,hot / V_mains,cold)
      = 70.7   × (0.036554 / 0.036421)  × (235.9 / 236.25)
      = 70.7   × 1.003652               × 0.99852
      = 70.8531 Ω
```

(The mains-drift term is the extension of the issue's `R_hot = R_cold ×
(vrms_cold/vrms_hot)` to account for the drift actually observed; at
constant mains voltage the two are the same formula.)

**R_hot = 70.85 Ω**, against the nameplate's 70.5539 Ω — a **0.42%**
difference, well within this measurement's own uncertainty (below). The
nameplate is confirmed, not contradicted.

Note the underlying surprise: `vrms_cold` and `vrms_hot` differ by only
0.37%, meaning this element's resistance barely rises from switch-on to
thermal steady state — nothing like the 4.6% the issue cited for
orientation. That figure was never claimed as more than a rough guide, and
the direct measurement supersedes it.

## Recomputed `calibrationFactor`

Using `R_hot` above with the existing turns-ratio capture (`sandwich_cycle_2`
from `turns-ratio.md`: `vrms` 0.036420 V, mains 236.75 V):

```
I_primary       = V_mains / R_hot        = 236.75 / 70.8531   = 3.34142 A
I_secondary     = vrms / 22 Ω            = 0.036420 / 22      = 0.0016555 A
ratio_effective = I_primary / I_secondary                     = 2018.43
calibrationFactor = ratio_effective / 2000                    = 1.0092
```

Against the currently configured **1.0135**: a **−0.42%** difference.

## Uncertainty budget (revised)

The unbounded `R` term in `turns-ratio.md`'s budget is now bounded:

| term | contribution | basis |
|---|---|---|
| `R_cold` (multimeter) | 1.18% | Fluke 117 Ω spec, ±(0.9% + 2 digits) at 70.7 Ω on the 600 Ω range |
| `vrms` cold/hot ratio | 0.32% | standard error of the mean, n=8 and n=31 (Type A; see caveat below) |
| mains-drift correction | 0.10% | spread across the paired multimeter V readings |
| **`R_hot` combined** | **1.23%** | quadrature sum of the three above |
| `V_mains` (`sandwich_cycle_2`) | 1.13% | Fluke 117 V spec, carried over unchanged from `turns-ratio.md` |
| `V_burden` (firmware) | 0.13% | cycle-to-cycle repeatability, carried over unchanged from `turns-ratio.md` |
| **Total (`calibrationFactor`)** | **~1.7%** | quadrature sum of `R_hot`, `V_mains`, `V_burden` |

This is higher than the issue's own "roughly 1.2%" orientation estimate —
the multimeter's Ω-range spec (1.18%) turned out to be nearly as large a
contributor as the V-range spec (1.13%), and the two don't cancel; they add
in quadrature. Still, **~1.7% is a hard bound**, which is the point: the
previously unbounded term is now bounded, even if not quite as tightly as
guessed.

**Caveat on the `vrms`-ratio term**: 0.32% is a within-window (Type A)
estimate from sample counts alone. Unlike `turns-ratio.md`'s `V_burden`
term — which compared two independent, fully separate on-cycles and got
0.13% — this session only captured one clean cold window and one clean hot
window, with no independent repeat to catch a systematic offset affecting
the whole window (the kind of thing the discarded first attempt's
connection issue could plausibly cause). Treat 0.32% as optimistic rather
than as tightly validated as the analogous `turns-ratio.md` figure.

**Caveat carried over**: the Fluke 117 accuracy figures (both Ω and V) are
the commonly published specifications, not confirmed against this unit's
own manual.

## Conclusion

`calibrationFactor` recomputed to **1.0092**, a **0.42%** difference from
the previously configured **1.0135** — well inside the ~1.7% uncertainty
above, so this is a confirmation rather than a correction: the previous
value was never wrong at any resolution this measurement can distinguish.
`src/main.cpp` has nonetheless been updated to the newly measured value,
1.0092, as the more precise of the two available point estimates.

## What this does not do

- **Does not isolate the root cause of the discarded attempt's connection
  artifact.** A bad socket adapter was hypothesized but not distinguished
  from a burden/CT-side connection (the actual cause the last time this
  exact signature appeared, per `turns-ratio.md`). The fix was confirmed
  to work; why it worked was not established.
- **Does not validate the `vrms`-ratio uncertainty term against an
  independent repeat.** Only one cold window and one hot window were
  captured this session; see the caveat in the uncertainty budget above.
- **Does not change `ctRatio`.** Only `calibrationFactor` was updated in
  `src/main.cpp`, to the newly measured 1.0092 — a change the recomputed
  value's own uncertainty budget calls unnecessary (0.42% difference inside
  a ~1.7% bound), made anyway as the more precise of the two point
  estimates.
- **Does not touch anything under `lib/`.**
- **Does not use any smart-plug reading**, for the resistance, the ratio,
  or as a cross-check.

## Acceptance criteria

- [x] Appliance measured cold, at least three readings, probes reseated
      between them
- [x] Multimeter leads nulled and the nulling value recorded
- [x] Presence or absence of a parallel indicator lamp recorded
- [x] Cold-to-hot `vrms` ratio captured, with mains voltage checked at both
      ends (and corrected for, since it drifted slightly)
- [x] `R_hot` computed and compared against the nameplate's 70.5539 Ω —
      agrees within 0.42%
- [x] `calibrationFactor` recomputed; agrees within uncertainty, updated in
      `src/main.cpp` to the new value (1.0135 → 1.0092) anyway, change noted
- [x] Uncertainty budget in `turns-ratio.md` revised with `R` now bounded
- [x] `docs/measurements/reference-load-resistance.md` committed
- [x] No smart-plug readings used anywhere
- [x] No changes under `lib/`
