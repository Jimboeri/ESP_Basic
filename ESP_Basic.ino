/****************************************************************************************************************************
   Copied from AutoConnectWithFSParameters.ino

   Licensed under MIT license
   Version: 1.0.0

   Version Modified By   Date      Comments
   ------- -----------  ---------- -----------
    1.0.0   J West      07/10/2019 Initial coding

 *****************************************************************************************************************************/
#include "param.h"

String swVersion = "1.0.0";
String swCode = "ESP_BASIC";

//Ported to ESP32
#ifdef ESP32
#include <FS.h>
#include "SPIFFS.h"
#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiClient.h>

#define ESP_getChipId() ((uint32_t)ESP.getEfuseMac())
String hwVersion = "ESP32";
#else
#include <FS.h> //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>

#define ESP_getChipId() (ESP.getChipId())
String hwVersion = "ESP8266";

#endif

#define CONFIG_PIN 0 // use pin 0 for ESP32 and ESP8266

// create WiFi client
WiFiClient wClient;

#include <ESP_WiFiManager.h> //https://github.com/khoih-prog/ESP_WiFiManager

// requires ArduinoJson 6.0.0+
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson

// include MQTT client
#include <PubSubClient.h>
PubSubClient mqttClient(wClient);

char configFileName[] = "/config.json";

// SSID and PW for your Router
String Router_SSID;
String Router_Pass;

// SSID and PW for Config Portal
String AP_SSID;
String AP_PASS;

//define your default values here, if there are different values in configFileName (config.json), they are overwritten.
#define MQTT_SERVER_MAX_LEN 40
#define MQTT_SERVER_PORT_LEN 6
#define MQTT_SERVER_USER_LEN 40
#define MQTT_SERVER_PASSWORD_LEN 50
#define NODEID_LEN 20

char mqtt_server[MQTT_SERVER_MAX_LEN] = DEF_MQTT_SERVER;
char mqtt_port[MQTT_SERVER_PORT_LEN] = DEF_MQTT_SERVERPORT;
char mqtt_user[MQTT_SERVER_USER_LEN] = "";
char mqtt_password[MQTT_SERVER_PASSWORD_LEN] = "";
String nodeid = "";
long int mqtt_port_int = 1883;

String chipID = String(ESP_getChipId(), HEX);

//flag for saving data
bool shouldSaveConfig = false;

char statusTopic[50];
char dataTopic[50];
char controlTopic[50];

// ###########################################################################

void saveConfigCallback(void)
//callback notifying us of the need to save config
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

// ##########################################################################
bool loadSPIFFSConfigFile(void)
{
  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("Mounting FS...");

  if (SPIFFS.begin())
  {
    Serial.println("Mounted file system");

    if (SPIFFS.exists(configFileName))
    {
      //file exists, reading and loading
      Serial.println("Reading config file");
      File configFile = SPIFFS.open(configFileName, "r");

      if (configFile)
      {
        Serial.print("Opened config file, size = ");
        size_t configFileSize = configFile.size();
        Serial.println(configFileSize);

        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[configFileSize + 1]);

        configFile.readBytes(buf.get(), configFileSize);

        Serial.print("\nJSON parseObject() result : ");

        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get(), configFileSize);

        if (deserializeError)
        {
          Serial.println("JSON deserialise failed");
          return false;
        }
        else
        {
          Serial.println("JSON deserialise OK");

          if (json["mqtt_server"])
          {
            Serial.println("MQTT Server found");
            strncpy(mqtt_server, json["mqtt_server"], sizeof(mqtt_server));
          }
          if (json["mqtt_port"])
            strncpy(mqtt_port, json["mqtt_port"], sizeof(mqtt_port));

          if (json["mqtt_user"])
          {
            strncpy(mqtt_user, json["mqtt_user"], sizeof(mqtt_user));
          }

          if (json["mqtt_password"])
            strncpy(mqtt_password, json["mqtt_password"], sizeof(mqtt_password));
        }

        //serializeJson(json, Serial);
        serializeJsonPretty(json, Serial);
        configFile.close();
      }
    }
  }
  else
  {
    Serial.println("failed to mount FS");
    SPIFFS.format();
#ifdef ESP8266
    ESP.reset();
#else //ESP32
    ESP.restart();
#endif
    delay(5000);
    return false;
  }
  return true;
}

// ##################################################################################
bool saveSPIFFSConfigFile(void)
{
  Serial.println("Saving config");

#if (ARDUINOJSON_VERSION_MAJOR >= 6)
  DynamicJsonDocument json(1024);
#else
  DynamicJsonBuffer jsonBuffer;
  JsonObject &json = jsonBuffer.createObject();
#endif

  json["mqtt_server"] = mqtt_server;
  json["mqtt_port"] = mqtt_port;
  json["mqtt_user"] = mqtt_user;
  json["mqtt_password"] = mqtt_password;

  File configFile = SPIFFS.open(configFileName, "w");

  if (!configFile)
  {
    Serial.println("Failed to open config file for writing");
  }

  //serializeJson(json, Serial);
  serializeJsonPretty(json, Serial);
  // Write data to file and close it
  serializeJson(json, configFile);
  configFile.close();
  //end save
}

// #########################################################################
void heartBeatPrint(void)
{
  static int num = 1;

  if (WiFi.status() == WL_CONNECTED)
    Serial.print("H"); // H means connected to WiFi
  else
    Serial.print("F"); // F means not connected to WiFi

  if (num == 80)
  {
    Serial.println();
    num = 1;
  }
  else if (num++ % 10 == 0)
  {
    Serial.print(" ");
  }
}

// #################################################################################
void run_config_portal()
{

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  Serial.println("Running config portal");

  ESP_WMParameter custom_mqtt_server("mqtt_server", "MQTT Server", mqtt_server, MQTT_SERVER_MAX_LEN + 1);
  ESP_WMParameter custom_mqtt_port("mqtt_port", "MQTT Port", mqtt_port, MQTT_SERVER_PORT_LEN + 1);
  ESP_WMParameter custom_mqtt_user("mqtt_user", "MQTT User", mqtt_user, MQTT_SERVER_USER_LEN + 1);
  ESP_WMParameter custom_mqtt_password("mqtt_password", "MQTT Password", mqtt_password, MQTT_SERVER_PASSWORD_LEN + 1);

  ESP_WiFiManager ESP_wifiManager("AutoConnect");

  //add all your parameters here
  ESP_wifiManager.addParameter(&custom_mqtt_server);
  ESP_wifiManager.addParameter(&custom_mqtt_port);
  ESP_wifiManager.addParameter(&custom_mqtt_user);
  ESP_wifiManager.addParameter(&custom_mqtt_password);

  //set config save notify callback
  ESP_wifiManager.setSaveConfigCallback(saveConfigCallback);

  // SSID for Config Portal
  AP_SSID = "ESP_" + chipID;
  char cAP_SSID[25];
  AP_SSID.toCharArray(cAP_SSID, 25);

  ESP_wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  if (!ESP_wifiManager.startConfigPortal(cAP_SSID))
  {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
#ifdef ESP8266
    ESP.reset();
#else //ESP32
    ESP.restart();
#endif
    delay(5000);
  }

  strncpy(mqtt_server, custom_mqtt_server.getValue(), sizeof(mqtt_server));
  strncpy(mqtt_port, custom_mqtt_port.getValue(), sizeof(mqtt_port));
  mqtt_port_int = atoi(custom_mqtt_port.getValue());
  //Serial.println(mqtt_port_int);
  strncpy(mqtt_user, custom_mqtt_user.getValue(), sizeof(mqtt_user));
  strncpy(mqtt_password, custom_mqtt_password.getValue(), sizeof(mqtt_password));

  //save the custom parameters to FS
  if (shouldSaveConfig)
  {
    saveSPIFFSConfigFile();
  }
#ifdef ESP8266
  ESP.reset();
#else //ESP32
  ESP.restart();
#endif
  delay(5000);
}

// ################################################################################
void check_status()
{
  static ulong checkstatus_timeout = 0;
  static ulong sendstatus_timeout = 0;
  static ulong configPin_timeout = 0;
  static ulong currentMillis;
  int buttonState;

#define HEARTBEAT_INTERVAL 10000L
#define STATUS_INTERVAL 600000L  // 10 min
#define CONFIGPIN_INTERVAL 1000L // 1 sec

  currentMillis = millis();

  // Print hearbeat every HEARTBEAT_INTERVAL (10) seconds.
  if ((currentMillis > checkstatus_timeout) || (checkstatus_timeout == 0))
  {
    heartBeatPrint();
    checkstatus_timeout = currentMillis + HEARTBEAT_INTERVAL;
  }

  // Check for long button press, then  run config portal
  buttonState = digitalRead(CONFIG_PIN);
  if (buttonState == 0) // 0 means pin is triggered
  {
    if (configPin_timeout == 0) // first noted
    {
      Serial.println("Button triggered");
      configPin_timeout = currentMillis; // the time first seen
    }
    else
    {
      if (currentMillis > (configPin_timeout + CONFIGPIN_INTERVAL))
      {
        run_config_portal();
      }
    }
  }
  else
  {
    configPin_timeout = 0; // reset the timer
  }

  // Check if wifi is OK, if down, run config portal
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("Wifi status is: ");
    Serial.println(WiFi.status());
    delay(10000); // 5 sec just to see
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.print("Wifi status is: ");
      Serial.println(WiFi.status());
      run_config_portal();
    }
  }

  //check for mqtt connection
  if (!mqttClient.connected())
  {
    mqtt_reconnect();
  }
  mqttClient.loop();

  // send sttus messages regularly
  if ((currentMillis > sendstatus_timeout))
  {
    sendstatus_timeout = currentMillis + STATUS_INTERVAL;
    DynamicJsonDocument payload(1024);
    payload["NodeID"] = nodeid;
    payload["status"] = "OK";
    payload["SW_Version"] = swVersion;
    payload["SW"] = swCode;
    payload["HW"] = hwVersion;
    payload["uptime"] = currentMillis / 60000;

    char cPayload[1024];
    serializeJson(payload, cPayload, 1024);
    mqttClient.publish(statusTopic, cPayload);
  }
}

// ############################################################################
void mqtt_reconnect()
// Function to reconnect to MQTT if the connection is not there
{
  // Loop until we're reconnected
  int retries = 0;
  //Serial.print("IP address: ");
  //Serial.println(WiFi.localIP());

  //Serial.print("Mqtt user: ");
  //Serial.print(mqtt_user);
  //Serial.print(" Mqtt password: ");
  //Serial.println(mqtt_password);

  String clientId = "ESPClient-";
  clientId += chipID;
  //clientId += String(random(0xffff), HEX);
  Serial.print("MQTT Client ID : ");
  Serial.println(clientId);

  while (!mqttClient.connected())
  {
    Serial.print("Attempting MQTT connection...");

    // Attempt to connect
    if (mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_password))
      //if (mqttClient.connect(clientId.c_str()))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...

      DynamicJsonDocument payload(1024);
      payload["NodeID"] = nodeid;
      payload["status"] = "newConnection";
      payload["IPaddress"] = WiFi.localIP().toString();
      payload["SW_Version"] = swVersion;
      payload["SW"] = swCode;
      payload["HW"] = hwVersion;

      char cPayload[1024];
      serializeJson(payload, cPayload, 1024);
      mqttClient.publish(statusTopic, cPayload);
      // put any subscriptions here
      // If the subscriptions are disabled the system will continuously reconnect to the broker. But that does not seem to be an issue
      mqttClient.subscribe(controlTopic);
      // Not receiving messages subscribed to, workl on this later
      Serial.print("Subscribed to : ");
      Serial.println(controlTopic);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      retries++;
      if (retries > 10)
      {
        Serial.println("Multiple failures connecting to MQTT server, run config");
        run_config_portal();
      }
      delay(5000);
    }
  }
}

// ##################################################################################
void MQTTcallback(char* topic, byte* payload, unsigned int length) {

  Serial.println("");
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);

  Serial.print("Message:");

  // If you want the payload as a string
  // String sPl = String((char *)payload);
  // Serial.println(sPl);

  // If the payload should be JSON
  char pl[length];

  for (int i = 0; i < length; i++) {
    pl[i] = (char)payload[i];
  }
  //pl[length + 1] = '\0';

  DynamicJsonDocument json(length + 1);
  DeserializationError error = deserializeJson(json, pl);
  if (error) {
    Serial.println(pl);
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }
  else
  {
    serializeJsonPretty(json, Serial);
  }
  Serial.println("-----------------------");
}

// ##################################################################################
void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("\nStarting " + swCode);

  chipID.toUpperCase();
  nodeid = "ESP-" + chipID;

  loadSPIFFSConfigFile();

  // Use this to default DHCP hostname to ESP8266-XXXXXX or ESP32-XXXXXX
  //ESP_WiFiManager ESP_wifiManager;
  // Use this to personalize DHCP hostname (RFC952 conformed)
  ESP_WiFiManager ESP_wifiManager("AutoConnect-Basic");

  //reset settings - for testing
  //ESP_wifiManager.resetSettings();

  ESP_wifiManager.setDebugOutput(true);

  //set minimum quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //ESP_wifiManager.setMinimumSignalQuality();

  //set custom ip for portal
  ESP_wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 100, 1), IPAddress(192, 168, 100, 1), IPAddress(255, 255, 255, 0));

  ESP_wifiManager.setMinimumSignalQuality(-1);
  // Set static IP, Gateway, Subnetmask, DNS1 and DNS2. New in v1.0.5+
  //ESP_wifiManager.setSTAStaticIPConfig(IPAddress(192, 168, 3, 54), IPAddress(192, 168, 3, 1), IPAddress(255, 255, 255, 0),
  //                                     IPAddress(192, 168, 3, 1), IPAddress(8, 8, 8, 8));

  // We can't use WiFi.SSID() in ESP32 as it's only valid after connected.
  // SSID and Password stored in ESP32 wifi_ap_record_t and wifi_config_t are also cleared in reboot
  // Have to create a new function to store in EEPROM/SPIFFS for this purpose
  Router_SSID = ESP_wifiManager.WiFi_SSID();
  Router_Pass = ESP_wifiManager.WiFi_Pass();

  //Remove this line if you do not want to see WiFi password printed
  //Serial.println("\nStored: SSID = " + Router_SSID + ", Pass = " + Router_Pass);

  if (Router_SSID != "")
  {
    ESP_wifiManager.setConfigPortalTimeout(120); //If no access point name has been previously entered disable timeout.
    Serial.println("Got stored Credentials. Timeout 120s");
  }
  else
  {
    Serial.println("No stored Credentials. No timeout");
    run_config_portal();
  }

  // Get Router SSID and PASS from EEPROM

  char cSSID[50];
  Router_SSID.toCharArray(cSSID, 50);
  char cPW[50];
  Router_Pass.toCharArray(cPW, 50);

  WiFi.begin(cSSID, cPW);
  while (WiFi.status() != WL_CONNECTED)
  { // Wait for the Wi-Fi to connect
    delay(500);
    Serial.print('.');
    if (millis() > 60000) // give up after a minute
    {
      run_config_portal();
    }
  }

  Serial.println('\n');
  Serial.println("Connection established!");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());

  //if you get here you have connected to the WiFi
  Serial.println("WiFi connected");

  // Get MQTT parameters
  ESP_WMParameter custom_mqtt_server("mqtt_server", "MQTT Server", mqtt_server, MQTT_SERVER_MAX_LEN + 1);
  ESP_WMParameter custom_mqtt_port("mqtt_port", "MQTT Port", mqtt_port, MQTT_SERVER_PORT_LEN + 1);

  strncpy(mqtt_server, custom_mqtt_server.getValue(), sizeof(mqtt_server));
  strncpy(mqtt_port, custom_mqtt_port.getValue(), sizeof(mqtt_port));
  mqtt_port_int = atoi(custom_mqtt_port.getValue());

  Serial.print("Mqtt host : ");
  Serial.print(mqtt_server);
  Serial.print(" Mqtt port : ");
  Serial.println(mqtt_port_int);

  mqttClient.setServer(mqtt_server, mqtt_port_int);
  mqttClient.setCallback(MQTTcallback);

  String sTopic = MQTT_TOPIC "/Network/" + nodeid;
  sTopic.toCharArray(statusTopic, 50);
  sTopic = MQTT_TOPIC "/Node/" + nodeid;
  sTopic.toCharArray(dataTopic, 50);
  sTopic = MQTT_TOPIC "/Control/" + nodeid;
  sTopic.toCharArray(controlTopic, 50);

  pinMode(CONFIG_PIN, INPUT_PULLUP);
}

// #####################################################################################
void loop()
{
  // put your main code here, to run repeatedly:
  check_status();
}
