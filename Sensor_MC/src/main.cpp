#include <WiFi.h>
#include <Arduino.h>
#include <DFRobot_DHT20.h>
#include <time.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <WiFiUdp.h>

#define DEEP_SLEEP_DURATION 3600000000  // 1 Stunde in Millisekunden

const char* ssid = "<Network_Name>";
const char* password = "<Password>";
const char* serverIP = "192.168.<IP-Adresse>";  
const int serverPort = 80;  // Port des Webservers

DFRobot_DHT20 dht20;

// meassure sensor DHT20
void getSensorData(float &temperature, float &humidity) {
  temperature = dht20.getTemperature();
  humidity = dht20.getHumidity();
  
  if (temperature < -40 || temperature > 80 || humidity < 0 || humidity > 100) {
    Serial.println("Fehlerhafte Messwerte! Bitte Sensor überprüfen.");
    temperature = NAN;
    humidity = NAN;
  }
}

// Send sensor data to Webserver
void sendSensorDataToMC1(float temperature, float humidity) {
  Serial.println("Daten an den WebServer_MC senden...");
  Serial.printf("Temperatur: %.2f°C, Luftfeuchtigkeit: %.2f%%\n", temperature, humidity);
  const char* sensorName = "<Sensor_Name>";  // Sensorname
  WiFiClient client;
  if (client.connect(serverIP, serverPort)) {
    String data = "name=" + String(sensorName) + "&temperature=" + String(temperature) + "&humidity=" + String(humidity);
    client.println("GET /receiveData?" + data + " HTTP/1.1");
    client.println("Host: " + String(serverIP));
    client.println("Connection: close");
    client.println();
    client.stop();
  }
}

// Go to deep sleep (wake-up after 1 hour)
void enterDeepSleep() {
  Serial.println("Gehe in Deep Sleep-Modus.");
  delay(2000);  // Kurz warten, um sicherzustellen, dass Nachrichten ausgegeben werden

  // Setze den Timer für Light Sleep
  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_DURATION);

  esp_deep_sleep_start();  // Wechsel in Deep Sleep
}

void setup() {
  Serial.begin(115200);

  // Verbindung zum WLAN herstellen
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nVerbunden mit WLAN");
  
  // Sensor-Initialisierung
  Wire.begin(21, 22);
  Wire.beginTransmission(0x38);
  byte error = Wire.endTransmission();
  if (error == 0) {
    Serial.println("DHT20-Sensor ist erreichbar!");
  } else {
    Serial.println("Fehler beim Erreichen des DHT20-Sensors!");
  }

  // Sensorwerte abrufen
  float temperature, humidity;
  getSensorData(temperature, humidity);
  
  // Daten an den WebServer_MC senden
  sendSensorDataToMC1(temperature, humidity);
  
  // In den Deep-Sleep-Modus wechseln
  enterDeepSleep();
}

void loop() {
  // Leere loop(), da der Sensor nur einmal pro Zyklus arbeitet
}