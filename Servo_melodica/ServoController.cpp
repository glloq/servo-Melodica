#include "ServoController.h"
#include "settings.h"

ServoController::ServoController() : isInitialized(false) {
  pwm1 = Adafruit_PWMServoDriver(PCA1_ADRESS);
  pwm2 = Adafruit_PWMServoDriver(PCA2_ADRESS);

  // Try to load calibration from EEPROM, otherwise use defaults
  if (!loadCalibration()) {
    Serial.println("No valid calibration found, using defaults");
    resetToDefaultCalibration();
  } else {
    Serial.println("Calibration loaded from EEPROM");
  }
}

bool ServoController::begin() {
  // Initialize first PWM driver
  if (!pwm1.begin()) {
    Serial.println("ERROR: PCA1 (0x40) I2C communication failed!");
    Serial.println("Check wiring and I2C address.");
    return false;
  }
  pwm1.setOscillatorFrequency(27000000);
  pwm1.setPWMFreq(SERVO_FREQUENCY);

  // Initialize second PWM driver
  if (!pwm2.begin()) {
    Serial.println("ERROR: PCA2 (0x41) I2C communication failed!");
    Serial.println("Check wiring and I2C address.");
    return false;
  }
  pwm2.setOscillatorFrequency(27000000);
  pwm2.setPWMFreq(SERVO_FREQUENCY);

  isInitialized = true;
  Serial.println("ServoController: Both PWM drivers initialized successfully");

  resetServosPosition();
  return true;
}

bool ServoController::isReady() {
  return isInitialized;
}

void ServoController::setServoAngle(uint8_t servoNum, uint16_t angle) {
  // Validate parameters
  if (!isInitialized) {
    if (DEBUG) {
      Serial.println("ERROR: ServoController not initialized!");
    }
    return;
  }

  if (servoNum >= NUMBER_OF_NOTES) {
    if (DEBUG) {
      Serial.print("ERROR: Invalid servo number: ");
      Serial.println(servoNum);
    }
    return;
  }

  if (angle < SERVO_MIN_ANGLE || angle > SERVO_MAX_ANGLE) {
    if (DEBUG) {
      Serial.print("WARNING: Angle ");
      Serial.print(angle);
      Serial.println(" out of range, clamping");
    }
    angle = constrain(angle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
  }

  // Convert angle to pulse width
  uint16_t pulsation = map(angle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE, SERVO_PULSE_MIN, SERVO_PULSE_MAX);

  // Optimized calculation without float conversion
  // analog_value = (pulsation * SERVO_FREQUENCY * 4096) / MICROSECONDS_PER_SECOND
  uint32_t analog_value = ((uint32_t)pulsation * SERVO_FREQUENCY * 4096UL) / MICROSECONDS_PER_SECOND;

  // Select the appropriate PWM driver
  if (servoNum < PWM_CHANNELS_PER_DRIVER) {
    pwm1.setPWM(servoNum, 0, analog_value);
  } else {
    pwm2.setPWM(servoNum - PWM_CHANNELS_PER_DRIVER, 0, analog_value);
  }
}

void ServoController::resetServosPosition() {
  // Utilisé au démarrage pour déplacer tout les servos en position initiale
  if (!isInitialized) {
    Serial.println("ERROR: Cannot reset servos - controller not initialized!");
    return;
  }

  Serial.println("Resetting all servos to initial positions...");
  for (uint8_t i = 0; i < NUMBER_OF_NOTES; ++i) {
    setServoAngle(i, currentAngles[i]);
    delay(SERVO_RESET_DELAY_MS); // délai pour laisser les servos se déplacer
  }
  Serial.println("All servos reset complete");
}

// Active la note avec le servo
void ServoController::noteOn(uint8_t servoNum, uint8_t velocity) {
  if (!isInitialized) {
    if (DEBUG) {
      Serial.println("ERROR: ServoController not initialized!");
    }
    return;
  }

  if (servoNum >= NUMBER_OF_NOTES) {
    if (DEBUG) {
      Serial.print("ERROR: Invalid servo number in noteOn: ");
      Serial.println(servoNum);
    }
    return;
  }

  // Calculate angle based on velocity if enabled
  uint16_t pressAngle = ANGLE_NOTE_ON;

  if (USE_VELOCITY_CONTROL) {
    // Map MIDI velocity (0-127) to angle range (MIN_VELOCITY_ANGLE to MAX_VELOCITY_ANGLE)
    // velocity 0 is treated as 1 to avoid complete silence
    uint8_t vel = (velocity == 0) ? 1 : velocity;
    pressAngle = map(vel, 1, 127, MIN_VELOCITY_ANGLE, MAX_VELOCITY_ANGLE);

    if (DEBUG) {
      Serial.print("Velocity ");
      Serial.print(velocity);
      Serial.print(" -> Angle ");
      Serial.println(pressAngle);
    }
  }

  setServoAngle(servoNum, currentAngles[servoNum] - pressAngle * currentDirections[servoNum]);
}

// Desactive la note avec le servo
void ServoController::noteOff(uint8_t servoNum) {
  if (!isInitialized) {
    if (DEBUG) {
      Serial.println("ERROR: ServoController not initialized!");
    }
    return;
  }

  if (servoNum >= NUMBER_OF_NOTES) {
    if (DEBUG) {
      Serial.print("ERROR: Invalid servo number in noteOff: ");
      Serial.println(servoNum);
    }
    return;
  }

  setServoAngle(servoNum, currentAngles[servoNum]);
}

// ========== CALIBRATION FUNCTIONS ==========

uint16_t ServoController::calculateChecksum(const CalibrationData& data) {
  uint16_t sum = 0;
  sum += data.magicNumber;
  sum += data.version;

  for (uint8_t i = 0; i < NUMBER_OF_NOTES; i++) {
    sum += data.servoAngles[i];
    sum += (uint8_t)data.servoDirections[i];
  }

  return sum;
}

bool ServoController::saveCalibration() {
  CalibrationData data;

  data.magicNumber = EEPROM_MAGIC_NUMBER;
  data.version = EEPROM_VERSION;

  // Copy current calibration
  for (uint8_t i = 0; i < NUMBER_OF_NOTES; i++) {
    data.servoAngles[i] = currentAngles[i];
    data.servoDirections[i] = currentDirections[i];
  }

  // Calculate and store checksum
  data.checksum = calculateChecksum(data);

  // Write to EEPROM
  EEPROM.put(EEPROM_START_ADDRESS, data);

  Serial.println("Calibration saved to EEPROM");
  return true;
}

bool ServoController::loadCalibration() {
  CalibrationData data;

  // Read from EEPROM
  EEPROM.get(EEPROM_START_ADDRESS, data);

  // Validate magic number
  if (data.magicNumber != EEPROM_MAGIC_NUMBER) {
    if (DEBUG) {
      Serial.println("DEBUG: Invalid magic number in EEPROM");
    }
    return false;
  }

  // Validate version
  if (data.version != EEPROM_VERSION) {
    Serial.print("WARNING: EEPROM version mismatch (expected ");
    Serial.print(EEPROM_VERSION);
    Serial.print(", got ");
    Serial.print(data.version);
    Serial.println(")");
    return false;
  }

  // Validate checksum
  uint16_t calculatedChecksum = calculateChecksum(data);
  if (calculatedChecksum != data.checksum) {
    Serial.println("ERROR: EEPROM checksum mismatch - data corrupted!");
    return false;
  }

  // Load calibration
  for (uint8_t i = 0; i < NUMBER_OF_NOTES; i++) {
    currentAngles[i] = data.servoAngles[i];
    currentDirections[i] = data.servoDirections[i];
  }

  return true;
}

void ServoController::setServoCalibration(uint8_t servoNum, uint16_t angle, int8_t direction) {
  if (servoNum >= NUMBER_OF_NOTES) {
    Serial.println("ERROR: Invalid servo number for calibration");
    return;
  }

  currentAngles[servoNum] = angle;
  currentDirections[servoNum] = direction;

  if (DEBUG) {
    Serial.print("Servo ");
    Serial.print(servoNum);
    Serial.print(" calibrated: angle=");
    Serial.print(angle);
    Serial.print(" direction=");
    Serial.println(direction);
  }
}

void ServoController::resetToDefaultCalibration() {
  // Load default values from settings.h
  for (uint8_t i = 0; i < NUMBER_OF_NOTES; i++) {
    currentAngles[i] = initialAngles[i];
    currentDirections[i] = sensRot[i];
  }

  Serial.println("Calibration reset to defaults");
}

bool ServoController::isCalibrationValid() {
  CalibrationData data;
  EEPROM.get(EEPROM_START_ADDRESS, data);

  if (data.magicNumber != EEPROM_MAGIC_NUMBER) {
    return false;
  }

  if (data.version != EEPROM_VERSION) {
    return false;
  }

  uint16_t calculatedChecksum = calculateChecksum(data);
  return (calculatedChecksum == data.checksum);
}