/**
 *    IOT laboratorio HW-3
 *        The Arduino sketch must be modified accordingly to perform the following tasks:
          i. register itself on the on Catalog (refer to Exercise 1 Lab Software – part 2).
          ii. send information about temperature, presence and noise via MQTT
          iii. receive actuation commands via MQTT
          iv. receive messages to be displayed on the LCD monitor via MQTT

 */

#include <MQTTclient.h>
#include <Bridge.h>
#include <BridgeServer.h>
#include <BridgeClient.h>
#include <ArduinoJson.h>
#include <math.h>
#include <LiquidCrystal_PCF8574.h>

const int capacity = JSON_OBJECT_SIZE(1) + JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(3) + 50;
DynamicJsonDocument doc_snd(capacity);
DynamicJsonDocument doc_rec(capacity);

float tempFanMinNoPeople = 25;
float tempFanMaxNoPeople = 30;
float tempLedMinNoPeople = 20;
float tempLedMaxNoPeople = 25;
float tempFanMinWithPeople = 25;
float tempFanMaxWithPeople = 35;
float tempLedMinWithPeople = 15;
float tempLedMaxWithPeople = 25;

int tempFanMin = 0;
int tempFanMax = 0;
int tempLedMin = 0;
int tempLedMax = 0;

float temp;

const int ledPin = 11;
const int tempPin = A1;
const long int R0 = 100000;
const int beta = 4275;
const int sleepTime = 5;

const int fanPin = 10;
/* Valore corrente speed da 0 a 255 */
float currentSpeed = 0;
float ledPower = 0;

const int soundPin = 7;
const unsigned long timeoutPir = 1800000;         /* timeout pir, circa 30 minuto 1800 secondi */
volatile unsigned long checkTimePir = 0;
unsigned long currentMillis;

/**
 * QUESTO FLAG INDICA LA PRESENZA DI PERSONE, 0 ASSENZA E 1 PRESENZA (sensor fusion, slide 03, pag +-70)
 */
int flag = 0;

const int pirPin = 4;
const unsigned long soundInterval = 600000;      /* 10 minuti in millis */
const unsigned long timeoutSound = 2400000;      /* 40 minuti timeout */
volatile unsigned long checkTimeSound = 0;
int countSoundEvent = 0;

int setupLCD = 0;
LiquidCrystal_PCF8574 lcd(0x27);

/* Variabili per Finestra Mobile */
const int TIME_INTERVAL = 10 + 1, EVENTS_NUM = 50;
int soundEvents[TIME_INTERVAL];

/* Variabili per LCD */
int timeout = 60000;

/**
 * parte per comunicazione mqtt
 */

String myBaseTopicLed = "/tiot/26/catalog/led";
String myBaseTopicTmp = "/tiot/26/catalog/tmp";
String myBaseTopicFan = "/tiot/26/catalog/fan";
String myBaseTopicPresence = "/tiot/26/catalog/presence";
String myBaseTopicResponse = "/tiot/26/catalog/+/res";

String basenameTmp = "Yun";
String basenameLed = "Yun";
String basenameFan = "Yun";
String basenamePresence = "Yun"; 

void setup() {
  // put your setup code here, to run once:
  pinMode(tempPin, INPUT);
  pinMode(fanPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  analogWrite(fanPin, currentSpeed);

  pinMode(soundPin, INPUT);

  pinMode(pirPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(soundPin), checkSound, FALLING);

  setupSoundEvents(soundEvents);

  lcd.begin(16, 2);
  lcd.setBacklight(255);
  lcd.home();
  lcd.clear();
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
  Bridge.begin();
  digitalWrite(13, HIGH);
  mqtt.begin("mqtt.eclipse.org", 1883);
  mqtt.subscribe(myBaseTopicResponse, setLedValue);
  /**
   * setLedValue, ovvero il secondo argomento della funzione
   * subscribe serve ad associare una funzione al segnale di 
   * callback data dala notify di mqtt quando i dati sono
   * disponibili
   */
  Serial.begin(9600);
  Serial.print("Lab 3.3 Starting:");
}

void loop() {
  // put your main code here, to run repeatedly:
  mqtt.monitor();
  /**
   * monitor attiva la comunicazione in arrivo dai topic
   * a cui si è iscritti, serve a chiamare la callback setLedValue
   */
  /**
   * Settaggio parametri di temperatura
  */
  if (flag==0){   
    tempFanMin = tempFanMinNoPeople;
    tempFanMax = tempFanMaxNoPeople;
    tempLedMin = tempLedMinNoPeople;
    tempLedMax = tempLedMaxNoPeople;
  }
  else if (flag==1){    
    tempFanMin = tempFanMinWithPeople;
    tempFanMax = tempFanMaxWithPeople;
    tempLedMin = tempLedMinWithPeople;
    tempLedMax = tempLedMaxWithPeople;
  }
  
   temp = checkTemp();

   /**
   * Controllo ventilatore
   */
  if (temp < tempFanMax && temp > tempFanMin){
    currentSpeed = (temp-tempFanMin)/(tempFanMax-tempFanMin)*255.0;
    analogWrite(fanPin, currentSpeed);
    Serial.print("Temperatura :");
    Serial.print(temp);
    Serial.print(" velocita :");
    Serial.println(currentSpeed);
  }
  else{
    currentSpeed = 0;
    analogWrite(fanPin, currentSpeed);
  }
  /**
   * Controllo led
   */
  if (temp < tempLedMax && temp > tempLedMin){
    ledPower = abs(temp-tempLedMax)/abs(tempLedMin-tempLedMax)*255.0;
    analogWrite(ledPin, ledPower);
  }
  else {
    ledPower = 0;
    analogWrite(ledPin, ledPower);
  }

  /**
   * COMUNICAZIONE MQTT 
   */

  String jsonTemp = senMlEncode("tmp", temp, "C", basenameTmp);
  mqtt.publish(myBaseTopicTmp, jsonTemp);
  Serial.print("published tmp on topic");
  Serial.println(jsonTemp);
  Serial.println(myBaseTopicTmp);

  String jsonLed = senMlEncode("tmp", ledPower, "", basenameLed);
  mqtt.publish(myBaseTopicLed, jsonTemp);
  String jsonFan = senMlEncode("fan", currentSpeed, "", basenameFan);
  mqtt.publish(myBaseTopicFan, jsonTemp);
  String jsonPresence = senMlEncode("pres", flag, "", basenamePresence);
  mqtt.publish(myBaseTopicPresence, jsonTemp);
  
  /**
   *  Questo current millis indica il tempo trasorso dall'ultimo movimento
   *  rilevato dal sensore PIR, ovvero dall'ultima volta che ho registrato 
   *  il checkTimePir, se questo valore scade significa che non c'è nessuno.
   */
  
  currentMillis = millis();
  if (currentMillis - checkTimePir >= timeoutPir) {
    flag = 0;
  }

  /**
   * Al posto della delay uitilizzo un ciclo while 
   * in cui attento il passare del tempo e checko la presenza di movimenti
   * il ciclo while dura 5 secondi, prima di ogni ciclo vado a cambiare la pagina
   * del LCD
   */
  lookLCD();
 
  int delayMillis = int(millis()/1000);
  while (int(millis()/1000) - delayMillis <= sleepTime){
    checkPresence(); 
//    if (Serial.available()){
//      listenSerial();
//    }
  }
   
  delay(5000);
}
/**
 * penso sia difficile da interpretare per via della funzione di callback
 * 
 */
void setLedValue(const String& topic, const String& subtopic, const String& message){
  DeserializationError err = deserializeJson(doc_rec, message);
  if (err){
    Serial.print(F("DeserializeJson() failed with code "));
    Serial.println(err.c_str());
  }
  String topicCompleto = String(topic)+String(subtopic);
  if (topicCompleto == myBaseTopicLed){
    if (doc_rec["e"][0]["n"] == "led"){
      if(doc_rec["e"][0]["v"]==0 || doc_rec["e"][0]["v"]==1){
        digitalWrite(ledPin, (int)doc_rec["e"][0]["v"]);  
      }
      else{
        Serial.println("Valore errato del led");
      }
    } 
    else {
      Serial.println("Errore nel json, mi aspetto un messaggio con n = led!"); 
    }
  }
  else {
    Serial.println("ti sei iscritto al topic sbagliato. riprogrammare la scheda");
  }
  
}

/**
 * funzione rileva temperatura
 */
float checkTemp(){
  int vDigit = analogRead(tempPin);
  /**
   * Calcolo il valore di R, successivamente
   * uso il datasheet per ricavare le formule di conversione e 
   * calcolo T, per poi convertire in Celsius
  */
  float R = ((1023.0/vDigit)-1.0);
  R = R0*R;
  float loga = log(R/R0)/beta;
  float TK = 1.0/((loga)+(1.0/298.15));
  float TC = TK-273.15;
  return TC;
}

/**
 * crea il json
 */
String senMlEncode(String res, float v, String unit, String bn){
  /**
   * params:  res = risorsa in uso. in questo caso sarà 
   *                sempre la temperatura
   *          v = valore della risorsa. la prendo dalla 
   *              checktemp tramite la variabile temp
   *              (si potrebbe semplificare per un caso reale
   *              ma preferisco mantenere leggibilità)
   *          unit = unità di misura del valore della risorsa
   */
  doc_snd.clear();
  doc_snd["bn"] = bn;
  doc_snd["e"][0]["n"] = res;
  doc_snd["e"][0]["v"] = v;
  if (unit != ""){
    doc_snd["e"][0]["v"] = unit;
  } else {
    doc_snd["e"][0]["u"] = (char*)NULL;
  }
  
  String output;
  serializeJson(doc_snd, output);
  return output;
}
//
//  doc_snd.clear();
//  doc_snd["res"] = res;
//  doc_snd["value"] = v;
//  if (unit != ""){
//    doc_snd["u"] = unit;
//  } else {
//    doc_snd["u"] = (char*)NULL;
//  }


/**
 * Forse sarebbe meglio mettere come interrupt il sensore di
 * rumore, avendo a disposizione questi sensori si portrebbe 
 * diminuire la sensibilità del sensore di prossimità così che
 * ogni ciclo del loop principale possa catturare comunque le 
 * variazioni del sensore, mentre quello di rumore (più semplice
 * e basilare, e soprattutto solo digitale) si potrebbe impostare
 * sull'interrupt
 */
 
void setupSoundEvents(int vect[]) {
  int i;
  for (i = 0; i < TIME_INTERVAL; i++) {
    vect[i] = 0;
  }
}

void checkPresence(){
  if (digitalRead(pirPin)==HIGH){
    flag = 1;
    checkTimePir = millis();
    Serial.println("Movimento rilevato!");
  }
}

void checkSound(){
  /* Implementazione Buffer Circolare */
  int index, count=0, i, tail;
  
  index = int(millis()/60000)%(TIME_INTERVAL);
  /* Metto a zero il successivo minuto */
  tail = (index + 1)%TIME_INTERVAL;
  soundEvents[tail] = 0;
  /* Aggiorno gli eventi della finestra corrente */
  if (digitalRead(soundPin)==LOW){
    soundEvents[index]++;
    delay(200);
    Serial.println("Suono ricevuto!");
    Serial.print("SOUND EVENTS NEL BLOCCO = ");
    Serial.println(String(soundEvents[index]));
  }
  /* Controllo se nella finestra ci sono abbastanza eventi */
  for(i = 0; i < TIME_INTERVAL; i++) {
    count += soundEvents[i];
  }
  if (count >= EVENTS_NUM) {
    flag = 1;
  } 
  else {
    flag = 0;
  };
}

void lookLCD(){
  /*
   * In base al setup corrente faccio la print della schermata 
   */
  if (setupLCD == 0){
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("T=");
    lcd.print(String(temp));
    lcd.print(" Pres:");
    lcd.print(String(flag));
    lcd.setCursor(0, 1);
    lcd.print("AC:");
    lcd.print(String(currentSpeed/2.55));
    lcd.print(" HT:");
    lcd.print(String(ledPower/2.55));
    setupLCD = 1;

    Serial.print("T=");
    Serial.print(String(temp));
    Serial.print(" Pres:");
    Serial.print(String(flag));
    Serial.println("");
    Serial.print("AC:");
    Serial.print(String(currentSpeed/2.55));
    Serial.print(" HT:");
    Serial.print(String(ledPower/2.55));
  }
  else if(setupLCD == 1){
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("AC m:");
    lcd.print(String(tempFanMin));
    lcd.print(" M:");
    lcd.print(String(tempFanMax));
    lcd.setCursor(0, 1);
    lcd.print("HT m:");
    lcd.print(String(tempLedMin));
    lcd.print(" M:");
    lcd.print(String(tempLedMax));
    setupLCD = 0;


    Serial.print("AC m:");
    Serial.print(String(tempFanMin));
    Serial.print(" M:");
    Serial.print(String(tempFanMax));
    Serial.println("");
    Serial.print("HT m:");
    Serial.print(String(tempLedMin));
    Serial.print(" M:");
    Serial.print(String(tempLedMax));
  }
}

/**
 * Dalla read devo leggere 8 valori, in sequenza saranno
  int tempFanMinNoPeople = 25;
  int tempFanMaxNoPeople = 30;
  int tempLedMinNoPeople = 20;
  int tempLedMaxNoPeople = 25;
  int tempFanMinWithPeople = 25;
  int tempFanMaxWithPeople = 35;
  int tempLedMinWithPeople = 15;
  int tempLedMaxWithPeople = 25;

  I VALORI IN INPUT DEVONO ESSERE DIVISI DA /
 */
//void listenSerial(){
//  char data[8][4] = {};
//  char inByte[41] = {};
//
//  //esempio stringa 25.1/26.0/20.0/21.0/23.0/28.0/15.0/22.0
//
//  if (Serial.available() > 0) {
//    int i, k = 0, j;
//
//    
//    int availableBytes = Serial.available();
//    for(int i=0; i<availableBytes; i++){
//       inByte[i] = char(Serial.read());
//    }
//    
//    for (i=0 ; i<40 ; i=i+5){
//      for(j=0 ; j<4; j++){
//        data[k][j] = inByte[j+i];
//      }
//      k++;
//    }
//    tempFanMinNoPeople = atof(data[0]);
//    tempFanMaxNoPeople = atof(data[1]);
//    tempLedMinNoPeople = atof(data[2]);
//    tempLedMaxNoPeople = atof(data[3]);
//    tempFanMinWithPeople = atof(data[4]);
//    tempFanMaxWithPeople = atof(data[5]);
//    tempLedMinWithPeople = atof(data[6]);
//    tempLedMaxWithPeople = atof(data[7]);
//    Serial.println("SETUP DELLE TEMPERATURE AVVENUTO CON SUCCESSO");
//  }
//} 
