// Import potrebných knižníc
#include <WiFi.h>
#include <WiFiUdp.h>

// Definícia premenných
const char* def_ssid = "TELLO-C5B5E1"; // Názov Wi-Fi siete drona Tello
const char* def_pass = ""; // Heslo pre Wi-Fi sieť drona Tello

const char* udpAddress = "192.168.10.1";
const int udpPort = 8889;

boolean connected = false;
WiFiUDP udp;

void setup() {
  // Nastavenie komunikácie série
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  // Pripojenie k Wi-Fi sieti drona Tello
  connectToWiFi(def_ssid, def_pass);

  delay(5000);
}

void loop() {
  if (connected) {
    // Príkazy pre ovládanie drona Tello
    TelloCommand("takeoff"); // Vzlietnuť
    delay(5000);
    // Ďalšie príkazy pre pohyb drona

    // Pristátie
    TelloCommand("land");
    delay(5000);

    // Vypnutie drona
    TelloCommand("emergency");
    delay(1000);

    // Zastavenie programu
    while (true) {
      delay(1000);
    }
  }
}

// Funkcia pre odosielanie príkazov dronu Tello
void TelloCommand(const char *cmd) {
  if (connected) {
    udp.beginPacket(udpAddress, udpPort);
    udp.printf(cmd);
    udp.endPacket();
    Serial.printf("Odosielanie príkazu [%s] pre dron Tello\n", cmd);
  }
}

// Funkcia pre pripojenie k Wi-Fi sieti drona Tello
void connectToWiFi(const char * ssid, const char * pwd) {
  Serial.println("Pripájanie sa k Wi-Fi sieti: " + String(ssid));

  WiFi.disconnect(true);

  WiFi.onEvent(WiFiEvent);

  WiFi.begin(ssid, pwd);

  Serial.println("Čaká sa na pripojenie k Wi-Fi sieti ...");
}

// Funkcia pre obsluhu udalostí Wi-Fi
void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.print("Wi-Fi úspešne pripojené, IP adresa: ");
      Serial.println(WiFi.localIP());

      // Inicializácia komunikácie s dronom Tello
      udp.begin(WiFi.localIP(), udpPort);
      connected = true;

      // Vykonanie príkazu pre dron Tello
      TelloCommand("command");
      delay(2000);

      // Nastavenie rýchlosti drona
      TelloCommand("speed 50");
      delay(2000);

      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("Pripojenie k Wi-Fi sieti bolo prerušené");
      connected = false;
      break;
  }
}

