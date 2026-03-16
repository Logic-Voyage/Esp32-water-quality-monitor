#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

// ==== Pin Configuration ====
#define DHTPIN 21
#define DHTTYPE DHT11
#define TDS_PIN 34
#define TRIG_PIN 18
#define ECHO_PIN 19
#define RED_LED 25
#define GREEN_LED 4
#define BUZZER 26

// ==== WiFi Details ====
const char* ssid = "Oneplus 11R";
const char* password = "Password";

// ==== Sensor Setup ====
DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);

// ==== Global Variables ====
float temperature, humidity, tds, waterLevel;
bool badWater, overflow;

void readSensors() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  int analogValue = analogRead(TDS_PIN);
  float voltage = analogValue * (3.3 / 4095.0);
  tds = (133.42 * voltage * voltage * voltage - 255.86 * voltage * voltage + 857.39 * voltage) * 0.5;

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  waterLevel = duration * 0.034 / 2;

  badWater = (tds > 1000 || temperature < 10 || temperature > 50);
  overflow = (waterLevel < 3);
  
  digitalWrite(RED_LED, badWater || overflow);
  digitalWrite(GREEN_LED, !badWater && !overflow);
  digitalWrite(BUZZER, badWater || overflow);
}

String getHTML() {
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <style>
        body {
          font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
          background: linear-gradient(to right, #eef2f3, #8e9eab);
          color: #333;
          text-align: center;
          padding: 20px;
        }
        .container {
          max-width: 420px;
          margin: auto;
        }
        .card {
          background: #fff;
          padding: 20px;
          border-radius: 16px;
          box-shadow: 0 8px 20px rgba(0,0,0,0.1);
          margin-top: 20px;
        }
        h2 {
          color: #0066cc;
          font-size: 24px;
          margin-bottom: 12px;
        }
        .value {
          font-size: 20px;
          margin: 12px 0;
        }
        .label {
          font-weight: bold;
          margin-right: 10px;
        }
        .status {
          font-size: 18px;
          font-weight: bold;
          margin-top: 15px;
        }
        .ok { color: green; }
        .alert { color: red; }
        .refreshing {
          font-size: 12px;
          color: #888;
          margin-top: 5px;
        }
      </style>
      <script>
        setInterval(() => {
          fetch("/data")
            .then(res => res.json())
            .then(data => {
              document.getElementById("temp").innerHTML = data.temperature + " °C";
              document.getElementById("humid").innerHTML = data.humidity + " %";
              document.getElementById("tds").innerHTML = data.tds + " ppm";
              document.getElementById("level").innerHTML = data.level + " cm";
              document.getElementById("status").innerHTML = data.statusText;
              document.getElementById("status").className = "status " + data.statusClass;
            });
        }, 2000);
      </script>
    </head>
    <body>
      <div class="container">
        <h2>💧 ESP32 Water Quality Monitor</h2>
        <div class="card">
          <p class="value"><span class="label">🌡️ Temperature:</span><span id="temp">--</span></p>
          <p class="value"><span class="label">💦 Humidity:</span><span id="humid">--</span></p>
          <p class="value"><span class="label">🧪 TDS:</span><span id="tds">--</span></p>
          <p class="value"><span class="label">📏 Water Level:</span><span id="level">--</span></p>
          <p id="status" class="status">--</p>
          <p class="refreshing">🔄 Updating every 2 seconds...</p>
        </div>
      </div>
    </body>
    </html>
  )rawliteral";
  return html;
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  server.on("/", []() {
    server.send(200, "text/html", getHTML());
  });

  server.on("/data", []() {
    readSensors();
    String statusText = "✅ All Normal";
    String statusClass = "ok";
    if (badWater && overflow) {
      statusText = "🚨 Bad Water + Overflow";
      statusClass = "alert";
    } else if (badWater) {
      statusText = "🚨 Bad Water Detected";
      statusClass = "alert";
    } else if (overflow) {
      statusText = "⚠️ Overflow Detected";
      statusClass = "alert";
    }

    String json = "{";
    json += "\"temperature\":" + String(temperature) + ",";
    json += "\"humidity\":" + String(humidity) + ",";
    json += "\"tds\":" + String(tds, 2) + ",";
    json += "\"level\":" + String(waterLevel, 1) + ",";
    json += "\"statusText\":\"" + statusText + "\",";
    json += "\"statusClass\":\"" + statusClass + "\"}";
    server.send(200, "application/json", json);
  });

  server.begin();
}

void loop() {
  server.handleClient();
}
