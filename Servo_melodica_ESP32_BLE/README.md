# Servo Melodica - ESP32 Bluetooth MIDI

Version ESP32 avec Bluetooth Low Energy MIDI pour contrôle sans fil.

## 📋 Description

Contrôleur MIDI pour mélodica 32 touches utilisant ESP32 avec communication Bluetooth MIDI :
- ✅ **Bluetooth MIDI** : Connexion sans fil (BLE-MIDI)
- ✅ Compatible iOS, macOS, Windows, Android
- ✅ Portée: ~10-30 mètres
- ✅ Faible latence
- ✅ Pas de câble USB nécessaire (sauf pour alimentation)

## 🔧 Matériel Requis

| Composant | Quantité | Notes |
|-----------|----------|-------|
| ESP32 DevKit | 1 | ESP32-WROOM, DevKit v1, etc. |
| PCA9685 (I2C) | 2 | Adresses 0x40 et 0x41 |
| Servos SG90 (touches) | 32 | 30 touches + 2 réserve |
| Servo SG90 (air) | 1 | Contrôle débit d'air |
| Alimentation 5V/10A | 1 | Pour les servos |

## 📚 Bibliothèques Requises

Installer via Arduino IDE Library Manager :

```
1. ESP32-BLE-MIDI (by lathoub)
   → Gère la communication Bluetooth MIDI

2. Adafruit PWM Servo Driver Library
   → Contrôle des PCA9685

3. ESP32Servo
   → Contrôle servo air (compatible ESP32)
```

## 🔌 Connexions ESP32

### I2C (PCA9685)
```
ESP32 GPIO 21 (SDA)  →  PCA9685 SDA
ESP32 GPIO 22 (SCL)  →  PCA9685 SCL
```

### Servos
```
PCA9685 #1 (0x40)  →  Servos 0-14
PCA9685 #2 (0x41)  →  Servos 15-31
ESP32 GPIO 25      →  Servo Air
```

### Alimentation
```
ESP32: 5V USB ou Vin
PCA9685: VCC + V+ (alimentation externe 5V/10A)
GND: Commun partout
```

### Contrôle PCA
```
ESP32 GPIO 26  →  PIN_PCA_OFF (désactive servos au repos)
```

## 📝 Configuration

### 1. Calibration des servos

Utiliser **Calibration_Manual** pour calibrer :
```bash
1. Téléverser Calibration_Manual/Calibration_Manual.ino
2. Calibrer via Serial Monitor
3. Copier les valeurs générées
4. Coller dans settings.h (lignes initialAngles[] et sensRot[])
```

### 2. Pins I2C (optionnel)

Par défaut : GPIO 21 (SDA) et GPIO 22 (SCL)

Modifier dans `settings.h` si nécessaire :
```cpp
#define I2C_SDA 21  // Changer si besoin
#define I2C_SCL 22  // Changer si besoin
```

### 3. Autres pins (optionnel)

```cpp
#define AIR_SERVO_PIN 25    // Servo air (GPIO PWM)
#define PIN_PCA_OFF 26      // Contrôle alimentation PCA
```

## 🚀 Installation

### 1. Configuration Arduino IDE pour ESP32

```bash
# Ajouter l'URL des boards ESP32 :
File → Preferences → Additional Board Manager URLs:
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

# Installer ESP32 boards :
Tools → Board Manager → Rechercher "ESP32" → Install
```

### 2. Sélection de la carte

```
Tools → Board → ESP32 Arduino → ESP32 Dev Module
(ou votre modèle spécifique)
```

### 3. Upload

```bash
1. Connecter ESP32 via USB
2. Sélectionner le bon port (Tools → Port)
3. Upload (Ctrl+U)
```

## 📱 Connexion Bluetooth MIDI

### iOS / iPadOS

```
1. Ouvrir une app MIDI (GarageBand, etc.)
2. Aller dans les paramètres Bluetooth de l'app
3. Sélectionner "Servo Melodica"
4. Connexion automatique
```

### macOS

```
1. Ouvrir "Audio MIDI Setup"
2. Fenêtre → Show MIDI Studio
3. Cliquer sur l'icône Bluetooth
4. Sélectionner "Servo Melodica"
5. Connecter
```

### Windows

```
1. Installer "MIDIberry" ou "Bluetooth LE Explorer"
2. Scanner les périphériques BLE
3. Connecter à "Servo Melodica"
4. Utiliser un logiciel MIDI (Ableton, FL Studio, etc.)
```

### Android

```
1. Installer "MIDI+BTLE" ou app compatible
2. Scanner et connecter "Servo Melodica"
3. Utiliser dans votre app de musique
```

## 🎹 Utilisation

### Messages MIDI supportés

| Message | Fonction |
|---------|----------|
| **Note On** | Active touche + ouvre air |
| **Note Off** | Désactive touche + ferme air |
| **Velocity** | Contrôle débit d'air (30°-90°) |
| **CC 7** | Volume master |
| **CC 123** | All Notes Off (panic) |
| **CC 121** | Reset controllers |

### Plage de notes

```
Notes MIDI : 65 (F3) à 96 (C6)
32 notes chromatiques
```

## 🔍 Dépannage

### ESP32 ne se connecte pas en Bluetooth

```
✓ Vérifier que le Bluetooth est activé sur l'appareil
✓ Redémarrer l'ESP32
✓ Vérifier Serial Monitor pour les messages d'erreur
✓ Certains ESP32 ont des problèmes BLE → Tester avec un autre modèle
```

### PCA9685 non détecté

```
✓ Vérifier connexions I2C (SDA/SCL)
✓ Vérifier adresses (0x40 et 0x41)
✓ Tester avec I2C scanner
✓ GND commun ESP32 + PCA9685
```

### Servos ne bougent pas

```
✓ Alimentation externe 5V/10A connectée
✓ V+ et VCC du PCA9685 alimentés
✓ Vérifier calibration dans settings.h
```

### Latence élevée

```
✓ Réduire distance ESP32 ↔ appareil
✓ Éviter interférences WiFi 2.4GHz
✓ Utiliser ESP32 avec bonne antenne
✓ Tester version WiFi si problème persiste
```

## 📊 Avantages / Inconvénients

### ✅ Avantages
- Sans fil (pas de câble USB)
- Compatible multiplateforme
- Setup simple
- Bonne portée (~10-30m)
- Faible consommation

### ❌ Inconvénients
- Latence légèrement supérieure à USB
- Nécessite appareil compatible Bluetooth MIDI
- Portée limitée vs WiFi
- Peut avoir interférences BLE

## 🔄 Versions Alternatives

| Version | Connexion | Avantages |
|---------|-----------|-----------|
| **Servo_melodica** | USB | Latence minimale |
| **Servo_melodica_Simple** | USB | Simple, pas EEPROM |
| **Servo_melodica_ESP32_BLE** | **Bluetooth** | **Sans fil, simple** |
| **Servo_melodica_ESP32_WiFi** | WiFi | Portée étendue |

## 📄 Licence

MIT License
