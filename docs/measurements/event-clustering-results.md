# Event clustering results session

The results session for Stage 3 (DBSCAN clustering) after #26 and #28, following the "Ensaio de
resultados da Etapa 3" section of `HANDOFF-agrupamento.md`. This is the first time the consolidated
report (tariff, per-cluster operating time, cost) ran on the hardware, and the first capture under a
firmware that includes both the merger release-clock fix (#28) and the report module (#26). Earlier
sessions (24/08, 25/08, confirm) remain valid as their own record but are not comparable to this one
firmware-for-firmware; the 25/08 and 26/08 files are untouched.

Date: 26 Aug 2026.
Firmware: commit `c221921` (working tree clean at this commit at build/flash time; `pio run -e esp32dev
-t upload --upload-port /dev/cu.usbserial-0001` succeeded on the second attempt after one upload-time
serial corruption error).

## Native test suite

56 test cases, 56 succeeded (`pio test -e native`), same suite as `c221921`'s working tree. `pio run -e
esp32dev` succeeded (RAM 6.7%, Flash 24.5%).

## Pre-session fix: tariff comments still read as pending

Before flashing, `src/main.cpp` and `lib/cluster_report/cluster_report.h` still had comments calling the
tariff `PROVISIONAL` and asking for a sourced value before quoting cost -- stale text contradicting the
26/08 decision recorded in the handoff (tariff is a declared installation constant, not something to
source). Fixed in commit `c221921` before flashing; the fix is comment-only, no behavioural change.

## Loads

- Sandwich maker, ~777-796 VA, thermostatic.
- Fan: head locked, no oscillation, speed 3, ~37-50 VA.
- Lenovo laptop charger: battery 43% noted before the boot-loop mishap (see below), re-checked at
  40% at the start of the charger item (after the session restarted) and 41% at the end -- narrow
  range, notebook kept near-idle as intended.
- Soldering iron, 60 W, ~53-63 VA.

## Capture

One continuous serial port opening for the entire valid session (`/dev/cu.usbserial-0001`, 921600
baud, DTR/RTS held low across open per the board's reset-on-reopen behaviour). Wall clock ~19:36 to
~20:30. `t_s` origin (`t_s=0`) is this boot's firmware start.

A first attempt (marco de sincronismo through all 6 sandwich-maker on/off pairs, ~t_s 1020-1710 of that
earlier boot) was lost to a boot-loop mishap before the charger item -- see "What went wrong". The
session restarted from the connection-artifact check; nothing from the lost attempt is in the committed
CSV or ground truth.

Single file, `event-clustering-results.csv`, spans the valid boot from `t_s=1` to `t_s=3256` (~54
minutes elapsed on this boot's clock, comfortably under the 71.6-minute `micros()` wraparound). `t_s`
monotonicity confirmed (0 violations) after removing 14 lines of intermittent serial binary noise (see
"What went wrong").

Ground truth in `event-clustering-results-ground-truth.csv`: 52 manually logged actions (26 on/off
pairs) across the connection-artifact check, the charger/fan/soldering-iron/sandwich-maker 5-cycle
items, and the (extended) autonomous-thermostat item. Cluster interpretation in
`event-clustering-results-cluster-map.csv`.

`event-clustering-results-report.txt` is the console block exactly as printed by `r`, unedited.
`event-clustering-results-report.csv` is the output of `c`; its 3 data rows were initially misrouted
into the live-CSV log by a capture-script bug (see "What went wrong") and recovered by hand from the
raw log -- content unedited, only relocated to the correct file.

## Results summary

3 clusters formed, 0 events reported as noise in the final aggregate (none of the 3 clusters is
`cluster_id=-1`; individual events transiently labelled `-1` mid-session were reassigned once enough
neighbours arrived, per the clusterer's online relabelling behaviour).

| cluster | appliance(s) | complete cycles | truncated | off-without-on | mean power (pair / cluster) | operating time |
|---|---|---|---|---|---|---|
| 1 | fan + soldering iron (fused, see below) | 10 | 1 (25.5137 VA·h open) | 0 | 51.8 / 52.3 VA | 00:13:02 |
| 2 | charger | 3 | 0 | 2 | 98.3 / 98.7 VA | 00:01:02 |
| 3 | sandwich maker | 8 | 0 | 2 | 783.3 / 783.6 VA | 00:05:09 |

Totals across the three unpaired-transition categories: 21 complete cycles, 1 truncated cycle, 4
off-without-on discarded.

59 events after merging; 9 of them with `fragments` > 1 (2 charger on-events fragments=2, 1 sandwich-maker
off-event fragments=2, 1 sandwich-maker on-event fragments=2, and 3 further sandwich-maker
thermostat-ramp events with fragments 2/4/3 -- consistent with the confirm session's finding that the
slow autonomous ramp fragments in part even after the #28 fix).

### Cluster 1 is a fusion of fan and soldering iron, not the fan alone

The fan's 10 events (37.1-50.0 VA) and the soldering iron's 10 events (53.3-62.8 VA) sit within
`epsilonVa=12` of each other end to end (gap of 3.3 VA between the fan's max and the iron's min) and
merged into one cluster. This is exactly the resolution-limit scenario the original handoff proposed as
an optional stress test (Passo 6) -- it happened unintentionally here because both loads ran in the same
session. The cluster's pooled mean (52.3 VA) matches neither device alone. **This is a measured result
for the thesis discussion, not a labelling error**: two appliances 1.24x apart in nominal power did not
separate under this `epsilonVa`.

A residual fragment from the charger's noisy start-up ramp (`t_s=898.02`, `delta_va=61.88`, on-direction,
part of charger cycle 3) also falls in this magnitude band and is presumed to have landed in cluster 1
by magnitude alone -- not confirmed against final per-event labels, since only the aggregate report (not
a per-event label dump) was captured this session.

### Power-factor assignment could not happen in this session, by design

`kClusterPowerFactorAssignments` is compiled into the firmware, and the cluster ids are only known after
the operator observes the session. Reflashing to fill it in would restart the board and erase the
clusterer's RAM state, destroying ~50 minutes of the session already run. Per the handoff's "A
atribuição de fator de potência não cabe na mesma sessão" section: **this is not a defect, it is the
supervision boundary appearing in the hardware.** The point-cost figures in
`event-clustering-results-cluster-map.csv` are computed offline from the committed report CSV
(`apparent_energy_vah x |power factor| / 1000 x tariff`), not by the firmware:

- Cluster 2 (charger): 1.725099 VA·h x 0.55 x R$0.92/kWh = **R$ 0.0009**
- Cluster 3 (sandwich maker): 67.085419 VA·h x 0.99 x R$0.92/kWh = **R$ 0.0611**
- Cluster 1 (fan + soldering iron, fused): no point PF assigned. It mixes two appliances with different
  power factors (fan 0.94, no dedicated soldering-iron category in `cluster_report.h`); forcing a single
  PF onto a fused cluster would be an invented estimate, not an interpretation. Only the device's
  unsupervised range (R$ 0.0049-0.0105, |PF| 0.47-1.00) applies.

Power factors and sources are the Hannagan et al. (2023, Sustainability 15(1):158) table already in
`cluster_report.h`.

## Scenario-by-scenario

1. **Connection-artifact check (both attempts)**: revised criterion (`sd(diff(power))/mean(power)`,
   excluding the thermostat-cutoff transition sample). First attempt (before the boot loop): 0.594%,
   clean. After the session restart: 0.104%, clean. Both pass; decline during conduction (not a defect,
   see the handoff's revised section) measured at 21.5 VA / 2.71% and 13.2 VA / 1.68% respectively.
2. **Charger, 5 cycles**: all 5 on/off pairs present in the raw CSV, magnitude ~101-102 VA after each
   noisy 2-4-sample start-up ramp settles -- matches the ~101 VA figure from the 25/08 confirm session.
   Only 3 of 5 pairs survived FIFO pairing inside cluster 2 (2 off-without-on discarded), consistent with
   at least one on-event fragment escaping to cluster 1 by magnitude (see above).
3. **Fan, 5 cycles**: clean, unambiguous, 5 on (37.1-50.0 VA) / 5 off pairs, no start-up noise.
4. **Soldering iron, 5 cycles**: clean, unambiguous, 5 on (58.9-62.8 VA) / 5 off pairs, smooth small
   decline cycle to cycle (61.4-62.6 VA cluster-level), consistent with 25/08's resolution finding.
5. **Sandwich maker, 5 cycles**: 6 real on/off pairs captured for 5 intended cycles -- the 3rd attempt
   cut off spontaneously via the thermostat after ~5 s (well under the 15 s minimum), operator waited
   ~4.7 minutes and re-did it; both the short pair and the redo are real electrical events, both in the
   ground truth. All pairs clean, ~777-790 VA, per-cycle decline 0.16-0.73%.
6. **Ten (extended to eighteen) minutes of sandwich maker alone**: the first 10 minutes yielded only 2
   autonomous cycles (well short of the "dozens" the handoff anticipated), extended by 8 more minutes at
   the operator's judgement call; total 4 complete autonomous cycles (heat-up 32-47 s, cool-down
   4.4-4.8 min). One further, unplanned autonomous cycle (`t_s` 3147-3178) ran later while the appliance
   stayed plugged in during the post-session power-factor discussion; included in the ground truth.

## What went wrong (not painted over)

- **Boot-loop mishap, operator-caused and seen.** During a long pause before the charger item (the
  operator reported possibly having touched the ESP32's USB cable), the board entered a rapid multi-boot
  cycle -- several reboots in quick succession, visible as repeated `t_s,vrms,...` headers close together
  in the log. This was never a reopened-port reset (the serial port was never closed): a genuine
  spontaneous reboot loop, most likely a power brownout or a loose USB connector. Everything from the
  sync mark through all 6 sandwich-maker on/off pairs of the first attempt was lost from the clusterer's
  RAM state; the session restarted cleanly from the connection-artifact check once the board stabilised
  (confirmed via 8+ minutes of continuous, reboot-free capture before proceeding). Not kept as a
  `-discarded` file: the loss was operator-caused and seen at the time, not a judgement call about what
  the data showed.
- **Connection-artifact criterion was wrong, not the capture.** The first attempt's connection check
  flagged 4.36% under the original 0.3-1%/4.4-4.8% rule. Investigation (matching the handoff's revised
  section) showed this was the thermostat-cutoff transition sample plus real, physical decline during
  conduction, not contact oscillation; the handoff's criterion was corrected in place and both attempts
  passed cleanly under the new `sd(diff)/mean` test. No connections were remade.
- **Intermittent serial binary noise.** 14 short runs of corrupted bytes appeared scattered through the
  raw log at 921600 baud, each swallowing 1-2 samples (`t_s` gaps of 2-3 s, all elsewhere in gaps between
  transition and cutoff or during rest). None coincided with a logged on/off transition; `t_s`
  monotonicity is exact (0 violations) after removing them. Judgement call, not operator-seen: identified
  by inspecting gaps after the fact, not noticed live.
- **Capture-script routing bug.** The `c` command's CSV output rows (beginning with a bare cluster-id
  digit, e.g. `1,10,...`) matched the same live-CSV-line pattern as the streamed sensor rows and were
  misrouted into the raw session log instead of the report-CSV file. Recovered by hand from the raw log
  (unedited content, three lines) once found; a defect in the capture tooling, not the firmware or the
  data.
- **Ten minutes of autonomous thermostat cycling yielded far fewer events than expected** (2 cycles
  against the handoff's "dozens"), most likely because the appliance was already warm from all the
  preceding cycling in this session; extended by 8 more minutes at the operator's call to get more
  material.
- **Power-factor assignment could not run in the same session as clustering**, by the structural argument
  in "Power-factor assignment could not happen in this session, by design" above -- not a mishap, a
  designed boundary; recorded here for completeness of the session's write-up as originally flagged by
  the operator mid-session.

## Follow-up filed, not implemented now

Issue opened for receiving the cluster-id -> power-factor-category assignment at report time over serial
(e.g. `a 1 fan` before `r`), so the operator can assign after observing without reflashing. Per the
handoff: small and worth doing, not during a bench session.
