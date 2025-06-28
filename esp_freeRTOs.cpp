#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

// --- Configurações do Projeto ---
const int reedPin = 4;

// Configurações do Wi-Fi
const char *ssid = "A05s de Lucas S Silva";
const char *password = "pudim987";

// Rotas do servidor web
const String AlarmURL =
    "https://door-alarm-backend-0qfm.shuttle.app/api/notify";
const String NotifyClosedURL =
    "https://door-alarm-backend-0qfm.shuttle.app/api/door-closed";

// Lógica do alarme
const int openForSecMax = 15;
QueueHandle_t notificationQueue; // Fila para comunicar qual notificação enviar

enum NotificationType { NOTIFY_DOOR_CLOSED, NOTIFY_ALARM };

void sendHttpRequest(const String &url) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi não está conectado!");
    return;
  }

  HTTPClient http;
  http.begin(url.c_str());
  // request POST
  int httpCode = http.POST("");

  if (httpCode > 0) {
    Serial.printf("[HTTP] Requisição enviada para %s. Código de resposta: %d\n",
                  url.c_str(), httpCode);
  } else {
    Serial.printf("[HTTP] Erro ao enviar requisição para %s. Erro: %s\n",
                  url.c_str(), http.errorToString(httpCode).c_str());
  }

  http.end();
}

// task que fica lendo a entrada ali
void sensorTask(void *parameter) {
  Serial.println("Task do sensor iniciada.");
  int lastState = digitalRead(reedPin);
  int openCounter = 0;

  for (;;) {
    int currentState = digitalRead(reedPin);

    if (currentState == HIGH) {
      if (lastState == LOW) {
        Serial.println("[Sensor] Porta fechada.");
        NotificationType notification = NOTIFY_DOOR_CLOSED;
        // Envia notificação de "porta fechada" para a fila
        xQueueSend(notificationQueue, &notification, portMAX_DELAY);
      }
      openCounter = 0; // Reseta o contador de porta aberta
    } else {           // Porta está aberta (circuito aberto)
      openCounter++;
      Serial.printf("[Sensor] Porta aberta por %d segundos...\n", openCounter);

      if (openCounter > openForSecMax) {
        Serial.println("[Sensor] ALARME! Enviando para a fila de notificação.");
        NotificationType notification = NOTIFY_ALARM;
        // Envia notificação de "alarme" para a fila
        xQueueSend(notificationQueue, &notification, portMAX_DELAY);
        openCounter = 0; // Reseta para não enviar múltiplos alarmes
      }
    }

    lastState = currentState;
    // Pausa a task por 1 segundo de forma eficiente, liberando o processador
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// a task que fica esperando e envia as notificações quando estão prontas
void networkTask(void *parameter) {
  Serial.println("Task de rede iniciada.");
  NotificationType receivedNotification;

  for (;;) {
    if (xQueueReceive(notificationQueue, &receivedNotification,
                      portMAX_DELAY) == pdPASS) {
      switch (receivedNotification) {
      case NOTIFY_DOOR_CLOSED:
        sendHttpRequest(NotifyClosedURL);
        break;
      case NOTIFY_ALARM:
        sendHttpRequest(AlarmURL);
        break;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(reedPin, INPUT_PULLDOWN);

  // Conexão com o Wi-Fi
  Serial.print("Conectando ao WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());

  // Uma fila de notificações, desse jeito funciona bem melhor
  notificationQueue = xQueueCreate(10, sizeof(NotificationType));

  // Copiei da documentação deles
  // isso fixa alguns núcleos para ter um pouco mais de performance
  // mas sinceramente não entendo tudo não rsrs
  xTaskCreatePinnedToCore(sensorTask,   // Função da task
                          "SensorTask", // Nome da task
                          2048,         // Tamanho da pilha (stack)
                          NULL,         // Parâmetros da task
                          1,            // Prioridade
                          NULL,         // Handle da task
                          0);           // Core 0

  xTaskCreatePinnedToCore(networkTask, "NetworkTask",
                          4096, // pra que a rede fique um pouco mais rápida
                          NULL, 1, NULL, 1, );
}

void loop() {
  // fica fazio mesmo
}