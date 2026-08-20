# SmartMine Sentinel AI — IoT

Module IoT du projet **SmartMine Sentinel AI**.

L’ESP32 collecte les données des capteurs et les envoie au backend via **MQTT**.

## Capteurs

* **BME280** : température et humidité
* **MPU6050** : vibration
* **Capteur de gaz** : valeur analogique
* **Capteur de courant** : valeur analogique

## Architecture

```text
Capteurs ➔ ESP32 ➔ JSON ➔ MQTT ➔ Backend SmartMine

```

### Détails MQTT

* **Broker** : `broker.hivemq.com`
* **Port** : `1883`
* **Topic** : `smartmine/sensors`

### Exemple de données (JSON)

```json
{
  "device": "SmartMine-ESP32",
  "machine": {
    "temperature": 30.86,
    "vibration": 0.0235,
    "current_raw": 2816,
    "current_voltage": 2.269
  },
  "environment": {
    "temperature": 30.86,
    "humidity": 51.50,
    "gas": 210
  }
}

```

## Technologies

* **ESP32** (PlatformIO / Arduino C++)
* **Protocole & Format** : MQTT, ArduinoJson
* **Capteurs** : BME280, MPU6050

## Connexions

| Capteur / Composant | Pin Capteur | Pin ESP32 | Alimentation |
| --- | --- | --- | --- |
| **BME280 & MPU6050** | SDA<br>

<br>SCL | GPIO 21<br>

<br>GPIO 22 | 3.3V / GND |
| **Capteur de gaz** | OUT | GPIO 34 | - |
| **Capteur de courant** | OUT | GPIO 35 | - |

## Installation & Utilisation

1. Installer **VS Code** avec l'extension **PlatformIO**.
2. Installer les dépendances :
```bash
pio pkg install

```


3. Configurer le Wi-Fi dans `src/main.cpp` :
```cpp
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

```



### Commandes utiles

* **Compilation** : `pio run`
* **Téléversement** : `pio run -t upload`
* **Moniteur série** (115200 baud) : `pio device monitor`

## Structure du projet

```text
iot/
├── src/
│   └── main.cpp
├── platformio.ini
└── README.md

```

## État du projet

* [x] Configuration ESP32 & Wi-Fi
* [x] Capteur BME280 (Température & Humidité)
* [x] Capteur MPU6050 (Mesure des vibrations)
* [x] Capteur de gaz
* [x] Capteur de courant
* [x] Sérialisation JSON & Publication MQTT

## Flux de données

```text
[ BME280 ]  ──┐
[ MPU6050 ] ──┼──> [ ESP32 ] ──> [ JSON ] ──> [ MQTT ] ──> [ Backend ] ──> [ IA / DB / Dashboard / Digital Twin ]
[ Gaz ]     ──┤
[ Courant ] ──┘

```

```

```
