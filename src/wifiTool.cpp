/**************************************************************
   wifiTool is a library for the ESP 8266&32/Arduino platform
   SPIFFS oriented AsyncWebServer based wifi configuration tool.
   https://github.com/oferzv/wifiTool
   
   Built by Ofer Zvik (https://github.com/oferzv)
   And Tal Ofer (https://github.com/talofer99)
   Licensed under MIT license
 **************************************************************/

#include "Arduino.h"
#include "wifiTool.h"
#include <stdlib.h>
#include <string>

/*
    class CaptiveRequestHandler
*/

class CaptiveRequestHandler : public AsyncWebHandler
{
public:
    CaptiveRequestHandler() {}
    virtual ~CaptiveRequestHandler() {}

    bool canHandle(AsyncWebServerRequest *request)
    {
        //request->addInterestingHeader("ANY");
        return true;
    }

    void handleRequest(AsyncWebServerRequest *request)
    {
        String RedirectUrl = "http://";
        if (ON_STA_FILTER(request))
        {
            RedirectUrl += WiFi.localIP().toString();
        }
        else
        {
            RedirectUrl += WiFi.softAPIP().toString();
        }
        RedirectUrl += "/wifi_index.html";
        //_WIFITOOL_PL(RedirectUrl);
        request->redirect(RedirectUrl);
    }
};

void WifiTool::begin()
{
    setUpSoftAP();
    setUpSTA();
}
/*
    WifiTool()
*/
WifiTool::WifiTool( AsyncWebServer& server, struct_solarhardwares *sol, strDateTime &strdt, NTPtime &ntp_) :
           _server (server),_sh(sol), _strdt(strdt), _ntp(ntp_)
{
    _restartsystem = 0;
    _last_connect_atempt = 0;
    _last_connected_network = 0;
    _connecting = false;

    WiFi.mode(WIFI_AP_STA);

    // start spiff
    if (!SPIFFS.begin())
    {
        // Serious problem
        Serial.println(F("SPIFFS Mount failed."));
        return;
    } //end if
}

WifiTool::~WifiTool()
{
    _apscredit.clear();
}

/*
    process()
*/
void WifiTool::process()
{
    ///DNS
    yield();
    dnsServer->processNextRequest();
    wifiAutoConnect();
    if (_restartsystem)
    {
        if ((unsigned long)millis() - _restartsystem > 10000)
        {

            ESP.restart();
        } //end if
    }     //end if
}

void WifiTool::wifiAutoConnect()
{
    if (WiFi.status() != WL_CONNECTED && !_connecting)
    {
        Serial.println(F("\nNo WiFi connection."));
        if (_apscredit[_last_connected_network].first != "")
        {
            WiFi.begin(_apscredit[_last_connected_network].first,
                       _apscredit[_last_connected_network].second);
        }
        _last_connect_atempt = millis();
        _connecting = true;
    }
    else if (WiFi.status() != WL_CONNECTED && _connecting && (millis() - _last_connect_atempt > WAIT_FOR_WIFI_TIME_OUT))
    {
        if (++_last_connected_network >= 3)
            _last_connected_network = 0;
        WiFi.begin(_apscredit[_last_connected_network].first,
                   _apscredit[_last_connected_network].second);
        _last_connect_atempt = millis();
    }
    else if (WiFi.status() == WL_CONNECTED && _connecting)
    {
        _connecting = false;
        Serial.println(F("\nWiFi connected."));
        Serial.println(F("IP address: "));
        Serial.println(WiFi.localIP());
        Serial.print(F("ssid: "));
        Serial.println(WiFi.SSID());
    }

} //end void

/*
   getRSSIasQuality(int RSSI)
*/
int WifiTool::getRSSIasQuality(int RSSI)
{
    int quality = 0;

    if (RSSI <= -100)
    {
        quality = 0;
    }
    else if (RSSI >= -50)
    {
        quality = 100;
    }
    else
    {
        quality = 2 * (RSSI + 100);
    }
    return quality;
}

/*
   getWifiScanJson()
*/
void WifiTool::getWifiScanJson(AsyncWebServerRequest *request)
{
    String json = "{\"scan_result\":[";
    int n = WiFi.scanComplete();
    if (n == -2)
    {
        WiFi.scanNetworks(true);
    }
    else if (n)
    {
        for (int i = 0; i < n; ++i)
        {
            if (i)
                json += ",";
            json += "{";
            json += "\"RSSI\":" + String(WiFi.RSSI(i));
            json += ",\"SSID\":\"" + WiFi.SSID(i) + "\"";
            json += "}";
        }
        WiFi.scanDelete();
        if (WiFi.scanComplete() == -2)
        {
            WiFi.scanNetworks(true);
        }
    }
    json += "]}";
    request->send(200, "application/json", json);
    json = String();
}

void WifiTool::handleGetTemp(AsyncWebServerRequest *request)
{
    ENUM_NBD_ERROR err = NBD_NO_ERRROR;
    int s_count = 0;
    unsigned int i=0;
    String jsonString = "{";
    for (auto  w = 0; w < _sh->wire.size(); w++)
    {
        i=0;
        if( !(!w && !i)) jsonString += ",";
        jsonString += "\"s";
        jsonString += s_count;
        jsonString += "\":[";
        for ( i=0 ; i < _sh->wire.at(w)->getSensorsCount(); i++)
        {
            String gpio = (String)_sh->wire.at(w)->getGPIO();

            DeviceAddress deva;
            _sh->wire.at(w)->getAddressByIndex(i, deva);

            String name = _sh->wire.at(w)->getSenorNameByIndex(i, err);

            float temp = _sh->wire.at(w)->getTempByIndex(i, err);

            String unitM;
            if(_sh->wire.at(w)->getUnitsOfMeasure()=="C")
            {
                unitM="\u2103";
            }
            else
            {
                unitM="\u2109";
            }

            jsonString += "\"";
            jsonString += name;
            jsonString += "\",\"";
            jsonString += _sh->wire.at(w)->addressToString(deva);
            jsonString += "\",\"";
            jsonString += gpio;
            jsonString += "\",\"";
            jsonString += temp;
            jsonString += "\",\"";
            jsonString += unitM;
            jsonString += ("\"]");
            s_count++;
        }
    }
    jsonString.concat("}");

    _WIFITOOL_PL(jsonString);
    request->send(200, "application/json", jsonString);
}

/*
   handleGetUnknownSenors()
   Send the address of unknown sensors.
*/
void WifiTool::handleGetUnknownSenors(AsyncWebServerRequest *request)
{
    String json = "[";
    bool first = true;
    for (size_t t = 0; t < _sh->wire.size(); t++)
    {
        for (size_t i = 0; i < _sh->wire.at(t)->getSensorsCount(); i++)
        {
            ENUM_NBD_ERROR err;
            if (_sh->wire.at(t)->getSenorNameByIndex(i, err) == "")
            {
                if (!first)
                    json += ",";
                first = false;
                json += "\"";
                DeviceAddress tempadd;
                _sh->wire.at(t)->getAddressByIndex(i, tempadd);
                json += _sh->wire.at(t)->addressToString(tempadd);
                json += "\"";
            }
        }
    }
    json += "]";
    _WIFITOOL_PL(json);
    request->send(200, "application/json", json);
}

void WifiTool::handleSaveSensorInventory(AsyncWebServerRequest *request)
{
    AsyncWebParameter *p = nullptr;
    String json = "{";
    unsigned int count = 0;
    String a = "h" + String(count);
    p = request->getParam(a, true, false);
    _WIFITOOL_PL(a);
    while (p != nullptr)
    {

        json += "\"";
        a = "h" + String(count);
        p = request->getParam(a, true, false); //hidden input
        json.concat(p->value());
        _WIFITOOL_PL(a);
        json.concat("\":\"");
        a = "n" + String(count);
        p = request->getParam(a, true, false); //name
        json.concat(p->value());
        _WIFITOOL_PL(a);
        json += "\"";
        count++;
        a = "h" + String(count);
        p = request->getParam(a, true, false);
        if (p != nullptr)
            json += ",";
    }
    json += "}";

    File file = SPIFFS.open("/sensnames.json", "w");
    if (!file)
    {
        Serial.println(F("Error opening file for writing"));
        return;
    }
    _WIFITOOL_PL(json);
    file.print(json);
    file.flush();
    file.close();
    handleRescanWires(request);
    request->redirect(F("/wifi_tempinvent.html"));
}
/*
   handleGetSavSecreteJson()
   Save the secrets: AP password, knonw AP passwords and SSIDs.
*/
void WifiTool::handleGetSaveSecretJson(AsyncWebServerRequest *request)
{
    AsyncWebParameter *p;
    String jsonString = "{";
    jsonString.concat("\"APpassw\":\"");
    p = request->getParam("APpass", true, false);
    jsonString.concat(p->value());
    jsonString.concat("\",");

    jsonString.concat("\"ssid0\":\"");
    p = request->getParam("ssid0", true);
    jsonString.concat(p->value());
    jsonString.concat("\",");

    jsonString.concat("\"pass0\":\"");
    p = request->getParam("pass0", true);
    jsonString.concat(p->value());
    jsonString.concat("\",");

    jsonString.concat("\"ssid1\":\"");
    p = request->getParam("ssid1", true);
    jsonString.concat(p->value());
    jsonString.concat("\",");

    jsonString.concat("\"pass1\":\"");
    p = request->getParam("pass1", true);
    jsonString.concat(p->value());
    jsonString.concat("\",");

    jsonString.concat("\"ssid2\":\"");
    p = request->getParam("ssid2", true);
    jsonString.concat(p->value());
    jsonString.concat("\",");

    jsonString.concat("\"pass2\":\"");
    p = request->getParam("pass2", true);
    jsonString.concat(p->value());
    jsonString.concat("\"}");

    File file = SPIFFS.open(SECRETS_PATH, "w");
    if (!file)
    {
        Serial.println(F("Error opening file for writing"));
        return;
    }

    file.print(jsonString);
    file.flush();
    file.close();

    setWifiIdetifiersfromString(jsonString);
    request->redirect(F("/wifi_manager.html"));
}

void WifiTool::handleSaveNTPJson(AsyncWebServerRequest *request)
{

    AsyncWebParameter *p;
    String jsonString = "{";
    jsonString.concat("\"NTPserver\":\"");
    p = request->getParam("NTPserver", true);
    jsonString.concat(p->value());
    jsonString.concat("\",");

    jsonString.concat("\"UTCh\":\"");
    p = request->getParam("UTCh", true);
    jsonString.concat(p->value().c_str());
    _ntp.setUtcHour((int8_t)p->value().toInt());
    jsonString.concat("\",");

    jsonString.concat("\"UTCm\":\"");
    p = request->getParam("UTCm", true);
    jsonString.concat(p->value().c_str());
    _ntp.setUtcMin((uint8_t)abs(p->value().toInt()));
    jsonString.concat("\",");

    jsonString.concat("\"extratsh\":\"");
    p = request->getParam("extratsh", true);
    jsonString.concat(p->value());

    if (p->value() == "ST")
    {
        _ntp.setSTDST(1); //Summer Time
    }
    else if (p->value() == "DST")
    {
        _ntp.setSTDST(2); //Daylight Saving Time
    }
    else
    {
        _ntp.setSTDST(0);
    }

    jsonString.concat("\"}");

    _WIFITOOL_PL(jsonString);

    File file = SPIFFS.open(F("/ntp.json"), "w");
    if (!file)
    {
        Serial.println(F("Error opening file for writing"));
        return;
    }
    file.print(jsonString);
    file.flush();
    file.close();
    request->redirect(F("/wifi_NTP.html"));
}

void WifiTool::handleRescanWires(AsyncWebServerRequest *request)
{
    for (size_t t = 0; t < _sh->wire.size(); t++)
    {
        _sh->wire.at(t)->rescanWire();
    }
    request->redirect(F("/wifi_tempinvent.html"));
}

void WifiTool::handleSaveThingspeakJson(AsyncWebServerRequest *request)
{
    AsyncWebParameter *p;
    String jsonString = "{";
    jsonString.concat("\"ChannelID\":\"");
    p = request->getParam("ChannelID", true);
    jsonString.concat(p->value());
    jsonString.concat("\",");

    jsonString.concat("\"WriteAPIKey\":\"");
    p = request->getParam("WriteAPIKey", true);
    jsonString.concat(p->value());
    jsonString.concat("\"}");

    _WIFITOOL_PL(jsonString);

    File file = SPIFFS.open(F("/thingspeak.json"), "w");
    if (!file)
    {
        Serial.println(F("Error opening file for writing"));
        return;
    }
    file.print(jsonString);
    file.flush();
    file.close();
    request->redirect(F("/wifi_thingspeak.html"));
}

void WifiTool::handleSendTime(AsyncWebServerRequest *request)
{
    AsyncWebParameter *p;
    p = request->getParam("time", true);
    if (_strdt.epochTime == 0 && p != nullptr)
    {
        char atm[11];
        memset(atm, 0, 11);
        strncpy(atm, p->value().c_str(), 10);
        char *ptr;
        unsigned long b;
        b = strtoul(atm, &ptr, 10); //string to unsigned long
        b = _ntp.adjustTimeZone(b, _ntp.getUtcHour(), _ntp.getUtcMin(), _ntp.getSTDST());
        _strdt = _ntp.ConvertUnixTimestamp(b);
    }
    request->send(200);
}

/**
  * setUpSTA()
  * Setup the Station mode
*/
void WifiTool::setUpSTA()
{
    String json = _sjsonp.fileToString(SECRETS_PATH);
    if (json == "" || json == nullptr)
    {
        Serial.println(F("Can't open the secret file."));
        return;
    }
    setWifiIdetifiersfromString(json);
}
//Set  Wifi Access Points identifiers.
void WifiTool::setWifiIdetifiersfromString(String &str)
{
    _apscredit.clear();
    for (byte i = 0; i < 3; i++)
    {
        String assid = _sjsonp.getJSONValueByKeyFromString(str, "ssid" + String(i));
        String apass = _sjsonp.getJSONValueByKeyFromString(str, "pass" + String(i));

        _apscredit.push_back(std::make_pair(assid, apass));
    } //end for
    _apscredit.shrink_to_fit();
}

/**
  * setUpSoftAP()
  * Setting up the SoftAP Service
*/
void WifiTool::setUpSoftAP()
{
    Serial.println(F("RUN AP"));
    dnsServer.reset(new DNSServer());

    WiFi.softAPConfig(IPAddress(DEF_AP_IP),
                      IPAddress(DEF_GATEWAY_IP),
                      IPAddress(DEF_SUBNETMASK));
    WiFi.softAP(DEF_AP_NAME, _sjsonp.getJSONValueByKey(SECRETS_PATH, "APpassw"), 1, 0, 4);

    delay(500);

    // Setup the DNS server redirecting all the domains to the apIP
    dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer->start(DEF_DNS_PORT, "*", IPAddress(DEF_DNS_IP));

    Serial.println(F("DNS server started."));

    Serial.print(F("SoftAP name and server IP address: "));
    Serial.print(String(DEF_AP_NAME) + "  ");
    Serial.println(WiFi.softAPIP());

    _server.serveStatic("/", SPIFFS, "/").setDefaultFile("/wifi_index.html");

    _server.on("/saveSecret/", HTTP_ANY, [&, this](AsyncWebServerRequest *request)
               { handleGetSaveSecretJson(request); });

    _server.on("/saveTempsens/", HTTP_POST, [&, this](AsyncWebServerRequest *request)
               { handleSaveSensorInventory(request); });

    _server.on("/saveNTP/", HTTP_ANY, [&, this](AsyncWebServerRequest *request)
               { handleSaveNTPJson(request); });

    _server.on("/saveThingspeak/", HTTP_ANY, [&, this](AsyncWebServerRequest *request)
               { handleSaveThingspeakJson(request); });

    _server.on("/sendTime/", HTTP_POST, [&, this](AsyncWebServerRequest *request)
               { handleSendTime(request); });

    _server.on("/rescanwires/", HTTP_ANY, [&, this](AsyncWebServerRequest *request)
               { handleRescanWires(request); });

    _server.on("/list", HTTP_ANY, [&, this](AsyncWebServerRequest *request)
               { handleFileList(request); });

    // spiff delete
    _server.on("/edit", HTTP_DELETE, [&, this](AsyncWebServerRequest *request)
               { handleFileDelete(request); });
    _server.on("/download", HTTP_GET, [&, this](AsyncWebServerRequest *request)
               { handleFileDownload(request); });

    // spiff upload
    _server.on(
        "/edit", HTTP_POST, [&, this](AsyncWebServerRequest *request) {},
        [&, this](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
                  size_t len, bool final)
        {
            handleUpload(request, filename, "/wifi_spiffs_admin.html", index, data, len, final);
        });

    _server.on("/wifiScan.json", HTTP_GET, [&, this](AsyncWebServerRequest *request)
               { getWifiScanJson(request); });

    _server.on("/temp.json", HTTP_GET, [&, this](AsyncWebServerRequest *request)
               { handleGetTemp(request); });

    _server.on("/getunknownsenses.json", HTTP_GET, [&, this](AsyncWebServerRequest *request)
               { handleGetUnknownSenors(request); });

    // Simple Firmware Update Form
    _server.on("/update", HTTP_GET, [&, this](AsyncWebServerRequest *request)
               { request->send(SPIFFS, "/wifi_upload.html"); });
    _server.on(
        "/update", HTTP_POST, [&, this](AsyncWebServerRequest *request)
        {
            uint8_t isSuccess = !Update.hasError();

            AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", isSuccess ? "OK" : "FAIL");
            response->addHeader("Connection", "close");
            request->send(response);
        },
        [&, this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
        {
            if (!index)
            {
                Serial.printf("Update Start: %s\n", filename.c_str());

#if defined(ESP8266)
                Update.runAsync(true);
                if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000))
                {
                    Update.printError(Serial);
                }
#else
                if (!Update.begin(UPDATE_SIZE_UNKNOWN))
                {
                    Update.printError(Serial);
                }
#endif
            }
            if (!Update.hasError())
            {
                if (Update.write(data, len) != len)
                {
                    Update.printError(Serial);
                }
            }
            if (final)
            {
                if (Update.end(true))
                {
                    Serial.printf("Update Success: %uB\n", index + len);
                }
                else
                {
                    Update.printError(Serial);
                }
            }
        });

    _server.onNotFound([](AsyncWebServerRequest *request)
                       {
                           Serial.println(F("Handle not found."));
                           request->send(404);
                       });

    _server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER); //only when requested from AP
    Serial.println(F("HTTP webserver started."));
    _server.begin();
}

void WifiTool::handleFileList(AsyncWebServerRequest *request)
{

    if (!request->hasParam("dir"))
    {
        request->send(500, "text/plain", "BAD ARGS");
        return;
    }

    AsyncWebParameter *p = request->getParam("dir");
    String path = p->value().c_str();
    _WIFITOOL_PL("handleFileList: " + path);
    String output = "[";
#if defined(ESP8266)

    Dir dir = SPIFFS.openDir(path);
    while (dir.next())
    {
        File entry = dir.openFile("r");
        if (output != "[")
        {
            output += ',';
        }
        bool isDir = false;
        output += "{\"type\":\"";
        output += (isDir) ? "dir" : "file";
        output += "\",\"name\":\"";
        output += String(entry.name()).substring(1);
        output += "\"}";
        entry.close();
    }

#else

    File root = SPIFFS.open("/", "r");
    if (root.isDirectory())
    {
        _WIFITOOL_PL("here ??");
        File file = root.openNextFile();
        while (file)
        {
            if (output != "[")
            {
                output += ',';
            }
            output += "{\"type\":\"";
            output += (file.isDirectory()) ? "dir" : "file";
            output += "\",\"name\":\"";
            output += String(file.name()).substring(1);
            output += "\"}";
            file = root.openNextFile();
        }
    }
#endif

    path = String();
    output += "]";
    _WIFITOOL_PL(output);
    request->send(200, "application/json", output);
}

void WifiTool::handleFileDelete(AsyncWebServerRequest *request)
{
    //Serial.println(F("in file delete"));
    if (request->params() == 0)
    {
        return request->send(500, "text/plain", "BAD ARGS");
    }
    AsyncWebParameter *p = request->getParam(0);
    String path = p->value();

    Serial.println("handleFileDelete: " + path);
    if (path == "/")
    {
        return request->send(500, "text/plain", "BAD PATH");
    }

    if (!SPIFFS.exists(path))
    {
        return request->send(404, "text/plain", "FileNotFound");
    }

    SPIFFS.remove(path);
    request->send(200, "text/plain", "");
    path = String();
}

void WifiTool:: handleFileDownload(AsyncWebServerRequest *request)
{
    if (request->params() == 0)
    {
        return request->send(500, "text/plain", "BAD ARGS");
    }

    AsyncWebParameter *p = request->getParam(0);
    String s = p->value();
    String path="/"+s;
    Serial.println("handleFileDownload: " + path);
    if (path == "/")
    {
        return request->send(500, "text/plain", "BAD PATH");
    }

    if (!SPIFFS.exists(path))
    {
        return request->send(404, "text/plain", "FileNotFound");
    }
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, path, String(), true);

   // request->send(SPIFFS, path, "application/x-download", true);
  
   request->send(response);


}
//==============================================================
//   handleUpload
//==============================================================
void WifiTool::handleUpload(AsyncWebServerRequest *request, String filename, String redirect, size_t index, uint8_t *data, size_t len, bool final)
{
    if (!index)
    {
        if (!filename.startsWith("/"))
            filename = "/" + filename;

        Serial.println((String) "UploadStart: " + filename);
        fsUploadFile = SPIFFS.open(filename, "w"); // Open the file for writing in SPIFFS (create if it doesn't exist)
    }
    for (size_t i = 0; i < len; i++)
    {
        fsUploadFile.write(data[i]);
        //Serial.write(data[i]);
    }
    if (final)
    {
        Serial.println(F("UploadEnd: ") + filename);
        fsUploadFile.close();
        request->send(200, "text/plain", "");
    }
}
