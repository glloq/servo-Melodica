#include "AudioCalibration.h"

AudioCalibration::AudioCalibration(ServoController& sc, Instrument& inst)
  : servoController(sc), instrument(inst), lastButtonState(HIGH), lastDebounceTime(0) {
  // Configure microphone pin
  pinMode(MIC_PIN, INPUT);

  // Configure calibration button with internal pull-up
  pinMode(CALIBRATION_BUTTON_PIN, INPUT_PULLUP);

  Serial.println("AudioCalibration initialized");
  Serial.println("Press calibration button (Pin 2) to start auto-calibration");
}

uint16_t AudioCalibration::readSoundLevel() {
  // Read raw analog value from microphone
  uint16_t soundLevel = analogRead(MIC_PIN);
  return soundLevel;
}

uint16_t AudioCalibration::readAverageSoundLevel(uint8_t samples) {
  // Read multiple samples and return average
  uint32_t sum = 0;

  for (uint8_t i = 0; i < samples; i++) {
    sum += readSoundLevel();
    delay(10);  // Short delay between samples
  }

  return sum / samples;
}

bool AudioCalibration::waitForSoundStabilization() {
  // Wait for ambient sound to stabilize
  delay(100);

  // Check if sound level is below threshold (quiet environment)
  uint16_t ambientLevel = readAverageSoundLevel(5);

  if (ambientLevel > SOUND_THRESHOLD * 2) {
    Serial.println("WARNING: Environment too noisy for calibration!");
    Serial.print("Ambient level: ");
    Serial.println(ambientLevel);
    return false;
  }

  return true;
}

void AudioCalibration::checkCalibrationButton() {
  // Read button state (LOW = pressed with pull-up)
  bool currentButtonState = digitalRead(CALIBRATION_BUTTON_PIN);

  // Debouncing: only process if state changed and enough time passed
  if (currentButtonState != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > 50) {  // 50ms debounce
    // Button pressed (LOW with pull-up)
    if (currentButtonState == LOW && lastButtonState == HIGH) {
      Serial.println("\n*** CALIBRATION BUTTON PRESSED ***");
      Serial.println("Starting full auto-calibration...");

      // Launch full calibration
      calibrateAllServos();

      Serial.println("*** CALIBRATION COMPLETE ***\n");
    }
  }

  lastButtonState = currentButtonState;
}

bool AudioCalibration::checkMicrophone() {
  Serial.println("\n=== Microphone Test ===");
  Serial.println("Reading ambient sound level...");

  uint16_t level = readAverageSoundLevel(20);

  Serial.print("Average ambient level: ");
  Serial.println(level);

  if (level < 10) {
    Serial.println("ERROR: Microphone appears disconnected or not working!");
    return false;
  }

  if (level > SOUND_THRESHOLD * 3) {
    Serial.println("WARNING: Environment too noisy!");
    Serial.println("Please reduce background noise for accurate calibration.");
    return false;
  }

  Serial.println("Microphone OK - Ready for calibration");
  return true;
}

uint16_t AudioCalibration::testServoAngle(uint8_t servoNum, uint16_t angle) {
  // Test a specific angle and return the sound level produced

  // Move servo to test position
  servoController.setServoCalibration(servoNum, angle, 1);

  // Open air valve slightly
  // (This is a simplified version - you may need to adapt based on your setup)

  // Wait for stabilization
  delay(CALIBRATION_DELAY_MS);

  // Trigger note (position fixe, pas de vélocité)
  servoController.noteOn(servoNum);

  // Wait for sound to develop
  delay(150);

  // Measure sound level
  uint16_t soundLevel = readAverageSoundLevel(MIC_SAMPLES);

  // Stop note
  servoController.noteOff(servoNum);

  // Wait for sound to decay
  delay(200);

  if (DEBUG) {
    Serial.print("  Angle ");
    Serial.print(angle);
    Serial.print("° → Sound level: ");
    Serial.println(soundLevel);
  }

  return soundLevel;
}

bool AudioCalibration::calibrateServo(uint8_t servoNum) {
  Serial.println("\n=== Calibrating Servo ===");
  Serial.print("Servo number: ");
  Serial.println(servoNum);

  // Validate servo number
  if (servoNum >= NUMBER_OF_NOTES) {
    Serial.println("ERROR: Invalid servo number!");
    return false;
  }

  // Check environment
  if (!waitForSoundStabilization()) {
    Serial.println("ERROR: Cannot calibrate in noisy environment!");
    return false;
  }

  // Variables for best angle detection
  uint16_t bestAngle = 90;
  uint16_t maxSoundLevel = 0;

  Serial.println("Testing angles...");
  Serial.println("Angle | Sound Level");
  Serial.println("------|------------");

  // Test each angle
  for (uint8_t i = 0; i < CALIBRATION_TEST_ANGLES; i++) {
    uint16_t testAngle = CALIBRATION_ANGLE_START + (i * CALIBRATION_ANGLE_STEP);

    // Test this angle
    uint16_t soundLevel = testServoAngle(servoNum, testAngle);

    // Print result
    Serial.print(testAngle);
    Serial.print("°   | ");
    Serial.println(soundLevel);

    // Check if this is the best angle so far
    if (soundLevel > maxSoundLevel) {
      maxSoundLevel = soundLevel;
      bestAngle = testAngle;
    }
  }

  // Check if we found a valid sound level
  if (maxSoundLevel < SOUND_THRESHOLD) {
    Serial.println("\nWARNING: No significant sound detected!");
    Serial.println("Possible issues:");
    Serial.println("- Microphone not properly positioned");
    Serial.println("- Air valve not opening");
    Serial.println("- Servo not pressing key properly");
    Serial.println("Using default angle...");
    bestAngle = 90;
  } else {
    Serial.println("\n=== Calibration Result ===");
    Serial.print("Best angle: ");
    Serial.print(bestAngle);
    Serial.println("°");
    Serial.print("Max sound level: ");
    Serial.println(maxSoundLevel);
  }

  // Save calibration
  servoController.setServoCalibration(servoNum, bestAngle, 1);

  // Test the final calibration
  Serial.println("\nTesting final calibration...");
  delay(500);
  servoController.noteOn(servoNum);
  delay(1000);
  servoController.noteOff(servoNum);
  delay(500);

  Serial.println("Calibration complete!");
  Serial.println("========================================\n");

  return true;
}

void AudioCalibration::calibrateAllServos() {
  Serial.println("\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║   FULL CALIBRATION - ALL SERVOS        ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println();

  // Check microphone first
  if (!checkMicrophone()) {
    Serial.println("ABORT: Microphone not working properly!");
    return;
  }

  Serial.println("\nStarting calibration in 3 seconds...");
  Serial.println("Please ensure:");
  Serial.println("- Quiet environment");
  Serial.println("- Air servo is functional");
  Serial.println("- All servos are properly mounted");
  delay(3000);

  // Calibrate each servo
  uint8_t successCount = 0;

  for (uint8_t i = 0; i < NUMBER_OF_NOTES; i++) {
    Serial.print("\nProgress: ");
    Serial.print(i + 1);
    Serial.print("/");
    Serial.println(NUMBER_OF_NOTES);

    if (calibrateServo(i)) {
      successCount++;
    }

    delay(1000);  // Delay between servos
  }

  // Save all calibrations to EEPROM
  Serial.println("\n=== Saving Calibration to EEPROM ===");
  servoController.saveCalibration();

  // Summary
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║      CALIBRATION COMPLETE              ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.print("Successfully calibrated: ");
  Serial.print(successCount);
  Serial.print("/");
  Serial.println(NUMBER_OF_NOTES);
  Serial.println("\nCalibration data saved to EEPROM.");
  Serial.println("System ready to play!");
  Serial.println();
}
