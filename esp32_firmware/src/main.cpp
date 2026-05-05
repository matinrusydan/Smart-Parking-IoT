#include <Arduino.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// --- KONFIGURASI WIFI & MQTT ---
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic_status = "proyek/smartparking/status123";
const char* mqtt_topic_control = "proyek/smartparking/control123"; 

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsgTime = 0;

// --- KONFIGURASI HARDWARE ---
#define NUM_SLOTS 8
#define HARDWARE_TYPE MD_MAX72XX::PAROLA_HW
#define MAX_DEVICES 5 
#define CS_PIN_L1 5   
#define CS_PIN_L2 19  

MD_Parola P1 = MD_Parola(HARDWARE_TYPE, CS_PIN_L1, MAX_DEVICES);
MD_Parola P2 = MD_Parola(HARDWARE_TYPE, CS_PIN_L2, MAX_DEVICES);
MD_MAX72XX *mx1; 
MD_MAX72XX *mx2;

byte arrowMasuk1[8]   = {0x30, 0x0C, 0x03, 0xFF, 0xFF, 0x03, 0x0C, 0x30}; 
byte arrowMasuk2[8]   = {0x60, 0x18, 0x06, 0xFE, 0xFE, 0x06, 0x18, 0x60}; 
byte arrowNaik1[8]    = {0x80, 0x40, 0x20, 0x11, 0x09, 0x05, 0x03, 0x1F};
byte arrowNaik2[8]    = {0x80, 0x40, 0x22, 0x12, 0x0A, 0x06, 0x3E, 0x00}; 
byte arrowTurun1[8]   = {0x0C, 0x30, 0xC0, 0xFF, 0xFF, 0xC0, 0x30, 0x0C};
byte arrowTurun2[8]   = {0x06, 0x18, 0x60, 0x7F, 0x7F, 0x60, 0x18, 0x06}; 

enum SlotState { EMPTY = 0, ANOMALY_DETECTED = 1, OCCUPIED = 2 };

struct ParkingSlot {
  String name;
  int trigPin;
  int echoPin;
  int pirPin;
  int lantai;
  long distance;
  bool isMotionDetected;
  SlotState state;
  unsigned long presenceStartTime;
  
  // Variabel untuk Developer Mode (Override Sensor)
  bool overrideActive;
  long overrideDist;
  bool overrideMotion;
};

ParkingSlot slots[NUM_SLOTS] = {
  {"A1", 13, 12, 34, 1, 0, false, EMPTY, 0, false, 0, false},
  {"A2", 13, 14, 35, 1, 0, false, EMPTY, 0, false, 0, false},
  {"A3", 13, 27, 32, 1, 0, false, EMPTY, 0, false, 0, false},
  {"A4", 13, 26, 33, 1, 0, false, EMPTY, 0, false, 0, false},
  {"B1", 4,  16, 25, 2, 0, false, EMPTY, 0, false, 0, false},
  {"B2", 4,  17, 2,  2, 0, false, EMPTY, 0, false, 0, false},
  {"B3", 4,  21, 15, 2, 0, false, EMPTY, 0, false, 0, false},
  {"B4", 4,  22, 36, 2, 0, false, EMPTY, 0, false, 0, false}
};

const int DISTANCE_THRESHOLD = 50; 
const unsigned long CONFIRMATION_TIME = 2000; 

unsigned long lastAnimTime = 0;
unsigned long lastSensorTime = 0;
int currentSlot = 0; 
bool animFrame = false; 

char textL1[70] = "CONNECTING..."; 
char textL2[70] = "CONNECTING..."; 
String oldTextL1 = "";
String oldTextL2 = "";
int emptyCountL1 = 0;
int emptyCountL2 = 0;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password, 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

// Fungsi Callback untuk menerima perintah Dev Mode dari Website
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  
  if (!error) {
    // Cek jika ada perintah reset semua dari website
    if (doc.containsKey("resetAll") && doc["resetAll"] == true) {
      for (int i = 0; i < NUM_SLOTS; i++) slots[i].overrideActive = false;
      Serial.println("Auto-Reset: Dev Mode Dimatikan. Sensor Fisik Wokwi Aktif!");
      return;
    }

    String targetSlot = doc["slot"];
    for (int i = 0; i < NUM_SLOTS; i++) {
      if (slots[i].name == targetSlot) {
        if (doc.containsKey("reset") && doc["reset"] == true) {
          slots[i].overrideActive = false; 
          Serial.println("Dev Mode Dimatikan untuk " + targetSlot);
        } else {
          slots[i].overrideActive = true;  
          slots[i].overrideDist = doc["dist"];
          slots[i].overrideMotion = doc["mot"];
          Serial.println("Dev Mode Aktif untuk " + targetSlot);
        }
      }
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-Parking-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(mqtt_topic_control);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  P1.begin(1); P1.setZone(0, 0, 3); P1.displayZoneText(0, textL1, PA_LEFT, 35, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  mx1 = P1.getGraphicObject(); mx1->control(MD_MAX72XX::INTENSITY, 4); 

  P2.begin(1); P2.setZone(0, 0, 3); P2.displayZoneText(0, textL2, PA_LEFT, 35, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  mx2 = P2.getGraphicObject(); mx2->control(MD_MAX72XX::INTENSITY, 4); 

  for (int i = 0; i < NUM_SLOTS; i++) {
    pinMode(slots[i].trigPin, OUTPUT);
    pinMode(slots[i].echoPin, INPUT);
    pinMode(slots[i].pirPin, INPUT);
  }

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback); 
  client.setBufferSize(1024); 
}

// PERBAIKAN: Timeout pulseIn diperbesar ke 20000 agar Wokwi bisa mendeteksi objek tanpa terputus/flicker
long readDistanceFast(int trig, int echo) {
  digitalWrite(trig, LOW); delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long duration = pulseIn(echo, HIGH, 20000); 
  if (duration == 0) return 999; 
  return duration * 0.034 / 2;
}

void drawArrow(MD_MAX72XX *mx, byte symbol[]) {
  mx->control(4, MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (int i = 0; i < 8; i++) mx->setColumn(4, i, symbol[i]); 
  mx->control(4, MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}

void publishMQTTData() {
  if (!client.connected()) return;
  StaticJsonDocument<1024> doc;
  JsonArray slotsArray = doc.createNestedArray("slots");
  
  for (int i = 0; i < NUM_SLOTS; i++) {
    JsonObject slotInfo = slotsArray.createNestedObject();
    slotInfo["name"] = slots[i].name;
    slotInfo["floor"] = slots[i].lantai;
    slotInfo["state"] = slots[i].state;
    slotInfo["dist"] = slots[i].distance; 
    slotInfo["mot"] = slots[i].isMotionDetected ? 1 : 0; 
    slotInfo["dev"] = slots[i].overrideActive ? 1 : 0; 
    
    unsigned long dur = 0;
    if (slots[i].state != EMPTY && slots[i].presenceStartTime > 0) {
      dur = (millis() - slots[i].presenceStartTime) / 1000;
    }
    slotInfo["dur"] = dur;
  }
  
  char jsonBuffer[1024];
  serializeJson(doc, jsonBuffer);
  client.publish(mqtt_topic_status, jsonBuffer);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop(); 

  P1.displayAnimate(); 
  P2.displayAnimate();
  if (P1.getZoneStatus(0)) P1.displayReset(0);
  if (P2.getZoneStatus(0)) P2.displayReset(0);

  if (millis() - lastAnimTime > 200) {
    animFrame = !animFrame; 
    lastAnimTime = millis();
    if (emptyCountL1 == 0 && emptyCountL2 == 0) drawArrow(mx1, animFrame ? arrowTurun1 : arrowTurun2);
    else if (emptyCountL1 == 0 && emptyCountL2 > 0) drawArrow(mx1, animFrame ? arrowNaik1 : arrowNaik2);
    else drawArrow(mx1, animFrame ? arrowMasuk1 : arrowMasuk2);
    if (emptyCountL2 == 0) drawArrow(mx2, animFrame ? arrowTurun1 : arrowTurun2);
    else drawArrow(mx2, animFrame ? arrowMasuk1 : arrowMasuk2);
  }

  // PERBAIKAN: Jeda diubah menjadi 30ms agar sensor di Wokwi punya cukup waktu untuk pantulan
  if (millis() - lastSensorTime > 30) {
    lastSensorTime = millis();
    
    if (slots[currentSlot].overrideActive) {
      slots[currentSlot].distance = slots[currentSlot].overrideDist;
      slots[currentSlot].isMotionDetected = slots[currentSlot].overrideMotion;
    } else {
      slots[currentSlot].distance = readDistanceFast(slots[currentSlot].trigPin, slots[currentSlot].echoPin);
      slots[currentSlot].isMotionDetected = digitalRead(slots[currentSlot].pirPin);
    }
    
    if (slots[currentSlot].distance > 0 && slots[currentSlot].distance <= 5) {
      // Jarak 0-5 cm dianggap ada benda mati menempel/menghalangi sensor (misal: batu/sampah/kertas)
      slots[currentSlot].presenceStartTime = 0; 
      slots[currentSlot].state = ANOMALY_DETECTED; 
    } else if (slots[currentSlot].distance > 5 && slots[currentSlot].distance < DISTANCE_THRESHOLD) {
      if (slots[currentSlot].presenceStartTime == 0) slots[currentSlot].presenceStartTime = millis(); 
      unsigned long duration = millis() - slots[currentSlot].presenceStartTime;
      if (duration >= CONFIRMATION_TIME) slots[currentSlot].state = OCCUPIED; 
      else if (slots[currentSlot].isMotionDetected) slots[currentSlot].state = ANOMALY_DETECTED; 
    } else {
      slots[currentSlot].presenceStartTime = 0; 
      slots[currentSlot].state = EMPTY; 
    }
    currentSlot++; 

    if (currentSlot >= NUM_SLOTS) {
      currentSlot = 0;
      int tempEmptyL1 = 0, tempEmptyL2 = 0;
      bool anomalyL1 = false, anomalyL2 = false;
      String strEmptyL1 = "", strEmptyL2 = "";

      for (int i = 0; i < NUM_SLOTS; i++) {
        if (slots[i].lantai == 1) {
          if (slots[i].state == EMPTY) { tempEmptyL1++; strEmptyL1 += (tempEmptyL1>1?", ":"") + slots[i].name; }
          if (slots[i].state == ANOMALY_DETECTED) anomalyL1 = true;
        } else {
          if (slots[i].state == EMPTY) { tempEmptyL2++; strEmptyL2 += (tempEmptyL2>1?", ":"") + slots[i].name; }
          if (slots[i].state == ANOMALY_DETECTED) anomalyL2 = true;
        }
      }

      emptyCountL1 = tempEmptyL1;
      emptyCountL2 = tempEmptyL2;
      String newTextL1 = "", newTextL2 = "";

      if (anomalyL1) newTextL1 = "AWAS PEJALAN KAKI L1!";
      else if (emptyCountL1 == 0 && emptyCountL2 == 0) newTextL1 = "SEMUA PENUH - KELUAR";
      else if (emptyCountL1 == 0 && emptyCountL2 > 0) newTextL1 = "PENUH - NAIK LT 2";
      else newTextL1 = "LT 1 KOSONG: " + strEmptyL1;

      if (anomalyL2) newTextL2 = "AWAS PEJALAN KAKI L2!";
      else if (emptyCountL1 == 0 && emptyCountL2 == 0) newTextL2 = "SEMUA PENUH - KELUAR";
      else if (emptyCountL2 == 0) newTextL2 = "PENUH - TURUN LT 1";
      else newTextL2 = "LT 2 KOSONG: " + strEmptyL2;

      if (newTextL1 != oldTextL1) { oldTextL1 = newTextL1; newTextL1.toCharArray(textL1, 70); P1.displayReset(0); }
      if (newTextL2 != oldTextL2) { oldTextL2 = newTextL2; newTextL2.toCharArray(textL2, 70); P2.displayReset(0); }
      
      if(millis() - lastMsgTime > 1000) {
        publishMQTTData();
        lastMsgTime = millis();
      }
    }
  }
}