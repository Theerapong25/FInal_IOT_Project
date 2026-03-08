#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <HTTPClient.h>

const char *ssid = "1617";
const char *password = "kukps1617";

WiFiClient wifiClient;
// MQTT config
const char *mqttServer = "mqtt.netpie.io";
const int mqttPort = 1883;
const char *mqttClientId = "587ab64a-bfa6-4c2e-8341-f9be792a92df";
const char *mqttUser = "sy1To3GuqL9gXqFkwh6bYxcHNDL3kWcQ";
const char *mqttPassword = "gZfUy4ZGLpPznSQqPhejMpn3JSJWsDWV";
const char *topic_pub = "@msg/lab_ict/speed_data";
const char *data_pub = "@shadow/data/update";
PubSubClient mqttClient(wifiClient);
String publishMessage;

#define TFT_CS    33
#define TFT_RST   32
#define TFT_DC    25

#define TRIG1 16
#define ECHO1 17

#define TRIG2 5
#define ECHO2 36

#define RED_LED   26
#define GREEN_LED 4
#define BUZZER    27

#define SENSOR_DISTANCE_CM 26.0
#define TRIGGER_DISTANCE   10.0

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

#define BUZZER_CHANNEL 0
#define BUZZER_FREQ 1000
#define BUZZER_RESOLUTION 8

unsigned long t_start = 0;
unsigned long t_end   = 0;
unsigned long t_buzzer = 0;

bool sensor1_triggered = false;

float speed_kmh = 0;
String status = "";

//////////////////////////////////////////////////

void setup_wifi()
{
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

//////////////////////////////////////////////////

void sendToFirebase(float speed_kmh, String status)
{
  HTTPClient http;

  String url = "https://iotspeed-73afe-default-rtdb.asia-southeast1.firebasedatabase.app/sensorData.json";   // ใส่ Firebase URL ตรงนี้

  String speedID = "speed" + String(millis());

  String payload = "{ \"" + speedID+ "\": {";
  payload += "\"speed\": " + String(speed_kmh, 2) + ",";
  payload += "\"status\": \"" + status + "\"";
  payload += "} }";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.PATCH(payload);

  if (httpResponseCode > 0)
  {
    Serial.println("Firebase response: " + http.getString());
  }
  else
  {
    Serial.println("Error sending to Firebase");
  }

  http.end();
}

//////////////////////////////////////////////////

void reconnectMQTT()
{
  while (!mqttClient.connected())
  {
    Serial.println("Attempting MQTT connection...");

    if (mqttClient.connect(mqttClientId, mqttUser, mqttPassword))
    {
      Serial.println("MQTT connected");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.println(mqttClient.state());
      delay(5000);
    }
  }
}

//////////////////////////////////////////////////

float readDistance(int trigPin, int echoPin)
{
  float total = 0;

  for (int i = 0; i < 5; i++)
  {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);

    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    long duration = pulseIn(echoPin, HIGH, 30000);
    float distance = (duration * 0.0343) / 2;

    total += distance;

    delay(5);
  }

  return total / 5.0;
}

//////////////////////////////////////////////////

void setup()
{
  Serial.begin(115200);

  setup_wifi();

  mqttClient.setServer(mqttServer, mqttPort);

  tft.begin();
  tft.setRotation(1);

  pinMode(TRIG1, OUTPUT);
  pinMode(ECHO1, INPUT);

  pinMode(TRIG2, OUTPUT);
  pinMode(ECHO2, INPUT);

  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  ledcSetup(BUZZER_CHANNEL, BUZZER_FREQ, BUZZER_RESOLUTION);
  ledcAttachPin(BUZZER, BUZZER_CHANNEL);
  ledcWrite(BUZZER_CHANNEL, 0);
}

//////////////////////////////////////////////////

void loop()
{
  if (!mqttClient.connected())
  {
    reconnectMQTT();
  }

  mqttClient.loop();

  float d1 = readDistance(TRIG1, ECHO1);
  delay(20);
  float d2 = readDistance(TRIG2, ECHO2);

  if (!sensor1_triggered && d1 < TRIGGER_DISTANCE)
  {
    t_start = millis();
    sensor1_triggered = true;
    Serial.println("Sensor 1 Triggered");
  }

  if (sensor1_triggered && d2 < TRIGGER_DISTANCE)
  {
    t_end = millis();
    t_buzzer = t_end + 1000;

    float deltaT = (float)(t_end - t_start) / 1000.0;

    if (deltaT > 0.01 && deltaT < 5)
    {
      float speed_cm_s = SENSOR_DISTANCE_CM / deltaT;
      speed_kmh = speed_cm_s * 0.036;

      Serial.print("Speed = ");
      Serial.print(speed_kmh);
      Serial.println(" km/h");

      publishMessage = "{\"data\":{\"speed\":" + String(speed_kmh, 2) + "}}";

      mqttClient.publish(topic_pub, publishMessage.c_str());
      mqttClient.publish(data_pub, publishMessage.c_str());

      tft.fillScreen(ILI9341_BLACK);

      tft.setTextSize(3);

      tft.setCursor(20,30);
      tft.setTextColor(ILI9341_GREEN);
      tft.print("Speed:");

      tft.setCursor(20,80);
      tft.print(speed_kmh,2);
      tft.print(" km/h");

      tft.setCursor(20,140);

      if (speed_kmh > 2)
      {
        status = "OVERSPEED";

        tft.setTextColor(ILI9341_RED);
        tft.println("OVERSPEED!");

        digitalWrite(RED_LED, HIGH);
        digitalWrite(GREEN_LED, LOW);

        while (millis() < t_buzzer)
        {
          ledcWrite(BUZZER_CHANNEL, 10);
        }

        ledcWrite(BUZZER_CHANNEL, 0);

        digitalWrite(GREEN_LED, HIGH);
        digitalWrite(RED_LED, LOW);
      }
      else
      {
        status = "NORMAL";

        tft.setTextColor(ILI9341_GREEN);
        tft.println("NORMAL");

        digitalWrite(RED_LED, LOW);
        digitalWrite(GREEN_LED, HIGH);

        ledcWrite(BUZZER_CHANNEL, 0);
      }

      sendToFirebase(speed_kmh, status);
    }

    sensor1_triggered = false;
  }

  delay(50);
}