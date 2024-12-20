#include <WiFi.h>
#include <WebServer.h>
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <time.h>
#include <vector>
#include <algorithm>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <Ticker.h>
#include <WiFiUdp.h>

// WiFi-Zugangsdaten
const char* ssid = "<Network_Name>";
const char* password = "<Password>";

// NTP-Einstellungen
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

// SD-Karten-Pins
#define SD_SCK 18
#define SD_MISO 19
#define SD_MOSI 23
#define SD_CS 5

// Webserver auf Port 80
WebServer server(80);

// Variablen für Temperatur und Luftfeuchtigkeit
float temperature = 0.0;
float humidity = 0.0;

//const char* fileName = "/data.txt";

std::vector<String> sensorList;

// Funktion zum Deaktivieren des SPI-Interfaces (Power Saving)
void disableSDPower() {
    SPI.end();  // Stoppt die SPI-Kommunikation
    pinMode(SD_CS, INPUT); // Deaktiviert den Chip Select Pin
    Serial.println("[INFO] SPI deaktiviert für SD-Karte.");
}

// Funktion zum Aktivieren des SPI-Interfaces (wenn SD benötigt wird)
void enableSDPower() {
    pinMode(SD_CS, OUTPUT); // Setzt den Chip Select Pin auf OUTPUT
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS); // Startet SPI für SD-Karte
    Serial.println("[INFO] SPI aktiviert für SD-Karte.");
}

// get the current timestamp
String getTimeStamp() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "Unbekannt";

    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buffer);
}

// generate a monthly file path
String getMonthlyFilePath(const String& sensorName) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "/unknown.txt";

    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m", &timeinfo); // YYYY-MM Format
    return "/" + sensorName + "/" + String(buffer) + ".txt";
}

// store data to file in monthly format
void saveDataToFile(const String& sensorName, float temperature, float humidity) {
    //enableSDPower();  // Aktivieren des SPI-Interfaces

    String filePath = getMonthlyFilePath(sensorName);
    String folderPath = "/" + sensorName;

    // Ordner prüfen/erstellen
    if (!SD.exists(folderPath.c_str())) {
        SD.mkdir(folderPath.c_str());
    }

    File file = SD.open(filePath, FILE_APPEND);
    if (file) {
        String timestamp = getTimeStamp();
        file.printf("%s, %.2f\u00b0C, %.2f%%\n",
                    timestamp.c_str(), temperature, humidity);
        file.close();
        Serial.printf("[OK] Daten gespeichert in %s\n", filePath.c_str());
    } else {
        Serial.printf("[FEHLER] Konnte Datei %s nicht öffnen!\n", filePath.c_str());
    }

    //disableSDPower();  // Deaktivieren des SPI-Interfaces
}

// update the avaible sensor list
void updateSensorList(const String& sensorName) {
    if (std::find(sensorList.begin(), sensorList.end(), sensorName) == sensorList.end()) {
        sensorList.push_back(sensorName);
    }
}

// shows the last data of the sensor
// webseite from where you can get all data of the sensor (last, month, all)
void handleSensorData() {
    if (!server.hasArg("name")) {
        server.send(400, "text/plain", "Sensorname fehlt");
        return;
    }

    // Aktuelles Datum ermitteln (Verwendung von getTimeStamp())
    String timestamp = getTimeStamp();  // Nutze die Funktion, um den aktuellen Zeitstempel zu bekommen
    
    // Lokale Zeit abfragen
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        server.send(400, "text/plain", "Zeit konnte nicht abgerufen werden");
        return;
    }

    int year = timeinfo.tm_year + 1900;  // Jahr ist von 1900 an
    int month = timeinfo.tm_mon + 1;     // Monat ist von 0 bis 11

    String sensorName = server.arg("name");
    String folderPath = "/" + sensorName;
    String filePath = folderPath + "/" + String(year) + "-" + String(month) + ".txt";

    String html = "<html><body style='background-color:yellow; color:black;'>";
    html += "<h1>Letzte gemessene Werte für Sensor: " + sensorName + "</h1>";

    File file = SD.open(filePath);
    if (file) {
        String lastLine = "";
        // Gehe durch alle Zeilen der Datei und lese die letzte
        while (file.available()) {
            lastLine = file.readStringUntil('\n'); // Lese Zeile für Zeile, speichere die letzte
        }
        file.close();

        if (lastLine.length() > 0) {
            int firstComma = lastLine.indexOf(',');
            int secondComma = lastLine.indexOf(',', firstComma + 1);

            String timestamp = lastLine.substring(0, firstComma);
            String temperatureValue = lastLine.substring(firstComma + 1, secondComma);
            String humidityValue = lastLine.substring(secondComma + 1);

            html += "<p>Zeitstempel: " + timestamp + "</p>";
            html += "<p>Temperatur: " + temperatureValue + " </p>";
            html += "<p>Luftfeuchtigkeit: " + humidityValue + "</p>";
        } else {
            html += "<p>[INFO] Keine Daten verfügbar!</p>";
        }
    } else {
        html += "<p>[FEHLER] Datei konnte nicht geöffnet werden!</p>";
    }

    html += "<button onclick=\"window.location.href='/sensorHistory?name=" + sensorName + "&type=D'\">Aktueller Tag</button><br><br>";
    html += "<button onclick=\"window.location.href='/sensorHistory?name=" + sensorName + "&type=M'\">Aktueller Monat</button><br><br>";
    html += "<button onclick=\"window.location.href='/sensorHistory?name=" + sensorName + "&type=A'\">Gesamte Historie</button>";


    html += "<br><br><button onclick=\"window.location.href='/'\">Zurück</button>";
    html += "</body></html>";

    server.send(200, "text/html; charset=UTF-8", html);
}

// shows the history of the sensor
// based on the type (day, month, all)
void handleSensorHistory() {
    if (!server.hasArg("name")) {
        server.send(400, "text/plain", "Sensorname oder Historientyp fehlt");
        return;
    }

    String sensorName = server.arg("name");
    String type = server.arg("type");

    // Lokale Zeit abfragen
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        server.send(400, "text/plain", "Zeit konnte nicht abgerufen werden");
        return;
    }

    int year = timeinfo.tm_year + 1900;  // Jahr ist von 1900 an
    int month = timeinfo.tm_mon + 1;     // Monat ist von 0 bis 11
    int day = timeinfo.tm_mday;          // Aktueller Tag

    // Ordnerpfad
    String folderPath = "/" + sensorName;
    String filePath;
    
    // Datei je nach Typ festlegen
    if (type == "D") {
        // Aktuellen Tag anzeigen
        filePath = folderPath + "/" + String(year) + "-" + (month < 10 ? "0" : "") + String(month) + ".txt";  // Tagesdatei
    } else if (type == "M") {
        // Aktuellen Monat anzeigen
        filePath = folderPath + "/" + String(year) + "-" + (month < 10 ? "0" : "") + String(month) + ".txt";  // Monatsdatei
    } else if (type == "A") {
        /// Gesamte Historie anzeigen
        // Keine einzelne Datei, sondern alle Dateien im Ordner durchgehen
    } else {
        server.send(400, "text/plain", "Ungültiger Historientyp");
        return;
    }

    String html = "<html><body style='background-color:yellow; color:black;'>";
    html += "<h1>Historie für Sensor: " + sensorName + " (" + type + ")</h1>";

    if (type == "A") {
        // Gesamte Historie anzeigen (alle .txt-Dateien im Ordner)
        File dir = SD.open(folderPath);
        if (!dir) {
            html += "<p>[FEHLER] Verzeichnis konnte nicht geöffnet werden!</p>";
            server.send(200, "text/html; charset=UTF-8", html);
            return;
        }

        String allData = "";
        while (File file = dir.openNextFile()) {
            if (!file.isDirectory() && String(file.name()).endsWith(".txt")) {
                String fileContent = "";
                file.seek(0);  // Datei an den Anfang setzen

                // Lese alle Zeilen der Datei
                while (file.available()) {
                    fileContent += file.readStringUntil('\n') + "\n";
                }

                // Füge die Datei zum Gesamtdaten-String hinzu
                allData = fileContent + allData;  // Die neueren Daten kommen zuerst
            }
            file.close();
        }
        dir.close();

        // Anzeige der zusammengeführten Daten
        if (allData.length() > 0) {
            html += "<table border='1'><tr><th>Zeitstempel</th><th>Temperatur (\u00b0C)</th><th>Luftfeuchtigkeit (%)</th></tr>";
            String currentLine = "";
            int firstComma = 0;
            int secondComma = 0;

            // Jede Zeile der gesamten Daten anzeigen
            while (allData.length() > 0) {
                firstComma = allData.indexOf(',');
                secondComma = allData.indexOf(',', firstComma + 1);

                if (firstComma == -1 || secondComma == -1) break;

                currentLine = allData.substring(0, allData.indexOf('\n'));
                allData = allData.substring(allData.indexOf('\n') + 1); // Entferne die gelesene Zeile

                String timestamp = currentLine.substring(0, firstComma);
                String temperatureValue = currentLine.substring(firstComma + 1, secondComma);
                String humidityValue = currentLine.substring(secondComma + 1);

                html += "<tr><td>" + timestamp + "</td><td>" + temperatureValue + "</td><td>" + humidityValue + "</td></tr>";
            }
        } else {
            html += "<p>[INFO] Keine Daten gefunden!</p>";
        }
    } 
    
    else if (type == "D") {
        // Nur die Daten des aktuellen Tages anzeigen
    File file = SD.open(filePath);
    if (file) {
        html += "<table border='1'><tr><th>Zeitstempel</th><th>Temperatur (\u00b0C)</th><th>Luftfeuchtigkeit (%)</th></tr>";
        String currentDate = String(year) + "-" + (month < 10 ? "0" : "") + String(month) + "-" + (day < 10 ? "0" : "") + String(day); // Aktuelles Datum

        while (file.available()) {
            String line = file.readStringUntil('\n');
            int firstComma = line.indexOf(',');
            int secondComma = line.indexOf(',', firstComma + 1);

            String timestamp = line.substring(0, firstComma);
            String temperatureValue = line.substring(firstComma + 1, secondComma);
            String humidityValue = line.substring(secondComma + 1);

            // Nur Zeilen mit dem aktuellen Datum anzeigen
            if (timestamp.startsWith(currentDate)) {
                html += "<tr><td>" + timestamp + "</td><td>" + temperatureValue + "</td><td>" + humidityValue + "</td></tr>";
            }
        }
        file.close();
    }
    }
    
    
    else {
        // Datei für den aktuellen Monat öffnen
        File file = SD.open(filePath);
        if (file) {
            html += "<table border='1'><tr><th>Zeitstempel</th><th>Temperatur (\u00b0C)</th><th>Luftfeuchtigkeit (%)</th></tr>";
            while (file.available()) {
                String line = file.readStringUntil('\n');
                int firstComma = line.indexOf(',');
                int secondComma = line.indexOf(',', firstComma + 1);

                String timestamp = line.substring(0, firstComma);
                String temperatureValue = line.substring(firstComma + 1, secondComma);
                String humidityValue = line.substring(secondComma + 1);

                html += "<tr><td>" + timestamp + "</td><td>" + temperatureValue + "</td><td>" + humidityValue + "</td></tr>";
            }
            file.close();
        } else {
            html += "<p>[FEHLER] Datei " + filePath + " konnte nicht geöffnet werden!</p>";
        }
    }

    html += "<br><button onclick=\"window.location.href='/sensor?name=" + sensorName + "'\">Zurück</button>";
    html += "</body></html>";

    server.send(200, "text/html; charset=UTF-8", html);
}

// HTTP-Handler: Root-Seite, where the user can choose a sensor
void handleRoot() {    
    String html = "<html><body style='background-color:pink; color:brown;'>";
    html += "<h1>Sensor-Auswahl</h1>";
    html += "<ul>";

    for (const String& sensorName : sensorList) {
        html += "<li><a href='/sensor?name=" + sensorName + "'>" + sensorName + "</a></li>";
    }

    html += "</ul>";
    html += "</body></html>";

    server.send(200, "text/html; charset=UTF-8", html);
}

// HTTP-Handler: Data receive and save
void handleReceiveData() {
    if (server.hasArg("name") && server.hasArg("temperature") && server.hasArg("humidity")) {
        String sensorName = server.arg("name");
        temperature = server.arg("temperature").toFloat();
        humidity = server.arg("humidity").toFloat();

        Serial.printf("temperature: %.2f, humidity: %.2f\n", temperature, humidity);

        updateSensorList(sensorName);

        saveDataToFile(sensorName, temperature, humidity);

        server.send(200, "text/plain", "Daten gespeichert");
    } else {
        server.send(400, "text/plain", "Fehlende Parameter");
    }
}

/*// HTTP-Handler: Historische Daten anzeigen
void handleStoreData() {
    File file = SD.open(fileName);
    String html = "<html><head><meta charset=\"UTF-8\"></head><body style='background-color:yellow; color:black;'> ";
    html += "<h1>Historische Sensordaten</h1>";

    if (file) {
        html += "<table border='1' style='width:100%;'><tr><th>Zeitstempel</th><th>Temperatur (\u00b0C)</th><th>Luftfeuchtigkeit (%)</th></tr>";

        String line;
        while (file.available()) {
            line = file.readStringUntil('\n');
            if (line.length() > 0) {
                int firstComma = line.indexOf(',');
                int secondComma = line.indexOf(',', firstComma + 1);

                String timestamp = line.substring(0, firstComma);
                String temperatureValue = line.substring(firstComma + 1, secondComma);
                String humidityValue = line.substring(secondComma + 1);

                html += "<tr><td>" + timestamp + "</td><td>" + temperatureValue + "</td><td>" + humidityValue + "</td></tr>";
            }
        }
        html += "</table>";
        file.close();
    } else {
        html += "<p>[FEHLER] Datei konnte nicht geöffnet werden!</p>";
    }

    html += "<br><button onclick=\"window.location.href='/'\">Zur\u00fcck zur Hauptseite</button>";
    html += "</body></html>";

    server.send(200, "text/html; charset=UTF-8", html);
}*/

void setup() {
    Serial.begin(115200);

    setCpuFrequencyMhz(80); // Setzt die Taktfrequenz auf 80 MHz statt 160 MHz

    WiFi.begin(ssid, password);
    Serial.print("Verbindungsaufbau mit WiFi...");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nVerbunden!");
    Serial.print("IP-Adresse: ");
    Serial.println(WiFi.localIP());

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // Zuerst SPI für SD-Karte aktivieren
    //enableSDPower();

    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    if (!SD.begin(SD_CS, SPI)) {
        Serial.println("[FEHLER] SD-Karte konnte nicht initialisiert werden!");
        return;
    }
    Serial.println("[OK] SD-Karte erfolgreich initialisiert.");
        
    // API-Endpunkte definieren
    server.on("/", HTTP_GET, handleRoot);
    server.on("/receiveData", HTTP_GET, handleReceiveData);
    //server.on("/storeData", HTTP_GET, handleStoreData);
    server.on("/sensor", HTTP_GET, handleSensorData);
    server.on("/sensorHistory", HTTP_GET, handleSensorHistory);

    server.begin();
    Serial.println("Webserver gestartet!");
}

void loop() {
    // Webserver-Anfragen bearbeiten
    server.handleClient();

    delay(100);
}
