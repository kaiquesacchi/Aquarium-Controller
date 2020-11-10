#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/* WiFi and Blynk Config */
#define BLYNK_AUTH_TOKEN "PTGGk0zD5ys9M35LVHWxw8SCgKvrhXkV"
#define WIFI_SSID "GalaxyS8"
#define WIFI_PASSWORD "abcd1234"

/* Pins */
// Pinos reais.
#define PIN_THERMOMETER_DATA 4  // IN:  Conectada ao DS18B20 (D4).
#define PIN_RELAY_LIGHT 27      // OUT: Conectado ao relé - canal da lâmpada LED. Ativo Baixo.
#define PIN_RELAY_HEATER 14     // OUT: Conectado ao relé - canal do aquecedor. Ativo Baixo.
// Pinos virtuais.
#define VPIN_TEMPERATURE V0               // OUT: Pino virtual com valor da temperatura medida.
#define VPIN_MAX_TEMPERATURE V1           // IN:  Temp. máxima da faixa aceitável. 
#define VPIN_MIN_TEMPERATURE V2           // IN:  Temp. mínima da faixa aceitável.
#define VPIN_SENSOR_ERROR V3              // OUT: Erro de medida (problema no sensor).
#define VPIN_HIGH_TEMPERATURE_WARNING V4  // OUT: Temperatura acima do limite superior.
#define VPIN_LOW_TEMPERATURE_WARNING V5   // OUT: Temperatura abaixo do limite inferior.
#define VPIN_LIGHT_STATE V6               // IN:  Controla a lâmpada LED.

/* Constants */
#define SENSOR_ERROR -127.0


/* Setup */
// Conexão ao sensor de temperatura.
OneWire oneWire(PIN_THERMOMETER_DATA);  // Instância OneWire para comunicação com o sensor.
DallasTemperature sensors(&oneWire);    // Instância DallasTemperature com referência ao sensor.

// Instanciação dos LEDs virtuais.
WidgetLED LED_sensor_error(VPIN_SENSOR_ERROR);
WidgetLED LED_high_temperature_warning(VPIN_HIGH_TEMPERATURE_WARNING);
WidgetLED LED_low_temperature_warning(VPIN_LOW_TEMPERATURE_WARNING);

// Inicialização de variáveis globais.
BlynkTimer timer;
float max_temperature = 100.0;
float min_temperature = -100.0;
bool light_state = false;
BLYNK_WRITE(VPIN_MAX_TEMPERATURE) {max_temperature = param.asFloat();}
BLYNK_WRITE(VPIN_MIN_TEMPERATURE) {min_temperature = param.asFloat();}
BLYNK_WRITE(VPIN_LIGHT_STATE) {light_state = param.asInt();}

void setup() {
  // Declaração do uso da GPIO.
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_RELAY_LIGHT, OUTPUT);
  pinMode(PIN_RELAY_HEATER, OUTPUT);

  // Inicialização dos pinos.
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(PIN_RELAY_LIGHT, HIGH);
  digitalWrite(PIN_RELAY_HEATER, HIGH);

  // Inicialização de conexões e timers.
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);
  sensors.begin();  // Inicia conexão ao sensor.
  timer.setInterval(2000L, timerEvent);
}

void loop() {
  Blynk.run();
  timer.run();
}

void timerEvent() {
  /* Controle da Lâmpada (Ativo baixo). */
  if (light_state) digitalWrite(PIN_RELAY_LIGHT, LOW);  
  else digitalWrite(PIN_RELAY_LIGHT, HIGH);
  
  /* Medição de temperatura e Controle do Aquecedor */
  // Indica início da medição.
  digitalWrite(LED_BUILTIN, HIGH);
  
  // Medição de temperatura.
  sensors.requestTemperatures(); 
  float temperatureC = sensors.getTempCByIndex(0);
  
  // Temperatura Inválida (problema no sensor).
  if (temperatureC == SENSOR_ERROR) {
    LED_sensor_error.on();
    return;
  }
  LED_sensor_error.off();

  // Checa se temperatura está na faixa aceitável.
  if (temperatureC > max_temperature) LED_high_temperature_warning.on();
  else LED_high_temperature_warning.off();
  if (temperatureC < min_temperature){
    LED_low_temperature_warning.on();
    digitalWrite(PIN_RELAY_HEATER, LOW); // Ativa aquecedor (Ativo baixo).
  }
  else{
    LED_low_temperature_warning.off();
    digitalWrite(PIN_RELAY_HEATER, HIGH); // Ativa aquecedor (Ativo baixo).
  }

  // Finaliza processo de medição.
  Blynk.virtualWrite(VPIN_TEMPERATURE, temperatureC);
  digitalWrite(LED_BUILTIN, LOW);
}
