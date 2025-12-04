#include <WiFi.h>
#include <PubSubClient.h>
#include "HX711.h"   // 로드셀 라이브러리

// ====== WiFi (개인용 WPA2) 설정 ======
// WPA2 Personal
const char* ssid     = "HY-DORM5-511";      // ← 여기 SSID
const char* password = "residence511";      // ← 여기 비번

// ====== MQTT 설정 ======
const char* mqtt_server = "192.168.0.8";    // ← 브로커 IP (노트북 IP)

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];

// ====== 로드셀(HX711) 설정 ======
const int LOADCELL_DOUT_PIN = 16;   // DOUT
const int LOADCELL_SCK_PIN  = 4;    // SCK
HX711 scale;

// 👉 여기 네가 구한 보정값 넣기 (예: 3342.35)
const float CALIBRATION_FACTOR = 3342.35;   // TODO: 실제 값으로 수정해서 사용

// ====== MQTT 콜백: 구독한 토픽에 메시지 도착했을 때 호출 ======
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // 예시: "univ/subin/led" 토픽에 '1' 오면 GPIO 15 ON, 그 외는 OFF
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

// ====== MQTT 재연결 함수 ======
void reconnect() {
  // MQTT 브로커에 연결될 때까지 반복
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    String clientId = "ESP32-";
    clientId += String(random(0xffff), HEX);

    // MQTT 서버에 연결 시도
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");

      // 연결 후 구독할 토픽
      client.subscribe("univ/subin/led");   // LED 제어용 토픽
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  pinMode(15, OUTPUT);   // LED/제어용 핀
  Serial.begin(115200);
  delay(1000);

  Serial.println("\nConnecting to WiFi (WPA2-Personal)...");

  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);

  // ===== WPA2-Personal (일반 공유기 / 핫스팟) 연결 =====
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

  // ===== MQTT 설정 =====
  client.setServer(mqtt_server, 1883);  // 브로커 주소/포트 설정
  client.setCallback(callback);         // 구독 메시지 처리 콜백

  // 랜덤 시드 (clientId 만들 때 사용)
  randomSeed(micros());

  // ===== HX711(로드셀) 초기화 =====
  Serial.println("Initializing HX711...");
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  // 보정값 적용 + 영점 잡기
  scale.set_scale();
  scale.tare();   // 현재 상태를 0으로 설정

  Serial.println("HX711 ready. Tare done.");
}

void loop() {
  // MQTT 연결 유지
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // ====== 1초마다 로드셀 값 publish ======
  unsigned long now = millis();
  if (now - lastMsg > 1000) {
    lastMsg = now;

    if (scale.is_ready()) {
      // 10번 평균값으로 무게 측정
      float weight = scale.get_units(10);  // g 단위처럼 나올 것

      // "weight: 0.0 g" 형식으로 문자열 만들기
      snprintf(msg, MSG_BUFFER_SIZE, "weight: %.1f g", weight);

      Serial.print("Publish message: ");
      Serial.println(msg);

      // 👉 여기 토픽으로 MQTT 브로커에 전송
      client.publish("univ/subin/weight", msg);
    } else {
      Serial.println("HX711 not found or not ready.");
    }
  }
}
