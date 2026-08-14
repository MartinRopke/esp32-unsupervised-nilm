# Background noise

Capture date: 14 Aug 2026
Firmware commit: `14df091249762fc00983513af482fcd9083cd882`

Load used for the steady-load condition: 60 W pencil-type soldering iron, no
temperature control (purely resistive, constant draw). Pre-heated for about a
minute before the capture started, as required — the element's resistance
varies while it warms up.

Raw data: `background-noise.csv` (columns `condition,t_s,vrms,irms,power`).

## Idle

Power strip with no load connected.

| metric | value |
|---|---|
| samples | 131 |
| duration | ~131 s |
| mean (VA) | 0.2785 |
| standard deviation (VA) | 0.0070 |
| minimum (VA) | 0.2604 |
| maximum (VA) | 0.3033 |
| 30 VA in standard deviations | ~4282x |

## Steady load (60 W soldering iron)

| metric | value |
|---|---|
| samples | 131 |
| duration | ~131 s |
| mean (VA) | 53.9835 |
| standard deviation (VA) | 0.5634 |
| minimum (VA) | 52.8571 |
| maximum (VA) | 54.9158 |
| 30 VA in standard deviations | ~53x |

Power settled below the 60 W nameplate figure because the element's resistance
rises with temperature — expected, and consistent with the steady-state
capture.

## Conclusion

In both conditions, 30 VA sits far above three standard deviations from the
mean (idle: ~4282x; steady load: ~53x). Three standard deviations put the
technical floor at roughly 1.7 VA under load, so the adopted threshold is about
18 times more conservative than strictly necessary.

Extrapolating the ~1% relative noise of the steady-load condition to the
sandwich maker (776 VA) gives a standard deviation of roughly 8 VA, still well
below the threshold.

The 30 VA threshold has ample margin even in the noisier scenario, with signal
present. No adjustment is required.

## Limitation

This noise floor is not representative of a full residential panel. The current
transformer sits on an isolated power strip, so the idle baseline is unusually
quiet: no refrigerator, no standby loads, no other circuits. On a real service
panel the floor would be substantially higher, and the threshold would likely
need to be revisited.
