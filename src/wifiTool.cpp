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
#include <vector>
#include <utility>


extern "C" uint32_t _FS_start;
extern "C" uint32_t _FS_end;
extern String getVersion();

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
    //setUpSTA();
}
/*
    WifiTool()
*/
WifiTool::WifiTool(AsyncWebServer &server, struct_hardwares *sol, strDateTime &strdt, NTPtime &ntp,
                    RtcDS3231<TwoWire> &rtc,MQTTMediator& mediator, WifiManager &wifimanager) : _server(server), _sh(sol), _strdt(strdt), _ntp(ntp),
                     _rtc(rtc), _mediator(mediator), _wifimanager(wifimanager)
{
    _restartsystem = 0;

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
}

/*
    process()
*/
void WifiTool::process()
{
    /// DNS
    yield();
    dnsServer->processNextRequest();
    yield();
    
    if (_restartsystem)
    {
        if ((unsigned long)millis() - _restartsystem > 10000)
        {
            Serial.println(F("\nRestarting..."));
            ESP.restart();
        } 
    }     
}


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
}
void WifiTool::handleGetMqttjson(AsyncWebServerRequest *request)
{
    SimpleJsonWriter sjw;
    SimpleJsonParser sjp;
    std::vector<std::pair<String, String>> container;
    container = sjp.extractKeysandValuesFromFile(MQTT_SETTINGS_JSON);
    
    for (unsigned int i = 0; i < container.size(); i++)
    {
        if (container[i].first == "CLIENTID" && container[i].second == "")
        {
            container[i].second = _mediator.getClientId();
        }
        sjw.addKeyValue(container[i].first, container[i].second);
    }
    _WIFITOOL_PL(sjw.getJsonString());
    request->send(200, "application/json", sjw.getJsonString());
}
void WifiTool::handleGetTemp(AsyncWebServerRequest *request)
{
    ENUM_NBD_ERROR err = NBD_NO_ERROR;
    String jsonString = "{";

    for(uint i=0;i<_sh->NBD_Array.getSensorsCount();i++)
    {
        if(i)
        {
            jsonString += ",";
        }
        jsonString += "\"s";
        jsonString += i;
        jsonString += "\":[";
        String gpio = String(_sh->NBD_Array.getGPIO(i,err));
        DeviceAddress deva;
            _sh->NBD_Array.getAddressByIndex(i, deva);

        String name = _sh->NBD_Array.getSensorNameByIndex(i, err);

        float temp = _sh->NBD_Array.getTempByIndex(i, err);

        String unitM;
            if (_sh->NBD_Array.getUnitsOfMeasureAsString() == "C")
            {
                unitM = "\u2103";
            }
            else
            {
                unitM = "\u2109";
            }

            jsonString += "\"";
            jsonString += name;
            jsonString += "\",\"";
            jsonString += _sh->NBD_Array.addressToString(deva);
            jsonString += "\",\"";
            jsonString += gpio;
            jsonString += "\",\"";
            jsonString += temp;
            jsonString += "\",\"";
            jsonString += unitM;
            jsonString += ("\"]");
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
    _sh->NBD_Array.rescanWire();
    String json = "[";
     size_t t=0;
        for (size_t i = 0; i < _sh->NBD_Array.getSensorsCount(); i++)
        {

            ENUM_NBD_ERROR err;
            if (_sh->NBD_Array.getSensorNameByIndex(i, err) == "")
            {
                DeviceAddress tempadd;
                _sh->NBD_Array.getAddressByIndex(i, tempadd);
                if (_sh->NBD_Array.addressToString(tempadd) != "0.0.0.0.0.0.0.0")
                {
                    if (t)
                        json += ",";
                    json += "\"";
                    json += _sh->NBD_Array.addressToString(tempadd);
                    json += "\"";
                    t++;
                }
            }
        }
    
    json += "]";
    _WIFITOOL_PL(json);
    request->send(200, "application/json", json);
}

void WifiTool::handleGetLiveSensors(AsyncWebServerRequest *request)
{
    String json = "[";
    size_t t=0;
    for (size_t i = 0; i < _sh->NBD_Array.getSensorsCount(); i++)
    {
        ENUM_NBD_ERROR err;
        DeviceAddress tempadd;
        _sh->NBD_Array.getAddressByIndex(i, tempadd);
        if (_sh->NBD_Array.addressToString(tempadd) != "0.0.0.0.0.0.0.0")
        {
            if (t)
                json += ",";
            json += "\"";
            json += _sh->NBD_Array.addressToString(tempadd);
            json += "\"";
            t++;
        }
    }
    json += "]";
    _WIFITOOL_PL(json);
    request->send(200, "application/json", json);
}

/**
 * Handles the request to get the names of all devices in command center.
 * Sends a JSON array of strings containing the names of all devices in the command center.
 * 
 * @param request The request object containing the HTTP request.
 *
 */
void WifiTool::handleGetDeviceNames(AsyncWebServerRequest *request)
{
    
    std::vector<String> devicetypes = _sh->comcenter.getDeviceTypes();
    String json = "[";
    for (size_t i = 0; i < devicetypes.size(); i++)
    {
        if (i)
            json += ",";
        json += "\"";
        json += devicetypes.at(i);
        json += "\"";
    }
    json += "]";
    _WIFITOOL_PL(json);
    request->send(200, "application/json", json);
}

void WifiTool::handleGetfilteredCommands(AsyncWebServerRequest *request)
{
    SimpleJsonWriter sjw;
    SimpleJsonParser sjp;
    String device = request->arg("devicename"); //devicename == type of device
    device.toUpperCase();
    std::vector<std::pair<String, String>> filter = sjp.extractKeysandValuesFromFile(FILTERFILE_PREFIX +  device +".json");
    _WIFITOOL_PL(filter.size());
    device_command_struct dcs= _sh->tuplefactory.invokeInitFunctionByType(device);
    
    for(uint i=0;i<dcs.device_commands.size();i++)
    {
        String value ="true";
        for(uint j=0;j<filter.size();j++)
        {   
           if(String(std::get<ENUM_COMTUPLEORDER_COMMAND>(dcs.device_commands[i])) == filter[j].first)
           {
            _WIFITOOL_PL(String(std::get<ENUM_COMTUPLEORDER_COMMAND>(dcs.device_commands[i])) + " " + filter[j].second);
            value = filter[j].second;
            break;
           }  
        }
        sjw.addKeyValue(std::get<ENUM_COMTUPLEORDER_COMMAND>(dcs.device_commands[i]),value);
    }
    _WIFITOOL_PL(sjw.getJsonString());
    request->send(200, "application/json", sjw.getJsonString());
}

void WifiTool::handleGetDeviceUsernames(AsyncWebServerRequest *request)
{
    SimpleJsonWriter sjw;
    String device = request->arg(F("devicetype"));
    device.toUpperCase();

    uint count = _sh->tuplefactory.numberOfInitFuns();
    _WIFITOOL_PL("Number of init funcs:" + String(count));
    uint find=0;
    for(uint i=0;i<count;i++)
    {
        device_command_struct dcs= _sh->tuplefactory.invokeNthInitFunction(i);
        if((String(dcs.devicetype))==device)
        {
            sjw.addKeyValue(String(dcs.devicetype),dcs.id_by_user);
            find++;
        }
    }
    
    _WIFITOOL_PL(String(__FUNCTION__) + " " + sjw.getJsonString());
    request->send(200, "application/json", sjw.getJsonString());
}

void WifiTool::handleSaveSensorInventory(AsyncWebServerRequest *request)
{
    SimpleJsonWriter sjw;
    std::vector<std::pair<String, String>> listA;
    std::vector<std::pair<String, String> *> listB;
    if (request->params() > 0)
    {
        for (unsigned int i = 0; i < request->args(); i = i + 2)
        {
            listA.emplace_back(std::make_pair(String{request->arg(i)},
                                              String{request->arg(i + 1)}));
        }

        for (unsigned int i = 0; i < listA.size() - 1; i++)
        {
            if (listA.at(i).second == "")
                continue;
            boolean found = false;
            for (unsigned int j = i + 1; j < listA.size(); j++)
            {
                if (listA.at(i).second == listA.at(j).second)
                    found = true;
            }
            if (!found)
                listB.emplace_back(&listA.at(i));
        }
        if (listA.size() > 0)
        {
            if (listA.at(listA.size() - 1).second != "")
                listB.emplace_back(&listA.at(listA.size() - 1));
        }
    }
    
    for (unsigned int i = 0; i < listB.size(); i++)
    {
        sjw.addKeyValue(listB.at(i)->first, listB.at(i)->second);
    }
    File file = SPIFFS.open(SENSOR_NAMES_JSON, "w");
    if (!file)
    {
        _WIFITOOL_PL(F("Error opening file for writing"));
        return;
    }
    _WIFITOOL_PL(sjw.getJsonString());
    file.print(sjw.getJsonString());
    file.flush();
    file.close();
    handleRescanWires(request);
}

void WifiTool::handleSaveCommandFilter(AsyncWebServerRequest *request)
{
     SimpleJsonWriter sjw;
     std::vector<std::pair<String, String>> filterpairs;
     String device;
     device = request->arg(F("DEVICELIST"));
     device.toUpperCase();
    if(device=="")
    {
        _WIFITOOL_PL(F("No device name given."));
        request->redirect("/wifi_command.html");
        return;
    }

     uint find=0;
     if (request->params() > 2 )
    {   
        for (unsigned int i = 0; i < request->params() ; i++)
        {   
            if( String(request->argName(i)).substring(0,1)!="n") continue;

            sjw.addKeyValue(request->getParam(i)->value(),request->getParam(i+1)->value());
            
            filterpairs.emplace_back(std::make_pair(request->getParam(i)->value(),request->getParam(i+1)->value()));
            find++;
        }
    }

    _WIFITOOL_PL(sjw.getJsonString());
   
    File file = SPIFFS.open(FILTERFILE_PREFIX + device + ".json", "w");
    if (!file)
    {
        _WIFITOOL_PL(F("Error opening file for writing"));
        request->redirect("/wifi_command.html");
        return;
    }

    device_command_struct dcs= _sh->tuplefactory.invokeInitFunctionByType(device); //the id_by_user is not important now
    
    _sh->comcenter.fillDeviceWithCommands(dcs);
    _sh->comcenter.filterRepoByStringPair(device,filterpairs);//shrink_to_fit done by filterRepoByStringPair
    file.print(sjw.getJsonString());
    file.flush();
    file.close();

    for (unsigned int i = 0; i < request->params() ; i++)
    {
         if( String(request->argName(i)).substring(0,2)!="io") continue;
         {
            if(request->getParam(i)->value()!=request->getParam(i+1)->value() && request->getParam(i+1)->value()!="") //io(old) !=in(new) && in(new) != ""
            {
                device_command_struct dcs= _sh->tuplefactory.invokeInitFunctionById(request->getParam(i)->value(),request->getParam(i+1)->value());
                _sh->comcenter.replaceDevice(request->getParam(i)->value(),dcs);
            }
         }
    }
    _sh->comcenter.filterRepoByStringPair(device,filterpairs);

    file = SPIFFS.open(MQTTCOMMANDDEVICEIDS_JSON , "w");
    if (!file)
    {
        _WIFITOOL_PL(F("Error opening file for writing") + String(MQTTCOMMANDDEVICEIDS_JSON));
        request->redirect("/wifi_command.html");
        return;
    }

    sjw.clear();

    for (unsigned int i = 0; i < _sh->comcenter.numberOfDevs() ; i++)
    {
        sjw.addKeyValue(_sh->comcenter.getDeviceTypeByIndex(i),_sh->comcenter.getDeviceIdByIndex(i));
    }
    _WIFITOOL_PL(sjw.getJsonString());
    file.print(sjw.getJsonString());
    file.flush();    
    file.close();   

    request->redirect("/wifi_command.html");
}

/***
 * @brief Save mqttserver address, port, username, password, willtopic, keep alive, QoS, retain, will message,
 *  temperature sensor names with topic name to mqtt.json
 * 
 * @param request 
 */
void WifiTool::handleSaveMqtt(AsyncWebServerRequest *request)
{
    std::vector<std::pair<String, String>> listA;
    std::vector<std::pair<String, String> *> listtopics;
    std::vector<std::pair<String, String> *> listsensors;
    std::vector<std::pair<String, String> *> listqos;

    if (request->params() > 0)
    {

        for (unsigned int i = 0; i < request->params(); i++)
        {
            listA.emplace_back(std::make_pair(String(request->argName(i)),
                                              String(request->arg(i))));
        }

        // Check if the mqtt server and port are defined
        if(request->arg("SERVERADDRESS") == "" || request->arg("SERVERPORT") == "")
        {
            _WIFITOOL_PL(F("The mqtt Server or Port is not defined."));
            request->redirect("/wifi_mqtt.html");
            return;
        }
        
        // select ListA to different lists
        for (unsigned int i = 0; i < listA.size(); i++)
        {
            if (listA.at(i).first == F("SERVERADDRESS"))
            {
                _sh->mqttstruct.mqttServer = listA[i].second;
                continue;
            }

            if (listA.at(i).first == F("SERVERPORT"))
            {
                _sh->mqttstruct.mqttPort = (uint16_t)listA[i].second.toInt();
                continue;
            }

            if (listA.at(i).first == F("USERNAME"))
            {
                _sh->mqttstruct.mqttUser = listA[i].second;
                continue;
            }

            if (listA.at(i).first == F("PASSWORD"))
            {
                _sh->mqttstruct.mqttPassword = listA[i].second;
                continue;
            }

            if (listA.at(i).first == F("CLIENTID"))
            {
                if (listA[i].second != "")
                {
                    _sh->mqttstruct.mqttClientId = listA[i].second;
                }
                continue;
            }

            if (listA.at(i).first == F("CLEANSESSION"))
            {
                 _sh->mqttstruct.mqttCleanSession = (listA.at(i).second == "true") ? true : false;
                continue;
            }

            if (listA.at(i).first == F("KEEPALIVE"))
            {
                _sh->mqttstruct.mqttKeepAlive = (uint16_t)(listA[i].second.toInt());
                continue;
            }

            if (listA.at(i).first == F("WILLRETAIN"))
            {
                _sh->mqttstruct.mqttRetain = (listA.at(i).second == "true") ? true : false;
                continue;
            }

            if (listA.at(i).first == F("WILLTOPIC") )
            { 
                _sh->mqttstruct.mqttWillTopic = listA[i].second;
                continue;
            }

            if (listA.at(i).first == F("WILLQOS"))
            {
                _sh->mqttstruct.mqttQoS = (uint8_t)listA[i].second.toInt();
                continue;
            }
            
            if (listA.at(i).first == F("WILLTEXT"))
            {
                _sh->mqttstruct.mqttWillText = listA[i].second;
                continue;
            }

            if(listA.at(i).first == F("COMMANDTOPIC"))
            {
                _sh->mqttCommand.setCommandTopic(listA[i].second);
                continue;
            }

            if(listA.at(i).first == F("MQTTCOMQOS"))
            {
                _sh->mqttCommand.setQos((uint8_t)listA[i].second.toInt());
                continue;
            }
            
            if(listA.at(i).first == F("RESPONSETOPIC"))
            {
                _sh->mqttCommand.setResponseTopic(listA[i].second);
                continue;
            }

            if (listA.at(i).first.substring(0, 1) == "t") // will topic
            {                                             // select server settings to listtopics
                listtopics.emplace_back(&listA.at(i));
                continue;
            }

            if (listA.at(i).first.substring(0, 1) == "s") // sensor
            {                                             // select server settings to listsensors
                listsensors.emplace_back(&listA.at(i));
                continue;
            }

            if (listA.at(i).first.substring(0, 1) == "q") // QoS
            {                                             // select server settings to listqos
                listqos.emplace_back(&listA.at(i));
                continue;
            }
        }
        IPAddress ipa;
        if (_sh->mqttstruct.mqttServer != "" && _sh->mqttstruct.mqttPort != 0)
        {
            
            if (!ipa.fromString(_sh->mqttstruct.mqttServer.c_str()))
            {
              _mediator.setServer(_sh->mqttstruct.mqttServer.c_str(), _sh->mqttstruct.mqttPort);
            }
            else
            {
              _mediator.setServer(ipa, _sh->mqttstruct.mqttPort);
            }
        }
        _mediator.disconnect();
        _mediator.setCredentials(_sh->mqttstruct.mqttUser.c_str(), _sh->mqttstruct.mqttPassword.c_str()); // Set password, username
        if (_sh->mqttstruct.mqttClientId != "")
            _mediator.setClientId(_sh->mqttstruct.mqttClientId.c_str());

        _mediator.setCleanSession(_sh->mqttstruct.mqttCleanSession);
        _mediator.setKeepAlive(_sh->mqttstruct.mqttKeepAlive);
        _mediator.setWill(_sh->mqttstruct.mqttWillTopic.c_str(), _sh->mqttstruct.mqttQoS, _sh->mqttstruct.mqttRetain, _sh->mqttstruct.mqttWillText.c_str(), _sh->mqttstruct.mqttWillText.length());
        
        _sh->mqttstruct.mqtt_Tempsens_Vector.clear();

        for (unsigned int i = 0; i < listtopics.size(); i++)
        {
            if (listsensors[i]->second != "" && listtopics[i]->second != "" && listqos[i]->second !="")
            {
                if(listqos[i]->second.toInt()>2)listqos[i]->second="2";
                if(listqos[i]->second.toInt()<0)listqos[i]->second="0";
                _sh->mqttstruct.mqtt_Tempsens_Vector.emplace_back((struct mqtt_tempsens_settings)
                                                        {listtopics[i]->second,
                                                         listsensors[i]->second,
                                                        (unsigned int) listqos[i]->second.toInt()});
            }
        }
        _sh->mqttstruct.mqtt_Tempsens_Vector.shrink_to_fit();
        _sh->mqttstruct.saveMQTTsettings();
        _sh->mqttCommand.saveMQTTCommandsettings();
        request->redirect(F("/wifi_mqtt.html"));
        _mediator.connect();
    }
}


void WifiTool::handleSaveRelays(AsyncWebServerRequest *request)
{
    _sh->relayarray.clearArray();
    SimpleJsonWriter sjwrelay;
    SimpleJsonWriter sjwlogic;
    int newsrsz = 0;
    String id;
    String pin;
    String istate;
    String onstatelevel;
    for (unsigned int i = 0; i < request->params(); i++)
    {
        if (request->argName(i).indexOf("i") == 0) // if the name of the argument starts with "i"
        {
            id = request->arg(i);
            String key= request->argName(i);
            key.replace("i", "");
            int srsz = key.toInt();
            for(unsigned int j=0;j<request->params();j++)
            {
                if(request->argName(j).substring(1).toInt()==srsz)
                {
                    if(request->argName(j).substring(0,1)=="p") // pin
                    {
                        pin = request->arg(j);
                        continue;
                    }
                    if(request->argName(j).substring(0,1)=="n") // initstate
                    {
                        istate = request->arg(j);
                        continue;
                    }
                    if(request->argName(j).substring(0,1)=="o") // onstatelevel
                    {
                        onstatelevel = request->arg(j);
                        continue;
                    } 
                }   
            }
            _sh->relayarray.addRelay(id, (uint8_t)pin.toInt(), (istate=="true" || istate=="1"), (onstatelevel=="true" || onstatelevel=="1"));
            newsrsz++;
        }
    }
    _sh->relayarray.saveRelays(RELAY_JSON);
    sjwrelay.clear();
    SimpleJsonParser sjprelay;
    std::vector<std::pair<String, String>> relay = sjprelay.extractKeysandValuesFromFile(RELAY_JSON);
    newsrsz = 0;
    for (unsigned int i = 0; i < request->params(); i++)
    {
        if(request->argName(i).indexOf("p") == 0)
        {
            for (size_t j = 0; j < relay.size(); j++)
            {
                if(relay[j].first.indexOf(":pin") >0 && relay[j].second == request->arg(i))
                {
                    int logicsrsz=request->argName(i).substring(1).toInt();
                    int relaysrsz = relay[j].first.substring(0, relay[j].first.indexOf(":pin")).toInt();
                    String relayid = sjprelay.getValueByKeyFromFile(RELAY_JSON, String(relaysrsz) + ":id");
                    for(size_t k=0;k<request->params();k++)
                    {
                        if(request->argName(k).substring(1).toInt() ==logicsrsz)
                        {
                            if(request->argName(k).substring(0,1)=="i") 
                            {
                                sjwlogic.addKeyValue(String(newsrsz)+":id", relayid);
                                continue;
                            }
                            if(request->argName(k).substring(0,1)=="g") // logic
                            {
                                sjwlogic.addKeyValue(String(newsrsz)+":logic", request->arg(k));
                                continue;
                            }
                            if(request->argName(k).substring(0,1)=="h") // diff_hightemp
                            {
                                sjwlogic.addKeyValue(String(newsrsz)+":diff_hightemp", request->arg(k));
                                continue;
                            }
                            if(request->argName(k).substring(0,1)=="l") // diff_lowtemp
                            {
                                sjwlogic.addKeyValue(String(newsrsz)+":diff_lowtemp", request->arg(k));
                                continue;
                            }
                            if(request->argName(k).substring(0,1)=="d") // diff_deltatemp
                            {
                                sjwlogic.addKeyValue(String(newsrsz)+":diff_deltatemp", request->arg(k));
                                continue;
                            }
                            if(request->argName(k).substring(0,1)=="t") // thermo_tempsensor
                            {
                                sjwlogic.addKeyValue(String(newsrsz)+":thermo_tempsensor", request->arg(k));
                                continue;
                            }
                            if(request->argName(k).substring(0,1)=="z") // thermo_temp
                            {
                                sjwlogic.addKeyValue(String(newsrsz)+":thermo_temp", request->arg(k));
                                continue;
                            }
                            if(request->argName(k).substring(0,1)=="e") // thermo_deltatemp
                            {
                                sjwlogic.addKeyValue(String(newsrsz)+":thermo_deltatemp", request->arg(k));
                                continue;
                            }
                            if(request->argName(k).substring(0,1)=="f") // freez_sensor
                            {
                                sjwlogic.addKeyValue(String(newsrsz)+":freez_sensor", request->arg(k));
                                continue;
                            }              
                            if(request->argName(k).substring(0,1)=="x") // freez_period
                            {
                                sjwlogic.addKeyValue(String(newsrsz)+":freez_period", request->arg(k));
                                continue;
                            }
                            if(request->argName(k).substring(0,1)=="r") // freez_temp
                            {
                                sjwlogic.addKeyValue(String(newsrsz)+":freez_temp", request->arg(k));
                                continue;
                            }              

                        }
                    }
                    newsrsz++;
                }
            }
             
        }
    }

    File file2 = SPIFFS.open(F(RELAY_LOGIC_JSON), "w");
    if (!file2)
    {
        _WIFITOOL_PL(F("Error opening file for writing"));
        return;
    }
    file2.print(sjwlogic.getJsonString());
    file2.flush();
    file2.close();
    _WIFITOOL_PL(sjwlogic.getJsonString());

    _sh->LoadRelayConrolLogics(RELAY_LOGIC_JSON);
    request->redirect(F("/wifi_relays.html"));
}
/*
   handleGetSavSecretJson()
   Save the secrets: AP password, knonw AP passwords and SSIDs.
*/
void WifiTool::handleSaveSecretJson(AsyncWebServerRequest *request)
{
    
    _wifimanager.setAPpassword(request->arg(F("APpass")));
    
    _wifimanager.addCredentials(request->arg(F("ssid0")),request->arg(F("pass0")),0);
    _wifimanager.addCredentials(request->arg(F("ssid1")),request->arg(F("pass1")),1);
    _wifimanager.addCredentials(request->arg(F("ssid2")),request->arg(F("pass2")),2);

    _wifimanager.saveCredentials();
    request->redirect(F("/wifi_manager.html"));
}

void WifiTool::handleSaveNTPJson(AsyncWebServerRequest *request)
{
    SimpleJsonWriter sjw;
    String extratsh = String(F("extratsh"));
    String UTCm = String(F("UTCm"));
    String UTCh = String(F("UTCh"));

    
    sjw.addKeyValue("NTPserver", request->arg("NTPserver"));
    
    _ntp.setNTPServer(request->arg("NTPserver"));
    
    sjw.addKeyValue("UTCh", request->arg(UTCh));
    
    _ntp.setUtcHour((int8_t)request->arg(UTCh).toInt());
    
    sjw.addKeyValue("UTCm", request->arg(UTCm));

    _ntp.setUtcMin((uint8_t)abs(request->arg(UTCm).toInt()));

    sjw.addKeyValue("extratsh", request->arg(extratsh));
    
    if (request->arg(extratsh) == "ST")
    {
        _ntp.setSTDST(1); // Summer Time
    }
    else if (request->arg(extratsh) == "DST")
    {
        _ntp.setSTDST(2); // Daylight Saving Time
    }
    else
    {
        _ntp.setSTDST(0);
    }

    _WIFITOOL_PL(sjw.getJsonString());

    File file = SPIFFS.open(F(NTP_JSON), "w");
    if (!file)
    {
        _WIFITOOL_PL(F("Error opening file for writing"));
        return;
    }
    file.print(sjw.getJsonString());
    file.flush();
    file.close();
    request->redirect(F("/wifi_NTP.html"));
}

void WifiTool::handleRescanWires(AsyncWebServerRequest *request)
{
    //rescan done by handleGetUnkonwSensors()
    request->redirect(F("/wifi_tempinvent.html"));
}

void WifiTool::handleSaveThingspeakJson(AsyncWebServerRequest *request)
{
    SimpleJsonWriter sjw;
    std::vector<std::pair<String, String>> chidwrapi;
    std::vector<std::vector<String>> fieldmap;

    for (unsigned int i = 0; i < request->args(); i = i + 10)
    {
        if (request->arg(i) != "" && request->arg(i + 1) != "")
        {
            chidwrapi.emplace_back(std::make_pair(String{request->arg(i)},
                                                  String{request->arg(i + 1)}));

            std::vector<String> fi;
            for (unsigned int n = i + 2; n < i + 2 + 8; n++)
            {
                fi.emplace_back(request->arg(n));
            }
            fieldmap.emplace_back(fi);
        }
    }


    for (unsigned int i = 0; i < chidwrapi.size(); i++)
    {
        sjw.addKeyValue(chidwrapi.at(i).first, chidwrapi.at(i).second);
    }
    _WIFITOOL_PL(sjw.getJsonString());

    File file = SPIFFS.open(TEAMSPEAK_SECRETS_JSON, "w");
    if (!file)
    {
        _WIFITOOL_PL(F("Error opening file for writing"));
        request->redirect(F("/wifi_thingspeak.html"));
        return;
    }

    file.print(sjw.getJsonString());
    file.flush();
    file.close();

    sjw.clear();

    for (unsigned int i = 0; i < chidwrapi.size(); i++)
    {

        for (unsigned int n = 0; n < 8; n++)
        {
            sjw.addKeyValue(String(chidwrapi.at(i).first + String("_") + String(n + 1)), fieldmap.at(i).at(n));
        }
    }

    _WIFITOOL_PL(sjw.getJsonString());

    file = SPIFFS.open(TEAMSPEAK_FIELDS_JSON, "w");
    if (!file)
    {
        _WIFITOOL_PL(F("Error opening file for writing"));
        return;
    }

    file.print(sjw.getJsonString());
    file.flush();
    file.close();

    request->redirect(F("/wifi_thingspeak.html"));
}

void WifiTool::handleSendTime(AsyncWebServerRequest *request)
{
    const AsyncWebParameter *p = request->getParam("time", true);
    if (_strdt.epochTime == 0 && p != nullptr)
    {
        char atm[11];
        memset(atm, 0, 11);
        strncpy(atm, p->value().c_str(), 10);
        char **ptr=nullptr;
        unsigned long b;
        b = strtoul(atm, ptr, 10); // string to unsigned long
        b = _ntp.adjustTimeZone(b, _ntp.getUtcHour(), _ntp.getUtcMin(), _ntp.getSTDST());
        _strdt.setFromUnixTimestamp(b);
        _strdt.valid = true;

        RtcDateTime dt;
        dt.InitWithEpoch32Time(b);
        _rtc.SetDateTime(dt);
        Serial.println(F("Synced with browser."));
    }
    request->send(200);
}
/**
 * @brief Handle a GET request to /version
 * @details This function responds with a text/plain response containing the
 *          version number of the firmware, the SSID and signal strength of the
 *          connected network, and the uptime of the device in days, hours, minutes
 *          and seconds.
 * @param[in] request The AsyncWebServerRequest object.
 */
void WifiTool::handleGetVersion(AsyncWebServerRequest *request)
{
    _WIFITOOL_PL("Send version:" + getVersion());
    // Calculate uptime
    unsigned long uptime = millis() / 1000;
    unsigned long days = uptime / 86400;    // 86400 seconds in a day
    unsigned long hours = (uptime % 86400) / 3600; // 3600 seconds in an hour
    unsigned long minutes = (uptime % 3600) / 60;   // 60 seconds in a minute
    unsigned long seconds = uptime % 60;            // remaining seconds
    
    String uptimeString = String(days) + " days, " + String(hours) + "h : " + String(minutes) + "m : " + String(seconds) + "s";
    request->send(200, "text/plain", "Version:" + getVersion() +"  ( Network:" + WiFi.SSID() + " Strength:" + String(WiFi.RSSI())+" dBm)" + " Uptime:" + uptimeString);
}


/**
 * setUpSoftAP()
 * Setting up the SoftAP Service
 */
void WifiTool::setUpSoftAP()
{

    dnsServer.reset(new DNSServer());
    delay(500);
    dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer->start(DEF_DNS_PORT, "*", IPAddress(DEF_DNS_IP));

    Serial.println(F("DNS server started."));

    
    _server.serveStatic("/", SPIFFS, "/").setDefaultFile("wifi_index.html").setCacheControl("max-age=0, no-store");

    // Handle ALL .json files with no-cache headers
  _server.on("/(.*\\.json)", HTTP_GET, [](AsyncWebServerRequest *request) {
    String path = request->url();
    if (!SPIFFS.exists(path)) {
      request->send(404, "text/plain", "File not found");
      return;
    }
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, path, "application/json");
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    request->send(response);
  });



    _server.on("/saveSecret/", HTTP_ANY, [&, this](AsyncWebServerRequest *request)
               { handleSaveSecretJson(request); });

    _server.on("/saveTempsens/", HTTP_POST, [&, this](AsyncWebServerRequest *request)
               { handleSaveSensorInventory(request); });

    _server.on("/saveNTP/", HTTP_ANY, [&, this](AsyncWebServerRequest *request)
               { handleSaveNTPJson(request); });

    _server.on("/saveThingspeak/", HTTP_ANY, [&, this](AsyncWebServerRequest *request)
               { handleSaveThingspeakJson(request); });

    _server.on("/savemqtt/", HTTP_POST, [&, this](AsyncWebServerRequest *request)
               { handleSaveMqtt(request); });

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

    _server.on("/mqttsettings.json", HTTP_GET, [&, this](AsyncWebServerRequest *request)
               { handleGetMqttjson(request); });

    _server.on("/getunknownsenses.json", HTTP_GET, [&, this](AsyncWebServerRequest *request)
               { handleGetUnknownSenors(request); });

    _server.on("/getlivesensors", HTTP_GET, [&, this](AsyncWebServerRequest *request)
               { handleGetLiveSensors(request); });
               
    _server.on("/getdevicenames.json", HTTP_GET, [&, this](AsyncWebServerRequest *request)
               { handleGetDeviceNames(request); });

    _server.on("/getfilteredcommands", HTTP_POST, [&, this](AsyncWebServerRequest *request)
               { handleGetfilteredCommands(request); });

    _server.on("/getdeviceusernames", HTTP_POST, [&, this](AsyncWebServerRequest *request)
               { handleGetDeviceUsernames(request); });

    _server.on("/savecommfilter/", HTTP_POST, [&, this](AsyncWebServerRequest *request)
               { handleSaveCommandFilter(request); });

    _server.on("/saverelays/", HTTP_POST, [&, this](AsyncWebServerRequest *request)
               { handleSaveRelays(request); });

    _server.on("/getversion", HTTP_GET, [&, this](AsyncWebServerRequest *request)
               { handleGetVersion(request); });

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

    const AsyncWebParameter *p = request->getParam("dir");
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
            output += String(file.name()).substring(0);
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
}

void WifiTool::handleFileDownload(AsyncWebServerRequest *request)
{
    if (request->params() == 0)
    {
        return request->send(500, "text/plain", "BAD ARGS");
    }

    const AsyncWebParameter *p = request->getParam(0);
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

     //request->send(SPIFFS, path, "application/x-download", true);

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
        Serial.println(String(F("UploadEnd: ")) + filename);
        _fsUploadFile.close();
        request->send(200, "text/plain", "");
    }
}
