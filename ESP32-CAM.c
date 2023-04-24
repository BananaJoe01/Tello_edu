#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiUdp.h>

#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"

extern TaskHandle_t the_camera_loop_task;
extern void the_camera_loop (void* pvParameter);
esp_err_t init_sdcard();

void startCameraServer();

const char* def_ssid = "TELLO-C5B5E1"; // Tello SSID
const char* def_pass = ""; // Tello Password

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
  // config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
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
    Serial.printf("Camera init failed with error 0x%x", err);
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
    Serial.printf("SD Card init failed with error 0x%x", card_err);
  }

  // start_wifi();

  connectToWiFi(def_ssid, def_pass);

  startCameraServer();

  xTaskCreatePinnedToCore(the_camera_loop, "the_camera_loop", 5000, NULL, 5, &the_camera_loop_task, 1);

  delay(100);
}

bool takeOff = false;

void loop() {

  if (takeOff) {
    // Up 60 cm
    TelloCommand("up 60");
    delay(3000);
    // Down 50 cm
    TelloCommand("down 50");
    delay(3000);  
    // Left 30 cm
    TelloCommand("left 30");
    delay(3000);
    // Right 30 cm
    TelloCommand("right 30");
    delay(3000);
    // Turn CW 360 degrees
    TelloCommand("cw 360");
    delay(3000);
    // Turn ccw 360 degrees
    TelloCommand("ccw 360");
    delay(3000);
    // Flip back    
    TelloCommand("flip b");
    delay(3000);
    // Flip front
    TelloCommand("flip f");
    delay(3000);
    // Land
    TelloCommand("land");
    delay(5000);
    takeOff = false;
  }

  delay(1000);
}

void TelloCommand(char *cmd) {
  if (connected) {
    udp.beginPacket(udpAddress, udpPort);
    udp.printf(cmd);
    udp.endPacket();
    Serial.printf("Send [%s] to Tello.\n", cmd);
  }
}

void connectToWiFi(const char * ssid, const char * pwd) {
  Serial.println("Connecting to WiFi network: " + String(ssid));

  WiFi.disconnect(true);

  WiFi.onEvent(WiFiEvent);

  WiFi.begin(ssid, pwd);

  Serial.println("Waiting for WiFi connection ...");
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_STA_GOT_IP:

      Serial.print("WiFi connected! IP address: ");
      Serial.println(WiFi.localIP());

      udp.begin(WiFi.localIP(), udpPort);
      connected = true;

      TelloCommand("command");
      delay(2000);

      TelloCommand("speed 50");
      delay(2000);

      TelloCommand("takeoff");
      delay(3000);

      takeOff = true;

      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      connected = false;

      break;
  }
}
