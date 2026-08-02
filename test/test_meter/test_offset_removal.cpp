#include <unity.h>

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

// A pure DC signal has no AC component: once the DC estimate settles, the
// estimated DC offset equals the input and the centered sample is ~zero.
void test_pure_dc_centers_to_zero(void) {
  Meter meter(makeConfig());
  const float dc = 1.65f;

  float centered = 0.0f;
  for (int i = 0; i < 10000; ++i) {
    centered = meter.addSample(dc);
  }

  TEST_ASSERT_FLOAT_WITHIN(1e-3f, dc, meter.dcOffset());
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, centered);
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
  for (int i = 0; i < 20000; ++i) {
    float volts = offset + amplitude * sinf(radPerSample * i);
    float centered = meter.addSample(volts);
    if (i >= 10000) {  // measure only after the estimate has settled
      sum += centered;
      if (centered > peak) peak = centered;
      ++counted;
    }
  }

  TEST_ASSERT_FLOAT_WITHIN(1e-3f, offset, meter.dcOffset());
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, static_cast<float>(sum / counted));
  TEST_ASSERT_FLOAT_WITHIN(5e-3f, amplitude, peak);
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_pure_dc_centers_to_zero);
  RUN_TEST(test_sine_offset_is_removed_ac_preserved);
  return UNITY_END();
}
