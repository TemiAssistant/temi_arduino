#include <WiFi.h>
#include <PubSubClient.h>
#include "HX711.h"

// ====== WiFi 설정 ======
const char* ssid     = "HY-DORM5-511";      // ← 와이파이 이름
const char* password = "residence511";      // ← 와이파이 비번

// ====== MQTT 설정 ======
const char* mqtt_server = "192.168.0.8";    // ← 노트북(브로커) IP

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];

// ====== 로드셀(HX711) 설정 ======
const int LOADCELL_DOUT_PIN = 16;   // HX711 DT
const int LOADCELL_SCK_PIN  = 4;    // HX711 SCK

HX711 scale;

// 너가 캘리브레이션해서 얻은 값 넣기!
const float CALIBRATION_FACTOR = 3342.35;   // <-- 여기 숫자 수정해서 사용

// ====== MQTT 콜백 (구독 메시지 처리) ======
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // 예: "univ/subin/led" 토픽으로 '1' 오면 GPIO 15 ON, 그 외 OFF
  if (strcmp(topic, "univ/subin/led") == 0) {
    if ((char)payload[0] == '1') {
      digitalWrite(15, HIGH);
      Serial.println("LED ON");
    } else {
      digitalWrite(15, LOW);
      Serial.println("LED OFF");
    }
  }
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
  pinMode(15, OUTPUT);   // LED 제어 핀
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
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  randomSeed(micros());

  // ====== 로드셀(HX711) 초기화 ======
  Serial.println("Initializing HX711...");
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  scale.set_scale();  // 보정값 적용
  scale.tare();                         // 현재 상태를 0으로

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
      // 10번 평균값으로 무게 측정 (단위: g 비슷하게 나올 것)
      float weight = scale.get_units(10);

      // 소수 한 자리까지 문자열로 변환
      snprintf(msg, MSG_BUFFER_SIZE, "weight: %.1f g", weight);

      Serial.print("Publish message: ");
      Serial.println(msg);

      // 로드셀 값 publish 하는 토픽
      client.publish("univ/subin/weight", msg);
    } else {
      Serial.println("HX711 not ready");
    }
  }
}
