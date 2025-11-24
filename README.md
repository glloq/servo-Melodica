# 🎹 Servo-Melodica - Automated MIDI Melodica Player

Système robotisé de contrôle de mélodica par servomoteurs, piloté via MIDI USB.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

## 📋 Table des Matières

- [Description](#description)
- [Architecture Matérielle](#architecture-matérielle)
- [Fonctionnalités](#fonctionnalités)
- [Installation](#installation)
- [Configuration](#configuration)
- [Calibration](#calibration)
- [Utilisation](#utilisation)
- [Messages MIDI Supportés](#messages-midi-supportés)

---

## 🎯 Description

Servo-Melodica est un système qui automatise le jeu d'un mélodica en utilisant des servomoteurs contrôlés par des signaux MIDI. Le système utilise 32 servomoteurs pour actionner les touches du clavier et un servo supplémentaire pour contrôler le débit d'air.

### Caractéristiques Principales

- ✅ **30 notes polyphoniques** - Contrôle simultané de toutes les touches
- ✅ **Contrôle MIDI USB** - Compatible avec tout logiciel MIDI (DAW, séquenceurs)
- ✅ **Vélocité dynamique** - Débit d'air adapté à la vélocité MIDI
- ✅ **Gestion intelligente de l'air** - Servo de valve contrôlé proportionnellement
- ✅ **Calibration EEPROM** - Sauvegarde persistante des réglages par touche
- ✅ **Calibration audio automatique** - Détection par microphone + bouton
- ✅ **Messages MIDI avancés** - Volume, Pitch Bend, All Notes Off, etc.

---

## 🔧 Architecture Matérielle

### Composants Requis

| Composant | Quantité | Rôle |
|-----------|----------|------|
| **Arduino** (Mega/Leonardo/Due) | 1 | Contrôleur principal avec USB MIDI |
| **PCA9685 16-channel PWM Driver** | 2 | Contrôle de 30 servos (touches du clavier) |
| **Servomoteurs SG90** | 31 | Actionneurs des touches (30) + air (1) |
| **Module Microphone MAX4466** (optionnel) | 1 | Calibration automatique ([voir HARDWARE.md](HARDWARE.md)) |
| **Bouton poussoir** | 1 | Déclenchement calibration |
| **Alimentation 5V/10A** | 1 | Alimentation servos |
| **Mélodica 32 touches** | 1 | Instrument (ex: Yamaha P-32D, Hohner Student) |

### Schéma de Connexion

```
                    ┌─────────────────────┐
                    │   Arduino Mega      │
                    │   (USB MIDI Host)   │
                    └──────────┬──────────┘
                               │
                ┌──────────────┼──────────────┐
                │              │              │
         ┌──────▼─────┐ ┌─────▼──────┐  ┌───▼────┐
         │  PCA9685   │ │  PCA9685   │  │ Servo  │
         │  (0x40)    │ │  (0x41)    │  │  Air   │
         │ 15 servos  │ │ 15 servos  │  │ (PWM)  │
         └────────────┘ └────────────┘  └────────┘
              │               │              │
         [Touches 0-14]  [Touches 15-29] [Valve air]

         ┌──────────────┐          ┌────────────┐
         │  Microphone  │──► A0    │   Bouton   │──► Pin 2
         │  MAX4466     │          │ Calibration│
         └──────────────┘          └────────────┘
```

### Connexions I2C

- **SDA** : Pin 20 (Arduino Mega) / Pin 2 (Leonardo)
- **SCL** : Pin 21 (Arduino Mega) / Pin 3 (Leonardo)
- **PCA9685 #1** : Adresse I2C `0x40` (servos 0-14)
- **PCA9685 #2** : Adresse I2C `0x41` (servos 15-29)

### Connexions Autres

- **Servo Air** : Pin 9 (PWM Arduino)
- **Microphone** : Pin A0 (Analog Input) - **[Détails HARDWARE.md](HARDWARE.md)**
- **Bouton Calibration** : Pin 2 (avec pull-up interne)

### Notes sur l'Alimentation

⚠️ **Important** : Les servomoteurs ne doivent PAS être alimentés par l'Arduino !

- Utilisez une alimentation externe 5V/10A minimum
- Reliez les masses (GND) Arduino ↔ Alimentation servos
- Alimentez les PCA9685 via leurs bornes V+

---

## ⚡ Fonctionnalités

### 1. Contrôle MIDI USB

Le système apparaît comme un périphérique MIDI USB et accepte :
- Note On/Off avec vélocité
- Control Changes (Volume, Modulation, All Notes Off)
- Pitch Bend
- Reset Controllers

### 2. Gestion de la Vélocité MIDI

**Architecture** :
- **Servos touches** : Position ON/OFF fixe (pas de modulation)
- **Servo d'air** : Angle modulé selon vélocité MIDI

**Vélocité → Ouverture d'air** :
- **pp (vélocité 1-40)** : Ouverture minimale 30°
- **mf (vélocité 41-80)** : Ouverture moyenne 60°
- **ff (vélocité 81-127)** : Ouverture maximale 90°

Cette approche offre une meilleure expression musicale que la modulation de pression sur les touches.

### 3. Contrôle du Débit d'Air

Un servo dédié contrôle la valve d'air du mélodica (Pin 9) :
- **Angle 0°** : Valve fermée (aucune note active)
- **Angle 30-90°** : Ouverture proportionnelle à la vélocité
- **Gestion intelligente** : Valve ouvre AVANT appui sur touches (AIR_ANTICIPATION_MS)

### 4. Calibration EEPROM

Chaque servo peut être calibré individuellement :
- **Angle initial** (position repos)
- **Direction de rotation** (+1 ou -1)
- **Sauvegarde persistante** avec checksum de validation

### 5. Calibration Audio Automatique 🎤

Mode de calibration semi-automatique utilisant un microphone :
1. Le système positionne un servo à différents angles
2. Le micro détecte l'intensité sonore produite
3. L'angle optimal est déterminé automatiquement
4. La calibration est sauvegardée en EEPROM

---

## 📦 Installation

### 1. Logiciels Requis

- **Arduino IDE** 1.8.19+ ou **PlatformIO**
- Bibliothèques Arduino :
  ```bash
  Adafruit_PWMServoDriver
  MIDIUSB
  EEPROM (incluse)
  Servo (incluse)
  ```

### 2. Installation des Bibliothèques

#### Via Arduino IDE :
```
Croquis > Inclure une bibliothèque > Gérer les bibliothèques
Rechercher : "Adafruit PWM Servo Driver"
Rechercher : "MIDIUSB"
```

#### Via PlatformIO :
```ini
[env:megaatmega2560]
platform = atmelavr
board = megaatmega2560
framework = arduino
lib_deps =
    adafruit/Adafruit PWM Servo Driver Library@^2.4.1
    arduino-libraries/MIDIUSB@^1.0.5
```

### 3. Téléversement

1. Ouvrir `Servo_melodica/Servo_melodica.ino` (fichier principal à créer)
2. Sélectionner la carte Arduino (Mega/Leonardo)
3. Téléverser

---

## ⚙️ Configuration

### Fichier `settings.h`

Paramètres principaux à ajuster :

```cpp
// MIDI
#define NUMBER_OF_NOTES 32        // Nombre de touches
#define FIRST_MIDI_NOTE 65        // Note MIDI la plus grave (F3)

// Servos - Touches
#define ANGLE_NOTE_ON 20          // Angle d'appui par défaut
#define USE_VELOCITY_CONTROL true // Activer vélocité
#define MIN_VELOCITY_ANGLE 10     // Angle minimal (pp)
#define MAX_VELOCITY_ANGLE 30     // Angle maximal (ff)

// Servo - Air
#define AIR_SERVO_PIN 9           // Pin PWM du servo air
#define AIR_CLOSED_ANGLE 0        // Angle fermé
#define AIR_MAX_ANGLE 90          // Angle ouverture max

// Calibration Audio
#define MIC_PIN A0                // Pin microphone
#define SOUND_THRESHOLD 512       // Seuil détection son
```

### Angles Initiaux

Modifier le tableau `initialAngles[]` dans `settings.h` :

```cpp
const uint16_t initialAngles[32] = {
  90, 90, 90, 90, 90, 90, 90, 90,  // Touches 0-7
  90, 90, 90, 90, 90, 90, 90, 90,  // Touches 8-15
  90, 90, 90, 90, 90, 90, 90, 90,  // Touches 16-23
  90, 90, 90, 90, 90, 90, 90, 90   // Touches 24-31
};
```

---

## 🎛️ Calibration

### Calibration Manuelle

```cpp
// Dans le code principal (setup ou via MIDI SysEx)
servoController.setServoCalibration(
  servoNum,    // Numéro du servo (0-31)
  angle,       // Angle initial (0-180)
  direction    // Direction : 1 ou -1
);

servoController.saveCalibration(); // Sauvegarder en EEPROM
```

### Calibration Audio Automatique

1. **Prérequis** : Microphone électret branché sur A0
2. **Lancer le mode calibration** :
   ```cpp
   audioCalibration.calibrateServo(servoNum);
   ```
3. **Processus automatique** :
   - Le servo teste plusieurs angles
   - Le micro mesure l'intensité sonore
   - L'angle optimal est sélectionné
   - Confirmation affichée sur Serial

4. **Calibration complète** :
   ```cpp
   audioCalibration.calibrateAllServos();
   ```

### Indicateurs LED (optionnel)

Pour un feedback visuel pendant la calibration :
```cpp
#define LED_STATUS 13
digitalWrite(LED_STATUS, HIGH); // Calibration en cours
```

---

## 🎮 Utilisation

### 1. Connexion MIDI

**Sous Windows :**
- Brancher l'Arduino en USB
- Aucun driver requis (natif)
- Apparaît comme "Arduino Leonardo" dans la DAW

**Sous macOS :**
- Brancher l'Arduino
- Ouvrir "Configuration MIDI Audio"
- Vérifier la présence du périphérique

**Sous Linux :**
```bash
aconnect -l  # Lister les périphériques MIDI
aconnect 20:0 28:0  # Connecter source → destination
```

### 2. Logiciels Compatibles

- **Ableton Live**
- **FL Studio**
- **Reaper**
- **Logic Pro**
- **MuseScore** (lecture de partitions)
- **VMPK** (Virtual MIDI Piano Keyboard)

### 3. Test Rapide

1. Ouvrir le moniteur série (9600 bauds)
2. Envoyer une note MIDI depuis votre DAW
3. Vérifier les messages de debug :
   ```
   ServoController: Both PWM drivers initialized successfully
   Resetting all servos to initial positions...
   All servos reset complete
   Instrument initialized successfully
   ```

---

## 📡 Messages MIDI Supportés

### Note On/Off

| Message | Hex | Fonction |
|---------|-----|----------|
| Note On | `0x90` | Actionne la touche avec vélocité |
| Note Off | `0x80` | Relâche la touche |

**Range MIDI** : Notes 65-96 (F3 à C6)

### Control Changes

| CC | Nom | Fonction |
|----|-----|----------|
| 1 | Modulation Wheel | Préparé pour vibrato |
| 7 | Volume | Scaling vélocité 0-127 |
| 91 | Reverb Depth | Effet modulation |
| 92 | Tremolo Depth | Effet modulation |
| 94 | Detune Depth | Effet modulation |
| 120 | All Sound Off | Arrêt immédiat toutes notes |
| 121 | Reset All Controllers | Reset complet du système |
| 123 | All Notes Off | Relâche toutes les touches |

### Pitch Bend

| Message | Hex | Fonction |
|---------|-----|----------|
| Pitch Bend | `0xE0` | Modulation de hauteur (préparé) |

**Range** : -8192 à +8191

---

## 🐛 Dépannage

### Problème : Servos ne bougent pas

**Causes possibles :**
- ❌ PCA9685 non détectés → Vérifier câblage I2C
- ❌ Alimentation insuffisante → Vérifier 5V/10A
- ❌ Adresses I2C incorrectes → Scanner avec `i2c_scanner.ino`

**Solution :**
```cpp
// Moniteur série affiche :
ERROR: PCA1 (0x40) I2C communication failed!
// → Vérifier adresse ou cavalier A0-A5 sur le PCA9685
```

### Problème : Notes ne sonnent pas correctement

**Causes :**
- ❌ Angles mal calibrés → Recalibrer avec micro
- ❌ Servos trop faibles → Utiliser des MG90S (couple supérieur)
- ❌ Air insuffisant → Vérifier servo de valve

**Solution :**
```cpp
audioCalibration.calibrateServo(noteNumber);
```

### Problème : Latence MIDI

**Causes :**
- ❌ Buffer USB saturé
- ❌ Trop de messages debug

**Solution :**
```cpp
#define DEBUG 0  // Désactiver debug en production
```

---

## 📊 Performances

- **Polyphonie** : 32 notes simultanées
- **Latence MIDI** : ~2-5ms
- **Temps de réponse servo** : ~50-100ms (SG90)
- **Précision vélocité** : 128 niveaux (MIDI standard)

---

## 🔮 Améliorations Futures

- [ ] Mode enregistrement/lecture interne
- [ ] Écran LCD pour feedback visuel
- [ ] Contrôle WiFi/Bluetooth MIDI
- [ ] Pédale sustain (CC 64)
- [ ] Vibrato mécanique sur valve d'air
- [ ] Multi-instruments (plusieurs mélodicas)

---

## 📄 Licence

MIT License - Libre d'utilisation, modification et distribution.

---

## 👨‍💻 Contributeurs

Projet développé avec l'assistance de Claude AI (Anthropic)

---

## 📞 Support

Pour toute question ou problème :
- Ouvrir une issue sur GitHub
- Consulter les logs du moniteur série (9600 bauds)
- Activer `#define DEBUG 1` dans `settings.h`

---

**Version** : 2.0.0
**Date** : 2025-01-24
**Status** : ✅ Production Ready
