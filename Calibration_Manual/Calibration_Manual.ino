/***********************************************************************************************
 *  SERVO MELODICA - OUTIL DE CALIBRATION MANUELLE (Serial Monitor)
 ***********************************************************************************************
 *
 *  Cet outil permet de calibrer manuellement chaque servo du mélodica via Serial Monitor :
 *  - Régler l'angle initial de chaque servo
 *  - Définir le sens de rotation (+1 ou -1)
 *  - Tester la position noteOn/noteOff
 *  - Générer le code à copier dans settings.h
 *
 *  MATÉRIEL REQUIS :
 *  - Arduino Mega/Leonardo
 *  - 2× PCA9685
 *  - 32 servos connectés (30 touches + 2 réserve)
 *  - Câble USB
 *
 *  UTILISATION :
 *  1. Téléverser ce code
 *  2. Ouvrir Serial Monitor (9600 bauds)
 *  3. Envoyer des commandes :
 *     p = Servo précédent
 *     n = Servo suivant
 *     - = Diminuer angle (1°)
 *     + = Augmenter angle (1°)
 *     [ = Diminuer angle (5°)
 *     ] = Augmenter angle (5°)
 *     i = Inverser sens rotation
 *     t = Tester noteOn/noteOff
 *     c = Afficher code pour settings.h
 *     h = Aide (afficher commandes)
 *
 ***********************************************************************************************/

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// ===== CONFIGURATION =====
#define NUMBER_OF_NOTES 32
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

// ===== VARIABLES GLOBALES =====
Adafruit_PWMServoDriver pwm1 = Adafruit_PWMServoDriver(PCA1_ADRESS);
Adafruit_PWMServoDriver pwm2 = Adafruit_PWMServoDriver(PCA2_ADRESS);

uint8_t currentServo = 0;
uint16_t servoAngles[NUMBER_OF_NOTES];
int8_t servoDirections[NUMBER_OF_NOTES];

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

void displayHelp() {
  Serial.println("\n╔══════════════════════════════════════════════════════════════╗");
  Serial.println("║                    COMMANDES DISPONIBLES                     ║");
  Serial.println("╠══════════════════════════════════════════════════════════════╣");
  Serial.println("║  p     → Servo précédent                                     ║");
  Serial.println("║  n     → Servo suivant                                       ║");
  Serial.println("║  -     → Diminuer angle de 1°                                ║");
  Serial.println("║  +     → Augmenter angle de 1°                               ║");
  Serial.println("║  [     → Diminuer angle de 5°                                ║");
  Serial.println("║  ]     → Augmenter angle de 5°                               ║");
  Serial.println("║  i     → Inverser sens de rotation                           ║");
  Serial.println("║  t     → Tester noteOn/noteOff                               ║");
  Serial.println("║  c     → Générer code pour settings.h                        ║");
  Serial.println("║  h     → Afficher cette aide                                 ║");
  Serial.println("╚══════════════════════════════════════════════════════════════╝\n");
}

void displayCurrentServo() {
  Serial.println("\n╔══════════════════════════════════════════╗");
  Serial.print("║  SERVO ");
  if (currentServo < 10) Serial.print(" ");
  Serial.print(currentServo);
  Serial.print(" / ");
  Serial.print(NUMBER_OF_NOTES - 1);
  Serial.println("                          ║");
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
  Serial.println("║  Envoyez 'h' pour voir les commandes    ║");
  Serial.println("╚══════════════════════════════════════════╝");

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
  Serial.println("// Angles initiaux (position repos, touche relâchée) - À calibrer");
  Serial.print("const uint16_t initialAngles[NUMBER_OF_NOTES] {");
  for (uint8_t i = 0; i < NUMBER_OF_NOTES; i++) {
    Serial.print(servoAngles[i]);
    if (i < NUMBER_OF_NOTES - 1) Serial.print(",");
  }
  Serial.println("};\n");

  // Sens de rotation
  Serial.println("// Sens de rotation pour chaque servo");
  Serial.println("// +1 = rotation horaire pour appuyer, -1 = rotation anti-horaire");
  Serial.println("// À ajuster selon le montage mécanique de chaque servo");
  Serial.print("const int8_t sensRot[NUMBER_OF_NOTES] {");
  for (uint8_t i = 0; i < NUMBER_OF_NOTES; i++) {
    Serial.print(servoDirections[i]);
    if (i < NUMBER_OF_NOTES - 1) Serial.print(",");
  }
  Serial.println("};\n");

  Serial.println("╔══════════════════════════════════════════════════════════════════════╗");
  Serial.println("║  1. Copier ce code                                                   ║");
  Serial.println("║  2. Ouvrir Servo_melodica/settings.h                                 ║");
  Serial.println("║  3. Remplacer les lignes initialAngles[] et sensRot[]                ║");
  Serial.println("║  4. Sauvegarder et téléverser le code MIDI principal                 ║");
  Serial.println("╚══════════════════════════════════════════════════════════════════════╝\n");
}

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("\n╔══════════════════════════════════════════════════════════════════════╗");
  Serial.println("║     SERVO MELODICA - OUTIL DE CALIBRATION MANUELLE (Serial)          ║");
  Serial.println("╚══════════════════════════════════════════════════════════════════════╝\n");

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
  Serial.println("║  2. Tester avec 't' si le servo appuie correctement                 ║");
  Serial.println("║  3. Si sens incorrect, appuyer sur 'i'                              ║");
  Serial.println("║  4. Passer au servo suivant avec 'n'                                ║");
  Serial.println("║  5. À la fin, appuyer sur 'c' pour générer le code                  ║");
  Serial.println("╠══════════════════════════════════════════════════════════════════════╣");
  Serial.println("║  Envoyez 'h' dans le Serial Monitor pour voir toutes les commandes  ║");
  Serial.println("╚══════════════════════════════════════════════════════════════════════╝\n");

  delay(1000);
  displayCurrentServo();
}

void loop() {
  if (Serial.available() > 0) {
    char command = Serial.read();

    switch (command) {
      case 'p':  // Servo précédent
        if (currentServo > 0) {
          currentServo--;
          Serial.println("← Servo précédent");
          displayCurrentServo();
        } else {
          Serial.println("⚠ Déjà au premier servo");
        }
        break;

      case 'n':  // Servo suivant
        if (currentServo < NUMBER_OF_NOTES - 1) {
          currentServo++;
          Serial.println("→ Servo suivant");
          displayCurrentServo();
        } else {
          Serial.println("⚠ Déjà au dernier servo");
        }
        break;

      case '-':  // Diminuer angle de 1°
        if (servoAngles[currentServo] > SERVO_MIN_ANGLE) {
          servoAngles[currentServo]--;
          setServoAngle(currentServo, servoAngles[currentServo]);
          Serial.print("Angle : ");
          Serial.print(servoAngles[currentServo]);
          Serial.println("°");
        }
        break;

      case '+':  // Augmenter angle de 1°
      case '=':  // Alternative pour +
        if (servoAngles[currentServo] < SERVO_MAX_ANGLE) {
          servoAngles[currentServo]++;
          setServoAngle(currentServo, servoAngles[currentServo]);
          Serial.print("Angle : ");
          Serial.print(servoAngles[currentServo]);
          Serial.println("°");
        }
        break;

      case '[':  // Diminuer angle de 5°
        if (servoAngles[currentServo] > SERVO_MIN_ANGLE + 4) {
          servoAngles[currentServo] -= 5;
          setServoAngle(currentServo, servoAngles[currentServo]);
          Serial.print("Angle : ");
          Serial.print(servoAngles[currentServo]);
          Serial.println("°");
        }
        break;

      case ']':  // Augmenter angle de 5°
        if (servoAngles[currentServo] < SERVO_MAX_ANGLE - 4) {
          servoAngles[currentServo] += 5;
          setServoAngle(currentServo, servoAngles[currentServo]);
          Serial.print("Angle : ");
          Serial.print(servoAngles[currentServo]);
          Serial.println("°");
        }
        break;

      case 'i':  // Inverser sens rotation
        servoDirections[currentServo] *= -1;
        Serial.print("Sens : ");
        Serial.println(servoDirections[currentServo] == 1 ? "+1 (horaire)" : "-1 (anti-horaire)");
        displayCurrentServo();
        break;

      case 't':  // Tester noteOn/noteOff
        testServo();
        break;

      case 'c':  // Générer code
        printCode();
        break;

      case 'h':  // Aide
      case '?':
        displayHelp();
        displayCurrentServo();
        break;

      case '\n':  // Ignorer newline
      case '\r':  // Ignorer carriage return
        break;

      default:
        Serial.print("⚠ Commande inconnue : ");
        Serial.println(command);
        Serial.println("Envoyez 'h' pour voir l'aide");
        break;
    }
  }
}
