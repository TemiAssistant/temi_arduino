#include <WiFi.h>
#include "esp_eap_client.h"
#include <PubSubClient.h>

// ====== WiFi (eduroam) 설정 ======
// WPA2 Enterprise
const char* ssid         = "eduroam";
const char* EAP_ID       = "lse173";     // identity
const char* EAP_USERNAME = "lse173";     // username
const char* EAP_PASSWORD = "Thddms159300!@";

// ====== MQTT 설정 ======
const char* mqtt_server = "192.168.0.8";  // 필요하면 broker.mqtt-dashboard.com 으로 변경 가능

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];

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

  Serial.println("\nConnecting to eduroam...");

  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);

  // ===== WPA2-Enterprise 설정 (v3.3.4 방식) =====
  esp_eap_client_set_identity((uint8_t*)EAP_ID, strlen(EAP_ID));
  esp_eap_client_set_username((uint8_t*)EAP_USERNAME, strlen(EAP_USERNAME));
  esp_eap_client_set_password((uint8_t*)EAP_PASSWORD, strlen(EAP_PASSWORD));
  esp_wifi_sta_enterprise_enable();

  // eduroam 연결
  WiFi.begin(ssid);

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

  Serial.println("\nConnected to eduroam!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // ===== MQTT 설정 =====
  client.setServer(mqtt_server, 1883);  // 브로커 주소/포트 설정
  client.setCallback(callback);         // 구독 메시지 처리 콜백

  // 랜덤 시드 (clientId 만들 때 사용)
  randomSeed(micros());
}

void loop() {
  // MQTT 연결 유지
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // ====== 예제: 1초마다 아날로그 값 publish ======
  unsigned long now = millis();
  if (now - lastMsg > 1000) {
    lastMsg = now;

    int adcVal = analogRead(34);  // 필요 없으면 다른 값/텍스트로 변경 가능
    snprintf(msg, MSG_BUFFER_SIZE, "ADC : %d", adcVal);

    Serial.print("Publish message: ");
    Serial.println(msg);

    client.publish("univ/subin/cds", msg);   // 센서값 publish 토픽
  }
}
