/*
57 secunde: "14:32:56  09/07 " 
 3 secunde: "14:32  Duminica "  (8 litere -> fără secunde)
 3 secunde: "14:32:56  Luni  "  (4 litere -> cu secunde)
 3 secunde: "14:32:56  Joi   "  (3 litere -> cu secunde)
 */

#include <WiFi.h>
#include <time.h>

// ============ PINII ============
#define CS_PIN   7
#define DATA_PIN 5
#define CLK_PIN  6

// ============ WIFI ============
const char* ssid     = "bbk2";
const char* password = "internet2";

// ============ NTP ============
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7200;
const int   daylightOffset_sec = 3600;

// ============ TIMP AFIȘARE ============
const unsigned long TIME_SCREEN_MS = 27000; // 57 secunde
const unsigned long DAY_SCREEN_MS  = 3000;  // 3 secunde

// ============ VARIABILE ============
bool dotBlink = true;
unsigned long lastBlink = 0;
unsigned long lastNTP = 0;
int displayMode = 0; 
unsigned long lastModeChange = 0;

// ============================================
// SOFTWARE SPI PT6302L
// ============================================

void Send(byte b) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(DATA_PIN, (b >> i) & 0x01);
    digitalWrite(CLK_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(CLK_PIN, LOW);
    delayMicroseconds(10);
  }
}

void VFD_init() {
  digitalWrite(CS_PIN, LOW);
  Send(B01110000);
  digitalWrite(CS_PIN, HIGH);
  delay(500);
  digitalWrite(CS_PIN, LOW);
  Send(B01110000);
  digitalWrite(CS_PIN, HIGH);  
}

void VFD_clear() {
  digitalWrite(CS_PIN, LOW);
  Send(B00010000);
  for (byte i = 0; i <= 15; i++)
    Send(B00100000);
  digitalWrite(CS_PIN, HIGH);  
  delay(10);
  digitalWrite(CS_PIN, LOW);
  Send(B00110000);
  for (byte i = 0; i <= 15; i++)
    Send(B00100000);
  digitalWrite(CS_PIN, HIGH);  
}

void Text2VFD(String s) {
  for (byte i = 0; i <= 15 && s.charAt(i) != 0; i++) {
    digitalWrite(CS_PIN, LOW);
    Send(0x10 | (15 - i));    
    Send(s.charAt(i));
    digitalWrite(CS_PIN, HIGH);  
  }
}

void VFD_bright(byte b) {
  digitalWrite(CS_PIN, LOW);
  Send(0x50 | b);
  digitalWrite(CS_PIN, HIGH);  
}

// ============================================
// FORMATARE
// ============================================

// Ora cu secunde: "14:32:56" (8 caractere)
String getTimeString(struct tm* t) {
  char buf[9];
  sprintf(buf, "%02d%c%02d%c%02d", 
    t->tm_hour, 
    dotBlink ? ':' : ' ',
    t->tm_min,
    dotBlink ? ':' : ' ',
    t->tm_sec);
  return String(buf);
}

// Ora scurtă: "14:32" (5 caractere)
String getTimeStringShort(struct tm* t) {
  char buf[6];
  sprintf(buf, "%02d%c%02d", 
    t->tm_hour, 
    dotBlink ? ':' : ' ',
    t->tm_min);
  return String(buf);
}

// Data scurtă: "09/07" (5 caractere)
String getDateString(struct tm* t) {
  char buf[6];
  sprintf(buf, "%02d/%02d", t->tm_mday, t->tm_mon + 1);
  return String(buf);
}

// Numele zilei: intre 3 si 8 caractere
String getDayName(int wday) {
  const char* days[] = {
    "Duminica",  // 8
    "Luni",      // 4
    "Marti",     // 5
    "Miercuri",  // 8
    "Joi",       // 3
    "Vineri",    // 5
    "Sambata"    // 8
  };
  return String(days[wday]);
}

// ============================================
// WIFI & NTP
// ============================================

void connectWiFi() {
  Text2VFD("Conectare WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int dots = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    dots = (dots + 1) % 4;
    String msg = "Conectare";
    for (int i = 0; i < dots; i++) msg += ".";
    while (msg.length() < 16) msg += " ";
    Text2VFD(msg);
  }
  Text2VFD("WiFi OK!        ");
  delay(1000);
}

void syncNTP() {
  Text2VFD("Sync NTP...     ");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  struct tm timeinfo;
  int timeout = 0;
  while (!getLocalTime(&timeinfo) && timeout < 20) {
    delay(500);
    timeout++;
  }
  
  if (timeout < 20) {
    Text2VFD("NTP OK!         ");
  } else {
    Text2VFD("NTP EROARE!     ");
  }
  delay(1000);
}

// ============================================
// AFISARE
// ============================================

void displayClock(struct tm* t) {
  String display;
  
  if (displayMode == 0) {
    // Ecran 1: "14:32:56  09/07 " (16 caractere)
    display = getTimeString(t) + "  " + getDateString(t);
  } else {
    // Ecran 2: Logica adaptivă în funcție de lungimea zilei
    String day = getDayName(t->tm_wday);
    
    if (day.length() >= 8) {
      // "14:32  Duminica " (5 + 2 + 8 + 1 = 16 caractere)
      display = getTimeStringShort(t) + "  " + day;
    } else {
      // "14:32:56  Luni  " (8 + 2 + 4 + 2 = 16 caractere)
      display = getTimeString(t) + "  " + day;
    }
  }
  
  // Completează cu spații dacă lipsește ceva
  while (display.length() < 16) display += " ";
  
  Text2VFD(display);
}

// ============================================
// SETUP
// ============================================

void setup() {
  delay(100);
  
  pinMode(CS_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLK_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  digitalWrite(DATA_PIN, LOW);
  digitalWrite(CLK_PIN, LOW);
  
  VFD_init();
  VFD_clear();
  VFD_bright(7);
  
  Text2VFD("VFD NTP Clock   ");
  delay(1000);
  
  connectWiFi();
  syncNTP();
  
  lastModeChange = millis();
  lastNTP = millis();
}

// ============================================
// LOOP
// ============================================

void loop() {
  struct tm timeinfo;
  
  if (!getLocalTime(&timeinfo)) {
    Text2VFD("Fara timp!      ");
    delay(1000);
    if (millis() - lastNTP > 60000) {
      syncNTP();
      lastNTP = millis();
    }
    return;
  }
  
  // Blink : la 500ms
  if (millis() - lastBlink > 500) {
    dotBlink = !dotBlink;
    lastBlink = millis();
  }
  
  // Comutare 57 secunde / 3 secunde
  unsigned long elapsed = millis() - lastModeChange;
  if (displayMode == 0 && elapsed >= TIME_SCREEN_MS) {
    displayMode = 1;
    lastModeChange = millis();
  } else if (displayMode == 1 && elapsed >= DAY_SCREEN_MS) {
    displayMode = 0;
    lastModeChange = millis();
  }
  
  displayClock(&timeinfo);
  
  // Re-sync NTP la fiecare oră
  if (millis() - lastNTP > 3600000) {
    syncNTP();
    lastNTP = millis();
  }
  
  // Luminozitate auto
  int hour = timeinfo.tm_hour;
  if (hour >= 22 || hour < 7) {
    VFD_bright(2);
  } else if (hour >= 19) {
    VFD_bright(4);
  } else {
    VFD_bright(7);
  }
  
  delay(100);
}
