#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <WebServer.h>

// Conexão Wi-Fi
const char* ssid = "wifi";
const char* password = "wifi";

#define DHTPIN 26
#define DHTTYPE DHT22
#define SOIL_PIN 34
#define RAIN_SENSOR_PIN 13
#define BOMBA1_PIN 23
#define BOMBA2_PIN 19

// Relé ativo em nível LOW
#define LIGADO LOW
#define DESLIGADO HIGH

// Sensores
#define SOIL_DRY 3500
#define SOIL_WET 1500
#define LIMITE_UMIDADE 30

DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);

// evitar mensagens repetidas
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
  digitalWrite(BOMBA2_PIN, DESLIGADO);

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
    if (temp == -127)
      resposta += "\"temperatura\":\"Erro\",";
    else
      resposta += "\"temperatura\":" + String(temp, 1) + ",";

    resposta += "\"umidadeSolo\":" + String(umidade, 1) + ",";
    resposta += "\"chuva\":\"" + chuva + "\",";
    resposta += "\"bomba1\":\"" + String(digitalRead(BOMBA1_PIN) == LIGADO ? "ligada" : "desligada") + "\",";
    resposta += "\"bomba2\":\"" + String(digitalRead(BOMBA2_PIN) == LIGADO ? "ligada" : "desligada") + "\",";
    resposta += "\"alerta\":\"" + alerta + "\"";
    resposta += "}";

    server.send(200, "application/json", resposta);
  });

  // POST /bomba1
  server.on("/bomba1", HTTP_POST, []() {
    String acao = server.arg("acao");
    if (acao == "on") digitalWrite(BOMBA1_PIN, LIGADO);
    else if (acao == "off") digitalWrite(BOMBA1_PIN, DESLIGADO);
    else return server.send(400, "text/plain", "Ação inválida");

    server.send(200, "text/plain", "Bomba 1 " + acao);
  });

  // POST /bomba2
  server.on("/bomba2", HTTP_POST, []() {
    String acao = server.arg("acao");
    if (acao == "on") digitalWrite(BOMBA2_PIN, LIGADO);
    else if (acao == "off") digitalWrite(BOMBA2_PIN, DESLIGADO);
    else return server.send(400, "text/plain", "Ação inválida");

    server.send(200, "text/plain", "Bomba 2 " + acao);
  });

  server.begin();
  Serial.println("🚀 Servidor HTTP iniciado");
}

void loop() {
  server.handleClient();

  // Verifica mudança no estado da chuva
  String estadoAtualChuva = getStatusChuva();
  if (estadoAtualChuva != estadoChuvaAnterior) {
    Serial.print("Status Atual da Chuva: ");
    Serial.println(estadoAtualChuva);
    estadoChuvaAnterior = estadoAtualChuva;
  }

  // Verifica umidade do solo
  float umidade = getUmidadeSolo();
  if (umidade < LIMITE_UMIDADE && !alertaUmidadeEmitido) {
    Serial.print(" Umidade do solo baixa: ");
    Serial.print(umidade);
    Serial.println("%");
    alertaUmidadeEmitido = true;
  } else if (umidade >= LIMITE_UMIDADE && alertaUmidadeEmitido) {
    Serial.println(" Umidade do solo está normal.");
    alertaUmidadeEmitido = false;
  }

  delay(1000); 
}
