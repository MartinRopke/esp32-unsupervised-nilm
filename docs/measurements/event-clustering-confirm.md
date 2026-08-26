# Event clustering confirmation session

Confirms the fix in `ISSUE-event-merger-release-clock.md` (#27) on the bench, following the
"Sessão de confirmação" section of `HANDOFF-agrupamento.md`. Reduced scenario set (connection
check, sync mark, charger x5, fan x5, ten minutes of sandwich maker alone), comparability
rather than full recharacterization. The 25/08 captures (`event-clustering-session-*.csv` and
their ground truth) are untouched.

Date: 25 Aug 2026.
Firmware: commit `ad31609` (working tree clean at this commit at build/flash time;
`platformio run -e esp32dev -t upload` on `/dev/cu.usbserial-0001` reported success).

## Native test suite

```
Processing test_session_csv in native environment
--------------------------------------------------------------------------------
Building...
Testing...
test/test_session_csv/test_session_csv.cpp:100: test_no_event_row_leaves_event_fields_blank	[PASSED]
test/test_session_csv/test_session_csv.cpp:101: test_switch_on_event_row_fills_event_fields	[PASSED]
test/test_session_csv/test_session_csv.cpp:102: test_switch_off_event_row_encodes_negative_direction	[PASSED]
test/test_session_csv/test_session_csv.cpp:103: test_outlier_event_row_reports_negative_one_cluster	[PASSED]
test/test_session_csv/test_session_csv.cpp:104: test_merged_event_row_reports_fragment_count_above_one	[PASSED]
-------------- native:test_session_csv [PASSED] Took 0.35 seconds --------------

Processing test_event_detector in native environment
--------------------------------------------------------------------------------
Building...
Testing...
test/test_event_detector/test_event_detector.cpp:330: test_step_above_threshold_produces_one_event_with_close_magnitude	[PASSED]
test/test_event_detector/test_event_detector.cpp:331: test_noise_below_threshold_produces_no_event	[PASSED]
test/test_event_detector/test_event_detector.cpp:332: test_switch_off_detected_with_opposite_direction	[PASSED]
test/test_event_detector/test_event_detector.cpp:333: test_magnitude_uses_window_means_not_adjacent_samples	[PASSED]
test/test_event_detector/test_event_detector.cpp:334: test_stable_plateau_produces_no_repeated_events_over_long_run	[PASSED]
test/test_event_detector/test_event_detector.cpp:335: test_two_steps_inside_confirmation_window_produce_single_event	[PASSED]
test/test_event_detector/test_event_detector.cpp:336: test_multi_second_ramp_does_not_fragment_into_several_events	[PASSED]
test/test_event_detector/test_event_detector.cpp:337: test_partial_transition_sample_excluded_from_candidate_window	[PASSED]
test/test_event_detector/test_event_detector.cpp:338: test_partial_transition_sample_excluded_from_candidate_window_switch_off	[PASSED]
test/test_event_detector/test_event_detector.cpp:339: test_fan_sized_step_stays_single_event_regardless_of_transition_sample_handling	[PASSED]
------------ native:test_event_detector [PASSED] Took 0.52 seconds ------------

Processing test_event_clusterer in native environment
--------------------------------------------------------------------------------
Building...
Testing...
test/test_event_clusterer/test_event_clusterer.cpp:238: test_group_of_three_with_no_neighbours_yields_all_outliers	[PASSED]
test/test_event_clusterer/test_event_clusterer.cpp:239: test_min_points_counted_including_the_point_itself	[PASSED]
test/test_event_clusterer/test_event_clusterer.cpp:240: test_border_point_joins_cluster_of_adjacent_core_point	[PASSED]
test/test_event_clusterer/test_event_clusterer.cpp:241: test_two_distant_core_points_stay_separate_despite_shared_border_point	[PASSED]
test/test_event_clusterer/test_event_clusterer.cpp:242: test_cluster_ids_assigned_by_increasing_magnitude_and_order_independent	[PASSED]
test/test_event_clusterer/test_event_clusterer.cpp:243: test_beyond_max_events_oldest_event_is_evicted	[PASSED]
test/test_event_clusterer/test_event_clusterer.cpp:244: test_confirmation_session_fixture_reproduces_three_clusters_and_six_outliers	[PASSED]
------------ native:test_event_clusterer [PASSED] Took 0.51 seconds ------------

Processing test_event_merger in native environment
--------------------------------------------------------------------------------
Building...
Testing...
test/test_event_merger/test_event_merger.cpp:322: test_two_same_direction_events_within_window_are_merged	[PASSED]
test/test_event_merger/test_event_merger.cpp:323: test_opposite_direction_events_within_window_are_not_merged	[PASSED]
test/test_event_merger/test_event_merger.cpp:324: test_same_direction_events_past_window_are_not_merged	[PASSED]
test/test_event_merger/test_event_merger.cpp:325: test_same_direction_past_window_in_dated_time_does_not_merge_despite_close_arrival	[PASSED]
test/test_event_merger/test_event_merger.cpp:326: test_event_with_no_follow_up_released_by_tick_alone	[PASSED]
test/test_event_merger/test_event_merger.cpp:327: test_single_event_comes_out_unchanged	[PASSED]
test/test_event_merger/test_event_merger.cpp:328: test_measured_series_regression_pairs_fuse_to_expected_magnitude	[PASSED]
test/test_event_merger/test_event_merger.cpp:329: test_charger_pairs_sum_matches_off_transition_band	[PASSED]
test/test_event_merger/test_event_merger.cpp:330: test_fragments_count_reaches_two_on_merge	[PASSED]
test/test_event_merger/test_event_merger.cpp:331: test_confirmation_session_fixture_merges_forty_three_events_into_thirty_nine	[PASSED]
------------- native:test_event_merger [PASSED] Took 0.50 seconds -------------

Processing test_meter in native environment
--------------------------------------------------------------------------------
Building...
Testing...
test/test_meter/test_meter.cpp:259: test_pure_dc_centers_to_zero	[PASSED]
test/test_meter/test_meter.cpp:260: test_sine_offset_is_removed_ac_preserved	[PASSED]
test/test_meter/test_meter.cpp:261: test_pure_dc_window_closes_with_zero_vrms	[PASSED]
test/test_meter/test_meter.cpp:262: test_sine_window_vrms_matches_amplitude_over_sqrt2	[PASSED]
test/test_meter/test_meter.cpp:263: test_window_closes_by_elapsed_time_not_sample_count	[PASSED]
test/test_meter/test_meter.cpp:264: test_first_window_before_dc_settles_is_bounded	[PASSED]
test/test_meter/test_meter.cpp:265: test_sine_window_irms_matches_calibrated_conversion	[PASSED]
test/test_meter/test_meter.cpp:266: test_sine_window_apparent_power_matches_mains_voltage_times_irms	[PASSED]
test/test_meter/test_meter.cpp:267: test_switching_mains_voltage_changes_apparent_power_proportionally	[PASSED]
----------------- native:test_meter [PASSED] Took 0.50 seconds -----------------

=================================== SUMMARY ===================================
Environment    Test                  Status    Duration
-------------  --------------------  --------  ------------
native         test_session_csv      PASSED    00:00:00.738
native         test_event_detector   PASSED    00:00:00.520
native         test_event_clusterer  PASSED    00:00:00.513
native         test_event_merger     PASSED    00:00:00.505
native         test_meter            PASSED    00:00:00.503
================= 41 test cases: 41 succeeded in 00:00:02.779 =================
```

## Loads

- Sandwich maker, ~790-800 VA nominal, thermostatic.
- Fan: head locked, no oscillation, speed 3, ~38-51 VA.
- Lenovo laptop charger: battery 47% at the start of the charger item, 43% at the end. The drop
  is expected, not a data-quality concern: this item is 5 short (~15-20 s) on/off pulses, not a
  continuous charge, so the notebook spends nearly all of the item's duration on battery between
  pulses.

## Capture

Two capture files, each its own continuous serial port opening: `event-clustering-confirm.csv`
(items 1-4: connection check, sync mark, charger x5, fan x5) and `event-clustering-confirm-2.csv`
(item 5: ten minutes of sandwich maker alone). Each file's `t_s` origin (`t_s=0`) is its own
firmware boot on port open. Ground truth in `event-clustering-confirm-ground-truth.csv` (capture
labels `confirm` and `confirm-2` matching the two files); cluster interpretation in
`event-clustering-confirm-cluster-map.csv`.

Monotonicity of `t_s` confirmed within each file after the repairs below.

## What went wrong (not painted over)

Two artefacts, both in the capture tooling, neither in the firmware, each recurring independently
in both capture files (each file opens its own port, so each pays the same startup cost once):

**1. Header row and the first data row's `t_s` field lost.** The capture script slept 3 s after
opening the port (to let the ESP32 finish its post-flash boot) and then called
`reset_input_buffer()` to discard whatever had accumulated during that wait. The header line
(`Serial.println` in `setup()`) is printed within the first second, well inside that 3 s window,
so it was discarded along with it, and the buffer clear landed mid-transmission of the very
first data row, keeping everything after its first comma but losing the `t_s` field itself. Fix:
the header was reconstructed from `src/main.cpp`'s literal header string (unchanged since
stage 2), and the one truncated row in each file (a single repose-phase baseline sample with no
`t_s`) was dropped rather than guessing its timestamp.

**2. Rows arrived prefixed with a long run of `U+FFFD` replacement-character bytes**: eight in
`event-clustering-confirm.csv` (`t_s` ~145, ~638, ~672, ~766, ~796, ~2027, ~2099, ~2405) and two
in `event-clustering-confirm-2.csv` (`t_s` ~47, ~360): a burst of raw bytes the USB-serial link
corrupted badly enough that `bytes.decode(errors="replace")` in the capture script turned each
corrupted byte into a placeholder, and because none of those placeholder bytes is a newline, the
whole burst got read as part of one line ending at the next real `\n`. In every case the burst
sits **before** an otherwise complete, well-formed CSV row: the digits, commas and decimal
points of the row itself are untouched. Repair: strip the garbage prefix, keep the row. None of
the ten affected rows across both files carries an event (`event` field is `0` in all of them);
the transmission noise never touched a row this session's analysis depends on.

**3. Item 5 was captured twice; the first take was discarded.** The first ten-minute sandwich-maker run
was cut short after the operator disturbed the sensor wire mid-capture and injected noise into the
signal. The item was re-run from the start and only the second take is in
`event-clustering-confirm-2.csv`; the first take's file was not kept. The discard was the operator's, on
a disturbance he had caused and observed, not on anything the data showed. Recorded here for the same
reason as item 1 of the 25/08 session's miscount: this bench notebook reports its mishaps.

## A merge missed by 1.1 milliseconds

The charger's second switch-on released as **two separate events**
(37.9 VA dated 714.019409, 47.5 VA dated 719.020508) that should have fused: they are the same direction
and nominally five sampling intervals apart. The measured gap is **5.001099 s**, against a
`mergeWindowSeconds` of 5.0. **The merge was missed by 1.1 ms.**

The dated instants come from `micros()` at window close, and windows are closed by elapsed time rather
than by a fixed sample count, so successive dated instants are not exactly 1.000000 s apart. Over five
windows the jitter accumulated past the boundary.

This is not the method's resolution limit and should not be written as one. The design states the window
as *"the confirmation window plus one sample of slack"* — an interval count — while the code compares
floating-point seconds against a jittery clock. A nominal five-interval gap that measures 5.0011 s
defeats the stated intent.

Cost, measured: with this pair fused, the charger would have fused **3 of 5** switch-ons instead of 2,
and this cluster would hold **1** charger event instead of 3.

Not changed in this session, per the standing "stop and flag, do not tune" rule. Expressing the window in
sampling intervals rather than seconds would fix it without changing the designed value; widening the
constant would be tuning. That is a decision for a separate issue, not for the bench.

## Counts

- 22 ground truth actions across both files (`event-clustering-confirm-ground-truth.csv`, capture
  labels `confirm` and `confirm-2`): sync mark (2), charger isolated cycles (9; one physical
  "on" action produced no detected event, see "Item 3"), fan isolated cycles (10), item 5's
  manual on (1). Autonomous thermostat transitions are real events but, matching the 24/08 and
  25/08 sessions' own convention, are not logged as individual ground truth rows, described
  narratively below instead.
- 29 events released total: 24 in `event-clustering-confirm.csv` (items 1-4) and 5 in
  `event-clustering-confirm-2.csv` (item 5). All 29 are valid and used in the reading below.

## Per-item summary

- **Item 1 (conferência do artefato de conexão)**: repouso limpo (0.29 +/- 0.007 VA, n=148).
  Sanduicheira conduzindo até o corte natural (n=134, t_s 166-300): desvio relativo 1.12%
  (mean 795.3 VA, sd 8.9 VA), quase idêntico ao 1.12% (796.5 VA) da sessão de 24/08, e longe do
  artefato de 4.4%-4.8% descrito no handoff. Conexão validada.
- **Item 2 (marco de sincronismo)**: liga 791.7 VA em `t_s=534.02`, reportado em `t_s=543.02`
  (datado+9s); desliga 794.2 VA em `t_s=552.02`, reportado em `t_s=561.02` (datado+9s). Ambos
  eventos únicos (`fragments`=1). A assinatura de tempo já mudou de +6s (defeito) para +9s
  (~8s teóricos mais até 1s de granularidade da janela de 1 Hz).
- **Item 3 (carregador, 5 ciclos)**: **o teste mais importante passou: `fragments` > 1
  apareceu**, coisa que não aconteceu em nenhuma das 84 liberações de 25/08. Dois dos 5 liga
  fundiram: 82.2 VA (fragments=2, ciclo 1) e 100.7 VA (fragments=2, ciclo 5), este último quase
  exatamente a previsão do handoff (~101 VA). Os outros 3 liga saíram como fragmento único
  subestimado (37.9, 47.5, 56.3 VA) ou, num caso, abaixo do limiar e não detectado: a rampa de
  partida ruidosa do carregador, já declarada fora de escopo desta correção (mesmo padrão do
  levantamento de 24/08). Os 5 desliga saíram limpos, evento único, 91.7-100.1 VA.
- **Item 4 (ventilador, 5 ciclos)**: limpo, sem ambiguidade: 5 liga (46.7-50.6 VA) e 5 desliga
  (38.3-42.6 VA), todos `fragments`=1.
- **Item 5 (dez minutos, sanduicheira sozinha, `event-clustering-confirm-2.csv`)**: liga manual
  limpo em `t_s=26.02` (812.2 VA, `fragments`=1). A janela de 10 minutos exigida (`t_s` 26-626)
  correu sem perturbação, com uma transição autônoma completa dentro dela: corte em `t_s=158.02`
  (797.4 VA, **fragments=2**, fusão completa) e um religa autônomo logo depois, em `t_s=455-463.02`,
  que **não fundiu**: dois fragmentos separados de direção "liga" (341.4 VA e 479.5 VA, ambos
  `fragments`=1, ~8s de distância um do outro), em vez de somarem à magnitude nominal (~790 VA)
  num único evento. Um segundo corte autônomo fechou a janela em `t_s=494.02` (794.3 VA,
  `fragments`=1). Desligamento manual às `t_s=677.02` (fim do arquivo): sem evento, porque a
  sanduicheira já estava em corte autônomo nesse instante: o desligamento manual foi
  deliberadamente cronometrado para coincidir com um corte natural, evitando qualquer ambiguidade
  sobre a origem da queda de potência.

## Clustering

`event-clustering-confirm-cluster-map.csv` reconstructs the clusters by hand from the released
magnitudes (epsilonVa=12, minPoints=4), the same convention as the 25/08 map. Three real clusters
emerged, plus 3 noise points, across the 29 events:

- **Cluster 1** (fan + charger, 38.3-56.3 VA, 13 events): the fan's 10 clean cycles plus exactly
  **3** charger on-events: the noisy-startup fragments/underestimates from item 3. The handoff
  predicted the charger's contamination of the fan cluster would drop from 9 to 3 on session-2's
  replay; this fresh capture reproduces that count directly.
- **Cluster 2** (charger, 82.2-100.7 VA, 7 events): the charger's 5 off-events plus its 2
  successfully merged on-events. The predicted growth of the charger's own cluster (6 to 9 on
  session-2's replay) is directionally reproduced, at a smaller n for this shorter session.
- **Cluster 3** (sandwich maker, clean full-power band, 785.1-797.4 VA, 6 events): the connection
  check's on/off, the sync mark's on/off, and item 5's two autonomous cutoffs: both cutoffs land
  in the same band as the manually-driven transitions, including the one that merged two fragments
  (`fragments`=2, 797.4 VA) into a still-full magnitude.
- **Noise, cluster -1** (3 events): item 5's manual on (812.2 VA) sits 14.8 VA above cluster 3's
  nearest member, just past epsilonVa; its autonomous on-transition split into two unmerged
  fragments 138 VA apart (341.4 and 479.5 VA) instead of summing to nominal in one release. Neither
  has the 4 same-magnitude neighbours minPoints requires. This is the same ramp-fusion limitation
  the handoff predicted (a ramp spanning more than `mergeWindowSeconds` does not fully fuse),
  showing up on the on-side this run rather than the off-side.

## Reading

The single decisive prediction (`fragments` > 1 appearing at all) held, twice, in the
charger's 5 cycles (82.2 and 100.7 VA), where 25/08 produced zero fusions across 84 released
events. One of the two (100.7 VA) lands within 0.6 VA of the measured-series prediction (~101 VA
mean). The `datado + Ns` signature moved from +6s to +9s on both clean sync-mark events, the
expected direction and within one 1 Hz sampling window of the theoretical +8s.

The charger's remaining on-transitions (3 of 5) still under-fuse or go undetected: the noisy
startup ramp this issue explicitly left out of scope, and the same pattern the 24/08 confirmation
session documented for a different fix. The fan stayed clean throughout, as expected for a load
with no comparable startup noise.

Item 5's autonomous transitions split the "not perfect" outcome the handoff called for as a health
signal across both directions of the same thermostat cycle: the off-side fused cleanly to full
magnitude (`fragments`=2, still landing in the clean band), while the on-side did not fuse at all,
releasing as two separate `fragments`=1 events 8 s apart instead of one summed release: a ramp
that spans more than `mergeWindowSeconds` is not supposed to fuse completely, and here it simply
did not, on the on-side this time.

`fragments` reaches the CSV correctly in every merged row observed, in both capture files
(`event-clustering-confirm.csv` and `event-clustering-confirm-2.csv`).

## What this does not do

Does not recharacterize the charger's noisy startup ramp or attempt to resolve it: declared out
of scope by the issue this session confirms. Does not repeat the full stage-3 protocol (staircase,
resolution-stress scenarios, soldering iron): reduced scope was the point, per the handoff. Does
not compare the charger's absolute magnitudes to the 24/08 or 25/08 sessions directly: battery
state differs (47%-43% here vs. those sessions' own ranges) and this item's brief on/off pulses
are a different usage pattern than a continuous charge.
