#include "ServoController.h"
#include "settings.h"

ServoController::ServoController() {
  pwm1 = Adafruit_PWMServoDriver(PCA1_ADRESS); 
  pwm2 = Adafruit_PWMServoDriver(PCA2_ADRESS);
  if(pwm1.begin()==false){
    Serial.println("DEBUG :pca1 i2c not found");
  }
  pwm1.setOscillatorFrequency(27000000);
  pwm1.setPWMFreq(SERVO_FREQUENCY);
   if(pwm2.begin()==false){
    Serial.println("DEBUG :pca2 i2c not found");
  }
  pwm2.setOscillatorFrequency(27000000);
  pwm2.setPWMFreq(SERVO_FREQUENCY);
  resetServosPosition();
}

void ServoController::setServoAngle(uint8_t servoNum, uint16_t angle) {
  // Adaptation de l'angle en plage de pulsations pour Adafruit_ServoDriver
  uint16_t pulsation = map(angle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE, SERVO_PULSE_MIN, SERVO_PULSE_MAX);
  int analog_value = int(float(pulsation) / 1000000 * SERVO_FREQUENCY * 4096);
  if(servoNum<15){
    pwm1.setPWM(servoNum, 0, analog_value);
  }else{
    pwm2.setPWM(servoNum-15, 0, analog_value);
  }

}

void ServoController::resetServosPosition() {
  // Utilisé au démarrage pour déplacer tout les servos en position initiale 
  for (uint8_t i = 0; i < NUMBER_OF_NOTES; ++i) {
    setServoAngle(i, initialAngles[i]);
    delay(200); // délai pour laisser les servos se déplacer
  }
}
//active la note avec le servo
void ServoController::noteOn(uint8_t servoNum) {
  setServoAngle(servoNum, initialAngles[servoNum]-ANGLE_NOTE_ON*sensRot[servoNum]);
}
//desactive la note avec le servo
void ServoController::noteOff(uint8_t servoNum) {
  setServoAngle(servoNum, initialAngles[servoNum]); 
}