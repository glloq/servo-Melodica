/***********************************************************************************************
----------------------------    MIDI servo lyre  16 notes   ------------------------------------
************************************************************************************************
Systeme construit pour le controle d'un melodica de 32 notes avec des servomoteur de type sg90 et deux cartes pac9685
les systeme recoit les messages midi via le cable usb,
- midiHandler s'occupe de dechiffrer les messages midi
- instrument s'occupe de verifier si il peut jouer les notes recues et demande ServoController de les jouer si c'est possible
- AirManager gere l'ouverture des vavles en fonction de la velocité
-------------------------------------------------------------------------------------------------
tout les parametres doivent etre mis dan settings.h afin de simplifier les adaptations au materiel 
Un autre fichier .ino sera fournit afin d'initialiser les servo a 90° lors du montage et aussi 
pour trouver le reglage de la position centrale du servo 
-------------------------------------------------------------------------------------------------
il va peut etre falloir ajouter un systeme pour attendre le deplacement des servomomoteur avant d'ouvrir l'air
=> ajouer un systeme de buffer fifo pour stocker les nouvelle noteOn ou noteOff 

************************************************************************************************/
#include <MIDIUSB.h>
#include "Instrument.h"
#include "MidiHandler.h"
#include "Arduino.h"

Instrument* instrument= nullptr;
MidiHandler* midiHandler= nullptr;

void setup() {
  Serial.begin(115200);
 // while (!Serial) {
  //  delay(10); // Attendre que la connexion série soit établie
  //}
  Serial.println("init");
  instrument= new Instrument();
  midiHandler = new MidiHandler(*instrument);
  Serial.println("fin init");
}

void loop() {
  midiHandler->readMidi();
  instrument->update();
}
