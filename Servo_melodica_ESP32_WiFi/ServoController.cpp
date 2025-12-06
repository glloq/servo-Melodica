#include "ServoController.h"
#include "settings.h"

ServoController::ServoController() : isInitialized(false) {
  pwm1 = Adafruit_PWMServoDriver(PCA1_ADRESS);
  pwm2 = Adafruit_PWMServoDriver(PCA2_ADRESS);

  // Load default values from settings.h
  for (uint8_t i = 0; i < NUMBER_OF_NOTES; i++) {
    currentAngles[i] = initialAngles[i];
    currentDirections[i] = sensRot[i];
  }
}

bool ServoController::begin() {
  // Initialize I2C with ESP32 custom pins
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.print("I2C initialized - SDA: GPIO");
  Serial.print(I2C_SDA);
  Serial.print(", SCL: GPIO");
  Serial.println(I2C_SCL);

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

// Active la note avec le servo (position fixe noteOn)
void ServoController::noteOn(uint8_t servoNum) {
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

  // Position fixe pour appuyer sur la touche
  // sensRot détermine le sens de rotation (+1 ou -1)
  setServoAngle(servoNum, currentAngles[servoNum] - ANGLE_NOTE_ON * currentDirections[servoNum]);
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