/**************************************************************
   wifiTool is a library for the ESP 8266&32/Arduino platform
   SPIFFS oriented AsyncWebServer based wifi configuration tool.
   https://github.com/oferzv/wifiTool
   
   Built by Ofer Zvik (https://github.com/oferzv)
   And Tal Ofer (https://github.com/talofer99)
   Licensed under MIT license
 **************************************************************/

#ifndef WIFITOOL_h
#define WIFITOOL_h

#include <Arduino.h>
#include <DNSServer.h>
#include <vector>
#include <utility>

#include <ArduinoOTA.h>
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
#include <SPIFFSEditor.h>
#include <SimpleJsonParser.h>
#include <NTPtimeESP.h>
#include <struct_solarhardwares.h>


#include "definitions.h"

//#define  DEBUG_WIFITOOL 

#ifdef DEBUG_WIFITOOL
#define _WIFITOOL_PP(a) Serial.print(a);
#define _WIFITOOL_PL(a) Serial.println(a);
#else
#define _WIFITOOL_PP(a)
#define _WIFITOOL_PL(a)
#endif




class WifiTool
{
public:
  WifiTool(AsyncWebServer& server, struct_solarhardwares* sol, strDateTime &strdt_ , NTPtime &ntp_);
  ~WifiTool();
  void process();
  void begin();

private:
  void setUpSoftAP();
  void setUpSTA();
  unsigned long     _restartsystem;
  unsigned long     _last_connect_atempt;
  bool              _connecting;
  byte              _last_connected_network;
  SimpleJsonParser  _sjsonp;
  strDateTime&      _strdt;
  NTPtime&          _ntp;
  struct_solarhardwares* _sh;
  std::vector< std::pair <String,String> > _apscredit;
  File              fsUploadFile;

  std::unique_ptr<DNSServer> dnsServer;
  AsyncWebServer& _server;


  //void  updateUpload();
  void  handleFileList(AsyncWebServerRequest *request);
  void  handleFileDelete(AsyncWebServerRequest *request);
  void  getWifiScanJson(AsyncWebServerRequest *request);
  void  handleGetSaveSecretJson(AsyncWebServerRequest *request);
  void  handleGetTemp(AsyncWebServerRequest *request);
  void  handleSaveNTPJson(AsyncWebServerRequest *request);
  void  handleSendTime(AsyncWebServerRequest *request);
  void  handleSaveThingspeakJson(AsyncWebServerRequest *request);
  int   getRSSIasQuality(int RSSI);
  void  handleUpload(AsyncWebServerRequest *request, String filename, String redirect, size_t index, uint8_t *data, size_t len, bool final);
  void  wifiAutoConnect();
  void  setWifiIdetifiersfromString(String& str);
  void  handleGetUnknownSenors(AsyncWebServerRequest *request);
  void  handleRescanWires(AsyncWebServerRequest *request);
  void  handleSaveSensorInventory(AsyncWebServerRequest *request);
  void  handleFileDownload(AsyncWebServerRequest *request);
};

#endif
