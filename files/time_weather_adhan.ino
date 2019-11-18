//------LIBRARIES------
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Wire.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_IS31FL3731.h>

//------CHARLIEWING------
Adafruit_IS31FL3731_Wing matrix = Adafruit_IS31FL3731_Wing();

//------FUNCTIONS--------
void setup_mode ();
void ap_mode ();
void webpage ();
void response ();
bool getData (int data);
bool showTime (char *json);
bool showWeather (char *json);
bool getPrayerTimes (char *json);
void showAdhan (int prayer);

//------CONSTANTS-------
#define TIME                1
#define WEATHER             2
#define ADHAN               3
#define BRIGHTNESS          4
#define DELAY_TIME          (0.3*60*1000)
#define DELAY_ERROR         (2*60*1000)
#define DELAY_WEATHER       (1*60*1000)
#define DELAY_ADHAN         (2*60*1000)
#define APIXU               "api.apixu.com"
#define TIMEZONE            "api.aladhan.com"
#define APIXU_API_KEY       "b3433ff511e24890be9114855171508"
char APIXU_REQ1[]         = "GET /v1/forecast.json?key=" APIXU_API_KEY "&q=";
char APIXU_REQ2[]         = " HTTP/1.1\r\nUser-Agent: ESP8266/0.1\r\nAccept: */*\r\nHost: " APIXU "\r\nConnection: close\r\n\r\n";
char TIMEZONE_REQ1[]      = "GET /currentTime?zone=Europe/Paris";
char TIMEZONE_REQ2[]      = " HTTP/1.1\r\nUser-Agent: ESP8266/0.1\r\nAccept: */*\r\nHost: " TIMEZONE "\r\nConnection: close\r\n\r\n";
char ALADHAN_REQ1[]       = "GET /timingsByCity?city=";
char ALADHAN_REQ2[]       = "&country=";
char ALADHAN_REQ3[]       = "&method=2";

//------VARIABLES-------
int  ssidAddr  = 0    , passwordAddr = 300 , cityAddr  = 600 , countryAddr = 900, min1 = 99, min2 = 99;
bool provision = false, provisioned = false, firstBoot = true, prayerTimesReady = false;
char city[32]         , country[32]        , hours[2]  = {'9','9'};
const char *currentTime = "99:99"          , *prayerTimes[5];
static char respBuf[4096];

//------ICONS------
static const uint8_t PROGMEM
  check_bmp[]    = { B00000011, B00000110, B11001100, B01111000, B00110000},
  fail_bmp[]     = { B11001100, B01111000, B00110000, B01111000, B11001100},
  wifi_bmp[]     = { B01111100, B00000000, B00111000, B00000000, B00010000},
  adhan_bmp[]    = { B11111110, B00000000, B11101010, B00101010, B11101010, B10101010, B11111111};
//  sun1_bmp[]     = { B00010001, B00001000, B00000011, B00010011, B00000011, B00001000, B00010001},
//  sun2_bmp[]     = { B00010000, B00100000, B10000000, B10010000, B10000000, B00100000, B00010000},
//  moon1_bmp[]    = { B00000111, B00001000, B00000000, B00000000, B00000000, B00001000, B00000111},
//  moon2_bmp[]    = { B11000000, B11100000, B01110000, B01110000, B01110000, B11100000, B11000000},
//  cloud1_bmp[]   = { B00001111, B00010000, B00100001, B00100001, B00100000, B00011111, B00000000},
//  cloud2_bmp[]   = { B00000000, B11110000, B10001000, B00000100, B00000100, B11111000, B00000000},
//  mist1_bmp[]    = { B00000000, B00001111, B00000000, B00011111, B00000000, B00111111, B00000000},
//  mist2_bmp[]    = { B00000000, B11111000, B00000000, B00011111, B00000000, B11100000, B00000000},
//  snow1_bmp[]    = { B00001010, B00000100, B00001010, B00000001, B00001010, B00000100, B00001010},
//  snow2_bmp[]    = { B01010000, B00100000, B01010000, B10000000, B01010000, B00100000, B01010000},
//  thunder1_bmp[] = { B00010000, B00110000, B01110000, B11111110, B00011100, B00011000, B00010000},
//  thunder2_bmp[] = { B00010000, B00110000, B01110000, B11111110, B00011100, B00011000, B00010000},
//  rain1_bmp[]    = { B00000000, B00001001, B00010010, B00100100, B01001001, B00000000, B00000000},
//  rain2_bmp[]    = { B00000000, B00100100, B01001000, B10010000, B00100000, B00000000, B00000000};
  
//------ANIMATIONS------
void check_anim() {
  matrix.clear();
  matrix.drawBitmap(4, 1, check_bmp, 8, 5, BRIGHTNESS);
  delay(300);
  matrix.clear();
  delay(300);
  matrix.drawBitmap(4, 1, check_bmp, 8, 5, BRIGHTNESS);
  delay(300);
}

void fail_anim() {
  matrix.clear();
  matrix.drawBitmap(4, 1, fail_bmp, 8, 5, BRIGHTNESS);
  delay(300);
  matrix.clear();
  delay(300);
  matrix.drawBitmap(4, 1, fail_bmp, 8, 5, BRIGHTNESS);
  delay(300);
  delay(DELAY_ERROR);
}

void wifi_anim() {
  matrix.clear();
  matrix.drawBitmap(4, 1, wifi_bmp, 8, 5, BRIGHTNESS);
  delay(300);
  matrix.clear();
  delay(300);
  matrix.drawBitmap(4, 1, wifi_bmp, 8, 5, BRIGHTNESS);
  delay(300);
}


//------DNS SERVER CONFIG------
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
const char *APssid = "ESP";
DNSServer dnsServer;
ESP8266WebServer webServer(80);

void setup() {
  
  delay(1000);
  Serial.begin(9600);
  EEPROM.begin(1024);
  matrix.begin();
  matrix.clear();
  matrix.setRotation(0);
  delay(500);
  setup_mode();
}

void loop() {

  
  //Show Time
  if (!(getData(TIME) && showTime(respBuf))) {
      fail_anim();
      setup_mode();
    }

  //Show Weather every 10 minutes
  if( min2 == 0 && getData(WEATHER))
      showWeather(respBuf);
          
  //Get prayer times at first boot and everyday at 02:00
  if (firstBoot || strcmp(currentTime, "02:00") == 0) {
    firstBoot = false;
    if (getData(ADHAN) && getPrayerTimes(respBuf))
          prayerTimesReady = true;
  }

  //Show Adhan on time
  for( int prayer = 0; prayer < 5; prayer++) {
    Serial.println(prayerTimes[prayer]);
    if (strcmp(currentTime, prayerTimes[prayer]) == 0)
          showAdhan(prayer);
  }
  
}

void setup_mode() {
  
  char ssid[32]      = "";
  char password[32]  = "";
  
  while (1) {

    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    
    EEPROM.get(ssidAddr, ssid);
    EEPROM.get(passwordAddr, password);
    EEPROM.get(cityAddr, city);
    EEPROM.get(countryAddr, country);
    WiFi.begin(ssid, password);
    
    matrix.clear();
    matrix.fillRect(1,2,3,3,BRIGHTNESS);
    
    int attempts = 20;
    while (WiFi.status() != WL_CONNECTED && attempts > 0) {
      matrix.fillRect(6,2,3,3,BRIGHTNESS * (bool)(attempts%3));
      matrix.fillRect(11,2,3,3,BRIGHTNESS * (bool)(!((attempts-1)%3)));
      delay(700);
      attempts--;
    }
   
    if   (WiFi.status() == WL_CONNECTED) { check_anim(); return; }
    else { provision = false; fail_anim(); if (!provisioned) ap_mode(); }
    
    }
}

void ap_mode () {

  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(APssid);
  wifi_anim();

  dnsServer.setTTL(5);
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
  dnsServer.start(DNS_PORT, "www.config.com", apIP);
  webServer.on("/",HTTP_GET, webpage);
  webServer.on("/",HTTP_POST,response);
  webServer.begin();

  while (!provision) {
    dnsServer.processNextRequest();
    webServer.handleClient(); 
  }
  
  WiFi.mode(WIFI_STA);
  return;
}

void webpage() {
  const char INDEX_HTML[] =
  "<!DOCTYPE HTML>"
  "<html>"
  "<head>"
  "<meta name = \"viewport\" content = \"width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0\">"
  "<title>Configuration</title>"
  "<style>"
  "\"body { background-color: #808080; font-family: \"Courier New\", Courier, monospace; Color: #000000; }\""
  "</style>"
  "</head>"
  "<body>"
  "<h3 style=\"text-align:center;\">Configuration</h3>"
  "<FORM action=\"/\" method=\"post\">"
  "<P>"
  "Wifissid : "
  "<INPUT type=\"text\" name=\"HTMLssid\"<br>"
  "</P>"
  "<P>"
  "Password : "
  "<INPUT type=\"text\" name=\"HTMLpassword\"<br>"
  "</P>"
  "<P>"
  "City : "
  "<INPUT type=\"text\" name=\"HTMLCity\"<br>"
  "</P>"
  "<P>"
  "Country : "
  "<INPUT type=\"text\" name=\"HTMLCountry\"<br>"
  "</P>"
  "<P>"
  "<INPUT type=\"submit\" value=\"Send\">"
  "</P>"
  "</FORM>"
  "</body>"
  "</html>";
  webServer.send(200, "text/html", INDEX_HTML);
}

void response(){

  if( webServer.hasArg("HTMLssid") && webServer.hasArg("HTMLCity") && webServer.hasArg("HTMLCountry")){
      
      char ssid[32]       = "";
      char password[32]   = "";
      char city[32]       = "";
      char country[32]    = "";
      
      webServer.arg("HTMLssid").toCharArray(ssid, 32);
      webServer.arg("HTMLpassword").toCharArray(password, 32);
      webServer.arg("HTMLCity").toCharArray(city, 32);
      webServer.arg("HTMLCountry").toCharArray(country, 32);
      
      EEPROM.put(ssidAddr, ssid);
      EEPROM.put(passwordAddr, password);
      EEPROM.put(cityAddr, city);
      EEPROM.put(countryAddr, country);
      EEPROM.commit();
      
      provision = true;
      
      webServer.send(200, "text/html", "<html><body><h3style=\"text-align:center;\">Configuration successful !</h3></body></html>");
    } else {
      webServer.send(400, "text/html", "<html><body><h3>HTTP Error 400</h3><p>Invalid SSID/Password ! </p><a href='/'>Try again</a></body></html>");
    }
}

bool getData (int data) {

  char *endPoint, *req1, *req2;
  
  if (data == TIME)    { endPoint = TIMEZONE; req1 = TIMEZONE_REQ1; req2 = TIMEZONE_REQ2; }
  if (data == WEATHER) { endPoint = APIXU   ; req1 = APIXU_REQ1   ; req2 = APIXU_REQ2;    }
  if (data == ADHAN)   { endPoint = TIMEZONE; req1 = ALADHAN_REQ1 ; req2 = ALADHAN_REQ2;  }
  
  provisioned = true;
  WiFiClient httpclient;
  const int httpPort = 80;
    
  if (!httpclient.connect(endPoint, httpPort)) {
    fail_anim();
    return false;
  }

  httpclient.print(req1);
  if (data == WEATHER || data == ADHAN) { httpclient.print(city); }
  httpclient.print(req2);
  if (data == ADHAN) { httpclient.print(city); httpclient.print(ALADHAN_REQ3); httpclient.print(TIMEZONE_REQ2);}
  httpclient.flush();

  // Collect http response headers and content
  // HTTP headers are discarded.
  // The content is formatted in and is left in respBuf.
  int respLen = 0;
  bool skip_headers = true;
  
  while (httpclient.connected() || httpclient.available()) {
    
    if (skip_headers) {
      String aLine = httpclient.readStringUntil('\n');
      // Blank line denotes end of headers
      if (aLine.length() <= 1) {
        skip_headers = false;
      }
    }
    
    else {
    
      int bytesIn;
      bytesIn = httpclient.read((uint8_t *)&respBuf[respLen], sizeof(respBuf) - respLen);
      
      if (bytesIn > 0) {
        respLen += bytesIn;
        if (respLen > sizeof(respBuf)) respLen = sizeof(respBuf);
      }
      else if (bytesIn < 0) fail_anim();
    }
    delay(10);
  }
  httpclient.stop();

  // Terminate the C string
  respBuf[respLen++] = '\0';
  
  return true;
}

bool showTime (char *json) {

  Serial.println("Time !");

  StaticJsonBuffer<3*1024> jsonBuffer;

  // Skip characters until first '{' found
  char *jsonstart = strchr(json, '{');
  if (jsonstart == NULL) return false;
  
  json = jsonstart;

  // Parse JSON
  JsonObject& root = jsonBuffer.parseObject(json);
  if (!root.success()) return false;

  // Extract time info from parsed JSON
  currentTime = root["data"];
  strncpy( hours, currentTime, 2);
  
  matrix.clear();
  matrix.setCursor(0,0); 
  matrix.setTextColor(BRIGHTNESS);
  matrix.println(hours);

  min1 = currentTime[3] - '0';
  if(min1)      matrix.drawFastHLine(12,0, min1 < 3 ? min1   : 3, BRIGHTNESS);
  if(min1 > 3)  matrix.drawFastHLine(12,1, min1 - 3, BRIGHTNESS);

  min2 = currentTime[4] - '0';
  if(min2)      matrix.drawFastHLine(12,4, min2 < 3 ? min2    : 3, BRIGHTNESS);
  if(min2 > 3)  matrix.drawFastHLine(12,5, min2 < 6 ? min2 -3 : 3, BRIGHTNESS);
  if(min2 > 6)  matrix.drawFastHLine(12,6, min2 - 3, BRIGHTNESS);

  delay(DELAY_TIME);
  
  return true;
}

bool showWeather(char *json) {

  Serial.println("Weather !");
  
  StaticJsonBuffer<3*1024> jsonBuffer;

  // Skip characters until first '{' found
  char *jsonstart = strchr(json, '{');
  if (jsonstart == NULL) return false;
  
  json = jsonstart;

  // Parse JSON
  JsonObject& root = jsonBuffer.parseObject(json);
  if (!root.success()) return false;

  // Extract weather info from parsed JSON
  JsonObject& forecast     = root["forecast"];
  JsonObject& forecastday  = forecast["forecastday"][0];
  JsonObject& dayWeather   = forecastday["day"];
  const int temp_c         = dayWeather["avgtemp_c"];
  JsonObject& condition    = dayWeather["condition"];
  const char *text         = condition["text"];

  Serial.println(temp_c);
  Serial.println(text);

  matrix.clear();
  matrix.setCursor(0,0); 
  matrix.setTextColor(BRIGHTNESS);

  if (temp_c < 10 && temp_c > -10)  { matrix.print("0");}
  matrix.print(abs(temp_c));
  matrix.drawRect(12,0,3,3,BRIGHTNESS);
  matrix.drawFastHLine(12, 5, 3, BRIGHTNESS);
  if (temp_c >= 0)  { matrix.drawFastVLine(13, 4, 3, BRIGHTNESS);}

  delay(DELAY_WEATHER/2);
  matrix.clear();

  matrix.setTextWrap(false);
  int scroll = -(strlen(text))*6;
  for ( int8_t repeat = 2; repeat > 0; repeat--) {
    for ( int8_t x=0; x >= scroll; x--) {
      matrix.clear();
      matrix.setCursor(x,0);
      matrix.print(text);
      delay(200);
    }
  }
  return true;
}

bool getPrayerTimes(char *json) {

  Serial.println("Prayer Times !");
  
  StaticJsonBuffer<3*1024> jsonBuffer;

  // Skip characters until first '{' found
  char *jsonstart = strchr(json, '{');
  if (jsonstart == NULL) return false;
  
  json = jsonstart;

  // Parse JSON
  JsonObject& root = jsonBuffer.parseObject(json);
  if (!root.success()) return false;

  // Extract weather info from parsed JSON
  JsonObject& data    = root["data"];
  JsonObject& timings = data["timings"];
  prayerTimes[0]      = timings["Fajr"];
  prayerTimes[1]      = timings["Dhuhr"];
  prayerTimes[2]      = timings["Asr"];
  prayerTimes[3]      = timings["Maghrib"];
  prayerTimes[4]      = timings["Isha"];

  return true;
}

void showAdhan(int prayer) {

  Serial.println("Adhan !");

  char* prayerInitials = "FDAMI";
  matrix.clear();
  matrix.setCursor(1,0); 
  matrix.setTextColor(BRIGHTNESS);
  matrix.println(prayerInitials[prayer]);
  matrix.drawBitmap(8, 0, adhan_bmp, 8, 7, BRIGHTNESS);
  delay(DELAY_ADHAN);
  return;
}



