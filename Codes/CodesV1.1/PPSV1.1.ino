/*<--------------------------------------------------------------------------------------->*/
/*
    Code de test pour envoyer un compteur de données via LoRaWAN.
    Ce programme est conçu pour démontrer l'envoi de données à travers un réseau LoRaWAN à l'aide du module RA-07H.
    Le compteur est envoyé périodiquement et la transmission est répétée en cas d'échec, dans la limite des tentatives prédéfinies.
*/
/*<--------------------------------------------------------------------------------------->*/

#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "ATcommands.h"
//#include <SoftwareSerial.h>

#define CapteurO2 A0
#define ONE_WIRE_BUS 4

OneWire oneWire(ONE_WIRE_BUS); // objet pour communication 1 wire 
DallasTemperature sensors(&oneWire); // passer l'objet à DallasTemperature

// Pins utilisés pour contrôler le module RA-07H
int PWR_KEY = 9;          // Pin pour activer/désactiver l'alimentation du module
int RST_KEY = 6;          // Pin pour réinitialiser le module
int LOW_PWR_KEY = 5;      // Pin pour activer/désactiver le mode basse consommation

// Variables pour gérer les envois et les éventuelles erreurs
int retryCount = 0;                   // Compteur de tentatives pour l'envoi de données
const int maxRetries = 3;             // Nombre maximum de tentatives d'envoi de paquets
bool ackReceived = false;             // Indicateur pour savoir si un accusé de réception (ACK) a été reçu

// Identifiants LoRaWAN modifiables à remplacer par les vôtres
String deveui = "b0b9dad0cd1a8168";   // DEVEUI unique de l'appareil
String appeui = "0000000000000000";   // APPEUI de l'application
String appkey = "c696f157814277021f37d86a2e1dd3b0";   // APPKEY pour l'authentification

// Compteur de données à envoyer via LoRaWAN
int counter = 0;                      // Valeur du compteur envoyé périodiquement

void setup() {
    pin_init();  // Initialisation des broches du module
    SerialUSB.begin(115200);   // Initialiser la communication avec le PC pour débogage
    Serial1.begin(115200);     // Initialiser la communication avec le module RA-07H

    delay(1000);
    SerialUSB.println("Initialisation du terminal LoRaWAN avec RA-07H...");

    configureModule();  // Configure le module LoRaWAN avec les paramètres nécessaires

    joinNetwork();  // Tente de rejoindre le réseau LoRaWAN
}

void loop() {
    // Lire les données des capteurs
    int iValeurCapteur = analogRead(CapteurO2); 
    float fTension = iValeurCapteur * (3.3 / 1023.0);
    float do_mg_per_l = fTension * 20; // Conversion en mg/L

    sensors.requestTemperatures();
    float temperatureC = sensors.getTempCByIndex(0);
        
    // Convertir en entiers
    int O2 = (int)do_mg_per_l; // Oxygène en mg/L (valeur entière)

    // // Tableau pour stocker la valeur en hexadécimal sous forme de chaîne
    // char hexString[5];
    // sprintf(hexString, "%04X", temperatureC);

    // Si un accusé de réception est reçu ou si nous n'avons pas atteint le maximum de tentatives
    if (ackReceived || retryCount < maxRetries) {
        sendData(temperatureC, O2);  // Envoie la valeur actuelle du compteur
        counter++;  // Incrémenter le compteur à chaque envoi réussi ou tenté
    } else {
        SerialUSB.println("Nombre maximum de tentatives atteint, abandon de l'envoi.");
    }

    delay(5000);  // Attendre 60 secondes avant d'envoyer de nouvelles données
}

/*
    * Fonction pour configurer le module RA-07H avec les paramètres requis pour la connexion LoRaWAN
    * Cette fonction désactive ADR, ajuste la puissance de transmission et configure les identifiants OTAA.
*/
void configureModule() {
    SerialUSB.println("Configuration du module RA-07H...");

    sendATCommand("AT+CRESTORE");
    sendATCommand("AT+CADR=1");  // activer ADR
    sendATCommand("AT+CFREQBANDMASK=0002");  // Fixer un débit de données élevé (DR3)
    sendATCommand("AT+CTXP=5");  // Configurer la puissance de transmission à 5 dBm

    // Configurer les identifiants OTAA (DEVEUI, APPEUI, APPKEY)
    sendATCommand("AT+CDEVEUI=" + deveui);  // Configurer le DEVEUI unique de l'appareil
    sendATCommand("AT+CAPPEUI=" + appeui);  // Configurer l'APPEUI de l'application
    sendATCommand("AT+CAPPKEY=" + appkey);  // Configurer l'APPKEY pour l'authentification

    sendATCommand("AT+CSAVE");  // Sauvegarder la configuration dans le module pour éviter de répéter la configuration à chaque redémarrage

    SerialUSB.println("Configuration terminée.");
}

/*
    * Fonction pour rejoindre le réseau LoRaWAN en utilisant OTAA (Over The Air Activation)
    * Une attente de 15 secondes est nécessaire pour laisser le temps au module de se connecter.
*/
void joinNetwork() {
    SerialUSB.println("Tentative d'adhésion au réseau LoRaWAN...");
    sendATCommand("AT+CJOIN=1,0,10,8");  // Rejoindre le réseau avec OTAA (essayer pendant 10 secondes)
    delay(15000);  // Attendre 15 secondes pour la jonction
}

/*
    * Fonction pour envoyer des données via le réseau LoRaWAN
    * L'envoi est effectué avec un payload comprenant la valeur actuelle du compteur
*/
void sendData(float temp, int O2Value) {
  // Convertir la température en entier (en supprimant la partie décimale)
  int tempInt = (int)temp; // On peut également utiliser un arrondi si nécessaire


  // Convertir la température en hexadécimal sous forme de chaîne
  char hexString[5];  // 4 caractères hexadécimaux + 1 pour le caractère nul
  sprintf(hexString, "%04X", tempInt);

  char hexString2[5];
  sprintf(hexString2, "%04X", O2Value);

  // Préparer le payload (en supposant qu'on envoie 2 valeurs)
  //byte payload[2]; // 2 octets pour la température et 2 pour l'oxygène
  String payload = String(hexString); // température en entier
  String payload2 = String(hexString2);
  //payload[1] = 0 ;//(byte)((temperatureC - (int)temperatureC) * 100); // partie décimale
  //payload[2] = lowByte(doInt); // oxygène en mg/L
  //payload[3] = highByte(doInt); 

  // Construire la commande d'envoi AT avec la taille du payload
  String command = "AT+DTRX=1,2," + String(payload.length()) + ",01" + payload;
  sendATCommand(command);
  delay(1000);

  SerialUSB.print("Envoi de données Temp : " + payload + "  / ");
  SerialUSB.println(tempInt);


  // Attendre un accusé de réception (ACK) pendant 10 secondes
  delay(10000);

    // Vérifier la réponse pour voir si le paquet a été envoyé avec succès
  if (Serial1.available()) {
    String response = Serial1.readStringUntil('\n');
    SerialUSB.println(response);  // Afficher la réponse du module

    // Vérifier si un ACK ou une erreur a été reçu
    if (response.indexOf("OK+SENT") != -1) {
        SerialUSB.println("Données envoyées avec succès à ThingsBoard.");
        ackReceived = true;
        retryCount = 0;  // Réinitialiser le compteur de tentatives
    } else if (response.indexOf("ERR+SENT") != -1) {
        SerialUSB.println("Erreur de transmission, tentative de réessai...");
        retryCount++;
        if (retryCount < maxRetries) {
            delay(30000);  // Attendre 30 secondes avant de réessayer
            sendATCommand(command);  // Réessayer l'envoi du même paquet
        }
    } else if (response.indexOf("ERROR") != -1) {
        SerialUSB.println("Erreur détectée dans la réponse du module.");
    } else {
        SerialUSB.println("Réponse inattendue : " + response);
    }
  } else {
    SerialUSB.println("Aucune réponse reçue du module.");
  }

  // Construire la commande d'envoi AT avec la taille du payload
  String command2 = "AT+DTRX=1,2," + String(payload2.length()) + ",02" + payload2;
  sendATCommand(command2);


  SerialUSB.print("Envoi de données O2 : " + payload2 + "  / ");
  SerialUSB.println(O2Value);
  

  // Attendre un accusé de réception (ACK) pendant 10 secondes
  delay(10000);

    // Vérifier la réponse pour voir si le paquet a été envoyé avec succès
  if (Serial1.available()) {
    String response = Serial1.readStringUntil('\n');
    SerialUSB.println(response);  // Afficher la réponse du module

    // Vérifier si un ACK ou une erreur a été reçu
    if (response.indexOf("OK+SENT") != -1) {
        SerialUSB.println("Données envoyées avec succès à ThingsBoard.");
        ackReceived = true;
        retryCount = 0;  // Réinitialiser le compteur de tentatives
    } else if (response.indexOf("ERR+SENT") != -1) {
        SerialUSB.println("Erreur de transmission, tentative de réessai...");
        retryCount++;
        if (retryCount < maxRetries) {
            delay(30000);  // Attendre 30 secondes avant de réessayer
            sendATCommand(command);  // Réessayer l'envoi du même paquet
        }
    } else if (response.indexOf("ERROR") != -1) {
        SerialUSB.println("Erreur détectée dans la réponse du module.");
    } else {
        SerialUSB.println("Réponse inattendue : " + response);
    }
  } else {
    SerialUSB.println("Aucune réponse reçue du module.");
  }
}

/*
    * Fonction pour envoyer une commande AT au module RA-07H et lire la réponse
    * Cette fonction envoie une commande AT et imprime la réponse du module sur le terminal de débogage
*/
void sendATCommand(String command) {
    Serial1.println(command);  // Envoyer la commande AT au module RA-07H
    delay(1500);  // Attendre une réponse du module

    // Lire et imprimer la réponse du module
    while (Serial1.available()) {
        String response = Serial1.readStringUntil('\n');
        SerialUSB.println(response);  // Afficher la réponse sur le terminal
    }
}

/*
    * Fonction pour initialiser les pins du module RA-07H
    * Cette initialisation est nécessaire pour contrôler correctement le module et le mettre sous tension.
*/
void pin_init() {
    pinMode(PWR_KEY, OUTPUT);  // Configurer la broche PWR_KEY comme sortie
    pinMode(RST_KEY, OUTPUT);  // Configurer la broche RST_KEY comme sortie
    pinMode(LOW_PWR_KEY, OUTPUT);  // Configurer la broche LOW_PWR_KEY comme sortie

    // Séquence d'initialisation pour mettre sous tension et réinitialiser le module
    digitalWrite(RST_KEY, LOW);  // Maintenir le module en état réinitialisé
    digitalWrite(LOW_PWR_KEY, HIGH);  // Désactiver le mode basse consommation
    digitalWrite(PWR_KEY, HIGH);  // Alimenter le module

    digitalWrite(PWR_KEY, LOW);  // Cycle de mise sous tension
    delay(3000);
    digitalWrite(PWR_KEY, HIGH);
    delay(10000);  // Attendre que le module soit complètement initialisé
}