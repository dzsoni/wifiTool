/**************************************************************
   wifiTool is a library for the ESP 8266&32/Arduino platform
   SPIFFS oriented AsyncWebServer based wifi configuration tool.

   Additional improvements by Füleki János https://github.com/dzsoni/wifiTool

   Forked from, and original authors:
   https://github.com/oferzv/wifiTool
   Ofer Zvik (https://github.com/oferzv)
   And Tal Ofer (https://github.com/talofer99)

   Licensed under MIT license
 **************************************************************/

#include "Arduino.h"
#include "wifiTool.h"
#include <stdlib.h>
#include <string>
#include <vector>
#include <utility>

extern "C" uint32_t _FS_start;
extern "C" uint32_t _FS_end;


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
        // request->addInterestingHeader("ANY");
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
WifiTool::WifiTool(AsyncWebServer &server) : _server(server)
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
    } // end if
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
    /// DNS
    yield();
    dnsServer->processNextRequest();
    wifiAutoConnect();
    if (_restartsystem)
    {
        if ((unsigned long)millis() - _restartsystem > 10000)
        {

            ESP.restart();
        } // end if
    }     // end if
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

} // end void

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


/*
   handleGetSavSecreteJson()
   Save the secrets: AP password, knonw AP passwords and SSIDs.
*/
void WifiTool::handleGetSaveSecretJson(AsyncWebServerRequest *request)
{
    String jsonString = "{";
    jsonString.concat("\"APpassw\":\"");
    jsonString.concat(request->arg(F("APpass")));
    jsonString.concat("\",");

    jsonString.concat("\"ssid0\":\"");
    jsonString.concat(request->arg(F("ssid0")));
    jsonString.concat("\",");

    jsonString.concat("\"pass0\":\"");
    jsonString.concat(request->arg(F("pass0")));
    jsonString.concat("\",");

    jsonString.concat("\"ssid1\":\"");
    jsonString.concat(request->arg(F("ssid1")));
    jsonString.concat("\",");

    jsonString.concat("\"pass1\":\"");
    jsonString.concat(request->arg(F("pass1")));
    jsonString.concat("\",");

    jsonString.concat("\"ssid2\":\"");
    jsonString.concat(request->arg(F("ssid2")));
    jsonString.concat("\",");

    jsonString.concat("\"pass2\":\"");
    jsonString.concat(request->arg(F("pass2")));
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
// Set  Wifi Access Points identifiers.
void WifiTool::setWifiIdetifiersfromString(String &str)
{
    _apscredit.clear();
    for (byte i = 0; i < 3; i++)
    {
        String assid = _sjsonp.getJSONValueByKeyFromString(str, "ssid" + String(i));
        String apass = _sjsonp.getJSONValueByKeyFromString(str, "pass" + String(i));

        _apscredit.push_back(std::make_pair(assid, apass));
    } // end for
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


    _server.onNotFound([](AsyncWebServerRequest *request)
                       {
                           Serial.println(F("Handle not found."));
                           request->send(404); });

    _server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER); // only when requested from AP
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
    // Serial.println(F("in file delete"));
    if (request->params() == 0)
    {
        return request->send(500, "text/plain", "BAD ARGS");
    }

    String path = String(request->arg(0u));

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

void WifiTool::handleFileDownload(AsyncWebServerRequest *request)
{
    if (request->params() == 0)
    {
        return request->send(500, "text/plain", "BAD ARGS");
    }

    AsyncWebParameter *p = request->getParam(0);
    String s = p->value();
    String path = "/" + s;
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
        _fsUploadFile = SPIFFS.open(filename, "w"); // Open the file for writing in SPIFFS (create if it doesn't exist)
    }
    for (size_t i = 0; i < len; i++)
    {
        _fsUploadFile.write(data[i]);
        // Serial.write(data[i]);
    }
    if (final)
    {
        Serial.println(F("UploadEnd: ") + filename);
        _fsUploadFile.close();
        request->send(200, "text/plain", "");
    }
}
