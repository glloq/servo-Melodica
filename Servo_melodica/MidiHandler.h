#ifndef MIDIHANDLER_H
#define MIDIHANDLER_H

#include <MIDIUSB.h>
#include "instrument.h"
/***********************************************************************************************
----------------------------    MIDI message handler    ----------------------------------------
************************************************************************************************
Recoit les messages midi et les envoit a l'instrument via les methodes .noteOn, .noteOff, ect ...
l'objectif est d'etre le plus complet au niveau de la selection des messages qui peuvent etre envoyé via MIDI
-messages de note On 
-message de note Off
-Message de Pitch Bend : Utilisé pour moduler la hauteur tonale d'une note en temps réel. Il est souvent généré par une molette de pitch sur un contrôleur MIDI.
-Message de Channel Pressure (Aftertouch) : Indique la pression appliquée à une seule note après qu'elle a été enfoncée. Cela peut être utilisé pour moduler divers paramètres.
-Message de Polyphonic Key Pressure : Similaire au message de Channel Pressure, mais il permet une pression individuelle sur chaque note d'un clavier polyphonique.
-Message de System Common : Comprend des messages qui affectent l'ensemble du système MIDI, tels que les messages de démarrage, d'arrêt et d'horloge.
-Message de System Real-Time : Comprend des messages qui sont transmis en temps réel pour synchroniser les appareils MIDI, tels que les messages de synchronisation d'horloge.
-Message de System Exclusive (SysEx) : Utilisé pour transmettre des données spécifiques au fabricant et aux modèles d'équipements MIDI. Ces messages peuvent être très variés et personnalisés.
------------------------------------------------------------------------------------------------
Chaque fonction qui peut etre utilisé doit etre decommenté et déclaré dans instrument.h 
************************************************************************************************/

class MidiHandler {
  private:
    Instrument& _instrument;
    void processMidiEvent(midiEventPacket_t midiEvent);
    void processControlChange( byte controller, byte value);
  public:
    MidiHandler(Instrument &instrument);
    void readMidi();
};

#endif // MIDIHANDLER_H
