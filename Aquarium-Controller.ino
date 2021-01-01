#include <BlynkSimpleEsp32.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <WiFi.h>
#include <WiFiClient.h>

/* WiFi and Blynk Config */
#define BLYNK_AUTH_TOKEN "<BLYNK_AUTH_TOKEN_HERE>"
#define WIFI_SSID "<WIFI_SSID_HERE>"
#define WIFI_PASSWORD "<WIFI_PASSWORD_HERE>"

/* GPIO */
// Real GPIO.
#define PIN_THERMOMETER_DATA 4  // IN  | OneWire | Connected to DS18B20 data cable.
#define PIN_RELAY_LIGHT 27      // OUT | bool    | Connected to relay - controls LED Lamp. Active Low.
#define PIN_RELAY_HEATER 14     // OUT | bool    | Connected to relay - controls Heater. Active Low.
// Virtual GPIO.
#define VPIN_TEMPERATURE V0               // OUT | float | Measured instant temperature.
#define VPIN_MAX_TEMPERATURE V1           // IN  | float | Max. acceptable temperature. 
#define VPIN_MIN_TEMPERATURE V2           // IN  | float | Min. acceptable temperature.
#define VPIN_SENSOR_ERROR V3              // OUT | bool  | Sensor error (invalid measuring).
#define VPIN_HIGH_TEMPERATURE_WARNING V4  // OUT | bool  | Temperature over acceptable limit.
#define VPIN_LOW_TEMPERATURE_WARNING V5   // OUT | bool  | Temperature under acceptable limit.
#define VPIN_LIGHT_STATE V6               // IN  | bool  | Controls LED Lamp.

/* Constants */
#define SENSOR_ERROR -127.0
#define SAMPLE_TIME 2                              // In seconds.
#define ALERT_LIMIT_OUT_OF_RANGE 60 / SAMPLE_TIME  // In samples.
#define ALERT_LIMIT_INVALID 10 / SAMPLE_TIME       // In samples.

/* Messages */
// Temperature out of acceptable bounds.
#define TEXT_ALERT_SUBJECT_OUT_OF_RANGE "ALERT: Temperature out of normal range."
#define TEXT_ALERT_BODY_OUT_OF_RANGE "The temperature is out of the normal range."
#define TEXT_OK_SUBJECT_OUT_OF_RANGE "OK: Temperature back to normal."
#define TEXT_OK_BODY_OUT_OF_RANGE "The temperature is back to the normal range."
// Invalid temperature measuring.
#define TEXT_ALERT_SUBJECT_INVALID "ALERT: Thermometer not working as intended."
#define TEXT_ALERT_BODY_INVALID "The thermometer might be disconnected. Be careful!"
#define TEXT_OK_SUBJECT_INVALID "OK: Thermometer is working as intended."
#define TEXT_OK_BODY_INVALID "The thermometer got reconnected."

/* Setup */
// Connecting to thermometer.
OneWire oneWire(PIN_THERMOMETER_DATA);
DallasTemperature sensors(&oneWire);
// Instantiating virtual LEDs.
WidgetLED LED_sensor_error(VPIN_SENSOR_ERROR);
WidgetLED LED_high_temperature_warning(VPIN_HIGH_TEMPERATURE_WARNING);
WidgetLED LED_low_temperature_warning(VPIN_LOW_TEMPERATURE_WARNING);
// Initializing global variables.
bool light_state = false;
int counter_invalid = 0;
int counter_out_of_range = 0;
float max_temperature = 100.0;
float min_temperature = -100.0;
BLYNK_WRITE(VPIN_MAX_TEMPERATURE) {max_temperature = param.asFloat();}
BLYNK_WRITE(VPIN_MIN_TEMPERATURE) {min_temperature = param.asFloat();}
BLYNK_WRITE(VPIN_LIGHT_STATE) {light_state = param.asInt();}
BlynkTimer timer;

void setup() {
  // Declaring GPIO usage.
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_RELAY_LIGHT, OUTPUT);
  pinMode(PIN_RELAY_HEATER, OUTPUT);

  // Initializing GPIO.
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(PIN_RELAY_LIGHT, HIGH);
  digitalWrite(PIN_RELAY_HEATER, HIGH);

  // Stablishing connections and starting the timer.
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);
  sensors.begin();
  timer.setInterval(SAMPLE_TIME * 1000L, timerEvent);
}

void loop() {
  lightControl();
  Blynk.run();
  timer.run();
}

void timerEvent() {
  sendAlerts();

  // Indicates the measuring is occuring.
  digitalWrite(LED_BUILTIN, HIGH);
  
  // Gets the measured temperature.
  sensors.requestTemperatures(); 
  float temperatureC = sensors.getTempCByIndex(0);
  
  // Checks if it's valid.
  if (!isValid(temperatureC)) return;

  // Checks if it's out of bounds.
  checkTemperatureRange(temperatureC);
  
  // Ends the process.
  Blynk.virtualWrite(VPIN_TEMPERATURE, temperatureC);
  digitalWrite(LED_BUILTIN, LOW);
}

void lightControl() {
  /* Controls the LED Lamp (Active Low) */
  if (light_state) digitalWrite(PIN_RELAY_LIGHT, LOW);  
  else digitalWrite(PIN_RELAY_LIGHT, HIGH);
}

void sendAlerts() {
  /* Emit warnings if needed */
  if (counter_invalid == ALERT_LIMIT_INVALID) {
    Blynk.email(TEXT_ALERT_SUBJECT_INVALID, TEXT_ALERT_BODY_INVALID);
    Blynk.notify(TEXT_ALERT_SUBJECT_INVALID);
    return;
  } 
  if (counter_out_of_range == ALERT_LIMIT_OUT_OF_RANGE) {
    Blynk.email(TEXT_ALERT_SUBJECT_OUT_OF_RANGE, TEXT_ALERT_BODY_OUT_OF_RANGE);
    Blynk.notify(TEXT_ALERT_SUBJECT_OUT_OF_RANGE);
  }
}

bool isValid(float temperatureC) {
  /* Checks if the measuring is valid */
  if (temperatureC == SENSOR_ERROR) {
    if (counter_invalid <= ALERT_LIMIT_INVALID) counter_invalid += 1; // Prevents overflow.
    LED_sensor_error.on();
    return false;
  }
  if (counter_invalid >= ALERT_LIMIT_INVALID) {
    // Notifies when it gets back to normal.
    Blynk.email(TEXT_OK_SUBJECT_INVALID, TEXT_OK_BODY_INVALID);
    Blynk.notify(TEXT_OK_SUBJECT_INVALID);
  }
  counter_invalid = 0;
  LED_sensor_error.off();
  return true;
}

void checkTemperatureRange(float temperatureC) {
  /* Checks if the temperature is in an acceptable range */
  if (temperatureC > max_temperature || temperatureC < min_temperature) {
    if (counter_out_of_range >= ALERT_LIMIT_OUT_OF_RANGE) counter_out_of_range += 1; // Prevents overflow.
  }
  else {
    if (counter_out_of_range >= ALERT_LIMIT_OUT_OF_RANGE) {
      // Notifies when it gets back to normal.
      Blynk.email(TEXT_OK_SUBJECT_OUT_OF_RANGE, TEXT_OK_BODY_OUT_OF_RANGE);
      Blynk.notify(TEXT_OK_SUBJECT_OUT_OF_RANGE);
    }
    counter_out_of_range = 0;
  }

  // Checks for high temperature.
  if (temperatureC > max_temperature) LED_high_temperature_warning.on();
  else LED_high_temperature_warning.off();
  
  // Checks for low temperature.
  if (temperatureC < min_temperature){
    LED_low_temperature_warning.on();
    digitalWrite(PIN_RELAY_HEATER, LOW); // Turns heater on (Active Low).
  }
  else{
    LED_low_temperature_warning.off();
    digitalWrite(PIN_RELAY_HEATER, HIGH); // Turns heater off (Active Low).
  }
}
