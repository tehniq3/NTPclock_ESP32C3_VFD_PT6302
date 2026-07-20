/*
 * Ceas NTP cu date meteo creat cu ajutorul AI-ului, pe baza cerintelor lui Nicu FLORICA (niq_ro) 
Ecran 1: "14:32:56  09/07 "
Ecran 2: "14:32:56 +25.5°C"
Ecran 3: "14:32:56  45%RH "
Ecran 4: "14:32  Duminica " (sau "14:32:56  Luni  ") 
Ecran 5  "14:32:56 Senin  " (sau "14:32  75% nori ")
Ecran 6: "14:32:56 761mmHg"
Ecran 7: "14:32 NV 9.3km/h" (sau "14:32 NV 15km/h")
Ecran 8: "14:32 UV 4[--|-]"
Ecran 9: "14:32 AQ32[|---]"
 */

#include <WiFi.h>
#include <time.h>
#include <HTTPClient.h>

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
const unsigned long TIME_SCREEN_MS = 3000; 
const unsigned long WIFI_CHECK_MS  = 30000; 

// ============ VARIABILE GLOBALE ============
bool dotBlink = true;
unsigned long lastBlink = 0;
unsigned long lastNTP = 0;
unsigned long lastWeatherUpdate = 0;
unsigned long lastWifiCheck = 0;
int displayMode = 0; 
unsigned long lastModeChange = 0;

// Date Meteo
float temperature = 0.0;
int humidity = 0;
int weatherCode = 0;
float windSpeed = 0.0;
int windDir = 0;
float pressure = 0.0; 
float uvIndex = 0.0;
int aqi = 0;
int cloudCover = 0;

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
  digitalWrite(CS_PIN, LOW); Send(B01110000); digitalWrite(CS_PIN, HIGH); delay(500);
  digitalWrite(CS_PIN, LOW); Send(B01110000); digitalWrite(CS_PIN, HIGH);  
}

void VFD_clear() {
  digitalWrite(CS_PIN, LOW); Send(B00010000);
  for(byte i=0;i<=15;i++) Send(B00100000);
  digitalWrite(CS_PIN, HIGH); delay(10);
  digitalWrite(CS_PIN, LOW); Send(B00110000);
  for(byte i=0;i<=15;i++) Send(B00100000);
  digitalWrite(CS_PIN, HIGH);  
}

void Text2VFD(String s) {
  for(byte i=0;i<=15 && s.charAt(i)!=0;i++){
    digitalWrite(CS_PIN, LOW);
    Send(0x10|(15-i));    
    Send(s.charAt(i));
    digitalWrite(CS_PIN, HIGH);  
  }
}

void VFD_bright(byte b) {
  digitalWrite(CS_PIN, LOW); Send(0x50|b); digitalWrite(CS_PIN, HIGH);  
}

// ============================================
// PARSER JSON ROBUST
// ============================================
String extractBlock(String json, String blockName) {
  String searchKey = "\"" + blockName + "\":";
  int startIdx = json.indexOf(searchKey);
  if (startIdx == -1) return "";
  startIdx = json.indexOf('{', startIdx);
  if (startIdx == -1) return "";
  int depth = 1;
  int endIdx = startIdx + 1;
  while (endIdx < json.length() && depth > 0) {
    if (json[endIdx] == '{') depth++;
    else if (json[endIdx] == '}') depth--;
    endIdx++;
  }
  return json.substring(startIdx, endIdx);
}

String getJsonValue(String json, String key) {
  String searchKey = "\"" + key + "\"";
  int idx = json.indexOf(searchKey);
  if (idx == -1) return "";
  idx = json.indexOf(':', idx + searchKey.length());
  if (idx == -1) return "";
  idx++; 
  while (idx < json.length() && json[idx] == ' ') idx++;
  int end = idx;
  while (end < json.length() && json[end] != ',' && json[end] != '}' && json[end] != ']') { end++; }
  String val = json.substring(idx, end);
  val.trim();
  return val;
}

// ============================================
// DESCĂRCARE DATE METEO
// ============================================
void updateWeather() {
  if (WiFi.status() != WL_CONNECTED) return;
  Text2VFD("Actualizari Date");
  HTTPClient http;
  
  String urlWeather = "https://api.open-meteo.com/v1/forecast?latitude=44.33&longitude=23.79&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m,wind_direction_10m,pressure_msl,uv_index,cloud_cover&timezone=Europe/Bucharest";
  http.begin(urlWeather);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    String currentBlock = extractBlock(payload, "current");
    if(currentBlock.length() > 0) {
      temperature = getJsonValue(currentBlock, "temperature_2m").toFloat();
      humidity = getJsonValue(currentBlock, "relative_humidity_2m").toInt();
      weatherCode = getJsonValue(currentBlock, "weather_code").toInt();
      windSpeed = getJsonValue(currentBlock, "wind_speed_10m").toFloat();
      windDir = getJsonValue(currentBlock, "wind_direction_10m").toInt();
      pressure = getJsonValue(currentBlock, "pressure_msl").toFloat();
      uvIndex = getJsonValue(currentBlock, "uv_index").toFloat();
      cloudCover = getJsonValue(currentBlock, "cloud_cover").toInt();
    }
  } else {
    Text2VFD("Eroare API Vreme");
    delay(1000);
  }
  http.end();

  String urlAQI = "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=44.33&longitude=23.79&current=european_aqi&timezone=Europe/Bucharest";
  http.begin(urlAQI);
  httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    String currentBlock = extractBlock(payload, "current");
    if(currentBlock.length() > 0) {
      aqi = getJsonValue(currentBlock, "european_aqi").toInt();
    }
  }
  http.end();
  delay(500);
}

// ============================================
// TRADUCERE DATE METEO
// ============================================
String getWeatherText(int code) {
  if (code == 0)  return "Senin";
  if (code <= 3)  return "Cer cu nori";
  if (code <= 48) return "Ceata";
  if (code <= 55) return "Burnita";
  if (code <= 65) return "Ploaie";
  if (code <= 75) return "Ninsoare";
  if (code <= 82) return "Averse";
  if (code >= 95) return "Furtuna";
  return "Nebulos";
}

// Modificat exact cum ai cerut
String getWindDirText(int deg) {
  if (deg >= 337.5 || deg < 22.5) return " N";
  if (deg < 67.5)  return "NE";
  if (deg < 112.5) return " E";
  if (deg < 157.5) return "SE";
  if (deg < 202.5) return " S";
  if (deg < 247.5) return "SV";
  if (deg < 292.5) return " V";
  return "NV";
}

// ============================================
// BAR GRAFIC CUSTOM (UV ȘI AQI)
// ============================================
String getBar(int level) {
  switch(level) {
    case 0: return "[____]"; // Gol
    case 1: return "[|___]"; // 1/4
    case 2: return "[_|__]"; // 1/2
    case 3: return "[__|_]"; // 3/4
    case 4: return "[___|]"; // Plin
    default: return "[    ]";
  }
}

int getUVLevel(float uv) {
  if (uv <= 0.2) return 0;
  if (uv <= 2) return 1;   // Scazut
  if (uv <= 5) return 2;   // Moderat
  if (uv <= 7) return 3;   // Ridicat
  return 4;                // Extrem
}

int getAQILevel(int val) {
  if (val <= 20) return 0;
  if (val <= 40) return 1;  // Bun / F.Bun
  if (val <= 60) return 2;  // Moderat
  if (val <= 80) return 3;  // Slab
  return 4;                 // F.Slab / Rau
}

// ============================================
// FORMATARE TIMP ȘI DATE
// ============================================
String getTimeString(struct tm* t) {
  char buf[9];
  sprintf(buf, "%02d%c%02d%c%02d", t->tm_hour, dotBlink ? ':' : ' ', t->tm_min, dotBlink ? ':' : ' ', t->tm_sec);
  return String(buf);
}

String getTimeStringShort(struct tm* t) {
  char buf[6];
  sprintf(buf, "%02d%c%02d", t->tm_hour, dotBlink ? ':' : ' ', t->tm_min);
  return String(buf);
}

String getDateString(struct tm* t) {
  char buf[6];
  sprintf(buf, "%02d/%02d", t->tm_mday, t->tm_mon + 1);
  return String(buf);
}

String getDayNameRaw(int wday) {
  const char* days[] = {"Duminica","Luni","Marti","Miercuri","Joi","Vineri","Sambata"};
  return String(days[wday]);
}

// ============================================
// WIFI & NTP & RECONECTARE
// ============================================
void connectWiFi() {
  Text2VFD("Conectare WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true); 
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Text2VFD("Conectare WiFi.");
    delay(500);
    Text2VFD("Conectare WiFi ");
  }
  Text2VFD("WiFi OK!        ");
  delay(1000);
}

void checkAndReconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return; 
  Text2VFD("WiFi Pierdut!   ");
  delay(1000);
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    Text2VFD("Reconectare.... "); delay(500);
    Text2VFD("Reconectare.    "); delay(500);
    timeout++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Text2VFD("WiFi Reconectat!");
    delay(1500);
    lastNTP = 0;
    lastWeatherUpdate = 0;
  } else {
    Text2VFD("Eroare Retea!   ");
    delay(2000);
  }
}

void syncNTP() {
  Text2VFD("Sync NTP...     ");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  int timeout = 0;
  while (!getLocalTime(&timeinfo) && timeout < 20) { delay(500); timeout++; }
  Text2VFD(timeout < 20 ? "NTP OK!         " : "NTP EROARE!     ");
  delay(1000);
}

// ============================================
// LOGICĂ AFIȘARE ECRANE (9 ECRANE)
// ============================================
void displayScreen(struct tm* t) {
  String display = "";
  String timeFull = getTimeString(t);      // "14:32:56" (8 chars)
  String timeShort = getTimeStringShort(t); // "14:32" (5 chars)
  
  switch(displayMode) {
    case 0: // "14:32:56  09/07 "
      display = timeFull + "  " + getDateString(t);
      break;
      
    case 1: // "14:32:56 +25.5°C"
      { 
        String tStr = (temperature >= 0) ? "+" + String(temperature, 1) : String(temperature, 1);
        display = timeFull + " " + tStr + String((char)159) + "C";
      }
      break;

    case 2: // "14:32:56  45%RH "
      { 
        String hStr = (humidity < 10) ? " " + String(humidity) : String(humidity);
        display = timeFull + "  " + hStr + "%RH ";
      }
      break;

    case 3: // "14:32  Duminica " (sau "14:32:56  Luni  ")
      { 
        String day = getDayNameRaw(t->tm_wday);
        if (day.length() >= 8) display = timeShort + "  " + day;
        else display = timeFull + "  " + day;
      }
      break;

    case 4: // "14:32:56  Senin    " (sau "14:32  Ninsoare  ")
      { 
        String wText = getWeatherText(weatherCode);
        if (wText == "Cer cu nori") {
          String cStr = (cloudCover < 10) ? " " + String(cloudCover) : String(cloudCover);
          display = timeShort + " " + cStr + "% nori "; // Păstrează partea cu norii
        } else {
          // Logică nouă: calculează lungimea textului
          if (wText.length() < 8) {
            display = timeFull + "  " + wText; // Arată secundele dacă e sub 8 litere
          } else {
            display = timeShort + "  " + wText; // Fără secundele dacă e 8+ litere
          }
        }
      }
      break;

    case 5: // "14:32:56 761mmHg"
      { 
        int presInt = (int)round(pressure * 0.750062);
        display = timeFull + " " + String(presInt) + "mmHg";
      }
      break;

    case 6: // "14:32 NV 9.3km/h" (sau "14:32 NV 15km/h")
      { 
        String windStr;
        if (windSpeed < 10.0) windStr = String(windSpeed, 1);
        else windStr = String((int)round(windSpeed));
        display = timeShort + " " + getWindDirText(windDir) + " " + windStr + "km/h";
      }
      break;

    case 7: // "14:32 UV 5 [|O  ]"
      { 
        int uvInt = (int)round(uvIndex);
        String uvNum = (uvInt < 10) ? " " + String(uvInt) : String(uvInt);
        display = timeShort + " UV" + uvNum + getBar(getUVLevel(uvIndex)); // Fix 16 chars
      }
      break;

    case 8: // "14:32 AQI25[|O  ]"
      { 
        int aqiDisp = (aqi > 99) ? 99 : aqi; 
        String aqiNum = (aqiDisp < 10) ? " " + String(aqiDisp) : String(aqiDisp);
        display = timeShort + " AQ" + aqiNum + getBar(getAQILevel(aqi)); // Fix 16 chars
      }
      break;
  }
  
  while (display.length() < 16) display += " ";
  Text2VFD(display);
}

// ============================================
// SETUP
// ============================================
void setup() {
  delay(100);
  pinMode(CS_PIN, OUTPUT); pinMode(DATA_PIN, OUTPUT); pinMode(CLK_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH); digitalWrite(DATA_PIN, LOW); digitalWrite(CLK_PIN, LOW);
  
  VFD_init();
  VFD_clear();
  VFD_bright(7);
  
  Text2VFD("VFD Meteo Clock ");
  delay(1000);
  
  connectWiFi();
  syncNTP();
  updateWeather(); 
  
  lastModeChange = millis();
  lastNTP = millis();
  lastWeatherUpdate = millis();
  lastWifiCheck = millis();
}

// ============================================
// LOOP
// ============================================
void loop() {
  if (millis() - lastWifiCheck > WIFI_CHECK_MS) {
    lastWifiCheck = millis();
    checkAndReconnectWiFi();
  }

  struct tm timeinfo;
  
  if (!getLocalTime(&timeinfo)) {
    Text2VFD("Fara timp!      ");
    delay(1000);
    lastWifiCheck = 0; 
    return;
  }
  
  if (millis() - lastBlink > 500) { dotBlink = !dotBlink; lastBlink = millis(); }
  
  unsigned long elapsed = millis() - lastModeChange;
  
  if (elapsed >= TIME_SCREEN_MS) {
    displayMode++;
    if (displayMode > 8) displayMode = 0; 
    lastModeChange = millis();
  }

  if (displayMode == 7 && uvIndex < 0.1) {
    displayMode = 8;
    lastModeChange = millis(); 
  }

  displayScreen(&timeinfo);
  
  if (millis() - lastNTP > 3600000) { syncNTP(); lastNTP = millis(); }
  if (millis() - lastWeatherUpdate > 600000) { updateWeather(); lastWeatherUpdate = millis(); }
  
  int hour = timeinfo.tm_hour;
  if (hour >= 22 || hour < 7) VFD_bright(2);
  else if (hour >= 19) VFD_bright(4);
  else VFD_bright(7);
  
  delay(150); 
}
