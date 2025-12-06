# Servo Melodica - Version Simple

Version simplifiée du contrôleur MIDI pour mélodica avec servomoteurs.

## 📋 Description

Cette version utilise uniquement les valeurs de calibration définies dans `settings.h` :
- ❌ Pas de calibration automatique avec microphone
- ❌ Pas de bouton de calibration
- ❌ Pas d'EEPROM
- ✅ Configuration directe via `settings.h`
- ✅ Contrôle MIDI USB complet
- ✅ Vélocité gérée par servo d'air

## 🔧 Matériel Requis

| Composant | Quantité |
|-----------|----------|
| Arduino Mega/Leonardo | 1 |
| PCA9685 (I2C PWM driver) | 2 |
| Servos SG90 (touches) | 32 |
| Servo SG90 (air) | 1 |
| Câble USB | 1 |

## 📝 Configuration

### 1. Calibration préalable

Avant d'utiliser cette version, calibrez vos servos avec **Calibration_Manual** :
```
1. Téléverser Calibration_Manual/Calibration_Manual.ino
2. Calibrer les 32 servos via Serial Monitor
3. Générer le code avec la commande 'c'
4. Copier le code généré
```

### 2. Mise à jour des valeurs

Ouvrir `settings.h` et remplacer :
```cpp
// Angles initiaux (position repos, touche relâchée) - À calibrer
const uint16_t initialAngles[NUMBER_OF_NOTES] {90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90};

// Sens de rotation pour chaque servo
// +1 = rotation horaire pour appuyer, -1 = rotation anti-horaire
// À ajuster selon le montage mécanique de chaque servo
const int8_t sensRot[NUMBER_OF_NOTES] {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
```

Par les valeurs générées par l'outil de calibration.

### 3. Téléversement

```
1. Ouvrir Servo_melodica_Simple/Servo_melodica_Simple.ino
2. Téléverser sur Arduino
3. Connecter un logiciel MIDI (DAW, clavier MIDI USB, etc.)
4. Jouer ! 🎹
```

## 🎛️ Paramètres dans settings.h

```cpp
// MIDI
#define NUMBER_OF_NOTES 32      // Nombre de touches
#define FIRST_MIDI_NOTE 65      // Note MIDI la plus grave (F3)

// Air servo
#define AIR_SERVO_PIN 9         // Pin PWM Arduino
#define AIR_CLOSED_ANGLE 0      // Valve fermée
#define AIR_MIN_ANGLE 30        // Notes douces (velocity faible)
#define AIR_MAX_ANGLE 90        // Notes fortes (velocity élevée)

// Servos touches
#define ANGLE_NOTE_ON 20        // Déplacement pour appuyer (degrés)
#define SERVO_RESET_DELAY_MS 200 // Délai entre servos au démarrage

// PCA9685
#define PCA1_ADRESS 0x40        // Adresse I2C premier driver
#define PCA2_ADRESS 0x41        // Adresse I2C second driver
```

## 🎹 Fonctionnalités MIDI

### Messages supportés

| Message | CC# | Fonction |
|---------|-----|----------|
| Note On | - | Active la touche + ouvre air |
| Note Off | - | Désactive la touche + ferme air |
| Velocity | - | Contrôle ouverture valve d'air (30° à 90°) |
| Volume | 7 | Volume master (0-127) |
| All Notes Off | 123 | Stop toutes les notes (panic) |
| Reset Controllers | 121 | Reset à l'état initial |

### Architecture

```
MIDI USB → MidiHandler → Instrument → ServoController (touches)
                              ↓
                          Air Servo (vélocité)
```

**Important** :
- Les servos de touches : position fixe ON/OFF (pas de vélocité)
- Le servo d'air : angle proportionnel à la vélocité MIDI (30° à 90°)

## 🔍 Différences avec version complète

| Fonctionnalité | Servo_melodica | Servo_melodica_Simple |
|----------------|----------------|----------------------|
| Calibration manuelle (Serial) | ❌ | ✅ (via outil séparé) |
| Calibration automatique (micro) | ✅ | ❌ |
| Sauvegarde EEPROM | ✅ | ❌ |
| Bouton calibration | ✅ | ❌ |
| Configuration | Dynamique | Statique (settings.h) |
| Complexité | Avancée | Simple |

## 🐛 Dépannage

### Servos ne bougent pas
1. Vérifier alimentation externe 5V/10A
2. Vérifier GND commun (Arduino + PCA + Alimentation)
3. Vérifier adresses I2C (0x40 et 0x41)
4. Vérifier connexions SDA/SCL

### Notes ne jouent pas correctement
1. Vérifier calibration dans `settings.h`
2. Ajuster `ANGLE_NOTE_ON` si nécessaire
3. Vérifier `sensRot[]` pour chaque servo
4. Recalibrer avec Calibration_Manual

### MIDI ne fonctionne pas
1. Vérifier que l'Arduino est reconnu comme périphérique MIDI USB
2. Tester avec MIDI Monitor ou logiciel DAW
3. Vérifier plage de notes (65 à 96 par défaut)

## 📄 Structure des Fichiers

```
Servo_melodica_Simple/
├── Servo_melodica_Simple.ino   # Fichier principal
├── settings.h                   # Configuration (CALIBRATION ICI)
├── Instrument.h/.cpp            # Gestion instrument
├── MidiHandler.h/.cpp           # Décodage MIDI
├── ServoController.h/.cpp       # Contrôle servos
└── README.md                    # Ce fichier
```

## 📚 Workflow Recommandé

```
1. Calibration_Manual → Générer valeurs calibration
           ↓
2. settings.h → Coller valeurs
           ↓
3. Servo_melodica_Simple → Téléverser
           ↓
4. Connecter MIDI → Jouer !
```

## 📄 Licence

MIT License - Voir fichier LICENSE
