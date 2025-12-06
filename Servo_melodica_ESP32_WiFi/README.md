# Servo Melodica - ESP32 WiFi MIDI (RTP-MIDI)

Version ESP32 avec WiFi MIDI (RTP-MIDI / AppleMIDI) pour contrôle réseau.

## 📋 Description

Contrôleur MIDI pour mélodica 32 touches utilisant ESP32 avec communication WiFi MIDI :
- ✅ **RTP-MIDI** : Protocole standard réseau MIDI
- ✅ Compatible macOS, Windows, iOS, Linux
- ✅ Portée réseau local illimitée
- ✅ Faible latence sur réseau local
- ✅ Connexion stable

## 🔧 Matériel Requis

| Composant | Quantité | Notes |
|-----------|----------|-------|
| ESP32 DevKit | 1 | ESP32-WROOM, DevKit v1, etc. |
| PCA9685 (I2C) | 2 | Adresses 0x40 et 0x41 |
| Servos SG90 (touches) | 32 | 30 touches + 2 réserve |
| Servo SG90 (air) | 1 | Contrôle débit d'air |
| Alimentation 5V/10A | 1 | Pour les servos |
| **Router WiFi** | 1 | Réseau local 2.4 GHz |

## 📚 Bibliothèques Requises

Installer via Arduino IDE Library Manager :

```
1. AppleMIDI (by lathoub)
   → Gère RTP-MIDI (WiFi MIDI)

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

### 1. Configuration WiFi (OBLIGATOIRE)

Éditer `settings.h` :
```cpp
//------------------------------------------- WiFi Configuration -----------------
#define WIFI_SSID "VotreSSID"          // Remplacer par votre SSID
#define WIFI_PASSWORD "VotreMotDePasse" // Remplacer par votre mot de passe
```

**Important** : Utiliser un réseau **2.4 GHz** (pas 5 GHz, ESP32 ne le supporte pas)

### 2. Calibration des servos

Utiliser **Calibration_Manual** pour calibrer :
```bash
1. Téléverser Calibration_Manual/Calibration_Manual.ino
2. Calibrer via Serial Monitor
3. Copier les valeurs générées
4. Coller dans settings.h (lignes initialAngles[] et sensRot[])
```

### 3. Pins I2C (optionnel)

Par défaut : GPIO 21 (SDA) et GPIO 22 (SCL)

Modifier dans `settings.h` si nécessaire :
```cpp
#define I2C_SDA 21  // Changer si besoin
#define I2C_SCL 22  // Changer si besoin
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
4. Ouvrir Serial Monitor (115200 bauds)
5. Noter l'adresse IP affichée
```

## 📱 Connexion WiFi MIDI

### macOS

```
1. Ouvrir "Audio MIDI Setup"
2. Fenêtre → Show MIDI Studio
3. Cliquer sur l'icône "Network" (globe)
4. Cliquer sur "+" pour ajouter une session
5. Chercher "Servo Melodica" dans la liste
6. Cliquer "Connect"
7. Le périphérique apparaît dans vos apps MIDI
```

### Windows

```
1. Installer "rtpMIDI" (Tobias Erichsen)
   → https://www.tobias-erichsen.de/software/rtpmidi.html

2. Lancer rtpMIDI
3. Dans "Directory", chercher "Servo Melodica"
4. Cliquer "Connect"
5. Le périphérique MIDI "Servo Melodica" est disponible
```

### iOS / iPadOS

```
1. Installer une app compatible RTP-MIDI (ex: "MIDIFlow")
2. Scanner le réseau
3. Connecter à "Servo Melodica"
4. Utiliser dans GarageBand, etc.
```

### Linux

```bash
# Installer QmidiNet
sudo apt-get install qmidinet

# Lancer QmidiNet
qmidinet

# Chercher "Servo Melodica" et connecter
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

### ESP32 ne se connecte pas au WiFi

```
✓ Vérifier WIFI_SSID et WIFI_PASSWORD dans settings.h
✓ Utiliser un réseau 2.4 GHz (pas 5 GHz)
✓ Vérifier Serial Monitor pour voir les erreurs
✓ Rapprocher ESP32 du router
✓ Redémarrer ESP32
```

### Périphérique MIDI non détecté

```
✓ Vérifier que ESP32 et ordinateur sont sur le même réseau
✓ Noter l'adresse IP dans Serial Monitor
✓ Ping l'adresse IP de l'ESP32
✓ Désactiver pare-feu temporairement pour tester
✓ Vérifier que rtpMIDI (Windows) ou Audio MIDI Setup (Mac) est ouvert
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

### Latence réseau

```
✓ Utiliser connexion WiFi directe (pas répéteur)
✓ Vérifier qualité signal WiFi
✓ Éviter trafic réseau élevé
✓ Utiliser réseau 2.4 GHz dédié si possible
```

## 📊 Avantages / Inconvénients

### ✅ Avantages
- Portée illimitée (réseau local)
- Très stable une fois connecté
- Protocole standard (RTP-MIDI)
- Compatible multiplateforme
- Faible latence sur bon réseau

### ❌ Inconvénients
- Nécessite réseau WiFi
- Configuration initiale (SSID/password)
- Peut avoir latence sur réseau chargé
- Dépend de la qualité du réseau WiFi

## 🔄 Versions Alternatives

| Version | Connexion | Avantages |
|---------|-----------|-----------|
| **Servo_melodica** | USB | Latence minimale |
| **Servo_melodica_Simple** | USB | Simple, pas EEPROM |
| **Servo_melodica_ESP32_BLE** | Bluetooth | Setup simple |
| **Servo_melodica_ESP32_WiFi** | **WiFi** | **Portée réseau** |

## 🌐 Adresse IP

L'adresse IP de l'ESP32 est affichée au démarrage dans Serial Monitor :
```
WiFi connected
IP address: 192.168.1.XXX
```

Notez cette adresse pour :
- Connexion RTP-MIDI
- Diagnostic réseau
- Configuration avancée

## 📄 Licence

MIT License
