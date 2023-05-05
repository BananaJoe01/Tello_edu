#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <SD_MMC.h>

#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"

extern TaskHandle_t the_camera_loop_task;
extern void the_camera_loop (void* pvParameter);
esp_err_t init_sdcard();

void startCameraServer();

const char* def_ssid = "TELLO-C5B5E1"; // Tello/AccessPoint SSID (pomenovanie Wi-Fi siete)
const char* def_pass = ""; // Tello/AccessPoint Password (heslo pre WiFi sieť)

const char * udpAddress = "192.168.10.1";
const int udpPort = 8889;

boolean connected = false;
WiFiUDP udp;

#include <SD_MMC.h>

/*
void start_wifi() {

  String junk;

  String cssid;
  String cssid2;
  String cpass;
  char ssidch[32];
  char ssidch2[32];
  char passch[64];

  File config_file = SD_MMC.open("/secret.txt", "r");
  if (config_file) {

    Serial.println("Reading secret.txt");
    cssid = config_file.readStringUntil(' ');
    cssid2 = config_file.readStringUntil(' ');
    junk = config_file.readStringUntil('\n');
    cpass = config_file.readStringUntil(' ');
    junk = config_file.readStringUntil('\n');
    config_file.close();

    cssid.toCharArray(ssidch, cssid.length() + 1);
    cssid2.toCharArray(ssidch2, cssid2.length() + 1);
    cpass.toCharArray(passch, cpass.length() + 1);
    
    if (String(cssid) == "ap" || String(cssid) == "AP") {
      WiFi.softAP(ssidch2, passch);

      IPAddress IP = WiFi.softAPIP();
      Serial.println("Using AP mode: ");
      Serial.print(ssidch2); Serial.print(" / "); Serial.println(passch);
      Serial.print("AP IP address: ");
      Serial.println(IP);
    } else {
      Serial.println(ssidch);
      Serial.println(passch);
      WiFi.begin(ssidch, passch);
      WiFi.setSleep(false);

      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("");
      Serial.println("Using Station mode: ");
      Serial.print(cssid); Serial.print(" / "); Serial.println(cpass);
      Serial.println("WiFi connected");

      Serial.print("Camera Ready! Use 'http://");
      Serial.print(WiFi.localIP());
      Serial.println("' to connect");
    }
  } else {
    Serial.println("Failed to open config.txt - writing a default");

    File new_simple = SD_MMC.open("/secret.txt", "w");
    new_simple.println("ap ESP32-CAM // your ssid - ap mean access point mode, put Router123 station mode");
    new_simple.println("123456789    // your ssid password");
    new_simple.close();

    WiFi.softAP(def_ssid, def_pass);

    IPAddress IP = WiFi.softAPIP();
    Serial.println("Using AP mode: ");
    Serial.print(def_ssid); Serial.print(" / "); Serial.println(def_pass);
    Serial.print("AP IP address: ");
    Serial.println(IP);

  }
}
*/

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.pixel_format = PIXFORMAT_RGB565; // pre detekciu/rozpoznávanie tváre
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 3;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Inicializácia kamery zlyhala s chybou 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();

  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }

  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

  esp_err_t  card_err = init_sdcard();
  if (card_err != ESP_OK) {
    Serial.printf("Inicializácia SD karty zlyhala s chybou 0x%x", card_err);
  }

  // start_wifi();

  connectToWiFi(def_ssid, def_pass);

  startCameraServer();

  xTaskCreatePinnedToCore(the_camera_loop, "the_camera_loop", 5000, NULL, 5, &the_camera_loop_task, 1);

  delay(100);
}

bool takeOff = false;

void loop() {

  if (takeOff) { // Ak dron vzlietol, potom nech sa vykonajú príkazy:
    // Stúpnuť hore o 60 cm
    TelloCommand("up 60");
    delay(3000);
    // Klesnúť dole o 50 cm
    TelloCommand("down 50");
    delay(3000);  
    // Posunúť vľavo o 30 cm
    TelloCommand("left 30");
    delay(3000);
    // Posunúť vpravo o 30 cm
    TelloCommand("right 30");
    delay(3000);
    // Otočiť v smere hodinových ručičiek o 360 stupňov
    TelloCommand("cw 360");
    delay(3000);
    // Otočiť proti smeru hodinových ručičiek o 360 stupňov
    TelloCommand("ccw 360");
    delay(3000);
    // Prevrátiť smerom dozadu
    TelloCommand("flip b");
    delay(3000);
    // Prevrátiť smerom dopredu
    TelloCommand("flip f");
    delay(3000);

    // Vytvoriť snímku a uložiť na SD kartu
    takePictureAndSaveToSD();
    
    // Pristáť
    TelloCommand("land");
    delay(5000);
    takeOff = false; // Po pristatí už nevzlietať
  }

  delay(1000);
}

void takePictureAndSaveToSD() {
  camera_fb_t* fb = NULL; // Obrázok zo snímača kamery
  File file; // Súbor na ukladanie obrázka
  
  // Vytvorenie súboru pre snímku
  file = SD_MMC.open("/picture.jpg", FILE_WRITE);
  
  // Ak sa podarilo otvoriť súbor, požiada sa dron o snímku a tá sa uloží na SD kartu
  if (file) {
    // Požiadať drona o snímku
    TelloCommand("takeoff");
    TelloCommand("streamon");
    TelloCommand("rc 0 0 0 20");
    delay(2000);
    TelloCommand("rc 0 0 0 0");
    delay(2000);
    TelloCommand("streamoff");
    TelloCommand("land");

    // Otvoriť kameru na module ESP32-CAM
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
      Serial.printf("Chyba pri inicializácii kamery: %d", err);
      return;
    }

    // Získanie snímky z kamery
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Chyba pri získaní snímky z kamery");
      return;
    }

    // Zapísanie snímky do súboru
    file.write(fb->buf, fb->len);

    // Uvoľnenie pamäte alokovanej pre snímku
    esp_camera_fb_return(fb);

    // Uzavretie súboru
    file.close();

    Serial.println("Snímka bola úspešne uložená na SD kartu");
  } else {
    Serial.println("Nepodarilo sa vytvoriť súbor pre snímku");
  }
}

void TelloCommand(const char* cmd) {
  if (connected) {
    udp.beginPacket(udpAddress, udpPort);
    udp.printf(cmd);
    udp.endPacket();
    Serial.printf("Odosiela sa príkaz [%s] pre dron Tello.\n", cmd);

    if (strcmp(cmd, "takeoff") == 0) { // Ak dron vzlietol, začni snímať
      takeOff = true;
    }
    if (strcmp(cmd, "land") == 0) { // Ak dron pristál, prestaň snímať
      takeOff = false;
    }
    if (takeOff && strcmp(cmd, "streamon") == 0) { // Ak dron sníma, ukladaj snímky
      captureImage();
    }
  }
}

void captureImage() {
  String filename = "/image" + String(imageCount) + ".jpg"; // Vytvor názov súboru
  camera.capture(filename.c_str()); // Sprav snímku a ulož ju na SD kartu

  Serial.println("Snímka bola uložená na SD kartu: " + filename);
  imageCount++;
}

void connectToWiFi(const char * ssid, const char * pwd) {
  Serial.println("Pripájanie sa k WiFi sieti: " + String(ssid));

  WiFi.disconnect(true);

  WiFi.onEvent(WiFiEvent);

  WiFi.begin(ssid, pwd);

  Serial.println("Čaká sa na WiFi pripojenie ...");
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_STA_GOT_IP:

      Serial.print("WiFi bolo úspešne pripojené. IP adresa: ");
      Serial.println(WiFi.localIP());

      udp.begin(WiFi.localIP(), udpPort);
      connected = true;

      TelloCommand("command");  // Vykoná sa príkaz pre dron
      delay(2000);

      TelloCommand("speed 50"); // Rýchlosť dronu 50
      delay(2000);

      TelloCommand("takeoff");  // Vzlietnúť 
      delay(3000);

      takeOff = true;           // Po vzletnutí nepristávať

      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("Strata WiFi pripojenia.");
      connected = false;

      break;
  }
}
