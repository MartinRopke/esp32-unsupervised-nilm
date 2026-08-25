# Event detection confirmation session

Confirms the fix in `ISSUE-exclude-transition-sample.md` on the bench, following the
"Sessão de confirmação" section of `HANDOFF-ensaio-de-eventos.md`. Reduced scenario
set, same loads as the 24 Aug session, comparability rather than full recharacterization.

Date: 24 Aug 2026.
Firmware: commit `201827b72bd30e6e68e40590220c93197569500f` (working tree was clean and
at this exact commit at build/flash time; `pio run -e esp32dev -t upload` on
`/dev/cu.usbserial-0001` reported success).

## Native test suite

```
Collected 3 tests

Processing test_session_csv in native environment
--------------------------------------------------------------------------------
Building...
Testing...
test/test_session_csv/test_session_csv.cpp:62: test_no_event_row_leaves_event_fields_blank	[PASSED]
test/test_session_csv/test_session_csv.cpp:63: test_switch_on_event_row_fills_event_fields	[PASSED]
test/test_session_csv/test_session_csv.cpp:64: test_switch_off_event_row_encodes_negative_direction	[PASSED]
-------------- native:test_session_csv [PASSED] Took 0.74 seconds --------------

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
------------ native:test_event_detector [PASSED] Took 0.49 seconds ------------

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
Environment    Test                 Status    Duration
-------------  -------------------  --------  ------------
native         test_session_csv     PASSED    00:00:00.738
native         test_event_detector  PASSED    00:00:00.495
native         test_meter           PASSED    00:00:00.502
================= 22 test cases: 22 succeeded in 00:00:01.735 =================
```

## Loads

Same physical loads as the 24 Aug session.

- Sandwich maker, ~790-800 VA nominal, thermostatic.
- Fan: head locked, no oscillation, speed 3, ~40-51 VA.
- Lenovo laptop charger: battery 5% at session start, 7% at the end. Much lower
  than the 24 Aug session's 40%-22% range, so charger magnitudes below are not
  directly comparable to that session's ~40-110 VA figure — noted, not hidden.

## Captures

Two files, split by an unplanned ESP32 reset (see "What went wrong").

| capture | wall clock anchor (`t_s=0`) | covers |
|---|---|---|
| `event-detection-confirm.csv` | 22:03:04 | items 1-5, up through the first (discarded) attempt at sandwich maker isolated repetition 3 |
| `event-detection-confirm-2.csv` | 22:49:10 | the clean redo of sandwich maker isolated repetition 3 |

## Counts

- 27 ground truth actions logged (`event-detection-confirm-ground-truth.csv`): manual
  switch actions only. Autonomous thermostat transitions (cutoffs and reengagements) are
  real events in the raw CSVs but are not logged as individual ground truth rows,
  matching the 24 Aug session's own convention for the thermostat cycling scenario --
  they are described narratively below instead.
- 41 events detected in `event-detection-confirm.csv`, 2 in `event-detection-confirm-2.csv`.

## What went wrong (not painted over)

**1. ESP32 reset on serial port reopen.** The capture script was stopped and restarted
partway through item 3 (to redo sandwich maker repetition 3 -- see item 3 below). Reopening
the serial port reset the board (`micros()` restarted from zero), which the first restart
attempt did not anticipate: it appended the rows recorded after the reset into the same file
as the ones from before it, producing two overlapping `t_s` ranges in one file. Caught before
any further rows were written, the tainted rows were discarded, and the continuation was
placed in a second file (`event-detection-confirm-2.csv`) with its own independent `t_s=0`
origin, the same way the 24 Aug session split `session-3`/`session-4` around its own reboot
partway through a capture.

**2. Two autonomous transitions missed in real time during isolated repetition 3
(first attempt).** Between the manual switch on for sandwich maker repetition 3 (confirmed,
`t_s=885.02`) and the next user confirmation (`t_s=1037.02`, ~152 s later), the log shows an
autonomous cutoff at `t_s=930.02` (~45 s of heating, consistent with the thermostat's own
cycle) and an autonomous reengagement at `t_s=1017.02` (~87 s off), neither flagged live
because the agent only checked the tail of the log after each user confirmation rather than
the full window in between. When asked, the user was unable to say with confidence whether
the `t_s=1037.02` action was their own switch off or a second autonomous cutoff coinciding
with their reply. Rather than fabricate a clean manual pair out of an uncertain sequence,
this whole episode (`t_s=885.02` through `1037.02`) is excluded from the ground truth file;
repetition 3 was redone from scratch in `event-detection-confirm-2.csv` instead, watched
continuously end to end this time. The raw events remain in `event-detection-confirm.csv`,
undeleted, for anyone who wants to look.

**3. An unplanned load during the wait between repetitions 2 and 3 (first attempt).** The
user cooled the sandwich maker with the fan and charged the laptop during the same pause,
producing an unplanned `fan`+`charger` on/off pair (`t_s=728.02`-`881.02`) ahead of that
attempt's sandwich maker switch on. It is logged in the ground truth as `fan` and `charger`
separately (the two onset crossings were 8 s apart, past the 3-sample confirmation window,
so the detector reported them as two events, not one fused event); which of the two the user
switched on first is not independently known, so that ordering is a best guess, not a
verified fact.

**4. One unexplained blip before the sync mark.** Between item 1's own natural cutoff
(`t_s=211.02`) and the sync mark's switch on (`t_s=496.02`), a small ~50 VA on/off pair
appears at `t_s=326.02`/`492.02` with nothing physically done in that window. Same
unexplained character as the ~45 VA blip noted in the 24 Aug session doc (item 7); not in
the ground truth, since no action corresponds to it.

## Per-scenario summary

- **Item 1 (conferência do artefato de conexão)**: repouso limpo (0.20 +/- 0.007 VA, n=79).
  Sanduicheira conduzindo, 106 amostras antes do corte natural: desvio relativo 1.12%
  (mean 796.5 VA, sd 8.9 VA) -- dentro da faixa saudável, longe do artefato de 4.4%-4.8%
  descrito no handoff. Conexão validada, sessão liberada para prosseguir.
- **Item 2 (marco de sincronismo)**: on 804.0 VA / off 790.8 VA, ambos eventos únicos, sem
  fragmentação.
- **Item 3 (9 pares isolados)**: sanduicheira 3/3 repetições limpas (magnitude
  795.8 +/- 6.1 VA, 0.77% de dispersão, nenhuma fragmentada -- ver "What went wrong" item 2
  para a primeira tentativa da repetição 3, descartada e refeita). Ventilador 3/3 limpas,
  sem fragmentação (38.5-51.0 VA). Carregador: liga fragmentou em 2 eventos em 2 das 3
  repetições (rep 1: 47.5+51.2 VA; rep 2: 30.4+68.7 VA), e na 3a repetição resolveu em 1
  evento só mas subestimado (54.6 VA contra ~99 VA reais em regime), porque a primeira
  janela candidata da rampa ficou abaixo do limiar e virou baseline sem ser reportada como
  evento -- mesma assinatura da partida ruidosa do carregador já declarada fora de escopo em
  `ISSUE-exclude-transition-sample.md`. Desligar do carregador sempre limpo (93.1, 100.1,
  96.7 VA), como no levantamento de 24/08.
- **Item 4 (um acionamento simultâneo)**: sanduicheira+ventilador, fundidos em 1 evento no
  liga (838.4 VA) e 1 no desliga (824.0 VA) -- limitação de fusão se comportando como
  esperado, inalterada pela correção.
- **Item 5 (5 min com a sanduicheira sozinha)**: liga manual limpo (782.9 VA, evento único).
  Corte autônomo fragmentou em 2 eventos (736.5+41.7 VA = 778.2 VA), com o decaimento de
  potência se espalhando por 3 amostras em vez de 1 -- fora do que a correção cobre, já que
  ela pressupõe uma única amostra de transição, não uma decadência genuinamente mais lenta
  neste ciclo. O religamento autônomo seguinte fragmentou ainda mais, em 4 eventos
  (236.8 on, 56.3 off espúrio em plena rampa, 502.4 on, 86.3 on, somando ~882 VA) antes de
  assentar em ~785 VA -- a rampa mais desorganizada do dia inteiro, e exatamente o tipo de
  transição autônoma que este item foi desenhado para estressar. Corte final também
  autônomo, limpo (780.6 VA, evento único).

## Reading

Nas transições manuais controladas -- as que a correção mira -- a fragmentação da
sanduicheira caiu para 0 em 8 (sync + 3 repetições isoladas, incluindo a redo), com
magnitude convergindo em 795.8 +/- 6.1 VA (0.77% de dispersão), consistente com os
793.7 +/- 4.5 VA (0.6%) do replay de 25/08 sobre os dados de 24/08. O ventilador seguiu
limpo em todas as 3 repetições, como antes.

O carregador continua fragmentando (2 de 3 repetições no liga) ou subestimando a magnitude
(1 de 3), confirmando que a rampa de partida ruidosa é mesmo um problema fora do escopo
desta correção, não resolvido por ela -- exatamente como `ISSUE-exclude-transition-sample.md`
já previa.

As transições autônomas do termostato (item 5) continuam fragmentando, e desta vez de forma
mais severa que qualquer transição manual observada em 24/08 ou hoje: um corte em 2 eventos
e um religamento em 4. Isso não contradiz a correção -- ela resolve especificamente o caso de
uma única amostra straddling a transição, e aqui o decaimento/rampa da própria carga se
espalhou por várias amostras, um regime que a correção nunca se propôs a cobrir. Reportado
tal como observado, sem maquiagem.

## What this does not do

Não caracteriza o carregador (a rampa de partida dele já era conhecida como fora de escopo).
Não resolve nem tenta resolver a fragmentação de transições autônomas do termostato quando
o decaimento/rampa físico se espalha por mais de uma amostra -- essa classe de caso
permanece em aberto. Não usa a tomada inteligente como conferência independente (fora de
escopo desta sessão, reservada para etapas futuras). Não recalibra ganho/tensão -- a leitura
de bateria do carregador estar bem mais baixa que em 24/08 (5%-7% contra 40%-22%) já é
motivo suficiente para não comparar as magnitudes do carregador diretamente entre as duas
sessões.
