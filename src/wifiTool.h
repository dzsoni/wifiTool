/**************************************************************
   wifiTool is a library for the ESP 8266&32/Arduino platform
   SPIFFS oriented AsyncWebServer based wifi configuration tool.
   https://github.com/oferzv/wifiTool
   
   Built by Ofer Zvik (https://github.com/oferzv)
   And Tal Ofer (https://github.com/talofer99)
   Licensed under MIT license
 **************************************************************/

#ifndef WIFITOOL_H
#define WIFITOOL_H

#include <Arduino.h>
#include <DNSServer.h>
#include <vector>
#include <utility>
#include "SimpleJsonWriter.h"

#ifdef ESP32
#include <FS.h>
#include <SPIFFS.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
//#include <ESP8266mDNS.h>
#endif

#include <ESPAsyncWebServer.h>
#include <SimpleJsonParser.h>
#include <NTPtimeESP.h>
#include <struct_hardwares.h>
#include <Wire.h>
#include <RtcDS3231.h>
#include <MQTTMediator.h>
#include <WifiManager.h>


#include "definitions.h"

//#define  DEBUG_WIFITOOL 

#ifdef DEBUG_WIFITOOL
#define _WIFITOOL_PP(a) Serial.print(a);
#define _WIFITOOL_PL(a) Serial.println(a);
#else
#define _WIFITOOL_PP(a)
#define _WIFITOOL_PL(a)
#endif

struct struct_hardwares;


class WifiTool
{
public:
  WifiTool(AsyncWebServer& server, struct_hardwares* sh, strDateTime &strdt_ , NTPtime &ntp_, RtcDS3231<TwoWire>& rtc, MQTTMediator& mediator , WifiManager& wifimanager);
  ~WifiTool();
  void process();
  void begin();

private:
  void setUpSoftAP();
  unsigned long         _restartsystem;
  unsigned long         _last_connect_atempt;
  bool                  _connecting;
  byte                  _last_connected_network;
  SimpleJsonParser      _sjsonp;
  AsyncWebServer&       _server;
  struct_hardwares*     _sh;
  strDateTime&          _strdt;
  NTPtime&               _ntp;
  RtcDS3231<TwoWire>&    _rtc;
  MQTTMediator&          _mediator;
  WifiManager&           _wifimanager;
  File                   _fsUploadFile;

  std::unique_ptr<DNSServer> dnsServer;
  


  //void  updateUpload();
  void  handleFileList(AsyncWebServerRequest *request);
  void  handleFileDelete(AsyncWebServerRequest *request);
  void  getWifiScanJson(AsyncWebServerRequest *request);
  void  handleSaveSecretJson(AsyncWebServerRequest *request);
  void  handleGetTemp(AsyncWebServerRequest *request);
  void  handleSaveNTPJson(AsyncWebServerRequest *request);
  void  handleSendTime(AsyncWebServerRequest *request);
  void  handleSaveThingspeakJson(AsyncWebServerRequest *request);
  void  handleSaveMqtt(AsyncWebServerRequest* request);
  void  handleGetMqttjson(AsyncWebServerRequest *request);
  int   getRSSIasQuality(int RSSI);
  void  handleUpload(AsyncWebServerRequest *request, String filename, String redirect, size_t index, uint8_t *data, size_t len, bool final);
  void  handleGetUnknownSenors(AsyncWebServerRequest *request);
  void  handleGetLiveSensors(AsyncWebServerRequest *request);
  void  handleGetDeviceNames(AsyncWebServerRequest *request);
  void  handleGetfilteredCommands(AsyncWebServerRequest *request);
  void  handleGetDeviceUsernames(AsyncWebServerRequest *request);
  void  handleRescanWires(AsyncWebServerRequest *request);
  void  handleSaveSensorInventory(AsyncWebServerRequest *request);
  void  handleSaveCommandFilter(AsyncWebServerRequest *request);
  void  handleFileDownload(AsyncWebServerRequest *request);
  void  handleGetVersion(AsyncWebServerRequest *request);
  void  handleSaveRelays(AsyncWebServerRequest *request);
};

#endif /* WIFITOOL_H */
