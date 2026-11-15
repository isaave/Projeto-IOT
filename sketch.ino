/*
 * CCIDC 2026.2 - Projeto Final CloudTech
 * Monitoramento de Rack - ESP32 + DHT22 + LED + Adafruit IO
 *
 * Hardware (Wokwi):
 *   - ESP32 DevKit
 *   - DHT22:  VCC -> 3V3, SDA/Data -> GPIO 15, GND -> GND
 *   - LED:    Anodo -> resistor 220 ohms -> GPIO 2, Catodo -> GND
 *
 * Feeds Adafruit IO:
 *   - temperatura  (ESP32 -> nuvem, publica a cada ~4s)
 *   - led          (nuvem -> ESP32, comandos "1"/"0"; ESP32 tambem publica
 *                   o estado do exaustor nas emergencias)
 *
 * IMPORTANTE: substitua SEU_USUARIO_AQUI e SUA_CHAVE_AQUI pelas
 * credenciais reais apenas na sua copia local. Nunca publique a chave
 * real no Wokwi publico nem no AVA.
 */

#include <WiFi.h>
#include <string.h>
#include "DHTesp.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// --- Configuracoes de Rede ---
#define WLAN_SSID       "Wokwi-GUEST"
#define WLAN_PASS       ""
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "isabellaave32"
#define AIO_KEY         "SUA_CHAVE_AQUI"

// --- Pinos ---
const int DHT_PIN = 15;
const int LED_PIN = 2;

// --- Sensor e cliente MQTT ---
DHTesp dhtSensor;
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// --- Feed de Publicacao (Sensor -> Nuvem) ---
Adafruit_MQTT_Publish temp_feed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperatura");

// --- Feed de Publicacao (Exaustor/LED -> Nuvem, usado nas emergencias) ---
Adafruit_MQTT_Publish led_pub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/led");

// --- Feed de Inscricao (Nuvem -> Dispositivo) ---
Adafruit_MQTT_Subscribe led_sub = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/led");

// --- Estado local ---
bool emergencia = false;          // true quando a temperatura esta >= 28.0 C
unsigned long ultimaPubLed = 0;   // instante da ultima publicacao no feed led (anti-eco)

void MQTT_connect();
void publicarLed(const char *valor);
void processarComandoLed(char *valor);

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);

  Serial.print("Conectando ao WiFi...");
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" \nWiFi Conectado!");

  mqtt.subscribe(&led_sub);
}

void loop() {
  MQTT_connect();

  float temperaturaAtual = 0.0;

  // Laco de 4 ciclos de 1 segundo = 4 segundos entre cada envio de temperatura
  // (15 envios por minuto, conforme o desafio)
  for (int i = 0; i < 4; i++) {

    // -----------------------------------------------------------------
    // DESAFIO 1: Ler a temperatura atual do DHT22 localmente (a cada 1s).
    // -----------------------------------------------------------------
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    if (!isnan(data.temperature)) {
      temperaturaAtual = data.temperature;
    }

    // -----------------------------------------------------------------
    // DESAFIO 2 (Edge Computing):
    //  - Se temp >= 28.0: ligar o LED e, se acabou de entrar em
    //    emergencia, publicar "1" em led_pub.
    //  - Se temp < 28.0 e estava em emergencia: apagar o LED e
    //    publicar "0" em led_pub.
    // -----------------------------------------------------------------
    if (temperaturaAtual >= 28.0) {
      digitalWrite(LED_PIN, HIGH);        // liga o exaustor
      if (!emergencia) {
        emergencia = true;
        publicarLed("1");                 // avisa a nuvem: emergencia!
        Serial.println("EMERGENCIA: exaustor LIGADO (temp >= 28 C)");
      }
    } else {
      digitalWrite(LED_PIN, LOW);         // desliga o exaustor
      if (emergencia) {
        emergencia = false;
        publicarLed("0");                 // avisa a nuvem: normalizado
        Serial.println("Normalizado: exaustor DESLIGADO (temp < 28 C)");
      }
    }

    // -----------------------------------------------------------------
    // DESAFIO 3 (Escutar o Botao Manual por 1000ms):
    //  - Ignorar o "eco" de um envio feito pelo proprio ESP32.
    //  - "1"/"On": ligar o LED manualmente.
    //  - "0"/"Off": verificar a temperatura.
    //      -> >= 28.0: recusar desligamento, manter LED aceso e
    //         publicar "1" em led_pub.
    //      -> <  28.0: apagar o LED normalmente.
    // -----------------------------------------------------------------
    Adafruit_MQTT_Subscribe *subscription;
    while ((subscription = mqtt.readSubscription(1000))) {
      if (subscription == &led_sub) {
        char *valor = (char *)led_sub.lastread;

        Serial.print("Comando recebido: ");
        Serial.println(valor);

        // Ignora mensagens que chegaram ate 1s depois de uma publicacao
        // do proprio ESP32 (eco do broker).
        if (millis() - ultimaPubLed < 1000) {
          Serial.println("(eco ignorado)");
          continue;
        }

        processarComandoLed(valor);
      }
    }
  }

  // -----------------------------------------------------------------
  // DESAFIO 4: Passados os 4 segundos do laco 'for', publique 1 unica
  // medida da temperatura em temp_feed.
  // -----------------------------------------------------------------
  if (!isnan(temperaturaAtual)) {
    if (temp_feed.publish(temperaturaAtual)) {
      Serial.print("Temperatura publicada: ");
      Serial.println(temperaturaAtual);
    } else {
      Serial.println("Falha ao enviar temperatura!");
    }
  }
}

// Aplica os comandos manuais vindos do feed led
void processarComandoLed(char *valor) {
  if (strcmp(valor, "1") == 0 || strcmp(valor, "On") == 0) {
    digitalWrite(LED_PIN, HIGH);          // liga manualmente
    Serial.println("Comando manual: exaustor LIGADO");
    return;
  }

  if (strcmp(valor, "0") == 0 || strcmp(valor, "Off") == 0) {
    // Verifica a temperatura atual antes de aceitar o desligamento
    TempAndHumidity agora = dhtSensor.getTempAndHumidity();
    float t = agora.temperature;

    if (!isnan(t) && t >= 28.0) {
      // Recusa o desligamento, mantem o LED aceso e republica "1"
      digitalWrite(LED_PIN, HIGH);
      publicarLed("1");
      Serial.println("Desligamento RECUSADO: temperatura >= 28 C");
    } else {
      digitalWrite(LED_PIN, LOW);         // desliga normalmente
      Serial.println("Comando manual: exaustor DESLIGADO");
    }
  }
}

// Publica no feed led marcando o instante (para ignorar o eco do broker)
void publicarLed(const char *valor) {
  ultimaPubLed = millis();
  if (!led_pub.publish(valor)) {
    Serial.println("Falha ao publicar no feed led!");
  }
}

void MQTT_connect() {
  int8_t ret;
  if (mqtt.connected()) return;
  Serial.print("Conectando ao MQTT... ");
  while ((ret = mqtt.connect()) != 0) {
    Serial.println(mqtt.connectErrorString(ret));
    mqtt.disconnect();
    delay(5000);
  }
  Serial.println("MQTT Conectado!");
}
