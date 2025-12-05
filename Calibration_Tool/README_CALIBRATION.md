# 🛠️ Outil de Calibration Manuelle - Servo Melodica

Outil Arduino pour calibrer manuellement chaque servo **sans microphone**.

---

## 📋 Description

Cet outil permet de :
- ✅ Régler l'angle initial de chaque servo (position repos)
- ✅ Définir le sens de rotation (+1 ou -1)
- ✅ Tester chaque servo (noteOn / noteOff)
- ✅ Générer automatiquement le code pour `settings.h`

---

## 🔧 Matériel Requis

| Composant | Quantité |
|-----------|----------|
| Arduino Mega/Leonardo | 1 |
| PCA9685 | 2 |
| Servos SG90 | 30 |
| Boutons poussoir | 7 |
| Câbles |quelques-uns |

---

## 🔌 Câblage des Boutons

Tous les boutons utilisent le **pull-up interne** (pas besoin de résistance).

```
Arduino Pin X ──┬── [Bouton NO] ── GND
                │
         (Pull-up interne)
```

### Affectation des Pins

| Bouton | Pin | Fonction |
|--------|-----|----------|
| **PREV** | 2 | Servo précédent |
| **NEXT** | 3 | Servo suivant |
| **ANGLE-** | 4 | Diminuer angle initial |
| **ANGLE+** | 5 | Augmenter angle initial |
| **INVERT** | 6 | Inverser sens de rotation |
| **TEST** | 7 | Tester noteOn/noteOff |
| **PRINT** | 8 | Afficher code pour settings.h |

---

## 📖 Mode d'Emploi

### 1. Téléversement

1. Ouvrir `Calibration_Tool.ino` dans Arduino IDE
2. Sélectionner la carte (Mega/Leonardo)
3. Téléverser

### 2. Utilisation

```
╔══════════════════════════════════════════╗
║  SERVO 00 / 29                           ║
╠══════════════════════════════════════════╣
║  Angle initial :  90°                    ║
║  Sens rotation : +1 (horaire)            ║
╠══════════════════════════════════════════╣
║  [2] PREV   [3] NEXT                     ║
║  [4] ANGLE- [5] ANGLE+                   ║
║  [6] INVERT SENS                         ║
║  [7] TEST noteOn/Off                     ║
║  [8] PRINT CODE                          ║
╚══════════════════════════════════════════╝
```

### 3. Calibration Étape par Étape

Pour **chaque servo** (0 à 29) :

#### Étape A : Positionner en position REPOS (touche relâchée)

1. Observer le servo et la touche
2. Utiliser **ANGLE+** ou **ANGLE-** pour ajuster
3. L'angle doit correspondre à la **position juste avant d'appuyer**
4. Vérifier que le servo ne touche PAS la touche

#### Étape B : Tester l'appui

1. Appuyer sur **TEST** (Pin 7)
2. Observer le mouvement :
   - **noteOn** : Le servo appuie sur la touche (1 seconde)
   - **noteOff** : Le servo revient en position repos

#### Étape C : Corriger le sens si nécessaire

Si le servo tourne **dans le mauvais sens** :
1. Appuyer sur **INVERT** (Pin 6)
2. Refaire le test (**TEST**)

#### Étape D : Passer au suivant

1. Appuyer sur **NEXT** (Pin 3)
2. Répéter pour tous les servos

### 4. Générer le Code

Une fois **tous les servos calibrés** :

1. Appuyer sur **PRINT** (Pin 8)
2. Le moniteur série affiche :

```cpp
// Angles initiaux (position repos, touche relâchée)
const uint16_t initialAngles[NUMBER_OF_NOTES] {85,92,88,90,87,95,...};

// Sens de rotation pour chaque servo
// +1 = rotation horaire, -1 = rotation anti-horaire
const int8_t sensRot[NUMBER_OF_NOTES] {1,1,-1,1,1,-1,1,1,-1,...};
```

3. **Copier ce code** dans `Servo_melodica/settings.h`
4. **Remplacer** les anciennes lignes `initialAngles[]` et `sensRot[]`

---

## 🎯 Exemples d'Utilisation

### Exemple 1 : Servo tourne dans le mauvais sens

```
État initial :
  Angle : 90°
  Sens : +1 (horaire)

[TEST] → Le servo recule au lieu d'avancer !

Solution :
  [INVERT] → Sens : -1 (anti-horaire)
  [TEST] → OK ! Le servo appuie correctement
```

### Exemple 2 : Position repos mal réglée

```
État initial :
  Angle : 90°
  Le servo touche déjà la touche !

Solution :
  [ANGLE+] → 91°
  [ANGLE+] → 92°
  [ANGLE+] → 93° → OK ! Plus de contact
  [TEST] → Vérifier que l'appui fonctionne
```

---

## 📊 Conseils de Calibration

### Position Repos (Angle Initial)

| ✅ CORRECT | ❌ INCORRECT |
|-----------|-------------|
| Servo ne touche PAS la touche | Servo appuie déjà |
| Distance ~1-2mm de la touche | Trop éloigné (>5mm) |
| Position stable | Servo vibre |

### Sens de Rotation

| Sens | Quand utiliser |
|------|----------------|
| **+1 (horaire)** | Servo monté normalement |
| **-1 (anti-horaire)** | Servo monté à l'envers |

**Règle** : Si TEST fait reculer le servo au lieu d'appuyer → INVERT

### Réglage Fin

1. **Trop serré** → Angle trop faible → ANGLE+
2. **Trop lâche** → Angle trop élevé → ANGLE-
3. **Appui faible** → Augmenter `ANGLE_NOTE_ON` (dans le code, ligne 21)

---

## 🔄 Navigation Rapide

### Raccourcis Clavier (via Serial Monitor)

Si vous préférez utiliser le moniteur série :

```
Envoyer :
  p → Servo précédent (PREV)
  n → Servo suivant (NEXT)
  - → Diminuer angle
  + → Augmenter angle
  i → Inverser sens
  t → Tester
  c → Print code
```

_(Cette fonctionnalité peut être ajoutée facilement au code)_

---

## 🐛 Dépannage

### Problème : PCA9685 non détecté

**Message** : `ERROR: PCA1 (0x40) non détecté!`

**Causes** :
- ❌ Câblage I2C incorrect
- ❌ Alimentation PCA9685 manquante
- ❌ Adresse I2C incorrecte

**Solution** :
1. Vérifier SDA/SCL (Mega : 20/21, Leonardo : 2/3)
2. Vérifier alimentation 5V du PCA9685
3. Scanner adresses I2C avec `i2c_scanner.ino`

### Problème : Servos ne bougent pas

**Causes** :
- ❌ Alimentation servos insuffisante
- ❌ Servos mal connectés aux PCA9685

**Solution** :
1. Vérifier alimentation externe 5V/10A
2. Vérifier connexion GND commune
3. Tester un servo directement sur Arduino Pin 9

### Problème : Boutons ne répondent pas

**Causes** :
- ❌ Câblage boutons incorrect
- ❌ Debounce trop court

**Solution** :
1. Vérifier connexion bouton → GND
2. Augmenter `debounceDelay` (ligne 51) à 300ms

---

## 📝 Modification du Code

### Changer l'angle de course

```cpp
// Ligne 21
#define ANGLE_NOTE_ON 20  // Changer si appui trop faible/fort
```

### Changer le debounce

```cpp
// Ligne 51
const unsigned long debounceDelay = 200;  // Augmenter si boutons rebondissent
```

### Ajouter un servo supplémentaire

```cpp
// Ligne 18
#define NUMBER_OF_NOTES 32  // Si vous avez 32 servos au lieu de 30
```

---

## 🚀 Workflow Complet

```
1. [Téléverser Calibration_Tool.ino]
          ↓
2. [Ouvrir Serial Monitor 9600 bauds]
          ↓
3. [Pour chaque servo 0-29:]
   → Ajuster angle repos (ANGLE+/-)
   → Tester (TEST)
   → Inverser si nécessaire (INVERT)
   → Passer au suivant (NEXT)
          ↓
4. [PRINT] → Copier code généré
          ↓
5. [Coller dans settings.h]
          ↓
6. [Téléverser code MIDI principal]
          ↓
7. [Jouer ! 🎹]
```

---

## 📄 Licence

MIT License - Même licence que le projet principal

---

## 🔗 Liens Utiles

- [README principal](../README.md)
- [Guide Hardware](../HARDWARE.md)
- [settings.h](../Servo_melodica/settings.h)

---

**Version** : 1.0
**Date** : 2025-01-24
**Auteur** : Projet Servo-Melodica
