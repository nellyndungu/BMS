#include <Arduino.h>

// Configurations
#define ADC_PIN 35
#define ADC_SAMPLES 32

// Update interval
const uint32_t SAMPLE_INTERVAL_MS = 1000;

// Voltage Divider Configuration
const float R1 = 10000.0f;
const float R2 = 20000.0f;
const float GAIN = (R1 + R2) / R2;

// Battery Model Configuration
const float VBAT_FULL = 4.2f;

// DMM Offset Correction
const float VOLT_OFFSET = -0.05f;

// EMA filter (0-1, lower = smoother)
const float EMA_ALPHA = 0.2f;

// ADC calibration table (measured → corrected)
const float ADC_CAL_IN[]  = {   0,  500, 1000, 1500, 2000, 2500, 3000 };
const float ADC_CAL_OUT[] = {   0,  490,  995, 1490, 2010, 2520, 3020 };
const size_t ADC_CAL_POINTS = sizeof(ADC_CAL_IN) / sizeof(ADC_CAL_IN[0]);

// States
float filteredVoltage = 0.0f;
bool firstRun = true;
uint32_t lastSampleTime = 0;

// Functions
// ADC averaging
uint32_t readAdcMilliVolts(uint8_t pin, uint16_t samples) {
  uint32_t acc = 0;
  for (uint16_t i = 0; i < samples; i++) {
    acc += analogRead(pin);
    delayMicroseconds(200);
  }
  Serial.print ("Sum of samples taken: ");
  Serial.println(acc);
  uint32_t avg = acc / samples;
  // Convert to mV 
  float v_mV = (avg * 3300.0) / 4095.0;
  return (uint32_t)v_mV;
}

// Piecewise linear interpolation
float linearInterp(float x, const float *xv, const float *yv, size_t n) {
  if (x <= xv[0]) return yv[0];
  if (x >= xv[n - 1]) return yv[n - 1];

  for (size_t i = 0; i < n - 1; i++) {
    if (x >= xv[i] && x <= xv[i + 1]) {
      float ratio = (x - xv[i]) / (xv[i + 1] - xv[i]);
      return yv[i] + ratio * (yv[i + 1] - yv[i]);
    }
  }
  return x;
}

// Get Battery Voltage
float measureBatteryVoltage() {
  // 1. Raw ADC
  uint32_t raw_mV = readAdcMilliVolts(ADC_PIN, ADC_SAMPLES);
  // 2. ADC calibration
  float corrected_mV = linearInterp((float)raw_mV, ADC_CAL_IN, ADC_CAL_OUT, ADC_CAL_POINTS);
  // 3. Undo divider
  float vbat = (corrected_mV * GAIN) / 1000.0f;
  // 4. Offset correction
  //vbat += VOLT_OFFSET;
  return vbat;
}

// Compute percentage (linear model)
float computeBatteryPercent(float vbat) {
  float soc = (vbat) / (VBAT_FULL) * 100.0f;

  if (soc > 100.0f) soc = 100.0f;
  if (soc < 0.0f)   soc = 0.0f;

  return soc;
}

// EMA filter
float applyEMA(float newValue, float prevValue, float alpha){
  return alpha * newValue + (1.0f - alpha) * prevValue;
}

void setup(){
  // Begin Serial. 
  Serial.begin(115200);
  // Serial Start delay
  delay(1000);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  Serial.println("Continous Battery Monitor");
}

void loop(){
  uint32_t now = millis();

  // Run a fixed interval
  if(now - lastSampleTime >= SAMPLE_INTERVAL_MS){
    lastSampleTime = now;

    // Measure voltage
    float vbat  = measureBatteryVoltage();
    // Apply smoothing 
    if(firstRun){
      filteredVoltage = vbat;
      firstRun = false;
    }else {
      filteredVoltage = applyEMA(vbat, filteredVoltage, EMA_ALPHA);
    }

    // Compute Battery percentage
    float percentage = computeBatteryPercent(filteredVoltage);

    Serial.printf("Raw: %.3f V | Filtered: %.3f V | Battey: %.1f %%\n", vbat, filteredVoltage, percentage);
    
  }
}