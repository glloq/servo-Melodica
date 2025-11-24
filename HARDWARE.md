# 🎤 Choix du Microphone pour Calibration Audio

## Objectif

Détecter l'**intensité sonore** (volume) produite par chaque touche du mélodica, pas l'analyse de fréquence. Le système doit simplement mesurer "quelle position de servo produit le son le plus fort".

---

## Microphone Recommandé

### ✅ Microphone Électret avec MAX4466 (RECOMMANDÉ)

**Module** : Adafruit MAX4466 ou MAX9814

**Caractéristiques :**
- Amplification réglable (25x à 125x)
- Sortie analogique 0-5V
- Filtre passe-bande intégré
- Faible bruit de fond

**Avantages :**
- ✅ Signal amplifié et propre
- ✅ Réglage de gain par potentiomètre
- ✅ Pas besoin de circuit externe
- ✅ Alimentation 3.3V ou 5V
- ✅ Prix : ~5-10€

**Câblage :**
```
MAX4466        Arduino
  VCC    →     5V
  GND    →     GND
  OUT    →     A0
```

**Configuration `settings.h` :**
```cpp
#define MIC_PIN A0
#define SOUND_THRESHOLD 100  // Ajuster selon le gain
#define MIC_SAMPLES 10
```

**Réglage du gain :**
- Tourner le potentiomètre pour que le signal soit entre 100-900 (sur 1023)
- Éviter la saturation (valeurs à 1023)

---

### ⚠️ Microphone Électret Simple (POSSIBLE MAIS PAS RECOMMANDÉ)

**Composant** : Capsule électret simple (ex: CMA-4544PF-W)

**Circuit requis :**
```
            +5V
             │
             ├──[R1: 2.2kΩ]──┬── OUT → A0
             │                │
          [Micro]          [C1: 10µF]
             │                │
            GND──────────────┴─ GND
```

**Avantages :**
- ✅ Prix très bas (~0.50€)
- ✅ Compact

**Inconvénients :**
- ❌ Signal faible (nécessite amplification)
- ❌ Sensible aux parasites
- ❌ Nécessite circuit externe
- ❌ Réglage difficile

---

### ❌ Microphones NON ADAPTÉS

**MEMS I2S/I2C** (ex: INMP441, ICS-43434)
- ❌ Interface numérique complexe
- ❌ Nécessite traitement FFT
- ❌ Surdimensionné pour l'application

**Microphones USB**
- ❌ Ne se connecte pas à Arduino
- ❌ Nécessite PC intermédiaire

**Microphones dynamiques** (XLR)
- ❌ Impédance inadaptée
- ❌ Nécessite préampli spécialisé
- ❌ Trop cher

---

## Positionnement du Microphone

### Position Optimale

```
        [Mélodica]
           │││││
      ┌────┴┴┴┴┴────┐
      │   Touches    │
      └──────────────┘
            │
         [Air out]
            ↓
         🎤 <── 5-10 cm
```

**Placement :**
- **5-10 cm** de la sortie d'air
- **Centré** par rapport au clavier
- **À l'abri** des vibrations mécaniques

**À éviter :**
- ❌ Trop près des servos (bruit mécanique)
- ❌ Trop près de l'Arduino (bruit électrique)
- ❌ Caché par le mélodica (son atténué)

---

## Configuration Logicielle

### Paramètres dans `settings.h`

```cpp
//------------------------------------------- Audio Calibration -------------------
// Bouton poussoir pour lancer la calibration automatique
#define CALIBRATION_BUTTON_PIN 2   // Pin digitale pour bouton (avec pull-up interne)

// Microphone pour détection audio
#define MIC_PIN A0                 // Pin analogique pour microphone
#define MIC_SAMPLES 10             // Nombre d'échantillons pour moyennage
#define SOUND_THRESHOLD 100        // Seuil de détection du son (0-1023)

// Paramètres de calibration automatique
#define CALIBRATION_TEST_ANGLES 9  // Nombre d'angles à tester
#define CALIBRATION_ANGLE_START 70 // Angle de départ pour calibration
#define CALIBRATION_ANGLE_STEP 5   // Pas entre chaque angle testé (70, 75, 80...)
#define CALIBRATION_DELAY_MS 300   // Délai entre chaque test d'angle
```

### Ajustement du Seuil

**Test rapide :**
```cpp
void setup() {
  Serial.begin(9600);
  pinMode(MIC_PIN, INPUT);
}

void loop() {
  int level = analogRead(MIC_PIN);
  Serial.println(level);
  delay(100);
}
```

**Analyse :**
- **Silence** : 10-50 → OK
- **Note jouée** : 200-800 → OK
- **Saturation** : 1023 → Réduire gain
- **Trop faible** : < 100 → Augmenter gain

**Ajuster `SOUND_THRESHOLD` :**
```cpp
// Si ambiant = 20, notes = 300
#define SOUND_THRESHOLD 100  // Entre les deux
```

---

## Bouton de Calibration

### Matériel

**Composant** : Bouton poussoir NO (Normally Open)

**Câblage :**
```
Arduino Pin 2 ──┬── [Bouton] ── GND
                │
         (Pull-up interne activé)
```

**Pas besoin de résistance** : Pull-up interne activée dans le code

### Utilisation

1. **Presser le bouton** sur Pin 2
2. **La calibration démarre** automatiquement
3. **Progression affichée** sur Serial Monitor
4. **Sauvegarde automatique** en EEPROM

**Durée** : ~5-10 minutes pour 32 servos

---

## Recommandations Finales

### Configuration Optimale

| Composant | Modèle | Prix |
|-----------|--------|------|
| Microphone | Adafruit MAX4466 | ~7€ |
| Bouton | Tactile 6x6mm | ~0.20€ |

**Total** : ~7.20€

### Alternative Budget

| Composant | Modèle | Prix |
|-----------|--------|------|
| Microphone | Électret CMA-4544 | ~0.50€ |
| Ampli | LM358 + Résistances | ~0.50€ |
| Bouton | Tactile 6x6mm | ~0.20€ |

**Total** : ~1.20€

---

## Dépannage Microphone

### Problème : "Ambient level too high"

**Cause :** Environnement trop bruyant ou gain trop élevé

**Solution :**
1. Réduire le gain du potentiomètre (MAX4466)
2. Augmenter `SOUND_THRESHOLD` dans settings.h
3. Calibrer dans un endroit plus silencieux

### Problème : "No significant sound detected"

**Cause :** Gain trop faible ou micro mal positionné

**Solution :**
1. Augmenter le gain du potentiomètre
2. Rapprocher le micro de la sortie d'air
3. Vérifier que le servo d'air s'ouvre pendant calibration
4. Réduire `SOUND_THRESHOLD`

### Problème : Résultats incohérents

**Cause :** Bruit mécanique des servos

**Solution :**
1. Isoler le micro des vibrations (mousse)
2. Augmenter `CALIBRATION_DELAY_MS` pour stabilisation
3. Augmenter `MIC_SAMPLES` pour plus de moyennage

---

**Date** : 2025-01-24
**Version** : 1.0
