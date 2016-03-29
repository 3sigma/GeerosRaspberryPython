/******************************************************************************
Firmware de l'Arduino des robots Raspberry Python, disponibles à l'adresse:
http://boutique.3sigma.fr/12-robots

Auteur: 3Sigma
Version 1.0 - 08/12/2015
*******************************************************************************/

// Inclusion d'une bibliothèque permettant l'exécution à cadence fixe
// d'une partie du programme. Télécharger à l'adresse http://www.3sigma.fr/telechargements/FlexiTimer2.zip
// et décompresser dans le sous-répertoire « libraries » de votre installation Arduino
// Pour plus de détails, voir les pages (en anglais): 
// http://www.arduino.cc/playground/Main/FlexiTimer2
// https://github.com/wimleers/flexitimer2
#include <FlexiTimer2.h>

// Inclusion d'une bibliothèque permettant de lire et d'écrire plus rapidement sur les entrées-sorties digitales.
// Télécharger à l'adresse http://www.3sigma.fr/telechargements/digitalWriteFast.zip
// et décompresser dans le sous-répertoire « libraries » de votre installation Arduino
#include <digitalWriteFast.h> 

// Inclusion de la bibliothèque permettant de contourner le bug de l'i2c sur
// le processeur Broadcomm BCM2835
// Voir https://github.com/pdg137/pi-a-star
// et http://www.advamation.com/knowhow/raspberrypi/rpi-i2c-bug.html
#include "RPiSlave.h"

RPiSlave slave;

#define Nmoy 10

// Définitions et déclarations pour le codeur incrémental du moteur droit
#define codeurDroitInterruptionA 0
#define codeurDroitPinA 2
#define codeurDroitPinB 8
volatile int32_t ticksCodeurDroit = 0;
int16_t indiceTicksCodeurDroit = 0;
int16_t ticksCodeurDroitTab[Nmoy];
int16_t codeurDroitDeltaPos;

// Définitions et déclarations pour le codeur incrémental du moteur gauche
#define codeurGaucheInterruptionA 1
#define codeurGauchePinA 3
#define codeurGauchePinB 9
volatile int32_t ticksCodeurGauche = 0;
int16_t indiceTicksCodeurGauche = 0;
int16_t ticksCodeurGaucheTab[Nmoy];
int16_t codeurGaucheDeltaPos;

// Définitions et déclarations pour le moteur à courant continu.
#define directionMoteurDroit  4
#define pwmMoteurDroit  5
#define directionMoteurGauche  7
#define pwmMoteurGauche  6

// ATTENTION: donner la bonne valeur de la tension d'alimentation sur la ligne ci-dessous. Ici, 7.4 V
double tensionAlim = 7.4;


// Certaines parties du programme sont exécutées à cadence fixe grâce à la bibliothèque FlexiTimer2.
// Cadence d'échantillonnage en ms
#define CADENCE_MS 10
volatile double dt = CADENCE_MS/1000.;

// Initialisations
void setup(void) {
  
  // Initialisation du bus I2C
  slave.init(20);
  
  // Codeur incrémental moteur droit
  pinMode(codeurDroitPinB, INPUT_PULLUP);      // entrée digitale pin B codeur
  // A chaque changement de niveau de tension sur le pin A du codeur,
  // on exécute la fonction GestionInterruptionCodeurDroitPinA (définie à la fin du programme)
  attachInterrupt(codeurDroitInterruptionA, GestionInterruptionCodeurDroitPinA, CHANGE);

  // Codeur incrémental moteur droit
  pinMode(codeurGauchePinB, INPUT_PULLUP);      // entrée digitale pin B codeur
  // A chaque changement de niveau de tension sur le pin A du codeur,
  // on exécute la fonction GestionInterruptionCodeurGauchePinA (définie à la fin du programme)
  attachInterrupt(codeurGaucheInterruptionA, GestionInterruptionCodeurGauchePinA, CHANGE);

  // Moteur à courant continu droit
  pinMode(directionMoteurDroit, OUTPUT);
  pinMode(pwmMoteurDroit, OUTPUT);

  // Moteur à courant continu gauche
  pinMode(directionMoteurGauche, OUTPUT);
  pinMode(pwmMoteurGauche, OUTPUT);
 
//  Serial.begin(115200);
//  while (!Serial) {
//    ; // Attente de la connexion du port série. Requis pour l'USB natif
//  }
  
  // Compteur d'impulsions des encodeurs
  ticksCodeurDroit = 0;
  ticksCodeurGauche = 0;
      
  /* Le programme principal est constitué:
     - de la traditionnelle boucle "loop" des programmes Arduino, dans laquelle on lit 
       en permanence la liaison série pour récupérer les nouvelles consignes de mouvement
     - de la fonction "isrt", dont l'exécution est définie à cadence fixe par les deux instructions
       ci-dessous. La bonne méthode pour exécuter une fonction à cadence fixe, c'est de l'exécuter sur interruption
       (la boucle "loop" est alors interrompue régulièrement, à la cadence voulue, par un timer).
       Une mauvaise méthode serait d'utiliser la fonction Arduino "delay". En effet, la fonction "delay" définit
       un délai entre la fin des instructions qui la précèdent et le début des instructions qui la suivent, mais si
       on ne connait pas le temps d'exécution de ces instructions (ou si celui-ci est inconnu), on n'obtiendra
       jamais une exécution à cadence fixe.
       Il est nécessaire d'exécuter la fonction "isrt" à cadence fixe car cette fonction:
       - doit renvoyer des données à cadence fixe sur la liaison série, pour monitoring
       - calcule l'asservissement de verticalité et de mouvement, conçu pour fonctionner à une cadence bien spécifiée
     
  */
  FlexiTimer2::set(CADENCE_MS, 1/1000., isrt); // résolution timer = 1 ms
  FlexiTimer2::start();

}

uint16_t getBatteryVoltage()
{
  // On lit la tension divisée par 3
  return 3 * (uint16_t)(((float)(5)) / 1024) * 1000 * analogRead(0);
}

uint16_t getAnalogValue(uint8_t analogPin) {
  return analogRead(analogPin);
}

void setMoteurDroit(int16_t tension_int) {
  // Cette fonction calcule et envoi les signaux PWM au pont en H

  // Commande PWM
  if (tension_int>=0) {
    digitalWrite(directionMoteurDroit, HIGH);
    analogWrite(pwmMoteurDroit, tension_int);
  }
  if (tension_int<0) {
    digitalWrite(directionMoteurDroit, LOW);
    analogWrite(pwmMoteurDroit, -tension_int);
  }
}

void setMoteurGauche(int16_t tension_int) {
  // Cette fonction calcule et envoi les signaux PWM au pont en H
  
  // Commande PWM
  if (tension_int>=0) {
    digitalWrite(directionMoteurGauche, HIGH);
    analogWrite(pwmMoteurGauche, tension_int);
  }
  if (tension_int<0) {
    digitalWrite(directionMoteurGauche, LOW);
    analogWrite(pwmMoteurGauche, -tension_int);
  }
}

void setMotors(int16_t left, int16_t right) {
  setMoteurDroit(right);
  setMoteurGauche(left);
}

int16_t getCodeurDroitDeltaPos() {
  return codeurDroitDeltaPos;
}

int16_t getCodeurGaucheDeltaPos() {
  return codeurGaucheDeltaPos;
}

int32_t getCodeursDeltaPos() {
  return ((int32_t)(codeurGaucheDeltaPos) << 16) | (int32_t)codeurDroitDeltaPos ;
}

void checkCommands() {
  slave.checkCommand(1, getBatteryVoltage);
  slave.checkCommand(2, setMoteurGauche);
  slave.checkCommand(3, setMoteurDroit);
  slave.checkCommand(5, getAnalogValue);
  slave.checkCommand(6, getCodeurDroitDeltaPos);
  slave.checkCommand(7, getCodeurGaucheDeltaPos);
  slave.checkCommand(8, setMotors);
  slave.checkCommand(9, getCodeursDeltaPos);
}


// Boucle principale
void loop() {
  if(slave.commandReady()) {
    checkCommands();
    slave.commandDone();
  }

}

// Fonction excutée sur interruption
void isrt() {

  // Compteur de boucle et variable d'activation de l'asservissement
  int i;

  // Mesure de la vitesse des moteurs grâce aux codeurs incrémentaux
  
  // On calcule une moyenne glissante de la vitesse sur les Nmoy derniers échantillons
  // Nombre de ticks codeur depuis la dernière fois accumulé pour les 1à derniers échantillons
  // Ce nombre est mis à jour par les fonctions GestionInterruptionCodeurDroitPinA et GestionInterruptionCodeurDroitPinA,
  // exécutées à chaque interruption due à une impulsion sur la voie A ou B du codeur incrémental
  for (int i=0; i<Nmoy; i++) {
    ticksCodeurDroitTab[i] += ticksCodeurDroit;
  }  
  // Une fois lu, ce nombre est remis à 0 pour pouvoir l'incrémenter de nouveau sans risquer de débordement de variable
  ticksCodeurDroit = 0;
  
  // Pour l'échantillon courant, calcule de l'angle de rotation du moteur pendant la période d'échantillonnage
  codeurDroitDeltaPos = ticksCodeurDroitTab[indiceTicksCodeurDroit];
  // Remise à zéro du compteur d'impulsion codeur pour l'échantillon courant
  ticksCodeurDroitTab[indiceTicksCodeurDroit] = 0;
  
  // Mise à jour de l'indice d'échantillon courant
  indiceTicksCodeurDroit++;
  if (indiceTicksCodeurDroit==Nmoy) {
    indiceTicksCodeurDroit = 0;
  }
  
  // On calcule une moyenne glissante de la vitesse sur les Nmoy derniers échantillons
  // Nombre de ticks codeur depuis la dernière fois accumulé pour les 1à derniers échantillons
  // Ce nombre est mis à jour par les fonctions GestionInterruptionCodeurGauchePinA et GestionInterruptionCodeurGauchePinA,
  // exécutées à chaque interruption due à une impulsion sur la voie A ou B du codeur incrémental
  for (int i=0; i<Nmoy; i++) {
    ticksCodeurGaucheTab[i] += ticksCodeurGauche;
  }  
  // Une fois lu, ce nombre est remis à 0 pour pouvoir l'incrémenter de nouveau sans risquer de débordement de variable
  ticksCodeurGauche = 0;
  
  // Pour l'échantillon courant, calcule de l'angle de rotation du moteur pendant la période d'échantillonnage
  codeurGaucheDeltaPos = ticksCodeurGaucheTab[indiceTicksCodeurGauche];
  // Remise à zéro du compteur d'impulsion codeur pour l'échantillon courant
  ticksCodeurGaucheTab[indiceTicksCodeurGauche] = 0;
  
  // Mise à jour de l'indice d'échantillon courant
  indiceTicksCodeurGauche++;
  if (indiceTicksCodeurGauche==Nmoy) {
    indiceTicksCodeurGauche = 0;
  }

}
 
void GestionInterruptionCodeurDroitPinA() {  
  // Routine de service d'interruption attachée à la voie A du codeur incrémental droit
  // On utilise la fonction digitalReadFast2 de la librairie digitalWriteFast
  // car les lectures doivent être très rapides pour passer le moins de temps possible
  // dans cette fonction (appelée de nombreuses fois, à chaque impulsion de codeur)
  if (digitalReadFast2(codeurDroitPinA) == digitalReadFast2(codeurDroitPinB)) {
    ticksCodeurDroit++;
  }
  else {
    ticksCodeurDroit--;
  }
}


void GestionInterruptionCodeurGauchePinA() {  
  // Routine de service d'interruption attachée à la voie A du codeur incrémental gauche
  // On utilise la fonction digitalReadFast2 de la librairie digitalWriteFast
  // car les lectures doivent être très rapides pour passer le moins de temps possible
  // dans cette fonction (appelée de nombreuses fois, à chaque impulsion de codeur)
  if (digitalReadFast2(codeurGauchePinA) == digitalReadFast2(codeurGauchePinB)) {
    ticksCodeurGauche++;
  }
  else {
    ticksCodeurGauche--;
  }
}


