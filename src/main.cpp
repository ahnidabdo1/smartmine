#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include <Adafruit_BME280.h>

// ============================================================
//                  WIFI CONFIGURATION
// ============================================================

const char* WIFI_SSID = "ahnid";
const char* WIFI_PASSWORD = "2000@2026";

// ============================================================
//                  MQTT CONFIGURATION
// ============================================================

const char* MQTT_BROKER = "broker.hivemq.com";
const int MQTT_PORT = 1883;

const char* MQTT_TOPIC = "smartmine/sensors";

const char* DEVICE_NAME = "SmartMine-ESP32";

// ============================================================
//                  ESP32 PINS
// ============================================================

// I2C
#define SDA_PIN 21
#define SCL_PIN 22

// Analog sensors
#define GAS_PIN 34
#define CURRENT_PIN 35

// ============================================================
//                  I2C SENSORS
// ============================================================

Adafruit_BME280 bme;

bool bmeOK = false;
bool mpuOK = false;

uint8_t MPU_ADDRESS = 0x68;

// ============================================================
//                  NETWORK
// ============================================================

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ============================================================
//                  MPU6050 REGISTERS
// ============================================================

#define MPU_PWR_MGMT_1   0x6B
#define MPU_ACCEL_CONFIG 0x1C
#define MPU_WHO_AM_I     0x75
#define MPU_ACCEL_XOUT_H 0x3B

// ============================================================
//                  VIBRATION FILTER
// ============================================================

float baselineX = 0.0;
float baselineY = 0.0;
float baselineZ = 1.0;

bool vibrationInitialized = false;

// ============================================================
//                  MPU WRITE
// ============================================================

void writeMPURegister(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(MPU_ADDRESS);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

// ============================================================
//                  MPU READ REGISTER
// ============================================================

uint8_t readMPURegister(uint8_t reg)
{
    Wire.beginTransmission(MPU_ADDRESS);
    Wire.write(reg);
    Wire.endTransmission(false);

    Wire.requestFrom(MPU_ADDRESS, (uint8_t)1);

    if (Wire.available())
    {
        return Wire.read();
    }

    return 0;
}

// ============================================================
//                  FIND MPU6050
// ============================================================

bool findMPU()
{
    uint8_t addresses[] = {0x68, 0x69};

    for (uint8_t address : addresses)
    {
        Wire.beginTransmission(address);

        if (Wire.endTransmission() == 0)
        {
            MPU_ADDRESS = address;

            Serial.print("MPU6050 found at 0x");
            Serial.println(MPU_ADDRESS, HEX);

            return true;
        }
    }

    return false;
}

// ============================================================
//                  INITIALIZE MPU6050
// ============================================================

bool initializeMPU()
{
    if (!findMPU())
    {
        Serial.println("MPU6050 NOT FOUND");

        return false;
    }

    // Wake up MPU6050
    writeMPURegister(
        MPU_PWR_MGMT_1,
        0x00
    );

    delay(100);

    // Accelerometer ±2g
    writeMPURegister(
        MPU_ACCEL_CONFIG,
        0x00
    );

    delay(100);

    uint8_t whoAmI =
        readMPURegister(MPU_WHO_AM_I);

    Serial.print("MPU6050 WHO_AM_I = 0x");
    Serial.println(whoAmI, HEX);

    Serial.println("MPU6050 initialized.");

    return true;
}

// ============================================================
//                  READ MPU6050
// ============================================================

bool readMPU(
    int16_t &ax,
    int16_t &ay,
    int16_t &az,
    int16_t &gx,
    int16_t &gy,
    int16_t &gz
)
{
    Wire.beginTransmission(MPU_ADDRESS);

    Wire.write(MPU_ACCEL_XOUT_H);

    if (Wire.endTransmission(false) != 0)
    {
        return false;
    }

    uint8_t received =
        Wire.requestFrom(
            MPU_ADDRESS,
            (uint8_t)14
        );

    if (received != 14)
    {
        return false;
    }

    ax =
        (Wire.read() << 8) |
        Wire.read();

    ay =
        (Wire.read() << 8) |
        Wire.read();

    az =
        (Wire.read() << 8) |
        Wire.read();

    // Skip temperature
    Wire.read();
    Wire.read();

    gx =
        (Wire.read() << 8) |
        Wire.read();

    gy =
        (Wire.read() << 8) |
        Wire.read();

    gz =
        (Wire.read() << 8) |
        Wire.read();

    return true;
}

// ============================================================
//                  READ VIBRATION
// ============================================================

float readVibration()
{
    if (!mpuOK)
    {
        return 0.0;
    }

    int16_t axRaw;
    int16_t ayRaw;
    int16_t azRaw;

    int16_t gxRaw;
    int16_t gyRaw;
    int16_t gzRaw;

    if (!readMPU(
            axRaw,
            ayRaw,
            azRaw,
            gxRaw,
            gyRaw,
            gzRaw))
    {
        return 0.0;
    }

    // ±2g = 16384 LSB/g
    float ax =
        axRaw / 16384.0;

    float ay =
        ayRaw / 16384.0;

    float az =
        azRaw / 16384.0;

    // --------------------------------------------------------
    // Initialize baseline
    // --------------------------------------------------------

    if (!vibrationInitialized)
    {
        baselineX = ax;
        baselineY = ay;
        baselineZ = az;

        vibrationInitialized = true;

        return 0.0;
    }

    // --------------------------------------------------------
    // Low-pass baseline
    // --------------------------------------------------------

    const float alpha = 0.02;

    baselineX =
        baselineX +
        alpha * (ax - baselineX);

    baselineY =
        baselineY +
        alpha * (ay - baselineY);

    baselineZ =
        baselineZ +
        alpha * (az - baselineZ);

    // --------------------------------------------------------
    // Dynamic acceleration
    // --------------------------------------------------------

    float vibrationX =
        ax - baselineX;

    float vibrationY =
        ay - baselineY;

    float vibrationZ =
        az - baselineZ;

    // --------------------------------------------------------
    // Vibration magnitude
    // --------------------------------------------------------

    float vibration =
        sqrt(
            vibrationX * vibrationX +
            vibrationY * vibrationY +
            vibrationZ * vibrationZ
        );

    return vibration;
}

// ============================================================
//                  READ GAS
// ============================================================

int readGas()
{
    return analogRead(GAS_PIN);
}

// ============================================================
//                  READ CURRENT SENSOR
// ============================================================

int readCurrentRaw()
{
    return analogRead(CURRENT_PIN);
}

// ============================================================
//                  CURRENT VOLTAGE
// ============================================================

float readCurrentVoltage()
{
    int raw =
        readCurrentRaw();

    return
        (raw / 4095.0) * 3.3;
}

// ============================================================
//                  CONNECT WIFI
// ============================================================

void connectWiFi()
{
    Serial.println();
    Serial.println("Connecting to WiFi...");

    WiFi.mode(WIFI_STA);

    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );

    int attempts = 0;

    while (
        WiFi.status() != WL_CONNECTED &&
        attempts < 30
    )
    {
        delay(500);

        Serial.print(".");

        attempts++;
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("WiFi connected!");

        Serial.print("IP address: ");
        Serial.println(
            WiFi.localIP()
        );
    }
    else
    {
        Serial.println(
            "WiFi connection failed."
        );
    }
}

// ============================================================
//                  CONNECT MQTT
// ============================================================

void connectMQTT()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    while (!mqttClient.connected())
    {
        Serial.println(
            "Connecting to MQTT broker..."
        );

        String clientId =
            String(DEVICE_NAME) +
            "-" +
            String(random(0xffff), HEX);

        if (
            mqttClient.connect(
                clientId.c_str()
            )
        )
        {
            Serial.println(
                "MQTT connected!"
            );
        }
        else
        {
            Serial.print(
                "MQTT connection failed. State = "
            );

            Serial.println(
                mqttClient.state()
            );

            delay(3000);
        }
    }
}

// ============================================================
//                  READ ALL SENSORS
// ============================================================

void readSensors(
    float &temperature,
    float &humidity,
    float &vibration,
    int &gas,
    int &currentRaw,
    float &currentVoltage
)
{
    // BME280
    if (bmeOK)
    {
        temperature =
            bme.readTemperature();

        humidity =
            bme.readHumidity();
    }
    else
    {
        temperature = 0.0;
        humidity = 0.0;
    }

    // MPU6050
    vibration =
        readVibration();

    // Gas
    gas =
        readGas();

    // Current
    currentRaw =
        readCurrentRaw();

    currentVoltage =
        (currentRaw / 4095.0) * 3.3;
}

// ============================================================
//                  CREATE MQTT JSON
// ============================================================

String createJSON(
    float temperature,
    float humidity,
    float vibration,
    int gas,
    int currentRaw,
    float currentVoltage
)
{
    JsonDocument doc;

    doc["device"] =
        DEVICE_NAME;

    doc["timestamp"] =
        millis();

    JsonObject machine =
        doc["machine"].to<JsonObject>();

    machine["temperature"] =
        temperature;

    machine["vibration"] =
        vibration;

    machine["current_raw"] =
        currentRaw;

    machine["current_voltage"] =
        currentVoltage;

    JsonObject environment =
        doc["environment"].to<JsonObject>();

    environment["temperature"] =
        temperature;

    environment["humidity"] =
        humidity;

    environment["gas"] =
        gas;

    String output;

    serializeJson(
        doc,
        output
    );

    return output;
}

// ============================================================
//                  PRINT DATA
// ============================================================

void printSensorData(
    float temperature,
    float humidity,
    float vibration,
    int gas,
    int currentRaw,
    float currentVoltage
)
{
    Serial.println();
    Serial.println("--------------------------------");

    Serial.print(
        "Temperature : "
    );

    Serial.print(
        temperature,
        2
    );

    Serial.println(" °C");

    Serial.print(
        "Humidity    : "
    );

    Serial.print(
        humidity,
        2
    );

    Serial.println(" %");

    Serial.print(
        "Vibration   : "
    );

    Serial.print(
        vibration,
        4
    );

    Serial.println(" g");

    Serial.print(
        "Gas RAW     : "
    );

    Serial.println(
        gas
    );

    Serial.print(
        "Current RAW : "
    );

    Serial.println(
        currentRaw
    );

    Serial.print(
        "Current ADC : "
    );

    Serial.print(
        currentVoltage,
        3
    );

    Serial.println(" V");

    Serial.println("--------------------------------");
}

// ============================================================
//                  SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);

    delay(1500);

    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        "       SMARTMINE SENTINEL AI"
    );

    Serial.println(
        "       ESP32 SENSOR SYSTEM"
    );

    Serial.println(
        "========================================"
    );

    // --------------------------------------------------------
    // I2C
    // --------------------------------------------------------

    Wire.begin(
        SDA_PIN,
        SCL_PIN
    );

    Wire.setClock(400000);

    // --------------------------------------------------------
    // BME280
    // --------------------------------------------------------

    if (bme.begin(0x76))
    {
        bmeOK = true;

        Serial.println(
            "BME280 detected at 0x76"
        );
    }
    else if (bme.begin(0x77))
    {
        bmeOK = true;

        Serial.println(
            "BME280 detected at 0x77"
        );
    }
    else
    {
        Serial.println(
            "BME280 NOT FOUND"
        );
    }

    // --------------------------------------------------------
    // MPU6050
    // --------------------------------------------------------

    mpuOK =
        initializeMPU();

    // --------------------------------------------------------
    // ANALOG SENSORS
    // --------------------------------------------------------

    pinMode(
        GAS_PIN,
        INPUT
    );

    pinMode(
        CURRENT_PIN,
        INPUT
    );

    analogReadResolution(12);

    Serial.println(
        "Gas sensor READY"
    );

    Serial.println(
        "Current sensor READY"
    );

    // --------------------------------------------------------
    // WIFI
    // --------------------------------------------------------

    connectWiFi();

    // --------------------------------------------------------
    // MQTT
    // --------------------------------------------------------

    mqttClient.setServer(
        MQTT_BROKER,
        MQTT_PORT
    );

    Serial.println();
    Serial.println(
        "System initialization complete."
    );
}

// ============================================================
//                  LOOP
// ============================================================

void loop()
{
    // --------------------------------------------------------
    // WIFI
    // --------------------------------------------------------

    if (
        WiFi.status() != WL_CONNECTED
    )
    {
        connectWiFi();
    }

    // --------------------------------------------------------
    // MQTT
    // --------------------------------------------------------

    if (
        !mqttClient.connected()
    )
    {
        connectMQTT();
    }

    mqttClient.loop();

    // --------------------------------------------------------
    // SENSOR DATA
    // --------------------------------------------------------

    float temperature;
    float humidity;
    float vibration;

    int gas;
    int currentRaw;

    float currentVoltage;

    readSensors(
        temperature,
        humidity,
        vibration,
        gas,
        currentRaw,
        currentVoltage
    );

    // --------------------------------------------------------
    // SERIAL
    // --------------------------------------------------------

    printSensorData(
        temperature,
        humidity,
        vibration,
        gas,
        currentRaw,
        currentVoltage
    );

    // --------------------------------------------------------
    // JSON
    // --------------------------------------------------------

    String json =
        createJSON(
            temperature,
            humidity,
            vibration,
            gas,
            currentRaw,
            currentVoltage
        );

    Serial.println();
    Serial.println(
        "--------------------------------"
    );

    Serial.print(
        "MQTT Topic: "
    );

    Serial.println(
        MQTT_TOPIC
    );

    Serial.print(
        "Message: "
    );

    Serial.println(
        json
    );

    // --------------------------------------------------------
    // MQTT PUBLISH
    // --------------------------------------------------------

    if (
        mqttClient.connected()
    )
    {
        bool success =
            mqttClient.publish(
                MQTT_TOPIC,
                json.c_str()
            );

        if (success)
        {
            Serial.println(
                "MQTT publish: OK"
            );
        }
        else
        {
            Serial.println(
                "MQTT publish: FAILED"
            );
        }
    }
    else
    {
        Serial.println(
            "MQTT not connected."
        );
    }

    Serial.println(
        "--------------------------------"
    );

    delay(2000);
}