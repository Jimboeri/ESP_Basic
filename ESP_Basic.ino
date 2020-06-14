/****************************************************************************************************************************
   Copied from AutoConnectWithFSParameters.ino

   Licensed under MIT license
   Version: 1.0.0

   Version Modified By   Date      Comments
   ------- -----------  ---------- -----------
    1.0.0   J West      07/10/2019 Initial coding

 *****************************************************************************************************************************/

//Ported to ESP32
#ifdef ESP32
#include <FS.h>
#include "SPIFFS.h"
#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiClient.h>

#define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())

#define LED_BUILTIN       2
#define LED_ON            HIGH
#define LED_OFF           LOW

#else
#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>

#define ESP_getChipId()   (ESP.getChipId())

#define LED_ON      LOW
#define LED_OFF     HIGH
#endif

// unsure if this will fork for ESP32 and ESP8266
WiFiClient wClient;

// Pin D2 mapped to pin GPIO2/ADC12 of ESP32, or GPIO2/TXD1 of NodeMCU control on-board LED
#define PIN_LED       LED_BUILTIN

#include <ESP_WiFiManager.h>              //https://github.com/khoih-prog/ESP_WiFiManager

// Now support ArduinoJson 6.0.0+ ( tested with v6.14.1 )
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

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
#define MQTT_SERVER_MAX_LEN             40
#define MQTT_SERVER_PORT_LEN            6
#define MQTT_SERVER_USER_LEN            40
#define MQTT_SERVER_PASSWORD_LEN        50
#define NODEID_LEN                      20

char mqtt_server   [MQTT_SERVER_MAX_LEN]     = "aws2.innovateauckland.nz";
char mqtt_port     [MQTT_SERVER_PORT_LEN]    = "1883";
char mqtt_user     [MQTT_SERVER_USER_LEN]    = "";
char mqtt_password [MQTT_SERVER_PASSWORD_LEN] = "";
String nodeid = "";
long int mqtt_port_int;
String swVersion = "1.0.0";
String hwVersion = "ESP8266";

//flag for saving data
bool shouldSaveConfig = false;

char statusTopic[50];
char dataTopic[50];

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

#if (ARDUINOJSON_VERSION_MAJOR >= 6)
        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get(), configFileSize);

        if ( deserializeError )
        {
          Serial.println("failed");
          return false;
        }
        else
        {
          Serial.println("OK");

          if (json["mqtt_server"])
          {
            Serial.println("MQTT Server found");
            strncpy(mqtt_server, json["mqtt_server"], sizeof(mqtt_server));
          }
          if (json["mqtt_port"])
            strncpy(mqtt_port,   json["mqtt_port"], sizeof(mqtt_port));

          if (json["mqtt_user"])
          {
            strncpy(mqtt_user, json["mqtt_user"], sizeof(mqtt_user));
          }

          if (json["mqtt_password"])
            strncpy(mqtt_password, json["mqtt_password"], sizeof(mqtt_password));
        }

        //serializeJson(json, Serial);
        serializeJsonPretty(json, Serial);
#else
        DynamicJsonBuffer jsonBuffer;
        // Parse JSON string
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        // Test if parsing succeeds.

        if (json.success())
        {
          Serial.println("OK");

          if (json["mqtt_server"])
          {
            Serial.println("MQTT Server found");
            strncpy(mqtt_server, json["mqtt_server"], sizeof(mqtt_server));
          }

          if (json["mqtt_port"])
            strncpy(mqtt_port,   json["mqtt_port"], sizeof(mqtt_port));

          if (json["mqtt_user"]) {
            strncpy(mqtt_user,   json["mqtt_user"], sizeof(mqtt_user));
          }

          if (json["mqtt_password"])
            strncpy(mqtt_password,   json["mqtt_password"], sizeof(mqtt_password));
        }
        else
        {
          Serial.println("failed");
          return false;
        }
        //json.printTo(Serial);
        json.prettyPrintTo(Serial);
#endif

        configFile.close();
      }
    }
  }
  else
  {
    Serial.println("failed to mount FS");
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
  JsonObject& json = jsonBuffer.createObject();
#endif

  json["mqtt_server"]   = mqtt_server;
  json["mqtt_port"]     = mqtt_port;
  json["mqtt_user"]     = mqtt_user;
  json["mqtt_password"] = mqtt_password;

  File configFile = SPIFFS.open(configFileName, "w");

  if (!configFile)
  {
    Serial.println("Failed to open config file for writing");
  }

#if (ARDUINOJSON_VERSION_MAJOR >= 6)
  //serializeJson(json, Serial);
  serializeJsonPretty(json, Serial);
  // Write data to file and close it
  serializeJson(json, configFile);
#else
  //json.printTo(Serial);
  json.prettyPrintTo(Serial);
  // Write data to file and close it
  json.printTo(configFile);
#endif

  configFile.close();
  //end save
}

// #########################################################################
void heartBeatPrint(void)
{
  static int num = 1;

  if (WiFi.status() == WL_CONNECTED)
    Serial.print("H");        // H means connected to WiFi
  else
    Serial.print("F");        // F means not connected to WiFi

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
void toggleLED()
{
  //toggle state
  digitalWrite(PIN_LED, !digitalRead(PIN_LED));
}

// ################################################################################
void check_status()
{
  static ulong checkstatus_timeout  = 0;
  static ulong LEDstatus_timeout    = 0;
  static ulong sendstatus_timeout  = 0;
  static ulong currentMillis;


#define HEARTBEAT_INTERVAL    10000L
#define LED_INTERVAL          2000L
#define STATUS_INTERVAL       600000L   // 10 min

  currentMillis = millis();

  if ((currentMillis > LEDstatus_timeout) || (LEDstatus_timeout == 0))
  {
    // Toggle LED at LED_INTERVAL = 2s
    toggleLED();
    LEDstatus_timeout = currentMillis + LED_INTERVAL;
  }

  // Print hearbeat every HEARTBEAT_INTERVAL (10) seconds.
  if ((currentMillis > checkstatus_timeout) || (checkstatus_timeout == 0))
  {
    heartBeatPrint();
    checkstatus_timeout = currentMillis + HEARTBEAT_INTERVAL;
  }

  if (!mqttClient.connected()) {
    mqtt_reconnect();
  }
  mqttClient.loop();

  if ((currentMillis > sendstatus_timeout))
  {
    sendstatus_timeout = currentMillis + STATUS_INTERVAL;
      DynamicJsonDocument payload(1024);
      payload["NodeID"] = nodeid;
      payload["status"] = "OK";
      payload["SW_Version"] = swVersion;
      payload["HW"] = hwVersion;
      payload["uptime"] = currentMillis / 60000;

      char cPayload[1024];
      serializeJson(payload, cPayload, 1024);
      mqttClient.publish(statusTopic, cPayload);

  }


}

// ############################################################################
void mqtt_reconnect() {
  // Loop until we're reconnected
  int retries = 0;
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESPClient-";
    clientId += String(random(0xffff), HEX);
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
      payload["HW"] = hwVersion;

      char cPayload[1024];
      serializeJson(payload, cPayload, 1024);
      mqttClient.publish(statusTopic, cPayload);
      // ... and resubscribe
      //client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      retries++;
      if (retries > 10)
      {
        Serial.println("Multiple failures connecting to MQTT server, restarting");
#ifdef ESP8266
        ESP.reset();
#else   //ESP32
        ESP.restart();
#endif
      }
      delay(5000);
    }
  }
}


// ##################################################################################
void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("\nStarting AutoConnectWithFSParams");

  loadSPIFFSConfigFile();

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length

  ESP_WMParameter custom_mqtt_server   ("mqtt_server",   "MQTT Server",   mqtt_server,   MQTT_SERVER_MAX_LEN + 1);
  ESP_WMParameter custom_mqtt_port     ("mqtt_port",     "MQTT Port",     mqtt_port,     MQTT_SERVER_PORT_LEN + 1);
  ESP_WMParameter custom_mqtt_user     ("mqtt_user",     "MQTT User",     mqtt_user,     MQTT_SERVER_USER_LEN + 1);
  ESP_WMParameter custom_mqtt_password ("mqtt_password", "MQTT Password", mqtt_password, MQTT_SERVER_PASSWORD_LEN + 1);

  // Use this to default DHCP hostname to ESP8266-XXXXXX or ESP32-XXXXXX
  //ESP_WiFiManager ESP_wifiManager;
  // Use this to personalize DHCP hostname (RFC952 conformed)
  ESP_WiFiManager ESP_wifiManager("AutoConnect-FSParams");

  //set config save notify callback
  ESP_wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here

  ESP_wifiManager.addParameter(&custom_mqtt_server);
  ESP_wifiManager.addParameter(&custom_mqtt_port);
  ESP_wifiManager.addParameter(&custom_mqtt_user);
  ESP_wifiManager.addParameter(&custom_mqtt_password);

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
  Serial.println("\nStored: SSID = " + Router_SSID + ", Pass = " + Router_Pass);

  if (Router_SSID != "")
  {
    ESP_wifiManager.setConfigPortalTimeout(120); //If no access point name has been previously entered disable timeout.
    Serial.println("Got stored Credentials. Timeout 120s");
  }
  else
  {
    Serial.println("No stored Credentials. No timeout");
  }

  String chipID = String(ESP_getChipId(), HEX);
  chipID.toUpperCase();

  // SSID and PW for Config Portal
  AP_SSID = "ESP_" + chipID + "_AutoConnectAP";
  AP_PASS = "MyESP_" + chipID;
  nodeid = "ESP_" + chipID;

  // Get Router SSID and PASS from EEPROM, then open Config portal AP named "ESP_XXXXXX_AutoConnectAP" and PW "MyESP_XXXXXX"
  // 1) If got stored Credentials, Config portal timeout is 60s
  // 2) If no stored Credentials, stay in Config portal until get WiFi Credentials
  if (!ESP_wifiManager.autoConnect(AP_SSID.c_str()))
  {
    Serial.println("failed to connect and hit timeout");

    //reset and try again, or maybe put it to deep sleep
#ifdef ESP8266
    ESP.reset();
#else   //ESP32
    ESP.restart();
#endif
    delay(1000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("WiFi connected");

  //read updated parameters

  strncpy(mqtt_server,   custom_mqtt_server.getValue(),   sizeof(mqtt_server));
  strncpy(mqtt_port,     custom_mqtt_port.getValue(),     sizeof(mqtt_port));
  mqtt_port_int = atoi(custom_mqtt_port.getValue());
  //Serial.println(mqtt_port_int);
  strncpy(mqtt_user,     custom_mqtt_user.getValue(),     sizeof(mqtt_user));
  strncpy(mqtt_password, custom_mqtt_password.getValue(), sizeof(mqtt_password));

  //save the custom parameters to FS
  if (shouldSaveConfig)
  {
    saveSPIFFSConfigFile();
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  mqttClient.setServer(mqtt_server, mqtt_port_int);
  String sTopic = "AKLC/Network/" + nodeid;
  sTopic.toCharArray(statusTopic, 50);
  sTopic = "AKLC/Node/" + nodeid;
  sTopic.toCharArray(dataTopic, 50);

}

// #####################################################################################
void loop()
{
  // put your main code here, to run repeatedly:
  check_status();



}