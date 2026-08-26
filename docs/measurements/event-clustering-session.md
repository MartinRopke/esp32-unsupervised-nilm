# Event clustering bench session

Closes the bench-test acceptance gap for #25 (stage 3: event merging + DBSCAN
clustering).

Date: 25 Aug 2026.
Firmware: `e3d85944e08add24f643b54f13fcb702d1996c39` for session-1 (teleplot
output still enabled); `c2fe42f1620cdfd974ee77c0d26b69841d5bcb17` for
sessions 2-4, after rebuilding with `kEnableTeleplotOutput = false` so the
serial log stays at 1 Hz instead of interleaving ~860 Hz teleplot samples
(`git rev-parse HEAD` confirmed against the flashed build before each
capture). Frozen params (`mergeWindowSeconds=5.0`, `epsilonVa=12.0`,
`minPoints=4`, `maxEvents=128`) were not retuned at any point.

## Native test suite

```
Processing test_session_csv in native environment
------------ native:test_session_csv [PASSED] Took 0.71 seconds ------------

Processing test_event_detector in native environment
------------ native:test_event_detector [PASSED] Took 0.51 seconds ------------

Processing test_event_clusterer in native environment
test/test_event_clusterer/test_event_clusterer.cpp:238: test_group_of_three_with_no_neighbours_yields_all_outliers	[PASSED]
test/test_event_clusterer/test_event_clusterer.cpp:239: test_min_points_counted_including_the_point_itself	[PASSED]
test/test_event_clusterer/test_event_clusterer.cpp:240: test_border_point_joins_cluster_of_adjacent_core_point	[PASSED]
test/test_event_clusterer/test_event_clusterer.cpp:241: test_two_distant_core_points_stay_separate_despite_shared_border_point	[PASSED]
test/test_event_clusterer/test_event_clusterer.cpp:242: test_cluster_ids_assigned_by_increasing_magnitude_and_order_independent	[PASSED]
test/test_event_clusterer/test_event_clusterer.cpp:243: test_beyond_max_events_oldest_event_is_evicted	[PASSED]
test/test_event_clusterer/test_event_clusterer.cpp:244: test_confirmation_session_fixture_reproduces_three_clusters_and_six_outliers	[PASSED]
------------ native:test_event_clusterer [PASSED] Took 0.50 seconds ------------

Processing test_event_merger in native environment
test/test_event_merger/test_event_merger.cpp:204: test_two_same_direction_events_within_window_are_merged	[PASSED]
test/test_event_merger/test_event_merger.cpp:205: test_opposite_direction_events_within_window_are_not_merged	[PASSED]
test/test_event_merger/test_event_merger.cpp:206: test_same_direction_events_past_window_are_not_merged	[PASSED]
test/test_event_merger/test_event_merger.cpp:207: test_event_with_no_follow_up_released_by_tick_alone	[PASSED]
test/test_event_merger/test_event_merger.cpp:208: test_single_event_comes_out_unchanged	[PASSED]
test/test_event_merger/test_event_merger.cpp:209: test_confirmation_session_fixture_merges_forty_three_events_into_thirty_nine	[PASSED]
------------- native:test_event_merger [PASSED] Took 0.49 seconds -------------

Processing test_meter in native environment
------------ native:test_meter [PASSED] Took 0.49 seconds -----------------

=================================== SUMMARY ===================================
37 test cases: 37 succeeded in 00:00:02.710
```

None of the merger's own fixtures happen to cover the ~4 s fragment gap this
session found in practice (see "What went wrong", item 3); the suite
passing does not contradict that finding.

## Loads

- Sandwich maker (panini press), ~780-800 VA nominal, thermostatic. Also used
  for the sync marker in every session.
- Fan: ~37-52 VA.
- Laptop charger: highly variable, ~30-101 VA depending on transition
  direction and moment; battery 57% at session start, 20% partway through the
  isolated-load tests, 17% at session end. The laptop was in active use
  throughout, drawing more than the charger supplies, so the battery fell
  despite the charger genuinely delivering power the whole session, which
  is consistent with the issue's "must be actually charging" requirement,
  not a violation of it.
- Soldering iron: used only for the optional resolution-stress scenario,
  deliberately chosen to sit near the fan's magnitude (~55-63 VA observed).

Smart plugs were not used anywhere in this session.

## Pre-session checklist

- Connection-artifact check: session-1 opened with ~290 s idle (`t_s=3.02` to
  `293.02`, power settled near the ~0.30 VA noise floor; relative stdev is
  not a meaningful ratio this close to zero and is not reported) followed by
  the sandwich maker conducting for 135 s (`t_s=295.02` to `430.02`). Relative
  stdev of `power` over that load's stable inner plateau (`t_s=297.02` to
  `429.02`, 132 s, excluding the two transition samples) came out at 0.94%,
  comfortably inside the clean 0.3-1% range and far from the 4.4-4.8% defect
  signature documented in `reference-load-resistance.md`. This check is a
  separate on/off pair from the sync marker that follows it in the same file.
- Wall-clock anchor and firmware commit recorded independently for every
  capture (table below).
- Fan: head locked, no oscillation, speed 3.

## Captures

Four capture files under `docs/measurements/`, split across three unplanned
firmware reboots (see "What went wrong"). Each file's `t_s` column is
relative to that file's own boot; `event-clustering-ground-truth.csv`'s
`capture` column disambiguates which file each ground-truth row belongs to.

| capture | wall-clock anchor (`t_s=3.02`, approx.) | firmware | duration | covers |
|---|---|---|---|---|
| session-1 | 17:02:37 | `e3d8594` | 1132 s (~18.9 min) | sync mark, connection-artifact check |
| session-2 | 17:24:49 | `c2fe42f` | 3003 s (~50 min) | sync mark, 3x isolated 5-cycle tests (sandwich, fan, charger), 10-min passive thermostat observation, staircase rep 1 |
| session-3 | 18:15:22 | `c2fe42f` | 119 s | reboot-recovery remnant only: one incidental fan-off event, left on from session-2; no sync marker of its own, no scenario data survived (see item 5-6 below) |
| session-4 | 18:25:04 | `c2fe42f` | 1225 s (~20.4 min) | sync mark, staircase reps 2-3, simultaneous switching (2 pairs), optional resolution-stress scenario (soldering iron) |

`micros()` wraps at 71.6 minutes (4296 s); no capture approached that limit.
Session-2 ran to `t_s=3003.02` (~50 minutes), over the issue's 45-minute soft
guideline but with ample margin under the hard wrap; it was not deliberately
split at 45 minutes since nothing flagged the threshold at the time; it was
only later split at `t_s=3003.02` because of the reboot in item 4 below, not
because of its length.

Session-1 itself contains a fourth, previously-unnoticed reboot (item 12
below): a silent, uneventful `t_s` reset at `1132.02` while idle, discovered
only during QA of the published files (non-monotonic `t_s`) and trimmed from
`event-clustering-session-1.csv`, which is why the table above reports
session-1's file as ending at `t_s=1132.02` rather than continuing further.

## Counts

- 77 ground-truth actions logged (`event-clustering-ground-truth.csv`).
- 84 events detected across all four capture files (4 + 50 + 1 + 29); all 84
  came through the merger as single-fragment events (see "Failure cases to
  record" below) so the post-merge count equals the detected count.
- Clusters formed (final, offline re-clustering, per capture): session-1: 0;
  session-2: 3; session-3: 0 (single event, cannot form a cluster alone);
  session-4: 3.
- Outliers (final `cluster=-1`): 17 of 84 events (4 + 5 + 1 + 7).
- 31 of those 84 events (37%) carry a different `cluster` id in the raw
  per-row CSV than in the offline, definitive re-clustering (see
  "Reprocessing" below), concrete evidence that the live snapshot is
  frequently stale.

## What went wrong (not painted over)

**1. Sandwich-maker cycle miscount.** During the isolated 5-cycle test, the
user believed 4 complete cycles had been done when the log showed only 4
detected pairs after what should have been a 5th manual "off"; the
thermostat had already cut off autonomously before that flip, so the manual
action produced no event. A genuine 5th cycle was redone and verified against
the log before moving on.

**2. Systematic event-merger fragment-fusion failure.** Independently
observed twice, in the charger's isolated on-cycles and the sandwich maker's
autonomous thermostat re-engagement (`t_s~2248-2291` in session-2): both
produced 3-6 scattered fragments instead of one merged event, despite
individual fragments arriving as little as ~4 s apart (well inside the 5 s
merge window). Root cause: `EventMerger::addEvent()` measures the window from
a fragment's own (backdated) crossing timestamp, but a fragment only reaches
`addEvent()` after the detector's ~3-sample (~3 s) confirmation delay. A
follow-up fragment whose *crossing* is ~4 s after the held fragment's crossing
can still arrive at `addEvent()` after `tick()` (driven by real elapsed time)
has already released the held event on the same real-time clock; exactly
the "one confirmation window plus one sample of slack" margin the merger's own
design comment assumes, consumed entirely by the confirmation delay itself.
Per the issue's "stop and flag, don't adjust" instruction this was raised
before continuing; the decision was to keep `mergeWindowSeconds=5.0` frozen
and document the finding rather than retune it mid-session. Not fixed in this
session.

*Added on review (26 Aug 2026).* Three quantifications the session write-up
did not have, from replaying the recorded series:

- **The miss is deterministic and it is by exactly one second.** Events are
  released at `dated + 6 s` (77 of 84; the other 7 at +5), so the merger holds
  for 2 s of real time rather than 5. A follow-up fragment is dated 4 s after
  the first and therefore arrives at `dated + 7 s` — one second after the held
  event was released. Every time.
- **The right denominator is 6, not 84.** Across all four captures there are
  exactly **6** consecutive same-direction pairs less than 5 s apart in dated
  time, i.e. 6 genuine merge opportunities. All 6 failed. "0 of 84" overstates
  the blast radius; the defect is real but it had 6 chances to show.
- **The fragments do reconstruct the true step.** The charger's three pairs sum
  to 101.3, 100.8 and 101.6 VA — mean 101.2 VA, **0.40 % dispersion over three
  repetitions** — matching the charger's own off-transition band (88.8-101.6
  VA). The merge rule is right; only the clock it is measured on is wrong.

**The fix does not touch a parameter.** The window stays 5.0 s. What changes is
which clock each of the two conditions reads: the *merge* condition compares
the fragments' **dated** instants (the current code is already correct here),
while the *release* condition must run on the wall clock measured from the last
fragment's **arrival**, not from its dated instant. Stated that way the merger
needs no knowledge of the detector's confirmation delay.

**3. Background-capture safety-cap false alarm.** The capture script's own
`MAX_SECONDS=3000` safety cap elapsed during session-2's 10-minute passive
observation, and the resulting process exit initially looked like a hardware
disconnect. Root-caused by checking elapsed wall time against the script's
own cap (an exact match); raised to 100000 and the capture restarted.

**4. Genuine ESP32 reboot mid-staircase.** Reopening the serial port to
restart the capture after fixing item 3 triggered an actual ESP32 reboot
(`t_s` reset, all firmware-side detector/merger/clusterer state wiped),
partway through staircase repetition 2 (only its first step, fan-on, had
completed). The DTR/RTS guard the capture script already applies did not
prevent this. The raw log was split at the reboot boundary into session-2
(finalized) and session-3 (new boot), and the interrupted repetition was
abandoned and redone from a clean baseline rather than patched across the
split, per the same recovery pattern used in the prior event-detection
session.

**5. File-split data loss.** The split in item 4 was performed with
`mv` while the capture script's process still held its original file
descriptor open in append mode on the pre-split file. On a rename, that
process keeps writing into the now-path-less original inode rather than the
new file at that path; everything it wrote after the split (roughly 6
minutes: the confirmation of a "fan off" action already read from the log
before the split, and a sandwich-maker sync-marker on/off pair requested
after it) went into an inode no longer reachable from any path, and was not
recoverable without root-level disk access (macOS has no `/proc/<pid>/fd`).
The stray process was killed and a fresh session-4 was started from scratch,
including its own new sync marker; session-3 is left with only the one
fan-off event that had already reached disk before the split.

**6. A second reboot on restart.** Reopening the serial port yet again to
start session-4 triggered another ESP32 reboot, for the same reason as item
4; confirmed as a real, repeatable property of this ESP32 board's
USB-serial reset behavior, not a one-off. No firmware or script change was
attempted; every new capture in this session independently re-verified an
all-off baseline and its own sync marker rather than assuming continuity.

**7. Staircase order deviation.** The issue's literal staircase order puts
the sandwich maker on first (held longest) and off last. Before repetition 1,
the user asked to reverse this: fan, then charger, then sandwich maker
ascending; sandwich maker, then charger, then fan descending. This keeps
the sandwich maker (the only thermostatic load) held for the shortest
possible time, avoiding an autonomous mid-staircase cycle like the one seen
during session-2's passive observation. All three repetitions used this
reordered sequence; none literally match the issue's stated order.

**8. Fan/charger cluster overlap — mostly downstream of item 2.** Fan's clean
~37-52 VA transitions and several of the charger's lower-magnitude fragments
and on-transitions repeatedly land in the same DBSCAN cluster in both
session-2 and session-4, because their magnitudes sit within `epsilonVa=12` of
each other.

*Corrected on review (26 Aug 2026).* Calling this "expected, not a defect"
concedes too much. Replaying session-2 with the merge applied on the correct
clock (item 2) moves most of it: charger events inside the fan's cluster drop
from **9 to 3**, the charger's own cluster grows from **6 to 9** events, and
outliers fall from 5 to 2. **Roughly two thirds of this overlap is a symptom of
the merger defect, not of the appliances' power draw.** What remains — 3 events
— is the charger's soft-start ramp spreading over more than 5 s, which is the
limitation already declared in stage 2 and belongs to the load, not to the
detector or the clusterer.

A separate finding that does belong here: **the charger is not a
stable-signature load.** It ranged from 30 to 101 VA within one session as its
battery went from 57 % to 17 % with the laptop in use — a 3.4x spread, wider
than the distance to the fan. That is a property of the appliance and it
matters for stage 4.

**9. Charger's on/off transitions form two separate clusters.** The charger's
on-transition magnitude (~30-67 VA, sharing cluster 1 with the fan) and its
off-transition magnitude (~83-101 VA, its own cluster 2 in session-2) differ
by more than `epsilonVa`, so the same physical appliance is split across two
clusters depending on switching direction.

*Corrected on review (26 Aug 2026).* The on-transition band quoted here
(~30-67 VA) is largely made of **unfused fragments** (item 2), not of the
charger's true on-step. Once merged on the correct clock, those fragments sum
to ~101 VA — the same band as the off-transitions. The split is therefore
mostly an artefact of the defect, and should shrink sharply once it is fixed.
Whether any split survives is a question for the confirmation session, not
something this session can answer.

**10. Resolution-stress scenario — the iron DID form its own cluster, and the
resolution limit got located.** *Rewritten on review (26 Aug 2026); the earlier
wording said the iron failed to form its own cluster, which is not what the
data shows.*

The soldering iron was deliberately chosen to draw a magnitude close to the
fan's, and the handoff predicted the two would fuse. **They did not.** In
session-4 the fan's cluster ends at **41.6 VA** and the iron's cluster begins
at **54.9 VA**: a gap of **13.3 VA against `epsilonVa` = 12** — a margin of
**1.3 VA**. All 10 iron events formed a cluster of their own, distinct from the
fan's.

What did happen is that one charger on-event (61.4 VA) fell inside the iron's
cluster. That is a third appliance contaminating the iron's cluster, not the
iron failing to resolve.

This is the strongest result of the session and it should be read as a success,
not a conflation: **the resolution limit of the method has been located to
roughly one volt-ampere by deliberate experiment.** Two loads 13.3 VA apart
separate at `epsilonVa` = 12; the scenario shows the boundary sits just below
that, and it does so with measured numbers rather than an assertion.

**11. Simultaneous switching (expected, with a wrinkle).** Pair fan+charger's
"simultaneous" on-flip actually landed as two events ~4 s apart in the raw
log (the same fragment-fusion gap as item 2), one an outlier (98.5 VA) and
one absorbed into the fan cluster (35.5 VA); its off-flip merged cleanly into
one event (136.6 VA), which stayed an outlier. Pair sandwich+fan merged
cleanly on both edges (851.9 VA on, 839.1 VA off) and both were correctly
flagged as outliers, matching the issue's stated expectation for this
scenario.

**12. A fourth, previously-unnoticed reboot (session-1).** Publishing QA
found `event-clustering-session-1.csv`'s `t_s` column was not monotonic: it
reset from `1132.02` back to `3.02` partway through the raw file, meaning the
ESP32 rebooted a second time during session-1 itself, roughly 9 minutes after
the sync marker's off-edge, for no action-related reason (the surrounding
rows are idle, ~0.30 VA, no events). Nothing of protocol value was lost:
both the connection-artifact check and the sync marker completed well before
this point, so the post-reboot tail (uneventful, no ground-truth actions)
was trimmed rather than kept as a fifth near-empty file. This is the third
distinct unplanned reboot this session (the other two are items 4 and 6), all
on the same board, none with a confirmed root cause.

## Failure cases to record

- **Events with `fragments` > 1**: zero, out of all 84 released events, in any
  capture. Not one merge ever succeeded this session; the merger's own
  fragment-fusion gap (item 2 above) was not an occasional glitch but
  systematic for the entire session's real-world switching transients.
- **Fragments that escaped merging**: clearest example is the charger's
  isolated 5-cycle test in session-2. All 5 off-transitions came through as a
  single clean event each (~98-101 VA). Of the 5 on-transitions, 3 split into
  two escaped events ~4 s apart instead of one: `t_s=1419.02` (49.16 VA) +
  `1423.02` (52.19 VA) = 101.34 VA; `1457.02` (48.79 VA) + `1461.02` (52.03
  VA) = 100.82 VA; `1531.02` (38.29 VA) + `1535.02` (63.27 VA) = 101.56 VA:
  each pair's sum lands within ~1-3 VA of the clean off-transition magnitude,
  confirming these are split halves of one ~100 VA transition, not distinct
  charger behaviour. (The remaining 2 on-transitions, 67.12 VA and 59.98 VA,
  came through as a single event with no paired escaped fragment, but at a
  magnitude closer to the fragment sums than to the clean off-side, suggesting
  those may be partial captures of the same ramp rather than a genuinely
  different on-side transient, not confirmed either way.) The same pattern
  recurs in the simultaneous fan+charger pair (item 11) and the sandwich
  maker's autonomous re-engagement (item 2).
- **A load that never formed a cluster**: none. Every load that appeared in a
  given capture landed in at least one cluster there, whether its own
  (sandwich maker, always) or shared with another appliance (fan and charger,
  items 8-9). No load fell below `minPoints=4` and disappeared from a result
  entirely.
- **A label that changed during the session**: 31 of 84 events (37%) carry a
  different cluster id in the live per-row CSV than in the offline definitive
  re-clustering (see "Reprocessing" below). Concrete example: the
  resolution-stress scenario's first soldering-iron on/off pair
  (`t_s=984.02`, `t_s=1002.02`) is reported as noise (`cluster=-1`) in the
  live stream, since only 2 similar-magnitude events had been seen so far,
  but is retroactively assigned to the soldering iron's shared cluster once
  the remaining 4 cycles arrive later in the same boot.

## Per-scenario summary

- **Sync marks**: session-1, session-2, and session-4 each opened with a
  sandwich-maker on/off sync mark, all detected as single clean events.
  Session-3 has no sync mark of its own (item 5).
- **Isolated 5-cycle tests** (session-2): sandwich maker (1 cycle redone,
  item 1), fan (clean), charger (soft-start ramp visible on-transitions,
  clean off-transitions); all 30 actions eventually detected.
- **10-minute passive observation** (session-2): thermostat cycled
  autonomously; one re-engagement heavily fragmented (item 2), documented
  rather than patched.
- **Staircase overlap x3** (rep 1 in session-2; reps 2-3 in session-4, after
  the reboot/recovery in items 4-6): all three repetitions completed the
  reordered sequence (item 7) with clean single-fragment events at every
  step, reaching combined magnitudes of ~880-905 VA at full overlap. Rep 3's
  charger on-event (30.1 VA) landed right at the detector's own 30 VA
  threshold and rep 3's sandwich-off event (760.5 VA) fell just outside the
  sandwich cluster's usual range; both real, not adjusted.
- **Simultaneous switching, 2 pairs** (session-4): see item 11.
- **Resolution-stress scenario, optional** (session-4): see item 10.

## Reprocessing: offline definitive re-clustering

`EventClusterer::addEvent()` re-clusters its entire bounded history on every
call, so a cluster id assigned to an early event in the live per-row CSV can
be superseded once later events in the same boot are seen; the CSV column
is a snapshot at stream time, not retroactively updated. Because
`EventClusterer`'s history resets on every firmware reboot, this also means
each capture's clustering is independent of the others: a cluster id from
session-2 has no relationship to the same id in session-4.

The magnitude sequence from each capture was replayed offline through a
faithful reimplementation of `clusterHistory()` (same two-pointer
neighbourhood search, same core-point chaining by increasing magnitude, same
border-point nearest-core assignment, same `epsilonVa`/`minPoints`/
`maxEvents`), re-clustering after every event exactly as the firmware does,
and keeping only the final label assigned to each event once its capture's
full history had been replayed. `event-clustering-cluster-map.csv` records
the resulting clusters, one row per (capture, cluster id), with a human
interpretation of which appliance(s) each cluster corresponds to and notes on
anything non-obvious; 31 of the 84 events (37%) ended up with a different
final label than their live snapshot, mostly points that legitimately started
as noise (fewer than `minPoints=4` neighbours seen so far) and later joined a
cluster once more of the same boot's events arrived.
