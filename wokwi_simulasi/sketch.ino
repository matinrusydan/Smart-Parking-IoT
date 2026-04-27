/*
  Sistem Smart Parking IoT Multi-Lantai (8 Slot)
  Fitur: Routing Antar Lantai, Double LED Matrix, & Sensor Fusion
*/

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#define NUM_SLOTS 8

// Konfigurasi LED Matrix Lantai 1 & Lantai 2
#define HARDWARE_TYPE MD_MAX72XX::PAROLA_HW
#define MAX_DEVICES 5 // 5 Modul per layar
#define CS_PIN_L1 5   // Chip Select Matrix Lantai 1
#define CS_PIN_L2 19  // Chip Select Matrix Lantai 2

// Dua instance Parola (Satu untuk tiap lantai)
MD_Parola P1 = MD_Parola(HARDWARE_TYPE, CS_PIN_L1, MAX_DEVICES);
MD_Parola P2 = MD_Parola(HARDWARE_TYPE, CS_PIN_L2, MAX_DEVICES);

MD_MAX72XX *mx1; 
MD_MAX72XX *mx2;

// --- FRAME ANIMASI LED MATRIX ---
// Panah Lurus (Masuk Line)
byte arrowMasuk1[8]   = {0x30, 0x0C, 0x03, 0xFF, 0xFF, 0x03, 0x0C, 0x30}; 
byte arrowMasuk2[8]   = {0x60, 0x18, 0x06, 0xFE, 0xFE, 0x06, 0x18, 0x60}; 

// Panah Serong Kiri Atas (Naik Lantai)
byte arrowNaik1[8]    = {0x80, 0x40, 0x20, 0x11, 0x09, 0x05, 0x03, 0x1F};
byte arrowNaik2[8]    = {0x80, 0x40, 0x22, 0x12, 0x0A, 0x06, 0x3E, 0x00}; 

// Panah Putar Balik / Turun (Arah Bawah)
byte arrowTurun1[8]   = {0x0C, 0x30, 0xC0, 0xFF, 0xFF, 0xC0, 0x30, 0x0C};
byte arrowTurun2[8]   = {0x06, 0x18, 0x60, 0x7F, 0x7F, 0x60, 0x18, 0x06}; 

enum SlotState { EMPTY, ANOMALY_DETECTED, OCCUPIED };

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
};

// Konfigurasi 8 Sensor (Trig digabung per lantai agar hemat pin ESP32)
ParkingSlot slots[NUM_SLOTS] = {
  // Lantai 1 (Index 0-3) - Trig 13
  {"A1", 13, 12, 34, 1, 0, false, EMPTY, 0},
  {"A2", 13, 14, 35, 1, 0, false, EMPTY, 0},
  {"A3", 13, 27, 32, 1, 0, false, EMPTY, 0},
  {"A4", 13, 26, 33, 1, 0, false, EMPTY, 0},
  
  // Lantai 2 (Index 4-7) - Trig 4
  {"B1", 4,  16, 25, 2, 0, false, EMPTY, 0},
  {"B2", 4,  17, 2,  2, 0, false, EMPTY, 0},
  {"B3", 4,  21, 15, 2, 0, false, EMPTY, 0},
  {"B4", 4,  22, 36, 2, 0, false, EMPTY, 0} // 36 = Pin VP di Devkit
};

const int DISTANCE_THRESHOLD = 50; 
const unsigned long CONFIRMATION_TIME = 3000; 

unsigned long lastAnimTime = 0;
unsigned long lastSensorTime = 0;
int currentSlot = 0; 
bool animFrame = false; 

char textL1[70] = "   "; 
char textL2[70] = "   "; 
String oldTextL1 = "";
String oldTextL2 = "";

int emptyCountL1 = 0;
int emptyCountL2 = 0;

void setup() {
  Serial.begin(115200);
  
  // Inisialisasi Matrix Lantai 1
  P1.begin(1); 
  P1.setZone(0, 0, 3); 
  P1.displayZoneText(0, textL1, PA_LEFT, 50, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  mx1 = P1.getGraphicObject();
  mx1->control(MD_MAX72XX::INTENSITY, 4); 

  // Inisialisasi Matrix Lantai 2
  P2.begin(1); 
  P2.setZone(0, 0, 3); 
  P2.displayZoneText(0, textL2, PA_LEFT, 50, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  mx2 = P2.getGraphicObject();
  mx2->control(MD_MAX72XX::INTENSITY, 4); 

  // Setup Sensor
  for (int i = 0; i < NUM_SLOTS; i++) {
    pinMode(slots[i].trigPin, OUTPUT);
    pinMode(slots[i].echoPin, INPUT);
    pinMode(slots[i].pirPin, INPUT);
  }
}

long readDistance(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  
  long duration = pulseIn(echo, HIGH, 15000); 
  if (duration == 0) return 999; 
  return duration * 0.034 / 2;
}

void drawArrow(MD_MAX72XX *mx, byte symbol[]) {
  mx->control(4, MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (int i = 0; i < 8; i++) {
    mx->setColumn(4, i, symbol[i]); 
  }
  mx->control(4, MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}

void loop() {
  // 1. UPDATE TEKS BERJALAN KEDUA LANTAI
  P1.displayAnimate(); 
  P2.displayAnimate();
  
  if (P1.getZoneStatus(0)) P1.displayReset(0);
  if (P2.getZoneStatus(0)) P2.displayReset(0);

  // 2. UPDATE ANIMASI PANAH KEDUA LANTAI
  if (millis() - lastAnimTime > 300) {
    animFrame = !animFrame; 
    lastAnimTime = millis();

    // Logika Panah Lantai 1
    if (emptyCountL1 == 0 && emptyCountL2 == 0) {
      drawArrow(mx1, animFrame ? arrowTurun1 : arrowTurun2); // Semua Penuh -> Panah Turun
    } else if (emptyCountL1 == 0 && emptyCountL2 > 0) {
      drawArrow(mx1, animFrame ? arrowNaik1 : arrowNaik2); // L1 Penuh, L2 Ada -> Panah Naik
    } else {
      drawArrow(mx1, animFrame ? arrowMasuk1 : arrowMasuk2); // L1 Ada Kosong -> Panah Lurus
    }

    // Logika Panah Lantai 2
    if (emptyCountL2 == 0) {
      drawArrow(mx2, animFrame ? arrowTurun1 : arrowTurun2); // L2 Penuh -> Panah Turun
    } else {
      drawArrow(mx2, animFrame ? arrowMasuk1 : arrowMasuk2); // L2 Ada Kosong -> Panah Lurus
    }
  }

  // 3. BACA 8 SENSOR SECARA ROUND-ROBIN (Anti-Ngelag)
  if (millis() - lastSensorTime > 15) {
    lastSensorTime = millis();

    slots[currentSlot].distance = readDistance(slots[currentSlot].trigPin, slots[currentSlot].echoPin);
    slots[currentSlot].isMotionDetected = digitalRead(slots[currentSlot].pirPin);
    
    if (slots[currentSlot].distance > 0 && slots[currentSlot].distance < DISTANCE_THRESHOLD) {
      if (slots[currentSlot].presenceStartTime == 0) slots[currentSlot].presenceStartTime = millis(); 
      unsigned long duration = millis() - slots[currentSlot].presenceStartTime;
      
      if (duration >= CONFIRMATION_TIME) {
        slots[currentSlot].state = OCCUPIED; 
      } else if (slots[currentSlot].isMotionDetected) {
        slots[currentSlot].state = ANOMALY_DETECTED; 
      }
    } else {
      slots[currentSlot].presenceStartTime = 0; 
      slots[currentSlot].state = EMPTY; 
    }

    currentSlot++; 

    // 4. REKAP DATA & UPDATE TEKS SETELAH SEMUA SENSOR TERBACA
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

      // Prioritas Pesan Lantai 1
      if (anomalyL1) {
        newTextL1 = "AWAS PEJALAN KAKI L1!";
      } else if (emptyCountL1 == 0 && emptyCountL2 == 0) {
        newTextL1 = "SEMUA PENUH - KELUAR";
      } else if (emptyCountL1 == 0 && emptyCountL2 > 0) {
        newTextL1 = "PENUH - NAIK LT 2";
      } else {
        newTextL1 = "LT 1 KOSONG: " + strEmptyL1;
      }

      // Prioritas Pesan Lantai 2
      if (anomalyL2) {
        newTextL2 = "AWAS PEJALAN KAKI L2!";
      } else if (emptyCountL1 == 0 && emptyCountL2 == 0) {
        newTextL2 = "SEMUA PENUH - KELUAR";
      } else if (emptyCountL2 == 0) {
        newTextL2 = "PENUH - TURUN LT 1";
      } else {
        newTextL2 = "LT 2 KOSONG: " + strEmptyL2;
      }

      // Perbarui Buffer Matrix 1
      if (newTextL1 != oldTextL1) {
        oldTextL1 = newTextL1;
        newTextL1.toCharArray(textL1, 70); 
        P1.displayReset(0); 
      }
      
      // Perbarui Buffer Matrix 2
      if (newTextL2 != oldTextL2) {
        oldTextL2 = newTextL2;
        newTextL2.toCharArray(textL2, 70); 
        P2.displayReset(0); 
      }
    }
  }
}