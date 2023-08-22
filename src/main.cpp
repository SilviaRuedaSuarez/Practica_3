#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <stdlib.h>

#include "data.h"
#include "Settings.h"

#include <UbiConstants.h>
#include <UbidotsEsp32Mqtt.h>
#include <UbiTypes.h>

#include <stdio.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>

#define BUTTON_LEFT 0        // btn activo en bajo
#define LONG_PRESS_TIME 3000 // 3000 milis = 3s

#define DHTPIN 27
#define DHTTYPE DHT11
#define MI_ABS(x) ((x) < 0 ? -(x) : (x))

TFT_eSPI tft = TFT_eSPI();
DHT dht(DHTPIN, DHTTYPE);

WebServer server(80);

Settings settings;
int lastState = LOW; // para el btn
int currentState;    // the current reading from the input pin
unsigned long pressedTime = 0;
unsigned long releasedTime = 0;

const char *UBIDOTS_TOKEN = "BBFF-rva1GqRKd6wMpqEcSWFtE4mDQ2lH0z"; // Put here your Ubidots TOKEN
const char *WIFI_SSID = "UPBWiFi";                                 // Put here your Wi-Fi SSID
const char *WIFI_PASS = "";                                        // Put here your Wi-Fi password
const char *DEVICE_LABEL = "Practica3";                            // Put here your Device label to which data will be published
const char *VARIABLE_LABEL1 = "sw1";
const char *VARIABLE_LABEL2 = "sw2";
const char *TEMPERATURA_VARIABLE_LABEL = "temp"; // Temperatura
const char *HUMEDAD_VARIABLE_LABEL = "hum";         // humedad

const int PUBLISH_FREQUENCY = 5000; // Update rate in milliseconds

unsigned long timer;

Ubidots ubidots(UBIDOTS_TOKEN);

int tamano;
int posicion;
char boton = '0';
char val = '0';

bool sw1State = false;             // Estado inicial del sw1 (apagado)
bool sw2State = false;             // Estado inicial del sw2 (apagado)


const int LED_PIN1 = 26;
const int LED_PIN2 = 2;

void load404();
void loadIndex();
void loadFunctionsJS();
void restartESP();
void saveSettings();
bool is_STA_mode();
void AP_mode_onRst();
void STA_mode_onRst();
void detect_long_press();

/******
 * Auxiliar Functions
 ******/

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  tamano = strlen(topic);
  posicion = tamano - 4;
  printf("switch: %c\n", topic[posicion]);
  boton = topic[posicion];
  val = payload[0];
  if (boton == '1')

    if ((char)payload[0] == '1')
    {
      sw1State = true;                          // Cambiar estado de sw1 a encendido
      tft.fillCircle(190, 25, 10, TFT_PURPLE); // Dibujar círculo Morado
    }
    else
    {
      sw1State = false;                        // Cambiar estado de sw1 a apagado
      tft.fillCircle(190, 25, 10, TFT_DARKGREY); // Dibujar círculo gris
    }
  if (boton == '2')
    if ((char)payload[0] == '1')
    {
      sw2State = true;                         // Cambiar estado de sw2 a encendido
      tft.fillCircle(220, 25, 10, TFT_NAVY); // Dibujar círculo azul
    }
    else
    {
      sw2State = false;                        // Cambiar estado de sw2 a apagado
      tft.fillCircle(220, 25, 10,TFT_DARKGREY); // Dibujar círculo gris
    }
}

// Rutina para iniciar en modo AP (Access Point) "Servidor"
void startAP()
{
  WiFi.disconnect();
  delay(19);
  Serial.println("Starting WiFi Access Point (AP)");
  WiFi.softAP("Flor", "Girasol123");
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
}

// Rutina para iniciar en modo STA (Station) "Cliente"
void start_STA_client()
{
  WiFi.softAPdisconnect(true);
  WiFi.disconnect();
  delay(100);
  Serial.println("Starting WiFi Station Mode");
  WiFi.begin((const char *)settings.ssid.c_str(), (const char *)settings.password.c_str());
  WiFi.mode(WIFI_STA);

  int cnt = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    // Serial.print(".");
    if (cnt == 100) // Si después de 100 intentos no se conecta, vuelve a modo AP
      AP_mode_onRst();
    cnt++;
    Serial.println("attempt # " + (String)cnt);
  }

  WiFi.setAutoReconnect(true);
  Serial.println(F("WiFi connected"));
  Serial.println(F("IP address: "));
  Serial.println(WiFi.localIP());
  pressedTime = millis();
  // Rutinas de Ubidots
  // put your setup code here, to run once:
  tft.init();
  tft.fillScreen(TFT_WHITE);
  tft.setRotation(3);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawString("Realizado por:", 10, 5, 2);
  tft.setTextColor(TFT_MAGENTA, TFT_WHITE);
  tft.drawString("Silvia Rueda", 10, 23, 4);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawString("Humedad", 10, 70, 2);
  tft.drawString("Temperatura", 140, 70, 2);
  tft.drawFastHLine(10, 50, 170, TFT_MAGENTA);
  tft.fillRect(110, 65, 3, 80, TFT_MAGENTA);

  tft.fillCircle(190,25, 10, TFT_DARKGREY); // Círculo Morado
  tft.fillCircle(220, 25, 10, TFT_DARKGREY); // Círculo azul

  pinMode(LED_PIN1, OUTPUT);               // Configurar el pin del LED como salida
  digitalWrite(LED_PIN1, LOW);             // Apagar el LED al inicio
  pinMode(LED_PIN2, OUTPUT);               // Configurar el pin del LED como salida
  digitalWrite(LED_PIN2, LOW);             // Apagar el LED al inicio

  ubidots.setCallback(callback);
  ubidots.setup();
  ubidots.reconnect();
  ubidots.subscribeLastValue(DEVICE_LABEL, VARIABLE_LABEL1);
  ubidots.subscribeLastValue(DEVICE_LABEL, VARIABLE_LABEL2);
  Serial.println(F("DHTxx test!"));
  dht.begin();
  timer = millis();
}

void setup()
{

  Serial.begin(115200);
  delay(2000);

  EEPROM.begin(4096);                 // Se inicializa la EEPROM con su tamaño max 4KB
  pinMode(BUTTON_LEFT, INPUT_PULLUP); // btn activo en bajo

  // settings.reset();
  settings.load(); // se carga SSID y PWD guardados en EEPROM
  settings.info(); // ... y se visualizan

  Serial.println("");
  Serial.println("starting...");

  if (is_STA_mode())
  {
    start_STA_client();
  }
  else // Modo Access Point & WebServer
  {
    startAP();

    /* ========== Modo Web Server ========== */

    /* HTML sites */
    server.onNotFound(load404);

    server.on("/", loadIndex);
    server.on("/index.html", loadIndex);
    server.on("/functions.js", loadFunctionsJS);

    /* JSON */
    server.on("/settingsSave.json", saveSettings);
    server.on("/restartESP.json", restartESP);

    server.begin();
    Serial.println("HTTP server started");
  }
}

void loop()
{
  if (is_STA_mode()) // Rutina para modo Station (cliente Ubidots)
  {
    if (!ubidots.connected())
    {
      ubidots.reconnect();
      ubidots.subscribeLastValue(DEVICE_LABEL, VARIABLE_LABEL1);
      ubidots.subscribeLastValue(DEVICE_LABEL, VARIABLE_LABEL2);
    }

    float hum = dht.readHumidity();
    float temp = dht.readTemperature();

    if ((MI_ABS(millis() - timer)) > PUBLISH_FREQUENCY) // triggers the routine every 5 seconds
    {
      Serial.print("Temperatura: ");
      Serial.print(temp);
      Serial.print(" | Humedad: ");
      Serial.println(hum);

      tft.drawString(String(temp), 150, 100);
      tft.drawString(String(hum), 20, 100);


      ubidots.add(TEMPERATURA_VARIABLE_LABEL, temp);
      ubidots.add(HUMEDAD_VARIABLE_LABEL, hum);
      ubidots.publish(DEVICE_LABEL);

      timer = millis();
    }

    ubidots.loop();
    // Cambiar color de los círculos en base a los estados de sw1 y sw2
    if (sw1State)
    {
      tft.fillCircle(190,25, 10, TFT_PURPLE);
      digitalWrite(LED_PIN1, HIGH); // Encender el LED
    }
    else
    {
      tft.fillCircle(190,25, 10, TFT_DARKGREY);
      digitalWrite(LED_PIN1, LOW); // Apagar el LED
    }

    if (sw2State)
    {
      tft.fillCircle(220, 25, 10, TFT_NAVY);
      digitalWrite(LED_PIN2, HIGH); // Encender el LED
    }
    else
    {
      tft.fillCircle(220, 25, 10,TFT_DARKGREY);
      digitalWrite(LED_PIN2, LOW); // Apagar el LED
    }
  }
  else // rutina para AP + WebServer
    server.handleClient();

  delay(10);
  detect_long_press();
}

// funciones para responder al cliente desde el webserver:
// load404(), loadIndex(), loadFunctionsJS(), restartESP(), saveSettings()

void load404()
{
  server.send(200, "text/html", data_get404());
}

void loadIndex()
{
  server.send(200, "text/html", data_getIndexHTML());
}

void loadFunctionsJS()
{
  server.send(200, "text/javascript", data_getFunctionsJS());
}

void restartESP()
{
  server.send(200, "text/json", "true");
  ESP.restart();
}

void saveSettings()
{
  if (server.hasArg("ssid"))
    settings.ssid = server.arg("ssid");
  if (server.hasArg("password"))
    settings.password = server.arg("password");

  settings.save();
  server.send(200, "text/json", "true");
  STA_mode_onRst();
}

// Rutina para verificar si ya se guardó SSID y PWD del cliente
// is_STA_mode retorna true si ya se guardaron
bool is_STA_mode()
{
  if (EEPROM.read(flagAdr))
    return true;
  else
    return false;
}

void AP_mode_onRst()
{
  EEPROM.write(flagAdr, 0);
  EEPROM.commit();
  delay(100);
  ESP.restart();
}

void STA_mode_onRst()
{
  EEPROM.write(flagAdr, 1);
  EEPROM.commit();
  delay(100);
  ESP.restart();
}

void detect_long_press()
{
  // read the state of the switch/button:
  currentState = digitalRead(BUTTON_LEFT);

  if (lastState == HIGH && currentState == LOW) // button is pressed
    pressedTime = millis();
  else if (lastState == LOW && currentState == HIGH)
  { // button is released
    releasedTime = millis();

    // Serial.println("releasedtime" + (String)releasedTime);
    // Serial.println("pressedtime" + (String)pressedTime);

    long pressDuration = releasedTime - pressedTime;

    if (pressDuration > LONG_PRESS_TIME)
    {
      Serial.println("(Hard reset) returning to AP mode");
      delay(500);
      AP_mode_onRst();
    }
  }

  // save the the last state
  lastState = currentState;
}