# 🎹 Servo-Melodica - Contrôleur MIDI Robotisé

Système robotisé pour contrôler un mélodica de 32 touches via MIDI, avec 3 options de connexion : **USB**, **Bluetooth** ou **WiFi**.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

---

## 📋 Qu'est-ce que c'est ?

Un robot qui joue du mélodica en contrôlant :
- **32 servos** pour appuyer sur les touches
- **1 servo** pour contrôler le débit d'air (vélocité MIDI)

**3 versions disponibles** : USB (Arduino), Bluetooth (ESP32), WiFi (ESP32)

---

## 🎯 Versions Disponibles

| Version | Hardware | Connexion | Portée | Latence | Difficulté |
|---------|----------|-----------|--------|---------|------------|
| **[USB](#-version-usb-arduino)** | Arduino Mega/Leonardo | Câble USB | 5m | Minimale | ⭐ Simple |
| **[Bluetooth](#-version-bluetooth-esp32)** | ESP32 | BLE-MIDI | 10-30m | Faible | ⭐⭐ Moyenne |
| **[WiFi](#-version-wifi-esp32)** | ESP32 | RTP-MIDI | Réseau local | Faible | ⭐⭐ Moyenne |

---

## 🔧 Matériel Commun (toutes versions)

| Composant | Quantité | Notes |
|-----------|----------|-------|
| PCA9685 (I2C PWM) | 2 | Adresses 0x40 et 0x41 |
| Servos SG90 | 33 | 32 touches + 1 air |
| Alimentation 5V/10A | 1 | Pour les servos uniquement |
| Mélodica 32 touches | 1 | Yamaha P-32D, Hohner, etc. |

---

## 🎛️ Calibration (OBLIGATOIRE)

**Avant d'utiliser le système, calibrer les servos :**

### Outil : Calibration_Manual (recommandé)

```bash
1. Téléverser Calibration_Manual/Calibration_Manual.ino
2. Ouvrir Serial Monitor (9600 bauds)
3. Utiliser commandes clavier :
   - p/n : Servo précédent/suivant
   - +/- : Ajuster angle (1°)
   - [/] : Ajuster angle (5°)
   - i   : Inverser sens rotation
   - t   : Tester noteOn/noteOff
   - c   : Générer code pour settings.h
4. Copier le code généré
5. Coller dans le fichier settings.h de votre version
6. Téléverser votre version
```

**Documentation** : [Calibration_Manual/README.md](Calibration_Manual/README.md)

---

## 🔌 Version USB (Arduino)

### Matériel supplémentaire
- Arduino Mega ou Leonardo (USB MIDI natif)

### Installation
```bash
1. Installer Arduino IDE
2. Installer bibliothèques :
   - Adafruit PWM Servo Driver Library
   - MIDIUSB
3. Ouvrir Servo_melodica_Simple/Servo_melodica_Simple.ino
4. Calibrer servos (voir section Calibration)
5. Téléverser sur Arduino
6. Connecter via USB à votre ordinateur/DAW
```

### Avantages / Inconvénients
✅ Latence minimale
✅ Configuration simple
✅ Connexion stable
❌ Câble USB requis
❌ Portée limitée (5m)

**Documentation** : [Servo_melodica_Simple/README.md](Servo_melodica_Simple/README.md)

---

## 📱 Version Bluetooth (ESP32)

### Matériel supplémentaire
- ESP32 DevKit (WROOM, DevKit v1, etc.)

### Installation
```bash
1. Installer Arduino IDE + support ESP32
2. Installer bibliothèques :
   - ESP32-BLE-MIDI (by lathoub)
   - Adafruit PWM Servo Driver Library
   - ESP32Servo
3. Ouvrir Servo_melodica_ESP32_BLE/Servo_melodica_ESP32_BLE.ino
4. Calibrer servos (voir section Calibration)
5. Téléverser sur ESP32
6. Sur votre appareil :
   - iOS : Ouvrir app MIDI → Connecter "Servo Melodica"
   - macOS : Audio MIDI Setup → Bluetooth → "Servo Melodica"
   - Windows : MIDIberry → Scanner → Connecter
   - Android : MIDI+BTLE → Scanner → Connecter
```

### Connexions ESP32
```
GPIO 21 (SDA) → PCA9685 SDA
GPIO 22 (SCL) → PCA9685 SCL
GPIO 25       → Servo Air
GPIO 26       → PIN_PCA_OFF
```

### Avantages / Inconvénients
✅ Sans fil (10-30m)
✅ Compatible iOS/macOS/Windows/Android
✅ Setup simple
❌ Latence légèrement supérieure
❌ Interférences possibles

**Documentation** : [Servo_melodica_ESP32_BLE/README.md](Servo_melodica_ESP32_BLE/README.md)

---

## 🌐 Version WiFi (ESP32)

### Matériel supplémentaire
- ESP32 DevKit
- Réseau WiFi 2.4 GHz

### Installation
```bash
1. Installer Arduino IDE + support ESP32
2. Installer bibliothèques :
   - AppleMIDI (by lathoub)
   - Adafruit PWM Servo Driver Library
   - ESP32Servo
3. Ouvrir Servo_melodica_ESP32_WiFi/Servo_melodica_ESP32_WiFi.ino
4. Configurer WiFi dans settings.h :
   #define WIFI_SSID "VotreSSID"
   #define WIFI_PASSWORD "VotreMotDePasse"
5. Calibrer servos (voir section Calibration)
6. Téléverser sur ESP32
7. Noter l'IP affichée dans Serial Monitor
8. Connecter depuis votre ordinateur :
   - macOS : Audio MIDI Setup → Network → "Servo Melodica"
   - Windows : Installer rtpMIDI → Connecter "Servo Melodica"
   - iOS : App compatible RTP-MIDI → Scanner réseau
```

### Connexions ESP32
```
GPIO 21 (SDA) → PCA9685 SDA
GPIO 22 (SCL) → PCA9685 SCL
GPIO 25       → Servo Air
GPIO 26       → PIN_PCA_OFF
```

### Avantages / Inconvénients
✅ Portée réseau local illimitée
✅ Très stable
✅ Protocole standard (RTP-MIDI)
❌ Nécessite réseau WiFi
❌ Configuration WiFi requise

**Documentation** : [Servo_melodica_ESP32_WiFi/README.md](Servo_melodica_ESP32_WiFi/README.md)

---

## 🎹 Utilisation (toutes versions)

### Messages MIDI supportés

| Message | Fonction |
|---------|----------|
| **Note On/Off** | Active/désactive touche + air |
| **Velocity (1-127)** | Contrôle débit d'air (30° à 90°) |
| **CC 7** | Volume master |
| **CC 123** | All Notes Off (panic button) |
| **CC 121** | Reset All Controllers |

### Plage de notes
```
Notes MIDI : 65 (F3) à 96 (C6)
32 notes chromatiques
```

### Architecture
```
Servos touches : Position ON/OFF fixe (pas de vélocité)
Servo air      : Angle 30°-90° (vélocité MIDI)
```

---

## 📁 Structure du Projet

```
servo-Melodica/
│
├── Servo_melodica/              # Version Arduino complète
│   └── Avec AudioCalibration + EEPROM
│
├── Servo_melodica_Simple/       # ⭐ Version Arduino simple (USB)
│   ├── settings.h               # Configuration angles + pins
│   └── README.md
│
├── Servo_melodica_ESP32_BLE/    # ⭐ Version ESP32 Bluetooth
│   ├── settings.h               # Configuration ESP32
│   └── README.md
│
├── Servo_melodica_ESP32_WiFi/   # ⭐ Version ESP32 WiFi
│   ├── settings.h               # Configuration WiFi + ESP32
│   └── README.md
│
└── Calibration_Manual/          # ⭐ Outil de calibration
    ├── Calibration_Manual.ino   # Serial Monitor (p, n, +, -, i, t, c)
    └── README.md
```

---

## 🚀 Démarrage Rapide

### Option 1 : USB (Arduino) - Le plus simple
```bash
1. Calibrer servos (Calibration_Manual)
2. Copier valeurs dans Servo_melodica_Simple/settings.h
3. Upload sur Arduino
4. Brancher USB → Jouer !
```

### Option 2 : Bluetooth (ESP32) - Sans fil
```bash
1. Calibrer servos (Calibration_Manual)
2. Copier valeurs dans Servo_melodica_ESP32_BLE/settings.h
3. Upload sur ESP32
4. Connecter Bluetooth → Jouer !
```

### Option 3 : WiFi (ESP32) - Portée maximale
```bash
1. Calibrer servos (Calibration_Manual)
2. Copier valeurs dans Servo_melodica_ESP32_WiFi/settings.h
3. Configurer WIFI_SSID et WIFI_PASSWORD
4. Upload sur ESP32
5. Connecter RTP-MIDI → Jouer !
```

---

## 🔍 Dépannage Rapide

### Servos ne bougent pas
```
✓ Vérifier alimentation 5V/10A externe
✓ GND commun (Arduino/ESP32 + Alimentation)
✓ Connexions I2C (SDA/SCL)
✓ Adresses PCA9685 (0x40 et 0x41)
```

### MIDI ne fonctionne pas
```
✓ USB : Vérifier périphérique MIDI détecté
✓ BLE : Appareil Bluetooth compatible BLE-MIDI
✓ WiFi : Même réseau + rtpMIDI (Windows) ou Audio MIDI Setup (Mac)
```

### Notes mal jouées
```
✓ Recalibrer avec Calibration_Manual
✓ Ajuster ANGLE_NOTE_ON dans settings.h
✓ Vérifier sens rotation (sensRot[] dans settings.h)
```

---

## 📊 Comparaison Détaillée

|  | USB Arduino | BLE ESP32 | WiFi ESP32 |
|---|---|---|---|
| **Setup** | ⭐⭐⭐ Simple | ⭐⭐ Moyen | ⭐⭐ Moyen |
| **Latence** | ~2-5ms | ~10-20ms | ~10-30ms |
| **Portée** | 5m (USB) | 10-30m | Illimitée (LAN) |
| **Câble** | USB requis | Aucun | Aucun |
| **Config** | Plug & Play | Plug & Play | WiFi requis |
| **Stabilité** | ⭐⭐⭐ | ⭐⭐ | ⭐⭐⭐ |
| **Compatibilité** | PC/Mac/Linux | iOS/Mac/Win/Android | PC/Mac/iOS/Linux |

---

## 📄 Licence

MIT License - Libre d'utilisation, modification et distribution.

---

## 🎓 En Savoir Plus

- **Calibration** : [Calibration_Manual/README.md](Calibration_Manual/README.md)
- **Version USB** : [Servo_melodica_Simple/README.md](Servo_melodica_Simple/README.md)
- **Version Bluetooth** : [Servo_melodica_ESP32_BLE/README.md](Servo_melodica_ESP32_BLE/README.md)
- **Version WiFi** : [Servo_melodica_ESP32_WiFi/README.md](Servo_melodica_ESP32_WiFi/README.md)

---

**Version** : 3.0.0
**Status** : ✅ Production Ready
**Date** : 2025-12-06
