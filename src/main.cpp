#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "--";
const char* password = "--";

#define DHTPIN 26
#define DHTTYPE DHT11
#define SOIL_PIN 25      
#define RAIN_SENSOR_PIN 14
#define BOMBA1_PIN 19
#define BOMBA2_PIN 23

#define LIGADO LOW
#define DESLIGADO HIGH

#define LIMITE_UMIDADE 30

DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);

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

// LOW = molhado, HIGH = seco
float getUmidadeSolo() {
  int leitura = digitalRead(SOIL_PIN);
  if (leitura == HIGH) {
    return 0.0; // Solo seco
  } else {
    return 100.0; // Solo molhado
  }
}

String getStatusChuva() {
  return digitalRead(RAIN_SENSOR_PIN) == LOW ? "Chovendo" : "Sem chuva";
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  delay(2000);

  pinMode(RAIN_SENSOR_PIN, INPUT);
  pinMode(SOIL_PIN, INPUT);            
  pinMode(BOMBA1_PIN, OUTPUT);
  pinMode(BOMBA2_PIN, OUTPUT);

  digitalWrite(BOMBA1_PIN, DESLIGADO); 
  digitalWrite(BOMBA2_PIN, LIGADO);    

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

  // GET /status
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
    resposta += "\"bomba1\":" + String(digitalRead(BOMBA1_PIN) == LIGADO ? "true" : "false") + ",";
    resposta += "\"bomba2\":true,";
    resposta += "\"alerta\":\"" + alerta + "\"";
    resposta += "}";

    server.send(200, "application/json", resposta);
  });

  // POST /bomba1
  server.on("/bomba1", HTTP_POST, []() {
    if (!server.hasArg("acao")) {
      server.send(400, "application/json", "{\"erro\":\"Parâmetro 'acao' ausente\"}");
      return;
    }

    String acao = server.arg("acao");
    String resposta;

    if (acao == "1") {
      digitalWrite(BOMBA1_PIN, LIGADO);
      resposta = "{\"bomba1\":true}";
      server.send(200, "application/json", resposta);
    } else if (acao == "0") {
      digitalWrite(BOMBA1_PIN, DESLIGADO);
      resposta = "{\"bomba1\":false}";
      server.send(200, "application/json", resposta);
    } else {
      server.send(400, "application/json", "{\"erro\":\"Ação inválida: use '1' ou '0'\"}");
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
  Serial.print("Umidade do Solo (digital): ");
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
