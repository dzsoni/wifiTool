# wifiTool

Forked from: https://github.com/oferzv/wifiTool

ESP8266 &amp; ESP32 wifi tool

SPIFFS oriented AsyncWebServer based wifi configuration tool.

This library was created to allow you with one include to have:
1. auto wifi connect, with up to 3 saved router/password with easy wifi configuration tool.
   (It's trying connect to three known AP ,one after the other, not only to the strongest as WiFiMulti do.)
2. AP access with captive portal  - for easy access.
3. SPIFFS management tool.
4. OTA over http.


## dependencies
This library uses the following libraries you need to install: 

https://github.com/me-no-dev/ESPAsyncWebServer

Only for ESP32 
- https://github.com/me-no-dev/AsyncTCP

Only for ESP8266
- https://github.com/me-no-dev/ESPAsyncTCP
- https://github.com/dzsoni/BasicOTA
- https://github.com/dzsoni/SimpleJsonParser
- https://github.com/me-no-dev/ESPAsyncWebServer


## SPIFFS.
All the files in the library are served and stored on the SPIFFS.
In order to get the tool running on the ESP you will need to upload the files.
To do so, you can download the following tool, and place it in a "tools" library in your sketchbook folder.

ESP8266

https://github.com/esp8266/arduino-esp8266fs-plugin

Download JAR file from 
https://github.com/esp8266/arduino-esp8266fs-plugin/releases/tag/0.5.0

ESP32

https://github.com/me-no-dev/arduino-esp32fs-plugin

Download JAR file from
https://github.com/me-no-dev/arduino-esp32fs-plugin/releases/tag/1.0

The files are stored in the "data" library. and you will need to upload with the upload tool 
in order to work the firmware well.  

The router passwords are kept in a json file named "secrets.json", so you can set the ssid-pasword pairs before uploading the 
json file into the SPIFSS, this way you do not even have to run the wifi manger.
And it contains all the HTML pages as well. 

Once the code is uploaded, an AP will run on the ESP and you can connect to it by looking for a AP name:"config". On connect, a web page will open and 
it will be redirected to the main page of the configuration tool.

The page has 3 links:

## Wifi setup
This page allows you to set up to 3 router/password pair for checking at connection.
Once you save the passwords, the board restarts, and the new passords will be used.

## OTA
Pre compile the firmware on your computer and use this page to upload and flash a new firmware to the ESP.(The OTAwebpage.htm run from program memory so its not necessary upload to SPIFFS. It's provided for source code.)

## SPIFFS Manager
Allow you to browse, upload, download or delete files from the SPIFSS, note not to delete any system files :) 


## Code:

In order to use it you will first have to import it 
```cpp
#include <wifiTool.h>
#include <BasicOTA.h>

AsyncWebServer webserver(80);
WifiTool wifiTool(webserver);
```

The simple way to run it is just adding this in the setup. 
```cpp
 BasicOTA.begin(&webserver);
 wifiTool.begin();
```
This will run the auto connect system, and if no available router is found it will automatically run the AP.


