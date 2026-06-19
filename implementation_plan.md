# Implementation Plan: ESP32-S3 Smart Display (3.5") & QMI8658 IMU Project

Dieses Dokument beschreibt den Plan zur Initialisierung deines ESP-IDF-Projekts.

## Verzeichnisstruktur

Das Projekt wird wie folgt aufgebaut:

```text
VIEWESMART-QMI8658/
├── CMakeLists.txt              # Haupt-CMake-Datei für ESP-IDF
├── sdkconfig.defaults          # Standardeinstellungen (PSRAM, CPU Speed)
├── agents.md                   # Deine Skill-Beschreibung
├── implementation_plan.md      # Dieses Dokument zur Referenz
├── main/
│   ├── CMakeLists.txt          # CMake-Datei für die Main-Komponente
│   └── main.cpp                # Hauptprogramm (Initialisierung von Display und IMU)
└── components/
    ├── LovyanGFX/              # LovyanGFX Bibliothek (wird geklont)
    └── qmi8658/                # C++ Treiber für den QMI8658 Sensor
        ├── CMakeLists.txt
        ├── qmi8658.hpp
        └── qmi8658.cpp
```

## Komponenten-Details

1. **LovyanGFX**: Extrem performante Grafikbibliothek für ESP32. Wir konfigurieren sie in `main.cpp` mit folgenden Pins des **3.5" Displays (320x480)**:
   - MOSI: IO45
   - SCK: IO40
   - CS: IO42
   - DC: IO41
   - RST: IO39
   - Backlight: IO13
   - Touch I2C: SDA=IO1, SCL=IO3

2. **QMI8658**: 6-Achsen-IMU. Wir verbinden sie über dieselben I2C-Pins (SDA=IO1, SCL=IO3), da sie eine andere Adresse als der Touchscreen nutzt.
