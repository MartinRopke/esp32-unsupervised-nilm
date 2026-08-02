#include <unity.h>

#include <cmath>

#include "meter.h"

// A representative install config. The offset removal behavior under test does
// not depend on these values, but the Meter is always constructed from config.
static MeterConfig makeConfig() {
  MeterConfig c;
  c.mainsVoltage = 220.0f;
  c.calibrationFactor = 1.0f;
  c.ctRatio = 2000.0f;
  c.burdenOhms = 22.0f;
  c.rmsWindowSeconds = 1.0f;
  c.tariff = 0.0f;
  return c;
}

// Sample spacing matching the firmware's 860 SPS loop (main.cpp).
static constexpr uint32_t kSampleIntervalMicros = 1163;

// Result of runUntilWindowCloses: the closing sample's result plus how many
// samples were fed to reach it.
struct WindowRun {
  SampleResult result;
  int samplesFed;
};

// Feeds samples from signal(i) (i starting at startIndex) spaced by
// intervalMicros, beginning at startTimestamp, until the RMS window closes.
template <typename SignalFn>
static WindowRun runUntilWindowCloses(Meter& meter, SignalFn signal, uint32_t intervalMicros,
                                       uint32_t startTimestamp = 0, int startIndex = 0) {
  uint32_t timestamp = startTimestamp;
  SampleResult result{};
  int i = startIndex;
  int fed = 0;
  do {
    result = meter.addSample(signal(i), timestamp);
    timestamp += intervalMicros;
    ++i;
    ++fed;
  } while (!result.windowClosed);
  return {result, fed};
}

// A pure DC signal has no AC component: once the DC estimate settles, the
// estimated DC offset equals the input and the centered sample is ~zero.
void test_pure_dc_centers_to_zero(void) {
  Meter meter(makeConfig());
  const float dc = 1.65f;

  SampleResult result{};
  uint32_t timestamp = 0;
  for (int i = 0; i < 10000; ++i) {
    result = meter.addSample(dc, timestamp);
    timestamp += kSampleIntervalMicros;
  }

  TEST_ASSERT_FLOAT_WITHIN(1e-3f, dc, meter.dcOffset());
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, result.centered);
}

// A burden signal is a sine riding on the bias offset. After the estimate
// settles, the DC offset equals the injected offset (not pulled by the AC), the
// centered samples have ~zero mean, and the AC swing is preserved (not flattened).
void test_sine_offset_is_removed_ac_preserved(void) {
  Meter meter(makeConfig());
  const float offset = 1.65f;
  const float amplitude = 0.1f;
  const float radPerSample = 2.0f * 3.14159265358979f * 60.0f / 860.0f;  // 60 Hz at 860 SPS

  double sum = 0.0;
  float peak = 0.0f;
  int counted = 0;
  uint32_t timestamp = 0;
  for (int i = 0; i < 20000; ++i) {
    float volts = offset + amplitude * sinf(radPerSample * i);
    SampleResult result = meter.addSample(volts, timestamp);
    timestamp += kSampleIntervalMicros;
    if (i >= 10000) {  // measure only after the estimate has settled
      sum += result.centered;
      if (result.centered > peak) peak = result.centered;
      ++counted;
    }
  }

  TEST_ASSERT_FLOAT_WITHIN(1e-3f, offset, meter.dcOffset());
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, static_cast<float>(sum / counted));
  TEST_ASSERT_FLOAT_WITHIN(5e-3f, amplitude, peak);
}

// A pure DC signal centers to ~0; a window closing over a stream with no AC
// component should report a burden V_rms of ~0.
void test_pure_dc_window_closes_with_zero_vrms(void) {
  Meter meter(makeConfig());
  auto dcSignal = [](int) { return 1.65f; };

  WindowRun run = runUntilWindowCloses(meter, dcSignal, kSampleIntervalMicros);

  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, run.result.vRms);
}

// A sine riding on a DC offset, once the DC estimate has settled, closes a
// window with burden V_rms = amplitude / sqrt(2) (the RMS of a sinusoid).
void test_sine_window_vrms_matches_amplitude_over_sqrt2(void) {
  Meter meter(makeConfig());
  const float offset = 1.65f;
  const float amplitude = 0.1f;
  const float radPerSample = 2.0f * 3.14159265358979f * 60.0f / 860.0f;  // 60 Hz at 860 SPS
  auto sineSignal = [&](int i) { return offset + amplitude * sinf(radPerSample * i); };

  // Warm up the DC estimate before trusting any window's V_rms.
  uint32_t timestamp = 0;
  int i = 0;
  for (; i < 10000; ++i) {
    meter.addSample(sineSignal(i), timestamp);
    timestamp += kSampleIntervalMicros;
  }

  // Measure the next window to close once the estimate has settled.
  WindowRun run = runUntilWindowCloses(meter, sineSignal, kSampleIntervalMicros, timestamp, i);

  const float expectedVrms = amplitude / sqrtf(2.0f);
  TEST_ASSERT_FLOAT_WITHIN(5e-3f, expectedVrms, run.result.vRms);
}

// The window closes when the elapsed timestamp reaches rmsWindowSeconds, not
// after a fixed sample count. At a coarser sample spacing than the nominal
// 860 SPS, fewer samples fit in the same 1 s window.
void test_window_closes_by_elapsed_time_not_sample_count(void) {
  Meter meter(makeConfig());
  auto dcSignal = [](int) { return 1.65f; };
  const uint32_t coarseIntervalMicros = 2000;  // ~500 SPS, not 860 SPS

  WindowRun run = runUntilWindowCloses(meter, dcSignal, coarseIntervalMicros);

  // ~500 samples close the window at this spacing; a window based on a fixed
  // sample count at the nominal 860 SPS rate would instead need 860 samples
  // regardless of spacing.
  TEST_ASSERT_TRUE(run.samplesFed < 600);
  TEST_ASSERT_TRUE(run.samplesFed > 400);
}

// A cold Meter's first RMS window can close (~860 samples) before the DC EMA
// has converged (settling time constant of kDcFilterWindow = 1024 samples),
// so its V_rms may run higher than the signal's true amplitude / sqrt(2). It
// must still stay bounded rather than diverge, since the underlying burden
// signal itself never exceeds offset + amplitude.
void test_first_window_before_dc_settles_is_bounded(void) {
  Meter meter(makeConfig());
  const float offset = 1.65f;
  const float amplitude = 0.1f;
  const float radPerSample = 2.0f * 3.14159265358979f * 60.0f / 860.0f;  // 60 Hz at 860 SPS
  const int startPhase = 5;  // off the zero crossing, so the DC seed is imperfect
  auto sineSignal = [&](int i) { return offset + amplitude * sinf(radPerSample * (i + startPhase)); };

  WindowRun run = runUntilWindowCloses(meter, sineSignal, kSampleIntervalMicros);

  TEST_ASSERT_TRUE(std::isfinite(run.result.vRms));
  TEST_ASSERT_TRUE(run.result.vRms < offset);
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_pure_dc_centers_to_zero);
  RUN_TEST(test_sine_offset_is_removed_ac_preserved);
  RUN_TEST(test_pure_dc_window_closes_with_zero_vrms);
  RUN_TEST(test_sine_window_vrms_matches_amplitude_over_sqrt2);
  RUN_TEST(test_window_closes_by_elapsed_time_not_sample_count);
  RUN_TEST(test_first_window_before_dc_settles_is_bounded);
  return UNITY_END();
}
