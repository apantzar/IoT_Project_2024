#include <DHT.h>
#include <WiFi.h>
#include <ArduinoMqttClient.h>
#include "DHTReadings.h"

// DHT is connected at pin #2
#define DHTPIN 2u

// Type of DHT is DHT22
#define DHTTYPE DHT22

#define SUBQOS 2

using config_id_t = enum ConfigID : size_t {
  SSID,
  PASSWORD,
  MQTT_BROKER,
  MQTT_PORT,
  MQTT_TOPIC,
  USE_STATIC_IP
};

const char * const config[] = {
  // The config.h file must contain
  // 2 strings (separated by comma
  // of course). The first one is
  // the WiFi SSID while the second
  // one is the Password.
  #include "config.h"
};

const unsigned long interval = 8000;
unsigned long previousMillis = 0;


DHT dht(DHTPIN, DHTTYPE);

WiFiClient wifiClient{ };
MqttClient mqttClient{ wifiClient };

/** Compare a "bool" string to
 * to a bool value. The string
 * is NOT converted to lowercase
 * before checking.
 */
bool boolstrcmp(const char *str, bool value) {
  return (bool)(String(str) == ((value) ? "true" : "false"));
}

bool get_validated_port(int *outPort) {
  if (outPort != nullptr) {
    String strPort = String(config[MQTT_PORT]);
    long bufPort = strPort.toInt();
    *outPort = (int)bufPort;
    return !(
      bufPort == 0l &&
      strPort == "0" &&
      bufPort != ((long)*outPort)
    );
  }
  return false;
}

bool should_publish() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    return true;
  }
  return false;
}

void publish_sensor_data(const DHTReadings& readings, const float cHeatIndex, const float fHeatIndex) {
  // Publich humidity
  mqttClient.beginMessage(String(config[MQTT_TOPIC]) + "/humidity");
  mqttClient.print(readings.humidity());
  mqttClient.endMessage();
  // Publich temperature (Celsius)
  mqttClient.beginMessage(String(config[MQTT_TOPIC]) + "/ctemp");
  mqttClient.print(readings.cTemperature());
  mqttClient.endMessage();
  // Publich temperature (Fahrenheit)
  mqttClient.beginMessage(String(config[MQTT_TOPIC]) + "/ftemp");
  mqttClient.print(readings.fTemperature());
  mqttClient.endMessage();
  // Publich head index (Celsius)
  mqttClient.beginMessage(String(config[MQTT_TOPIC]) + "/cheatindex");
  mqttClient.print(cHeatIndex);
  mqttClient.endMessage();
  // Publich head index (Fahrenheit)
  mqttClient.beginMessage(String(config[MQTT_TOPIC]) + "/fheatindex");
  mqttClient.print(fHeatIndex);
  mqttClient.endMessage();
}

void print_sensor_data(const DHTReadings& readings, const float cHeatIndex, const float fHeatIndex) {
  Serial.print(F("Humidity: "));
  Serial.print(readings.humidity());
  Serial.print(F("%  Temperature: "));
  Serial.print(readings.cTemperature());
  Serial.print(F("°C "));
  Serial.print(readings.fTemperature());
  Serial.print(F("°F  Heat index: "));
  Serial.print(cHeatIndex);
  Serial.print(F("°C "));
  Serial.print(fHeatIndex);
  Serial.println(F("°F"));
}

void on_mqtt_message(int messageSize) {
  Serial.println();
  Serial.print("Received a message with topic '");
  Serial.print(mqttClient.messageTopic());
  Serial.print("', duplicate = ");
  Serial.print(mqttClient.messageDup() ? "true" : "false");
  Serial.print(", QoS = ");
  Serial.print(mqttClient.messageQoS());
  Serial.print(", retained = ");
  Serial.print(mqttClient.messageRetain() ? "true" : "false");
  Serial.print("', length ");
  Serial.print(messageSize);
  Serial.println(" bytes:");

  // use the Stream interface to print the contents
  char msg[(size_t) messageSize + 1u] = { '\0' };
  size_t i = 0;
  while (mqttClient.available() && i < messageSize) {
    msg[i] = (char)mqttClient.read();
    ++i;
  }

  Serial.println(msg);

  if (String(msg) == "alert") {
    digitalWrite(LED_BUILTIN, HIGH);
  } else if (String(msg) == "stopalert") {
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    Serial.println("Unknown message");
  }

  Serial.println();
}

void setup() {
  // The baud rate is ignored as is
  // a USB connection. The method
  // has a default value of 115200
  // for parameter "baud" which is
  // later ignored.
  Serial.begin(/*115200*/);

  pinMode(LED_BUILTIN, OUTPUT);

  // Set the WiFi mode to STATION
  WiFi.mode(WIFI_STA);

  // Start the WiFi connection
  WiFi.begin(config[SSID], config[PASSWORD]);

  Serial.print(F("Connecting"));
  // Wait till the device connects to
  // the WiFi. Wait at least 1 second
  // between status checks.
  while (WiFi.status() != WL_CONNECTED) {
    delay(500u);
    Serial.print(F("."));
    delay(500u);
  }

  // After the device connects print the SSID
  Serial.println(F(""));
  Serial.println(F("Pico W is connected to WiFi network"));
  Serial.println(WiFi.SSID());

  if (boolstrcmp(config[USE_STATIC_IP], true)) {
    // ...
  }

  // Also, print the IP address of the device
  Serial.print(F("Assigned IP Address: "));
  Serial.println(WiFi.localIP());

  int port = 0;
  if (!get_validated_port(&port)) {
    Serial.printf("Validating port %s failed\n", config[MQTT_PORT]);
  } else {
    if (!mqttClient.connect(config[MQTT_BROKER], port)) {
      Serial.print(F("MQTT connection failed! Error code: "));
      Serial.println(mqttClient.connectError());
    } else {
      mqttClient.onMessage(on_mqtt_message);
      mqttClient.subscribe(String(config[MQTT_TOPIC]) + "/alerts", SUBQOS);
      Serial.println("Connected to the MQTT broker!");
    }
  }

  // Start measuring
  dht.begin();
}

void loop() {
  // Wait 2 seconds between measurements
  //delay(2000u);

  mqttClient.poll();

  if (should_publish()) {

    // Reading temperature or humidity
    // takes about 250 ms (sensor reading
    // can be up to 2 seconds old).

    DHTReadings readings{
      // Read humidity
      dht.readHumidity(),
      // Read temperature as Celsius (the default)
      dht.readTemperature(),
      // Read temperature as Fahrenheit (isFahrenheit = true)
      dht.readTemperature(true)
    };
  
    // Check if any of the readX calls
    // failed. If any one of them failed
    // return (in order to try again).
    if (readings.validate()) {
      Serial.println(F("Failed to read from DHT22 sensor!"));
      return;
    }

    // Compute heat index in Fahrenheit
    float fHeatIndex = readings.computeHeatIndex(&dht, true);
    // Compute heat index in Celsius
    float cHeatIndex = readings.computeHeatIndex(&dht);

    
    publish_sensor_data(readings, cHeatIndex, fHeatIndex);
    print_sensor_data(readings, cHeatIndex, fHeatIndex);
  }

}
