#include <SPI.h>
#include <Adafruit_PN532.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define PN532_SCK  (5) // SCK (Serial Clock)
#define PN532_MISO (19) // MISO (Master In Slave Out)
#define PN532_MOSI (23) // MOSI (Master Out Slave In)
#define PN532_SS   (18) // SS (Slave Select)

#define Buzzer 14
#define Indicator 13

int size1 = 50;
int size2 = 200;

char details[40];
char detailsLong[250];

WiFiClient espClient;
PubSubClient mqttClient(espClient);

Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
String idcard;
String idcardOld;

void setup() {
  Serial.begin(115200);     

  setupWifi();

  setupMqtt();

  setupPn532();

  pinMode(Buzzer, OUTPUT);
  digitalWrite(Buzzer, LOW);

  pinMode(Indicator, OUTPUT);
  digitalWrite(Indicator, LOW);
}

void loop() {

  if (!mqttClient.connected()){
    connectToBroker();
  }

  mqttClient.loop();

  readPn532();
}

void setupWifi() {

  WiFi.begin("NTWS-4G", "XXXXXXXXXXXX");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void setupMqtt() {
  mqttClient.setServer("test.mosquitto.org", 1883);      
}

void setupPn532(){
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {     
    while (1); // halt
  }

  nfc.setPassiveActivationRetries(0xFF);
}

void readPn532(){
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  if (success) {
    // Display some basic information about the card

    idcardOld = idcard;

    idcard = "";
    for (byte i = 0; i <= uidLength - 1; i++) {
      idcard += (uid[i] < 0x10 ? "0" : "") + String(uid[i], HEX);
    }

    if (idcard != idcardOld){
      String(idcard).toCharArray(details, size1);
      mqttClient.publish("210610H-CardDetails", details);
      Indicate();
    }
  }
}

void connectToBroker(){
  while (!mqttClient.connected()){

    if (mqttClient.connect("210610H-forklift")){
      String("RFID based asset tracking device CONNECTED").toCharArray(details, size2);
      mqttClient.publish("210610H-Details", details);
    }
    else{
      delay(5000);
    }
  }
}

void Indicate(){
  tone(Buzzer, 400);
  digitalWrite(Indicator, HIGH);
  delay(400);

  noTone(Buzzer);
  digitalWrite(Indicator, LOW);
}