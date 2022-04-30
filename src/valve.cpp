#include <Arduino.h>
#include <IBMIOTF8266.h>
#include <Wire.h>
#include <SSD1306.h>
#include <DHTesp.h>

// USER CODE EXAMPLE : Publish Interval. The periodic update is normally recommended.
// And this can be a good example for the user code addition
String user_html = ""
    "<p><input type='text' name='meta.pubInterval' placeholder='Publish Interval'>";
// USER CODE EXAMPLE : command handling

char*               ssid_pfix = (char*)"LCHVALVE";
unsigned long       lastPublishMillis = - pubInterval;
const int RELAY = 15;


#define interval 2000
#define DHTPIN  14
DHTesp  dht;
SSD1306 display(0x3c, 4, 5, GEOMETRY_128_32);


void publishData() {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");

// USER CODE EXAMPLE : command handling
    
    data["valve"] = digitalRead(RELAY) ? "on" : "off";

    // data["lux"] = digitalRead(RELAY) == 1 ? "on" : "off";
// USER CODE EXAMPLE : command handling

    serializeJson(root, msgBuffer);
    client.publish(publishTopic, msgBuffer);
}

void handleUserCommand(JsonDocument* root) {
    JsonObject d = (*root)["d"];
    
// USER CODE EXAMPLE : status/change update
// code if any of device status changes to notify the change
 
    if(d.containsKey("valve")) {
      if(!strcmp(d["valve"], "on")) {
        digitalWrite(RELAY, HIGH);
      } else {
        digitalWrite(RELAY, LOW);
      }
      lastPublishMillis = - pubInterval;
    }
// USER CODE EXAMPLE
}



void message(char* topic, byte* payload, unsigned int payloadLength) {
    byte2buff(msgBuffer, payload, payloadLength);
    StaticJsonDocument<512> root;
    DeserializationError error = deserializeJson(root, String(msgBuffer));
    
    Serial.println(topic);
  
    if (error) {
        Serial.println("handleCommand: payload parse FAILED");
        return;
    }

    handleIOTCommand(topic, &root);
    if (!strcmp(updateTopic, topic)) {
// USER CODE EXAMPLE : meta data update
// If any meta data updated on the Internet, it can be stored to local variable to use for the logic
        pubInterval = cfg["meta"]["pubInterval"];
// USER CODE EXAMPLE
    } else if (!strncmp(commandTopic, topic, 10)) {            // strcmp return 0 if both string matches
        handleUserCommand(&root);
    }
}

void setup() {
    Serial.begin(115200);
// USER CODE EXAMPLE : meta data update
  pinMode(RELAY, OUTPUT);

// USER CODE EXAMPLE

    initDevice();
    // If not configured it'll be configured and rebooted in the initDevice(),
    // If configured, initDevice will set the proper setting to cfg variable

    WiFi.mode(WIFI_STA);
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    // main setup
    Serial.printf("\nIP address : "); Serial.println(WiFi.localIP());
// USER CODE EXAMPLE : meta data to local variable
    JsonObject meta = cfg["meta"];
    pubInterval = meta.containsKey("pubInterval") ? atoi((const char*)meta["pubInterval"]) : 0;
    lastPublishMillis = - pubInterval;
// USER CODE EXAMPLE
    
    set_iot_server();
    client.setCallback(message);
    iot_connect();
}

void loop() {
    if (!client.connected()) {
        iot_connect();
    }
// USER CODE EXAMPLE : main loop
//     you can put any main code here, for example, 
//     the continous data acquistion and local intelligence can be placed here
// USER CODE EXAMPLEX
    client.loop();
    if ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)) {
        publishData();
        lastPublishMillis = millis();
    }
}