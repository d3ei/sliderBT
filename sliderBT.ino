/*
 * Slider pour caméra
 * 10-2020
 * J
 */

/*
 * moteur pas à pas
 */
#include <AccelStepper.h>

/*
 * Serie
 */
#include<SoftwareSerial.h>


/*****************************************************
 * Parametres
 */

/*
 * Nombre de pas du moteur pour 1 tour
 * A adapter en fonction de votre moteur
 * et modifié suite à des mesures sur les moteurs
 */
const int motorStepsPerRevolution = 2048;

/*
 * Vitesse max du moteur
 * Avec les deux 28BYJ-48 et accelStepper je ne dépasse pas 600
 */
const int motorSpeedMax = 600;

/*
 * Acceleration du moteur lors de mouvements de placement ou de retour à la maison
 */
const int motorAcceleration = 300;

/*
 * Translation ou rotation max du moteur en pas lors d'un appui sur les bouton translation ou rotation
 */
const int motorMaxStepbsOneClick = 10000;

/*
 * Initialisation du moteur pour la translation
 * Attention à l'ordre des pins
 * Sur mon arduino Nano la liaison avec le driver UKN2003 est la suivante : 
 *  IN1 => Arduino D2
 *  IN2 => Arduino D3
 *  IN3 => Arduino D4
 *  IN4 => Arduino D5
 * Et l'initialisation du moteur comme ci-dessous :
 * Stepper myStepperTranslation(stepsPerRevolution, IN2, IN4, IN3, IN1);
 */
AccelStepper myStepperTranslation(AccelStepper::FULL4WIRE, 3, 5, 4, 2);


/*
 * Initialisation du moteur pour la rotation
 * Attention à l'ordre des pins
 * Sur mon arduino Nano la liaison avec le driver UKN2003 est la suivante : 
 *  IN1 => Arduino D8
 *  IN2 => Arduino D9
 *  IN3 => Arduino D10
 *  IN4 => Arduino D11
 * Et l'initialisation du moteur comme ci-dessous :
 * Stepper myStepper(stepsPerRevolution, IN2, IN4, IN3, IN1);
 */
AccelStepper myStepperRotation(AccelStepper::FULL4WIRE, 9, 11, 10, 8);

/*
 * Broches du module bluetooth HC-05
 */
#define TxD 7
#define RxD 6

SoftwareSerial bluetoothSerial(TxD, RxD);

/*
 * Fin Parametres
 *****************************************************/

#define CMD_TR_LEFT       'a'
#define CMD_TR_RIGHT      'b'
#define CMD_TR_INVERT     'e'
#define CMD_TR_NORMAL     'f'
#define CMD_ROT_LEFT      'c'
#define CMD_ROT_RIGHT     'd'
#define CMD_ROT_INVERT    'g'
#define CMD_ROT_NORMAL    'h'
#define CMD_STOP          's'
#define CMD_TR_POS_START  'p'
#define CMD_TR_POS_END    'q'
#define CMD_ROT_POS_START 't'
#define CMD_ROT_POS_END   'u'
#define CMD_BACK_HOME     'm'
#define CMD_TEST          'v'
#define CMD_GO            'z'


String txtReceived;
String txtCommand;
char   txtToPhone;
bool   trInvert = false;
bool   rotInvert = false;
bool   backHome = false;
bool   backHomeTransOk = false;
bool   backHomeRotOk = false;
bool   cmdTest = false;
bool   cmdTestTransOk = false;
bool   cmdTestRotOk = false;
bool   cmdGo = false;
bool   cmdGoTransOk = false;
bool   cmdGoRotOk = false;
long   intTransPosStart = 0;
long   intTransPosEnd = 0;
long   intTransNbStep = 0;
long   intRotPosStart = 0;
long   intRotPosEnd = 0;
long   intRotNbStep = 0;
unsigned long intValue;
unsigned long intTransDurationSecond = 0;
unsigned long intRotDurationSecond = 0;

float speedRotation = 0;
float speedTranslation = 0;

void setup() {
  /*
   * série bluetooth
   */
  bluetoothSerial.begin(9600);

  /*
   *¨Position à 0 et 
   * Moteurs off
   */
  myStepperRotation.setCurrentPosition(0);
  myStepperTranslation.setCurrentPosition(0);
  myStepperRotation.disableOutputs();
  myStepperTranslation.disableOutputs();
  
  Serial.begin(9600);
  Serial.println("Prêt");
}

void loop() {
  /*
   * Mouvements moteur(s)
   */
  
  /*
   * Translation
   */
  if (myStepperTranslation.distanceToGo() == 0) {
    if (backHome) {
      backHomeTransOk = true;
    } else if (cmdTest) {
      cmdTestTransOk = true;
    } else if (cmdGo) {
      cmdGoTransOk = true;
    }
  } else {
    myStepperTranslation.run();
  }
  
  /*
   * Rotation
   */
  if (myStepperRotation.distanceToGo() == 0) {
    if (backHome) {
      backHomeRotOk = true;
    } else if (cmdTest) {
      cmdTestRotOk = true;
    } else if (cmdGo) {
      cmdGoRotOk = true;
    }
  } else {
    myStepperRotation.run();
  }

  if (backHomeTransOk and backHomeRotOk) {
    /*
     * Retour à la position initiale Ok
     */
    backHome = false;
    backHomeTransOk = false;
    backHomeRotOk = false;
    myStepperTranslation.disableOutputs();
    myStepperRotation.disableOutputs();
    if (cmdTest == false and cmdGo == false) {
      /*
       * Pas de test ni de mouvement demandé
       * acquittement retour à la maison
       */
      Serial.println("Retour maison effectué");
      bluetoothSerial.print("mOk");
      bluetoothSerial.write(1);
    } else if (cmdTest) {
      /*
       * Test demandé
       * il se déclenche une fois le retour à la maison effectué
       */
      
      /*
       * Attente avant le début du Test
       */
      delay(4000);

      /*
       * Go Test
       */
      calculStepsVitesse();
      
      if (speedTranslation > motorSpeedMax) {
        Serial.println("Vitesse de Translation trop élevée.");
        bluetoothSerial.print("zTv");
        bluetoothSerial.write(1);
      } else if (speedRotation > motorSpeedMax) {
        Serial.println("Vitesse de Rotation trop élevée.");
        bluetoothSerial.print("zRv");
        bluetoothSerial.write(1);
      } else {
        /*
         * Calcul vitesse la plus rapide pour le test
         * augmentation de 1% tant que la vitesse maximale n'est pas atteinte
         */
        while (speedTranslation < motorSpeedMax and speedRotation < motorSpeedMax) {
          speedTranslation = float(speedTranslation) * 1.01;
          speedRotation = float(speedRotation) * 1.01;
        }
        Serial.println("Test Translation : " + String(intTransNbStep) + " pas - de : " + String(intTransPosStart) + " à " + String(intTransPosEnd) + " - vitesse : " + String(speedTranslation));
        Serial.println("Test Rotation : " + String(intRotNbStep) + " pas - de : " + String(intRotPosStart) + " à " + String(intRotPosEnd) + " - vitesse : " + String(speedRotation));
        /*
         * Translation
         */
        myStepperTranslation.enableOutputs();
        myStepperTranslation.setAcceleration(20000);
        myStepperTranslation.setMaxSpeed(speedTranslation);
        myStepperTranslation.moveTo(intTransPosEnd);
        myStepperTranslation.setSpeed(speedTranslation);

        /*
         * Rotation
         */
        myStepperRotation.enableOutputs();
        myStepperTranslation.setAcceleration(20000);
        myStepperRotation.setMaxSpeed(speedRotation);
        myStepperRotation.moveTo(intRotPosEnd);
        myStepperRotation.setSpeed(speedRotation);
      }
    } else if (cmdGo) {
      /*
       * Attente avant le début du Mouvement
       */
      delay(4000);

      /*
       * Go mouvement
       */
      
      calculStepsVitesse();

      if (speedTranslation > motorSpeedMax) {
        Serial.println("Vitesse de Translation trop élevée.");
        bluetoothSerial.print("zTv");
        bluetoothSerial.write(1);
      } else if (speedRotation > motorSpeedMax) {
        Serial.println("Vitesse de Rotation trop élevée.");
        bluetoothSerial.print("zRv");
        bluetoothSerial.write(1);
      } else {
        /*
         * Translation
         */
        myStepperTranslation.enableOutputs();
        myStepperTranslation.setAcceleration(2000);
        myStepperTranslation.setMaxSpeed(speedTranslation);
        myStepperTranslation.moveTo(intTransPosEnd);
        myStepperTranslation.setSpeed(speedTranslation);

        /*
         * Rotation
         */
        myStepperRotation.enableOutputs();
        myStepperTranslation.setAcceleration(2000);
        myStepperRotation.setMaxSpeed(speedRotation);
        myStepperRotation.moveTo(intRotPosEnd);
        myStepperRotation.setSpeed(speedRotation);
      }
    }
  }
  if (cmdTestTransOk and cmdTestRotOk) {
    cmdTest = false;
    cmdTestTransOk = false;
    cmdTestRotOk = false;
    myStepperTranslation.disableOutputs();
    myStepperRotation.disableOutputs();
    Serial.println("Test effectué");
    bluetoothSerial.print("vOk");
    bluetoothSerial.write(1);
  }
  if (cmdGoTransOk and cmdGoRotOk) {
    cmdGo = false;
    cmdGoTransOk = false;
    cmdGoRotOk = false;
    myStepperTranslation.disableOutputs();
    myStepperRotation.disableOutputs();
    Serial.println("Mouvement effectué");
    bluetoothSerial.print("zOk");
    bluetoothSerial.write(1);
  }

  if(bluetoothSerial.available()){
    txtReceived = bluetoothSerial.readString();
    Serial.println("txtReceived : " + txtReceived);
    if (txtReceived.indexOf("!") != -1) {
      /*
       * Commande sur 2 caractères + Nombre
       * tS102 = position de départ translation à 102
       */
      txtCommand = txtReceived.substring(0, 2);
      intValue = txtReceived.substring(2).toInt();
      if (txtCommand == "tD") {
        /*
         * Durée translation
         */
        intTransDurationSecond = intValue;
        Serial.println("Duree translation : " + String(intValue));
        bluetoothSerial.print("tDOk");
        bluetoothSerial.write(1);
      } else if (txtCommand == "rD") {
        /*
         * Durée rotation
         */
        intRotDurationSecond = intValue;
        Serial.println("Duree rotation : " + String(intValue));
        bluetoothSerial.print("rDOk");
        bluetoothSerial.write(1);
      } else {
        Serial.println("Commande inconnue");
        bluetoothSerial.print("i");
        bluetoothSerial.write(1);
      }
    } else {
      /*
       * Texte : liste de commandes sur 1 caractère
       */
      for (byte i = 0 ; i < txtReceived.length() ; i ++) {
        txtToPhone = txtReceived[i];
        switch (txtToPhone) {
          case CMD_TR_LEFT:
            Serial.println("Translation gauche depuis " + String(myStepperTranslation.currentPosition()));
            myStepperTranslation.enableOutputs();
            myStepperTranslation.setMaxSpeed(motorSpeedMax);
            myStepperTranslation.setAcceleration(motorAcceleration);
            if (trInvert) {
              myStepperTranslation.moveTo(abs(myStepperTranslation.currentPosition()) + motorMaxStepbsOneClick);
            } else {
              myStepperTranslation.moveTo(-(abs(myStepperTranslation.currentPosition()) + motorMaxStepbsOneClick));
            }
            bluetoothSerial.print(txtToPhone);
            bluetoothSerial.write(1);
            break;
          case CMD_TR_RIGHT:
            Serial.println("Translation droite depuis " + String(myStepperTranslation.currentPosition()));
            myStepperTranslation.enableOutputs();
            myStepperTranslation.setMaxSpeed(motorSpeedMax);
            myStepperTranslation.setAcceleration(motorAcceleration);
            if (trInvert) {
              myStepperTranslation.moveTo(-(abs(myStepperTranslation.currentPosition()) + motorMaxStepbsOneClick));
            } else {
              myStepperTranslation.moveTo(abs(myStepperTranslation.currentPosition()) + motorMaxStepbsOneClick);
            }
            bluetoothSerial.print(txtToPhone);
            bluetoothSerial.write(1);
            break;
          case CMD_ROT_LEFT:
            Serial.println("Rotation anti-horaire depuis " + String(myStepperRotation.currentPosition()));
            myStepperRotation.enableOutputs();
            myStepperRotation.setMaxSpeed(motorSpeedMax);
            myStepperRotation.setAcceleration(motorAcceleration);
            if (rotInvert) {
              myStepperRotation.moveTo(abs(myStepperRotation.currentPosition()) + motorMaxStepbsOneClick);
            } else {
              myStepperRotation.moveTo(-(abs(myStepperRotation.currentPosition()) + motorMaxStepbsOneClick));
            }
            bluetoothSerial.print(txtToPhone);
            bluetoothSerial.write(1);
            break;
          case CMD_ROT_RIGHT:
            Serial.println("Rotation horaire depuis " + String(myStepperRotation.currentPosition()));
            myStepperRotation.enableOutputs();
            myStepperRotation.setMaxSpeed(motorSpeedMax);
            myStepperRotation.setAcceleration(motorAcceleration);
            if (rotInvert) {
              myStepperRotation.moveTo(-(abs(myStepperRotation.currentPosition()) + motorMaxStepbsOneClick));
            } else {
              myStepperRotation.moveTo(abs(myStepperRotation.currentPosition()) + motorMaxStepbsOneClick);
            }
            bluetoothSerial.print(txtToPhone);
            bluetoothSerial.write(1);
            break;
          /*
           * Retour Maison
           */
          case CMD_BACK_HOME:
            Serial.println("Retour maison rotation : " + String(intRotPosStart) + " - Translation : " + String(intTransPosStart));
            bluetoothSerial.print(txtToPhone);
            bluetoothSerial.write(1);
            backHome = true;
            myStepperRotation.enableOutputs();
            myStepperRotation.setMaxSpeed(motorSpeedMax);
            myStepperRotation.setAcceleration(motorAcceleration);
            myStepperTranslation.enableOutputs();
            myStepperTranslation.setMaxSpeed(motorSpeedMax);
            myStepperTranslation.setAcceleration(motorAcceleration);
            
            myStepperRotation.moveTo(intRotPosStart);
            myStepperTranslation.moveTo(intTransPosStart);

            break;
          /*
           * Test le chemin
           */
          case CMD_TEST:
            Serial.println("Test lancé");
            bluetoothSerial.print(txtToPhone);
            bluetoothSerial.write(1);
            backHome = true;
            cmdTest = true;
            /*
             * Retour à la maison si besoin
             */
            myStepperRotation.enableOutputs();
            myStepperRotation.setMaxSpeed(motorSpeedMax);
            myStepperRotation.setAcceleration(motorAcceleration);
            myStepperTranslation.enableOutputs();
            myStepperTranslation.setMaxSpeed(motorSpeedMax);
            myStepperTranslation.setAcceleration(motorAcceleration);
            
            myStepperRotation.moveTo(intRotPosStart);
            myStepperTranslation.moveTo(intTransPosStart);

            break;
          /*
           * Mouvement !!
           */
          case CMD_GO:
            Serial.println("Mouvement lancé");
            bluetoothSerial.print(txtToPhone);
            bluetoothSerial.write(1);
            backHome = true;
            cmdGo = true;
            /*
             * Retour à la maison si besoin
             */
            myStepperRotation.enableOutputs();
            myStepperRotation.setMaxSpeed(motorSpeedMax);
            myStepperRotation.setAcceleration(motorAcceleration);
            myStepperTranslation.enableOutputs();
            myStepperTranslation.setMaxSpeed(motorSpeedMax);
            myStepperTranslation.setAcceleration(motorAcceleration);
            
            myStepperRotation.moveTo(intRotPosStart);
            myStepperTranslation.moveTo(intTransPosStart);

            break;
          case CMD_STOP:
            Serial.println("Stop position : " + String(myStepperTranslation.currentPosition()));
            myStepperTranslation.setMaxSpeed(0);
            myStepperTranslation.stop();
            myStepperTranslation.disableOutputs();
            myStepperRotation.setMaxSpeed(0);
            myStepperRotation.stop();
            myStepperRotation.disableOutputs();
            bluetoothSerial.print(txtToPhone);
            bluetoothSerial.write(1);
            break;
          case CMD_TR_INVERT:
            trInvert = true;
            bluetoothSerial.print(txtToPhone);
            bluetoothSerial.write(1);
            break;
          case CMD_TR_NORMAL:
            trInvert = false;
            bluetoothSerial.print(txtToPhone);
            bluetoothSerial.write(1);
            break;
          case CMD_ROT_INVERT:
            rotInvert = true;
            bluetoothSerial.print(txtToPhone);
            bluetoothSerial.write(1);
            break;
          case CMD_ROT_NORMAL:
            rotInvert = false;
            bluetoothSerial.print(txtToPhone);
            bluetoothSerial.write(1);
            break;
          case CMD_TR_POS_START:
            intTransPosStart = myStepperTranslation.currentPosition();
            Serial.println("Start TR position : " + String(intTransPosStart));
            bluetoothSerial.print("pOk");
            bluetoothSerial.write(1);
            break;
          case CMD_TR_POS_END:
            intTransPosEnd = myStepperTranslation.currentPosition();
            Serial.println("End TR position : " + String(intTransPosEnd));
            bluetoothSerial.print("qOk");
            bluetoothSerial.write(1);
            break;
          case CMD_ROT_POS_START:
            intRotPosStart = myStepperRotation.currentPosition();
            Serial.println("Start ROT position : " + String(intRotPosStart));
            bluetoothSerial.print("tOk");
            bluetoothSerial.write(1);
            break;
          case CMD_ROT_POS_END:
            intRotPosEnd = myStepperRotation.currentPosition();
            Serial.println("End ROT position : " + String(intRotPosEnd));
            bluetoothSerial.print("uOk");
            bluetoothSerial.write(1);
            break;
          default:
            Serial.println("Commande inconnue");
            bluetoothSerial.print("i");
            bluetoothSerial.write(1);
            break;
        }
      }
    }
  }
}

/*
 * Calcul le nombre de pas et la vitesse
 */
void calculStepsVitesse(void) {
  intTransNbStep = max(intTransPosStart, intTransPosEnd) - min(intTransPosStart, intTransPosEnd);
  if (intTransDurationSecond > 0) {
    speedTranslation = (float)intTransNbStep / (float)intTransDurationSecond;
  } else {
    speedTranslation = 0;
  }
  Serial.println("Translation : " + String(intTransNbStep) + " pas - durée : " + String(intTransDurationSecond) + " - vitesse : " + String(speedTranslation));
  intRotNbStep = max(intRotPosStart, intRotPosEnd) - min(intRotPosStart, intRotPosEnd);
  if (intRotDurationSecond > 0) {
    speedRotation = (float)intRotNbStep / (float)intRotDurationSecond;
  } else {
    speedRotation = 0;
  }
  Serial.println("Rotation : " + String(intRotNbStep) + " pas - durée : " + String(intRotDurationSecond) + " - vitesse : " + String(speedRotation));
}
