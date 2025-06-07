#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <driver/ledc.h>

#define DHTPIN     4
#define PIRPIN     5
#define FANPIN     18
#define DHTTYPE    DHT11

#define CHANNEL     LEDC_CHANNEL_0
#define TIMER       LEDC_TIMER_0
#define MODE        LEDC_HIGH_SPEED_MODE
#define FREQ_HZ     25000
#define RESOLUTION  LEDC_TIMER_8_BIT

DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);

const char* ssid     = "R6_11";
const char* password = "0987654321";

float temperature = 0.0;
float humidity    = 0.0;
bool presence     = false;
int fanSpeed      = 0;
unsigned long lastPrint = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(PIRPIN, INPUT);
  dht.begin();

  ledc_timer_config_t ledc_timer = {
    .speed_mode       = MODE,
    .duty_resolution  = RESOLUTION,
    .timer_num        = TIMER,
    .freq_hz          = FREQ_HZ,
    .clk_cfg          = LEDC_AUTO_CLK
  };
  ledc_timer_config(&ledc_timer);

  ledc_channel_config_t ledc_channel = {
    .gpio_num   = FANPIN,
    .speed_mode = MODE,
    .channel    = CHANNEL,
    .intr_type  = LEDC_INTR_DISABLE,
    .timer_sel  = TIMER,
    .duty       = 0,
    .hpoint     = 0
  };
  ledc_channel_config(&ledc_channel);

  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nTerhubung ke WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/data", HTTP_GET, sendSensorData);
  server.on("/setFanSpeed", HTTP_OPTIONS, handleOptions); // CORS preflight
  server.on("/setFanSpeed", HTTP_POST, setFanSpeed);
  server.begin();
  Serial.println("Web server aktif.");

  delay(2000);
}

void loop() {
  server.handleClient();

  temperature = dht.readTemperature();
  humidity    = dht.readHumidity();
  presence    = digitalRead(PIRPIN);

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Gagal membaca sensor DHT!");
    temperature = 0.0;
    humidity = 0.0;
  }

  int duty = map(fanSpeed, 0, 100, 0, 255);
  ledc_set_duty(MODE, CHANNEL, duty);
  ledc_update_duty(MODE, CHANNEL);

  Serial.print("Suhu: "); Serial.print(temperature);
  Serial.print("°C | Kelembapan: "); Serial.print(humidity);
  Serial.print("% | Kehadiran: "); Serial.print(presence ? "Ya" : "Tidak");
  Serial.print(" | FanSpeed: "); Serial.print(fanSpeed);
  Serial.print("% | Duty: "); Serial.println(duty);

  if (millis() - lastPrint > 30000) {
    Serial.print("IP saat ini: ");
    Serial.println(WiFi.localIP());
    lastPrint = millis();
  }

  delay(5000);
}

// ============ Tambahan CORS untuk semua endpoint ============

void sendCORSHeader() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ============ Endpoint GET data sensor ============

void sendSensorData() {
  sendCORSHeader();
  String json = "{\"temperature\":" + String(temperature, 1) +
                ",\"humidity\":" + String(humidity, 1) +
                ",\"presence\":" + String(presence ? 1 : 0) + "}";
  server.send(200, "application/json", json);
}

// ============ Endpoint POST set kipas ============

void setFanSpeed() {
  sendCORSHeader();
  if (server.hasArg("plain")) {
    fanSpeed = constrain(server.arg("plain").toInt(), 0, 100);
    server.send(200, "text/plain", "OK");
    Serial.print("Kecepatan kipas diatur ke: ");
    Serial.print(fanSpeed);
    Serial.println("%");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

// ============ Endpoint untuk OPTIONS request (preflight) ============

void handleOptions() {
  sendCORSHeader();
  server.send(204);
}
