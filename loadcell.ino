#include <WiFi.h>
#include <PubSubClient.h>
#include "HX711.h"

// ====== WiFi 설정 ======
const char* ssid     = "bin";
const char* password = "";

// ====== MQTT 설정 ======
const char* mqtt_server = "xxx.xx.xx.x";

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;

#define MSG_BUFFER_SIZE (128)      // [CHANGED] JSON 문자열이라 버퍼 증가
char msg[MSG_BUFFER_SIZE];

// ====== 로드셀(HX711) 설정 ======
const int LOADCELL_DOUT_PIN = 16;
const int LOADCELL_SCK_PIN  = 4;

HX711 scale;
const float CALIBRATION_FACTOR = -2500;

// [ADDED] JSON 고정 필드
const char* BARCODE   = "prod_006";        // [ADDED]
const char* SENSOR_ID = "loadcell-A01";    // [ADDED]

// ====== MQTT 콜백 (구독 메시지 처리) ======
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // (원하면 여기서 LED 제어 코드 추가 가능)
}

// ====== MQTT 재연결 ======
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    String clientId = "ESP32-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("connected");

      // 구독할 토픽 (LED 제어용)
      client.subscribe("univ/subin/led");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\nConnecting to WiFi (WPA2-Personal)...");

  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    if (++tries > 60) {
      Serial.println("\nWiFi connection failed. Restarting...");
      ESP.restart();
    }
  }

  Serial.println("\nConnected to WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // MQTT 설정
  client.setServer(mqtt_server, 1884);
  client.setCallback(callback);
  randomSeed(micros());

  // ====== 로드셀(HX711) 초기화 ======
  Serial.println("Initializing HX711...");
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  scale.set_scale(CALIBRATION_FACTOR);
  delay(3000);
  scale.tare();

  Serial.println("Tare done. Ready to measure weight.");
}

void loop() {
  // MQTT 연결 유지
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // 1초마다 로드셀 값 읽어서 MQTT로 publish
  unsigned long now = millis();
  if (now - lastMsg > 1000) {
    lastMsg = now;

    if (scale.is_ready()) {
      float weight = scale.get_units(10);

      // [ADDED] 예시처럼 정수로 보내고 싶으면 반올림해서 long으로
      long measured_weight = lround(weight);   // [ADDED]

      // [CHANGED] JSON payload 생성
      snprintf(
        msg, MSG_BUFFER_SIZE,
        "{\"barcode\":\"%s\",\"sensor_id\":\"%s\",\"measured_weight\":%ld}",
        BARCODE, SENSOR_ID, measured_weight
      );

      Serial.print("Publish JSON: ");
      Serial.println(msg);

      // [CHANGED] JSON publish
      client.publish("temi/inventory/loadcell", msg);
    } else {
      Serial.println("HX711 not ready");
    }
  }
}
