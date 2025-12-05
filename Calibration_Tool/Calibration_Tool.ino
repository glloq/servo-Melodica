/***********************************************************************************************
 *  SERVO MELODICA - OUTIL DE CALIBRATION MANUELLE
 ***********************************************************************************************
 *
 *  Cet outil permet de calibrer manuellement chaque servo du mélodica :
 *  - Régler l'angle initial de chaque servo
 *  - Définir le sens de rotation (+1 ou -1)
 *  - Tester la position noteOn/noteOff
 *  - Générer le code à copier dans settings.h
 *
 *  MATÉRIEL REQUIS :
 *  - Arduino Mega/Leonardo
 *  - 2× PCA9685
 *  - 30 servos connectés
 *  - 3 boutons : PREV, NEXT, INVERT, ANGLE-, ANGLE+, TEST
 *
 *  CÂBLAGE BOUTONS (avec pull-up interne) :
 *  - PREV     : Pin 2  → Servo précédent
 *  - NEXT     : Pin 3  → Servo suivant
 *  - ANGLE-   : Pin 4  → Diminuer angle
 *  - ANGLE+   : Pin 5  → Augmenter angle
 *  - INVERT   : Pin 6  → Inverser sens rotation
 *  - TEST     : Pin 7  → Tester noteOn/noteOff
 *  - PRINT    : Pin 8  → Afficher code pour settings.h
 *
 ***********************************************************************************************/

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// ===== CONFIGURATION =====
#define NUMBER_OF_NOTES 30
#define ANGLE_NOTE_ON 20

// PCA9685
#define PCA1_ADRESS 0x40
#define PCA2_ADRESS 0x41
#define PWM_CHANNELS_PER_DRIVER 15

// Servos
#define SERVO_MIN_ANGLE 0
#define SERVO_MAX_ANGLE 180
#define MICROSECONDS_PER_SECOND 1000000L
const uint16_t SERVO_PULSE_MIN = 500;
const uint16_t SERVO_PULSE_MAX = 2500;
const uint16_t SERVO_FREQUENCY = 50;

// Boutons
#define BTN_PREV    2
#define BTN_NEXT    3
#define BTN_MINUS   4
#define BTN_PLUS    5
#define BTN_INVERT  6
#define BTN_TEST    7
#define BTN_PRINT   8

// ===== VARIABLES GLOBALES =====
Adafruit_PWMServoDriver pwm1 = Adafruit_PWMServoDriver(PCA1_ADRESS);
Adafruit_PWMServoDriver pwm2 = Adafruit_PWMServoDriver(PCA2_ADRESS);

uint8_t currentServo = 0;
uint16_t servoAngles[NUMBER_OF_NOTES];
int8_t servoDirections[NUMBER_OF_NOTES];

// Debouncing
unsigned long lastDebounceTime[7] = {0};
bool lastButtonState[7] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
const unsigned long debounceDelay = 200;

// ===== FONCTIONS =====

void setServoAngle(uint8_t servoNum, uint16_t angle) {
  if (servoNum >= NUMBER_OF_NOTES) return;

  angle = constrain(angle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
  uint16_t pulsation = map(angle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE, SERVO_PULSE_MIN, SERVO_PULSE_MAX);
  uint32_t analog_value = ((uint32_t)pulsation * SERVO_FREQUENCY * 4096UL) / MICROSECONDS_PER_SECOND;

  if (servoNum < PWM_CHANNELS_PER_DRIVER) {
    pwm1.setPWM(servoNum, 0, analog_value);
  } else {
    pwm2.setPWM(servoNum - PWM_CHANNELS_PER_DRIVER, 0, analog_value);
  }
}

void displayCurrentServo() {
  Serial.println("\n╔══════════════════════════════════════════╗");
  Serial.print("║  SERVO ");
  if (currentServo < 10) Serial.print(" ");
  Serial.print(currentServo);
  Serial.println(" / 29                          ║");
  Serial.println("╠══════════════════════════════════════════╣");
  Serial.print("║  Angle initial : ");
  if (servoAngles[currentServo] < 10) Serial.print("  ");
  else if (servoAngles[currentServo] < 100) Serial.print(" ");
  Serial.print(servoAngles[currentServo]);
  Serial.println("°                  ║");
  Serial.print("║  Sens rotation : ");
  Serial.print(servoDirections[currentServo] == 1 ? "+1 (horaire)    " : "-1 (anti-horaire)");
  Serial.println(" ║");
  Serial.println("╠══════════════════════════════════════════╣");
  Serial.println("║  [2] PREV   [3] NEXT                     ║");
  Serial.println("║  [4] ANGLE- [5] ANGLE+                   ║");
  Serial.println("║  [6] INVERT SENS                         ║");
  Serial.println("║  [7] TEST noteOn/Off                     ║");
  Serial.println("║  [8] PRINT CODE                          ║");
  Serial.println("╚══════════════════════════════════════════╝\n");

  // Positionner le servo à sa position de repos
  setServoAngle(currentServo, servoAngles[currentServo]);
}

void testServo() {
  Serial.println("\n→ Test noteOn...");
  uint16_t noteOnAngle = servoAngles[currentServo] - (ANGLE_NOTE_ON * servoDirections[currentServo]);
  setServoAngle(currentServo, noteOnAngle);
  delay(1000);

  Serial.println("→ Test noteOff...");
  setServoAngle(currentServo, servoAngles[currentServo]);
  delay(500);

  Serial.println("✓ Test terminé\n");
  displayCurrentServo();
}

void printCode() {
  Serial.println("\n╔══════════════════════════════════════════════════════════════════════╗");
  Serial.println("║  CODE À COPIER DANS settings.h                                       ║");
  Serial.println("╚══════════════════════════════════════════════════════════════════════╝\n");

  // Angles initiaux
  Serial.println("// Angles initiaux (position repos, touche relâchée)");
  Serial.print("const uint16_t initialAngles[NUMBER_OF_NOTES] {");
  for (uint8_t i = 0; i < NUMBER_OF_NOTES; i++) {
    Serial.print(servoAngles[i]);
    if (i < NUMBER_OF_NOTES - 1) Serial.print(",");
  }
  Serial.println("};\n");

  // Sens de rotation
  Serial.println("// Sens de rotation pour chaque servo");
  Serial.println("// +1 = rotation horaire, -1 = rotation anti-horaire");
  Serial.print("const int8_t sensRot[NUMBER_OF_NOTES] {");
  for (uint8_t i = 0; i < NUMBER_OF_NOTES; i++) {
    Serial.print(servoDirections[i]);
    if (i < NUMBER_OF_NOTES - 1) Serial.print(",");
  }
  Serial.println("};\n");

  Serial.println("╔══════════════════════════════════════════════════════════════════════╗");
  Serial.println("║  Copier ce code dans Servo_melodica/settings.h                      ║");
  Serial.println("║  Remplacer les lignes initialAngles[] et sensRot[]                  ║");
  Serial.println("╚══════════════════════════════════════════════════════════════════════╝\n");
}

bool checkButton(uint8_t btnIndex, uint8_t btnPin) {
  bool currentState = digitalRead(btnPin);

  if (currentState != lastButtonState[btnIndex]) {
    lastDebounceTime[btnIndex] = millis();
  }

  if ((millis() - lastDebounceTime[btnIndex]) > debounceDelay) {
    if (currentState == LOW && lastButtonState[btnIndex] == HIGH) {
      lastButtonState[btnIndex] = currentState;
      return true;
    }
  }

  lastButtonState[btnIndex] = currentState;
  return false;
}

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("\n╔══════════════════════════════════════════════════════════════════════╗");
  Serial.println("║        SERVO MELODICA - OUTIL DE CALIBRATION MANUELLE               ║");
  Serial.println("╚══════════════════════════════════════════════════════════════════════╝\n");

  // Configuration boutons
  pinMode(BTN_PREV, INPUT_PULLUP);
  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_MINUS, INPUT_PULLUP);
  pinMode(BTN_PLUS, INPUT_PULLUP);
  pinMode(BTN_INVERT, INPUT_PULLUP);
  pinMode(BTN_TEST, INPUT_PULLUP);
  pinMode(BTN_PRINT, INPUT_PULLUP);

  // Initialisation PCA9685
  Serial.println("Initialisation PCA9685...");

  if (!pwm1.begin()) {
    Serial.println("ERROR: PCA1 (0x40) non détecté!");
    while (1);
  }
  pwm1.setOscillatorFrequency(27000000);
  pwm1.setPWMFreq(SERVO_FREQUENCY);
  Serial.println("✓ PCA1 initialisé");

  if (!pwm2.begin()) {
    Serial.println("ERROR: PCA2 (0x41) non détecté!");
    while (1);
  }
  pwm2.setOscillatorFrequency(27000000);
  pwm2.setPWMFreq(SERVO_FREQUENCY);
  Serial.println("✓ PCA2 initialisé\n");

  // Initialiser les valeurs par défaut
  for (uint8_t i = 0; i < NUMBER_OF_NOTES; i++) {
    servoAngles[i] = 90;  // Angle par défaut
    servoDirections[i] = 1;  // Sens horaire par défaut
  }

  Serial.println("╔══════════════════════════════════════════════════════════════════════╗");
  Serial.println("║  INSTRUCTIONS                                                        ║");
  Serial.println("╠══════════════════════════════════════════════════════════════════════╣");
  Serial.println("║  1. Pour chaque servo, ajuster l'angle initial (touche relâchée)    ║");
  Serial.println("║  2. Tester avec bouton TEST si le servo appuie correctement         ║");
  Serial.println("║  3. Si sens incorrect, appuyer sur INVERT                           ║");
  Serial.println("║  4. Passer au servo suivant avec NEXT                               ║");
  Serial.println("║  5. À la fin, appuyer sur PRINT pour générer le code                ║");
  Serial.println("╚══════════════════════════════════════════════════════════════════════╝\n");

  delay(2000);
  displayCurrentServo();
}

void loop() {
  // Bouton PREV
  if (checkButton(0, BTN_PREV)) {
    if (currentServo > 0) {
      currentServo--;
      displayCurrentServo();
    }
  }

  // Bouton NEXT
  if (checkButton(1, BTN_NEXT)) {
    if (currentServo < NUMBER_OF_NOTES - 1) {
      currentServo++;
      displayCurrentServo();
    }
  }

  // Bouton ANGLE-
  if (checkButton(2, BTN_MINUS)) {
    if (servoAngles[currentServo] > SERVO_MIN_ANGLE) {
      servoAngles[currentServo]--;
      setServoAngle(currentServo, servoAngles[currentServo]);
      Serial.print("Angle : ");
      Serial.print(servoAngles[currentServo]);
      Serial.println("°");
    }
  }

  // Bouton ANGLE+
  if (checkButton(3, BTN_PLUS)) {
    if (servoAngles[currentServo] < SERVO_MAX_ANGLE) {
      servoAngles[currentServo]++;
      setServoAngle(currentServo, servoAngles[currentServo]);
      Serial.print("Angle : ");
      Serial.print(servoAngles[currentServo]);
      Serial.println("°");
    }
  }

  // Bouton INVERT
  if (checkButton(4, BTN_INVERT)) {
    servoDirections[currentServo] *= -1;
    Serial.print("Sens : ");
    Serial.println(servoDirections[currentServo] == 1 ? "+1 (horaire)" : "-1 (anti-horaire)");
    displayCurrentServo();
  }

  // Bouton TEST
  if (checkButton(5, BTN_TEST)) {
    testServo();
  }

  // Bouton PRINT
  if (checkButton(6, BTN_PRINT)) {
    printCode();
  }
}
