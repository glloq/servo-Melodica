# 🛠️ Outil de Calibration Manuelle - Servo Melodica

Outil Arduino pour calibrer manuellement chaque servo **sans microphone** via **Serial Monitor**.

---

## 📋 Description

Cet outil permet de :
- ✅ Régler l'angle initial de chaque servo (position repos)
- ✅ Définir le sens de rotation (+1 ou -1)
- ✅ Tester chaque servo (noteOn / noteOff)
- ✅ Générer automatiquement le code pour `settings.h`

**Interface** : Commandes clavier via Serial Monitor (pas de boutons physiques requis)

---

## 🔧 Matériel Requis

| Composant | Quantité |
|-----------|----------|
| Arduino Mega/Leonardo | 1 |
| PCA9685 | 2 |
| Servos SG90 | 30 |
| Câble USB | 1 |

**Aucun bouton physique requis** - Tout se fait via Serial Monitor !

---

## 📖 Mode d'Emploi

### 1. Téléversement

1. Ouvrir `Calibration_Tool.ino` dans Arduino IDE
2. Sélectionner la carte (Mega/Leonardo)
3. Téléverser

### 2. Ouvrir Serial Monitor

1. Ouvrir le Serial Monitor (Ctrl+Shift+M)
2. Configurer à **9600 bauds**
3. L'interface de calibration s'affiche

### 3. Commandes Clavier

```
╔══════════════════════════════════════════════════════════════╗
║                    COMMANDES DISPONIBLES                     ║
╠══════════════════════════════════════════════════════════════╣
║  p     → Servo précédent                                     ║
║  n     → Servo suivant                                       ║
║  -     → Diminuer angle de 1°                                ║
║  +     → Augmenter angle de 1°                               ║
║  [     → Diminuer angle de 5°                                ║
║  ]     → Augmenter angle de 5°                               ║
║  i     → Inverser sens de rotation                           ║
║  t     → Tester noteOn/noteOff                               ║
║  c     → Générer code pour settings.h                        ║
║  h     → Afficher cette aide                                 ║
╚══════════════════════════════════════════════════════════════╝
```

**Astuce** : Taper la lettre dans le champ en haut du Serial Monitor et appuyer sur Entrée

### 4. Calibration Étape par Étape

Pour **chaque servo** (0 à 29) :

#### Étape A : Positionner en position REPOS (touche relâchée)

1. Observer le servo et la touche
2. Envoyer **+** ou **-** pour ajuster l'angle (1° à la fois)
3. Utiliser **[** ou **]** pour ajustement rapide (5° à la fois)
4. L'angle doit correspondre à la **position juste avant d'appuyer**
5. Vérifier que le servo ne touche PAS la touche

#### Étape B : Tester l'appui

1. Envoyer la commande **t** (Test)
2. Observer le mouvement :
   - **noteOn** : Le servo appuie sur la touche (1 seconde)
   - **noteOff** : Le servo revient en position repos

#### Étape C : Corriger le sens si nécessaire

Si le servo tourne **dans le mauvais sens** :
1. Envoyer **i** (Invert)
2. Refaire le test avec **t**

#### Étape D : Passer au suivant

1. Envoyer **n** (Next)
2. Répéter pour tous les servos

**Navigation** : Utiliser **p** (Previous) pour revenir au servo précédent si nécessaire

### 5. Générer le Code

Une fois **tous les servos calibrés** :

1. Envoyer la commande **c** (Code)
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

Envoyer : t → Le servo recule au lieu d'avancer !

Solution :
  Envoyer : i → Sens : -1 (anti-horaire)
  Envoyer : t → OK ! Le servo appuie correctement
```

### Exemple 2 : Position repos mal réglée

```
État initial :
  Angle : 90°
  Le servo touche déjà la touche !

Solution :
  Envoyer : + → 91°
  Envoyer : + → 92°
  Envoyer : + → 93° → OK ! Plus de contact
  Envoyer : t → Vérifier que l'appui fonctionne
```

### Exemple 3 : Ajustement rapide

```
Situation : Le servo est à 120° mais devrait être vers 85°

Solution rapide :
  Envoyer : [ → 115° (diminution de 5°)
  Envoyer : [ → 110°
  Envoyer : [ → 105°
  Envoyer : [ → 100°
  Envoyer : [ → 95°
  Envoyer : [ → 90°
  Envoyer : - → 89° (ajustement fin de 1°)
  Envoyer : - → 88°
  Envoyer : - → 87°
  Envoyer : - → 86°
  Envoyer : - → 85° → OK !
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

### Récapitulatif des Commandes

**Navigation entre servos** :
- `p` : Servo précédent
- `n` : Servo suivant

**Ajustement angle** :
- `+` ou `=` : Augmenter de 1°
- `-` : Diminuer de 1°
- `]` : Augmenter de 5° (ajustement rapide)
- `[` : Diminuer de 5° (ajustement rapide)

**Configuration** :
- `i` : Inverser le sens de rotation

**Actions** :
- `t` : Tester noteOn/noteOff
- `c` : Générer le code pour settings.h
- `h` ou `?` : Afficher l'aide

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

### Problème : Serial Monitor ne répond pas aux commandes

**Causes** :
- ❌ Mauvaise vitesse (bauds) configurée
- ❌ Caractères non reconnus (encodage)

**Solution** :
1. Vérifier que Serial Monitor est configuré à **9600 bauds**
2. S'assurer d'envoyer les caractères en minuscule (p, n, i, t, c, h)
3. Vérifier que "No line ending" ou "Newline" est sélectionné

---

## 📝 Modification du Code

### Changer l'angle de course

```cpp
// Ligne 39
#define ANGLE_NOTE_ON 20  // Changer si appui trop faible/fort
```

### Ajouter un servo supplémentaire

```cpp
// Ligne 38
#define NUMBER_OF_NOTES 32  // Si vous avez 32 servos au lieu de 30
```

### Modifier les limites d'angle

```cpp
// Lignes 47-48
#define SERVO_MIN_ANGLE 0    // Angle minimal autorisé
#define SERVO_MAX_ANGLE 180  // Angle maximal autorisé
```

---

## 🚀 Workflow Complet

```
1. [Téléverser Calibration_Tool.ino]
          ↓
2. [Ouvrir Serial Monitor 9600 bauds]
          ↓
3. [Pour chaque servo 0-29:]
   → Ajuster angle repos (+ / - / [ / ])
   → Tester (t)
   → Inverser si nécessaire (i)
   → Passer au suivant (n)
          ↓
4. [Envoyer : c] → Copier code généré
          ↓
5. [Coller dans settings.h]
          ↓
6. [Téléverser code MIDI principal]
          ↓
7. [Jouer ! 🎹]
```

**Durée estimée** : 15-30 minutes pour calibrer 30 servos

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
