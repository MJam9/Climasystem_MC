# Climasystem_MC
A distributed climate monitoring system based on ESP32. It tracks temperature and humidity across multiple rooms, logs data locally to an SD card, and provides a user-friendly web interface.

## Project Overview
The system consists of two main components:
1.  **Sensor Nodes:** A microcontrollers (ESP32 or D1 Mini) equipped with DHT20 sensors. They measure the climate and send data via WiFi before entering Deep Sleep to conserve energy.
2.  **Base Station (Web Server):** A central ESP32 that receives data, saves it in monthly files on an SD card, and hosts a web interface for data visualization.

<img width="431" height="215" alt="WebListe" src="https://github.com/user-attachments/assets/ff5e219e-f9e1-416a-a7b0-5c411607d112" />

## Features
- **Multi-Room Monitoring:** Support for multiple sensors (e.g., "Livingroom", "Keller").
- **Local Data Logging:** Persistent storage on SD card in a structured directory format (`/SensorName/YYYY-MM.txt`).
- **Power Efficiency:** Sensor nodes utilize Deep Sleep (1-hour intervals).
- **Web Dashboard:**
    - **Root Menu:** Quick selection of all active sensors.
    - **Live View:** Displays the most recent measurement and timestamp.
    - **History Tracking:** View data filtered by Day, Month, or the entire History.
    - **Analytics:** Calculation of daily average values.
- **Standalone Operation:** Works entirely in your local network without requiring cloud services.

<img width="540" height="238" alt="WebSensor" src="https://github.com/user-attachments/assets/aca2012a-7684-438a-9ab8-54fc72ae12d8" />


### Web Interface Structure
- **Sensor Selection:** The entry point listing all detected rooms.
- **Room Detail View:** Shows current climate and navigation to history/averages.
- **History View:** Tabular display of recorded data points.

## Technical Components
### Hardware
- **Base Station:**
    - ESP32 Development Board
    - MicroSD Card Module (connected via SPI: SCK 18, MISO 19, MOSI 23, CS 5)
- **Sensor Nodes:**
    - ESP32 or Wemos D1 Mini
    - DFRobot DHT20 Sensor (I2C)

### Software & Libraries
- **Framework:** Arduino / PlatformIO
- **Key Libraries:**
    - `DFRobot_DHT20` (Sensor communication)
    - `WiFi` & `WebServer` (Network handling)
    - `SD` & `SPI` (File system)
    - `Time` (NTP synchronization for accurate timestamps)

## Why this project?
This system was originally developed to monitor cellar temperatures, which reached critical levels during certain times of the year. To prevent moisture issues or overheating, a long-term monitoring solution was needed. 

The project focuses on accessibility: since the end-users are not microcontroller experts, the web interface provides a simple way to check data from any browser without needing to touch the hardware.

## Getting Started
1.  **Configure WiFi:** Update the `ssid` and `password` in the `main.cpp` files.
2.  **Set Server IP:** Ensure the Sensor Nodes point to the static IP of your Base Station.
3.  **Hardware Setup:** Connect the DHT20 via I2C and the SD module via SPI as defined in the source code.
4.  **Flash:** Upload the `WebServer_MC` code to the base station and the `Sensor_MC` code to your nodes.

## Documentation
For more detailed information, please refer to:
- User Documentation ([UserDoku_MC_Climasystem.pdf](https://github.com/user-attachments/files/28749686/UserDoku_MC_Climasystem.pdf))
- Technical Documentation ([TecDoku_MC_Climasystem.pdf](https://github.com/user-attachments/files/28749680/TecDoku_MC_Climasystem.pdf))

