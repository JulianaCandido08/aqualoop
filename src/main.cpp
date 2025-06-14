#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "wifi";
const char* password = "wifi";

#define DHTPIN 26
#define DHTTYPE DHT11
#define SOIL_PIN 34
#define RAIN_SENSOR_PIN 14
#define BOMBA1_PIN 19
#define BOMBA2_PIN 23

#define LIGADO LOW
#define DESLIGADO HIGH

#define SOIL_DRY 3500
#define SOIL_WET 1500
#define LIMITE_UMIDADE 30

DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);

// Estados anteriores
String estadoChuvaAnterior = "";
bool alertaUmidadeEmitido = false;

float getTemperatura() {
  float t = dht.readTemperature();
  if (isnan(t)) {
    Serial.println("❌ Erro ao ler temperatura!");
    return -127;
  }
  return t;
}

float getUmidadeSolo() {
  int leitura = analogRead(SOIL_PIN);
  leitura = constrain(leitura, SOIL_WET, SOIL_DRY);
  return map(leitura, SOIL_DRY, SOIL_WET, 0, 100);
}

String getStatusChuva() {
  return digitalRead(RAIN_SENSOR_PIN) == LOW ? "Chovendo" : "Sem chuva";
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  delay(2000);

  pinMode(RAIN_SENSOR_PIN, INPUT);
  pinMode(BOMBA1_PIN, OUTPUT);
  pinMode(BOMBA2_PIN, OUTPUT);

  digitalWrite(BOMBA1_PIN, DESLIGADO); 
  digitalWrite(BOMBA2_PIN, LIGADO);    

  // Conexão Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Conectando");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Wi-Fi conectado");
    Serial.print("📡 IP da ESP32: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Falha na conexão Wi-Fi");
    return;
  }

  // Rota GET /status
  server.on("/status", HTTP_GET, []() {
    float temp = getTemperatura();
    float umidade = getUmidadeSolo();
    String chuva = getStatusChuva();
    String alerta = "";

    if (chuva == "Chovendo") alerta += "Alerta: Está chovendo. ";
    if (umidade < LIMITE_UMIDADE) alerta += "Alerta: Umidade do solo baixa. ";

    String resposta = "{";
    resposta += (temp == -127)
      ? "\"temperatura\":\"Erro\","
      : "\"temperatura\":" + String(temp, 1) + ",";

    resposta += "\"umidadeSolo\":" + String(umidade, 1) + ",";
    resposta += "\"chuva\":\"" + chuva + "\",";
    resposta += "\"bomba1\":\"" + String(digitalRead(BOMBA1_PIN) == LIGADO ? "ligada" : "desligada") + "\",";
    resposta += "\"bomba2\":\"ligada\",";
    resposta += "\"alerta\":\"" + alerta + "\"";
    resposta += "}";

    server.send(200, "application/json", resposta);
  });

  // POST /bomba1
  server.on("/bomba1", HTTP_POST, []() {
    if (!server.hasArg("acao")) {
      server.send(400, "text/plain", "Parâmetro 'acao' ausente");
      return;
    }

    String acao = server.arg("acao");

    if (acao == "on") {
      digitalWrite(BOMBA1_PIN, LIGADO);
      server.send(200, "text/plain", "Bomba 1 ligada");
    } else if (acao == "off") {
      digitalWrite(BOMBA1_PIN, DESLIGADO);
      server.send(200, "text/plain", "Bomba 1 desligada");
    } else {
      server.send(400, "text/plain", "Ação inválida: use 'on' ou 'off'");
    }
  });

  server.begin();
  Serial.println("🚀 Servidor HTTP iniciado");
}

void loop() {
  server.handleClient();

 
  String estadoAtualChuva = getStatusChuva();
  if (estadoAtualChuva != estadoChuvaAnterior) {
    Serial.print("Status Atual da Chuva: ");
    Serial.println(estadoAtualChuva);
    estadoChuvaAnterior = estadoAtualChuva;
  }


  float umidade = getUmidadeSolo();
  Serial.print("Umidade do Solo: ");
  Serial.print(umidade);
  Serial.println("%");


  if (umidade < LIMITE_UMIDADE && !alertaUmidadeEmitido) {
    Serial.println("Umidade do solo baixa!");
    alertaUmidadeEmitido = true;
  } else if (umidade >= LIMITE_UMIDADE && alertaUmidadeEmitido) {
    Serial.println("Umidade do solo está normal.");
    alertaUmidadeEmitido = false;
  }

  delay(1000);
}
