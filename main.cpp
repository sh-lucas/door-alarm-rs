#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

const int reedPin = 4;
const long interval = 1000;
unsigned long lastTime = 0;

const int alarmTime = 15;

// configurações do Wi-Fi
const char *ssid = "A05s de Lucas S Silva";
const char *password = "pudim987";
// rota do meu servidor web
String AlarmURL = "https://door-alarm-backend-0qfm.shuttle.app/api/notify";
String NotifyClosedURL =
    "https://door-alarm-backend-0qfm.shuttle.app/api/door-closed";

void setup() {
  Serial.begin(9600);
  pinMode(reedPin, INPUT_PULLDOWN);

  // conexão wifi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.localIP());
}

void notify_close() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi não está conectado!");
    return;
  }

  HTTPClient http;

  http.begin(NotifyClosedURL.c_str());
  int httpCode = http.POST("");
  if (httpCode > 0) {
    Serial.printf("Ping enviado! Código de resposta do servidor: %d\n",
                  httpCode);
    String payload = http.getString();
    Serial.println("Resposta do servidor:");
    Serial.println(payload);
  } else {
    Serial.printf("Erro ao enviar o ping. Código de erro: %s\n",
                  http.errorToString(httpCode).c_str());
  }

  http.end();
}

void notify_alarm() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi não está conectado!");
    return;
  }

  HTTPClient http;

  http.begin(AlarmURL.c_str());
  int httpCode = http.POST("");
  if (httpCode > 0) {
    Serial.printf("Ping enviado! Código de resposta do servidor: %d\n",
                  httpCode);
    String payload = http.getString();
    Serial.println("Resposta do servidor:");
    Serial.println(payload);
  } else {
    Serial.printf("Erro ao enviar o ping. Código de erro: %s\n",
                  http.errorToString(httpCode).c_str());
  }

  http.end();
}

int openForSec = 0;
int openForSecMax = 15;
void loop() {
  unsigned long currentTime = millis();
  if (currentTime - lastTime >= interval) {
    lastTime = currentTime;
    // Lê o D4 pra ver se o reedswitch fehcou ou ta aberto ainda
    if (digitalRead(reedPin) == HIGH) {
      Serial.println("Corrente passando (fechou circuito!)");
      openForSec = 0;
      notify_close();
    } else {
      Serial.println("Aberto (ta safe ainda)");
      if (openForSec++ > openForSecMax) {
        notify_alarm();
      }
    }
  }
  delay(50);
}