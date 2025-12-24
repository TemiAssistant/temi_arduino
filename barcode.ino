#include <WiFiS3.h>
#include <ArduinoMqttClient.h>

#include <usbhid.h>
#include <usbhub.h>
#include <hiduniversal.h>
#include <hidboot.h>
#include <SPI.h>

char ssid[] = "bin";
char pass[] = "";

const char broker[] = "xxx.xx.xx.x";
int port = 1884;
const char topic[] = "cart/scan";

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

// 바코드 버퍼
String barcode = "";
bool scanDone = false;

// [ADDED] JSON에 넣을 고정 값들(원하면 나중에 바꿔도 됨)
const char OWNER_ID[] = "temi";   // [ADDED]
const int QUANTITY = 1;              // [ADDED]

// HID Parser
class MyParser : public HIDReportParser {
public:
  void Parse(USBHID *hid, bool is_rpt_id, uint8_t len, uint8_t *buf);
protected:
  uint8_t mapKey(uint8_t key);
  virtual void onFinish();
};

uint8_t MyParser::mapKey(uint8_t key) {
  // 숫자 1~0
  if (key >= 0x1E && key <= 0x27) {
    if (key == 0x27) return '0';
    return '0' + (key - 0x1E + 1);
  }

  // 알파벳 a~z
  if (key >= 0x04 && key <= 0x1D) {
    return 'a' + (key - 0x04);
  }

  return 0;
}

void MyParser::Parse(USBHID *hid, bool, uint8_t len, uint8_t *buf) {
  uint8_t key = buf[2];

  if (key == 0) return;

  if (key == 0x28) {  // Enter
    onFinish();
    return;
  }

  uint8_t c = mapKey(key);
  if (c != 0) {
    barcode += (char)c;
    Serial.print((char)c);
  }
}

void MyParser::onFinish() {
  scanDone = true;
  Serial.println("  <-- scan finished");
}

// USB Host Shield 객체
USB Usb;
USBHub Hub(&Usb);
HIDUniversal Hid(&Usb);

MyParser Parser;

void connectWiFi() {
  Serial.print("WiFi connecting");
  WiFi.begin(ssid, pass);

  int t = 0;
  while (WiFi.status() != WL_CONNECTED && t < 40) {
    delay(300);
    Serial.print(".");
    t++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi FAIL");
  }
}

void connectMQTT() {
  Serial.print("MQTT connecting...");
  while (!mqttClient.connect(broker, port)) {
    Serial.print(" retry");
    delay(1000);
  }
  Serial.println(" connected!");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  Serial.println("=== START BARCODE + MQTT ===");

  if (Usb.Init() == -1) {
    Serial.println("USB Host FAIL");
  } else {
    Serial.println("USB Host OK");
  }
  Hid.SetReportParser(0, &Parser);

  connectWiFi();
  connectMQTT();
}

void loop() {
  Usb.Task();

  if (scanDone) {
    // [ADDED] 바코드 값으로 JSON payload 만들기 (공백 없이 정확한 형태)
    String payload = "{\"owner_id\":\"";              // [ADDED]
    payload += OWNER_ID;                             // [ADDED]
    payload += "\",\"barcode\":\"";                  // [ADDED]
    payload += barcode;                              // [ADDED]
    payload += "\",\"quantity\":";                   // [ADDED]
    payload += String(QUANTITY);                     // [ADDED]
    payload += "}";                                  // [ADDED]

    Serial.print("Publishing JSON: ");               // [ADDED]
    Serial.println(payload);                         // [ADDED]

    // ✅ MQTT publish (이 3줄이 "mqtt에 띄우는" 동작)
    mqttClient.beginMessage(topic);
    mqttClient.print(payload);                       // [CHANGED] barcode+"test" -> JSON payload
    mqttClient.endMessage();

    barcode = "";
    scanDone = false;
  }

  mqttClient.poll();
}