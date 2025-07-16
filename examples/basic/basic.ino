#include <wifiTool.h>
#include <BasicOTA.h>
AsyncWebServer webserver(80);
WifiTool wifiTool(webserver);

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.begin(115200);
  Serial.println("System started");

  if (!SPIFFS.begin())
    {
        // Serious problem
        Serial.println(F("SPIFFS Mount failed."));
    }

    BasicOTA.begin(&webserver);
     //WifiTool init
    wifiTool.begin();

}

void loop() {
  wifiTool.process();

}