# ESP_Basic

This project supplies a template for future projects that are based on ESP8266 or ESP32

## Credits
This is based on [WiFiManager](https://github.com/tzapu/WiFiManager), and in particular the example [AutoConnectWithFSParameters](https://github.com/tzapu/WiFiManager/tree/master/examples/AutoConnectWithFSParameters)

## Basic functionality required
* Web portal to update parameters
  - Wifi details
  - MQTT settings
* Send status updates on startup and at regular intervals over MQTT
* Be able to go into config mode if a button is pressed for a second
* Run the config portal if WiFi connection fails, or if MQTT cannot connect

## Dependencies
* [ESP8266WiFi.h](https://github.com/esp8266/Arduino)
* [ESP_WiFiManager.h](https://github.com/khoih-prog/ESP_WiFiManager)
* [ArduinoJson.h](https://github.com/bblanchon/ArduinoJson) (Must be version 6 or greater)
* [PubSubClient.h](https://pubsubclient.knolleary.net/)

## How to use
### Software
* Clone this repository
* Open in Arduino (or favourite editor) and save to your new name
* Change the String `swCode` to the name of your sketch
* Change the String `swVersion` to your version number
* Modify the setup() function
* Add your code in loop()
### Hardware
* Wire a normally open switch between GND and pin 0. You may need to check the documentation of your board, GPIO0 is called D3 on a Wemos D1 mini