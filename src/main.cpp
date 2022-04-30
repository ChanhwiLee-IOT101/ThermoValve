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

char*               ssid_pfix = (char*)"LCHTHERMOSTAT";
unsigned long       lastPublishMillis = - pubInterval;

const int pulseA = 12;
const int pulseB = 13;
const int pushSW = 2;
volatile int lastEncoded = 0;
volatile long encoderValue = 70;

float humidity;
float temp_f;
unsigned long lastDHTReadMillis = 0;
float tgtT;

#define interval 2000
#define DHTPIN  14
DHTesp  dht;
SSD1306 display(0x3c, 4, 5, GEOMETRY_128_32);

void gettemperature() {
    unsigned long currentMillis = millis();
    if(currentMillis - lastDHTReadMillis >= interval) {
    lastDHTReadMillis = currentMillis;
    humidity = dht.getHumidity(); // Read humidity (percent)
    temp_f = dht.getTemperature(); // Read temperature as Fahrenheit
  }
}

IRAM_ATTR void handleRotary() {
// Never put any long instruction
  int MSB = digitalRead(pulseA); //MSB = most significant bit
  int LSB = digitalRead(pulseB); //LSB = least significant bit
  int encoded = (MSB << 1) |LSB; //converting the 2 pin value to single number
  int sum = (lastEncoded << 2) | encoded; //adding it to the previous encoded value
  if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderValue ++;
  if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderValue --;
  lastEncoded = encoded; //store this value for next time
  if (encoderValue > 255) {
  encoderValue = 255;
  } else if (encoderValue < 0 ) {
    encoderValue = 0;
  }
  lastPublishMillis = millis() - pubInterval + 200; // send and publish immediately
}





void publishData() {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");

    char dht_buffer[10];
    char dht_buffer2[10];
    char dht_buffer3[10];

// USER CODE EXAMPLE : command handling
  
    gettemperature();

    display.setColor(BLACK);
    display.fillRect(80, 11, 100, 33);
    display.setColor(WHITE);
    sprintf(dht_buffer, "%2.1f", temp_f);
    display.drawString(80, 11, dht_buffer);
    data["temperature"] = dht_buffer;

    sprintf(dht_buffer3, "%2.1f", humidity);
    data["humidity"] = dht_buffer3;
    display.drawString(2, 10, dht_buffer3);

    tgtT = map(encoderValue, 0, 255, 10, 50);
    sprintf(dht_buffer2, "%2.1f", tgtT);
    data["target"] = dht_buffer2;
    display.drawString(80, 22, dht_buffer2);
    display.display();
    data["rotary"] = encoderValue;
    // data["lux"] = digitalRead(RELAY) == 1 ? "on" : "off";
// USER CODE EXAMPLE : command handling

    serializeJson(root, msgBuffer);
    client.publish(publishTopic, msgBuffer);
    Serial.print("pub");
}

void handleUserCommand(JsonDocument* root) {
    JsonObject d = (*root)["d"];
    
// USER CODE EXAMPLE : status/change update
// code if any of device status changes to notify the change

    if(d.containsKey("target")) {
      tgtT = d["target"];
      encoderValue = map(tgtT, 10, 50, 0, 255);
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
  pinMode(pulseA, INPUT_PULLUP);
  pinMode(pulseB, INPUT_PULLUP);
  attachInterrupt(pulseA, handleRotary, CHANGE);
  attachInterrupt(pulseB, handleRotary, CHANGE);
  dht.setup(DHTPIN, DHTesp::DHT22);
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_16);
  display.drawString(35, 0, "IOT Thermostat");
  display.drawString(20, 11, "Current : ");
  display.drawString(20, 22, "Target : ");
  display.display();

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
// USER CODE EXAMPLE
    client.loop();
    if ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)) {
       publishData();
       lastPublishMillis = millis(); 
    }
}