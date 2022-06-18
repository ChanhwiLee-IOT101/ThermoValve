#include <Arduino.h>
#include <IBMIOTF8266.h>
#include <Wire.h>
#include <DHTesp.h>
#include "ESP8266_ISR_Servo.h"
#include <SSD1306.h>

SSD1306             display(0x3c, 4, 5, GEOMETRY_128_32);

String user_html = "";

char*               ssid_pfix = (char*)"lchthermo";
unsigned long       lastPublishMillis = - pubInterval;
const int           pulseA = 12;
const int           pulseB = 13;
volatile int        lastEncoded = 0;
volatile long       encoderValue = 70;

int temp = 0;

// DHT22 is attached to GPIO14 on this Kit
#define             DHTPIN  14
DHTesp              dht;
float               humidity;
float               temp_f;

unsigned long       lastDHTReadMillis = 0;
#define             interval  2000   
float               tgtT;
char weather[20];
char tomorrow[20];
char wind_speed[20];

#define TIMER_INTERRUPT_DEBUG       1
#define ISR_SERVO_DEBUG             1

// Published values for SG90 servos; adjust if needed
#define MIN_MICROS      800  //544
#define MAX_MICROS      2450

#define SW_DOOR         14
#define SW_STORE        15
// #define BUZZER_PIN      16
#define TRIG_PIN        13
#define ECHO_PIN        12
#define DOOR_PIN        2
#define SIG_PIN         16

#define S_CLOSE         0
#define S_OPEN          1
#define D_CLOSE         0
#define D_OPEN          1

int door_open_degree = 45;
int door_close_degree = 135;

int Store_State = S_CLOSE;
int Door_State = D_CLOSE;

int Sound = 900;
long duration;
float distance;

int servoIndex  = -1;



void publishData() {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");
    
    char dht_buffer[20];
    char dht_buffer2[20];
    char dht_buffer3[10];

    char dht_buffer4[10];
    char dht_buffer5[10];
    char dht_buffer6[10];

    if (Store_State == S_OPEN) {
      sprintf(dht_buffer, "%s", "open");
      data["Store_State"] = dht_buffer;
      sprintf(dht_buffer3, "%s", "false");
      data["isDanger"] = dht_buffer3;

      if (Door_State == D_OPEN) {
        sprintf(dht_buffer2, "%s", "open");
        data["Door_State"] = dht_buffer2;
      }
      else if (Door_State == D_CLOSE) {
        sprintf(dht_buffer2, "%s", "close");
        data["Door_State"] = dht_buffer2;
      }
    }

    else if (Store_State == S_CLOSE)
    {
      sprintf(dht_buffer, "%s", "close");
      data["Store_State"] = dht_buffer;
      sprintf(dht_buffer2, "%s", "close");
      data["Door_State"] = dht_buffer2;

      if (distance <= 5) {
        sprintf(dht_buffer3, "%s", "true");
        data["isDanger"] = dht_buffer3;
      }
      else {
        sprintf(dht_buffer3, "%s", "false");
        data["isDanger"] = dht_buffer3;
      }
    }

    display.setColor(BLACK);
    display.fillRect(80, 11, 100, 33);
    display.setColor(WHITE);
    sprintf(dht_buffer4, "%s", weather);
    display.drawString(80, 11, dht_buffer4);
    data["current"] = dht_buffer4;

    sprintf(dht_buffer5, "%s", tomorrow);
    data["tomorrow"] = dht_buffer5;
    display.drawString(80, 22, dht_buffer5);
    display.display();

    sprintf(dht_buffer6, "%s", wind_speed);
    data["wind"] = dht_buffer6;
    display.drawString(80, 0, dht_buffer6);
    display.display();

    serializeJson(root, msgBuffer);
    client.publish(publishTopic, msgBuffer);
}

void handleUserCommand(JsonDocument* root) {
    JsonObject d = (*root)["d"];
    if(d["Door"] == "Open") {
      if(Store_State == S_OPEN) {
        if(Door_State == D_CLOSE) {
          Door_State = D_OPEN;
          ISR_Servo.setPosition(servoIndex, door_open_degree);
        }
      }
    }
    if(d["Door"] == "Close") {
      if(Store_State == S_OPEN) {
        if(Door_State == D_OPEN) {
          Door_State = D_CLOSE;
          ISR_Servo.setPosition(servoIndex, door_close_degree);
        }
      }
    }

    if(d["Store"] == "Open") {
      if(Store_State == S_CLOSE) {
        Store_State = S_OPEN;
      }
    }
    if(d["Store"] == "Close") {
      if(Store_State == S_OPEN) {
        Store_State = S_CLOSE;
      }
    }
    if(d.containsKey("current")) {
        strcpy(weather,d["current"]["weather"]);
        strcpy(wind_speed,d["current"]["wind"]);
        lastPublishMillis = - pubInterval;
    }

    if(d.containsKey("day2")) {
        strcpy(tomorrow,d["day2"]["weather"]);
        Serial.print(tomorrow);
    }
}

void message(char* topic, byte* payload, unsigned int payloadLength) {
    byte2buff(msgBuffer, payload, payloadLength);
    StaticJsonDocument<512> root;
    DeserializationError error = deserializeJson(root, String(msgBuffer));
  
    if (error) {
        Serial.println("handleCommand: payload parse FAILED");
        return;
    }
    
    handleIOTCommand(topic, &root);
    Serial.println(topic);
    Serial.println(msgBuffer);
    if (!strncmp(updateTopic, topic, cmdBaseLen)) {
        // user variable update
    } else if (!strncmp(commandTopic, topic, cmdBaseLen)) {            // strcmp return 0 if both string matches
        handleUserCommand(&root);
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(SW_DOOR, INPUT);
    pinMode(SW_STORE, INPUT);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(SIG_PIN, OUTPUT);
    servoIndex = ISR_Servo.setupServo(DOOR_PIN, MIN_MICROS, MAX_MICROS);

    Store_State = S_OPEN;
    Door_State = D_OPEN;

    if (servoIndex != -1)
        Serial.println(F("Setup Servo1 OK"));
    else
        Serial.println(F("Setup Servo1 failed"));

    digitalWrite(SIG_PIN, LOW);

    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    display.drawString(20, 0, "Wind :");
    display.drawString(20, 11, "Today : ");
    display.drawString(20, 22, "Tomorrow : ");
    display.display();

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
    JsonObject meta = cfg["meta"];
    pubInterval = meta.containsKey("pubInterval") ? atoi((const char*)meta["pubInterval"]) : 0;
    lastPublishMillis = - pubInterval;
    
    set_iot_server();
    client.setCallback(message);
    iot_connect();
}

void loop() {

     // int position;
  // 
  // if (servoIndex != -1){
  //   for (position = 0; position <= 180; position++){
  //     ISR_Servo.setPosition(servoIndex, position);
  //     delay(100);
  //   }
  // }
  // delay(1000);


  // if (servoIndex != -1){
  //   for (position = 180; position >= 0; position--){
  //     ISR_Servo.setPosition(servoIndex, position);
  //     delay(100);
  //   }
  // }
  // delay(1000);
  
  Serial.print("----- State -----\nDoor : ");
  Serial.print(Door_State ? "OPEN" : "CLOSE");
  Serial.print(", Store : ");
  Serial.println(Store_State ? "OPEN" : "CLOSE");


  // Store OPEN -> Working Mode
  if ( Store_State == S_OPEN ) {
    if( digitalRead(SW_DOOR) == 1  ){
      // Door CLOSE -> OPEN
      if(Door_State == D_CLOSE) {
        Serial.println("Door Open!");
        ISR_Servo.setPosition(servoIndex, door_open_degree);
        Door_State = D_OPEN;
        delay(1000);
      }
      // Door OPEN -> CLOSE
      else if(Door_State == D_OPEN) {
        Serial.println("Door Close!");
        ISR_Servo.setPosition(servoIndex, door_close_degree);
        Door_State = D_CLOSE;
        delay(1000);
      }
    }

    // Stop Work Mode -> Change to Monitor Mode
    // Store -> CLOSE
    if( digitalRead(SW_STORE) == 1) {
      Store_State = S_CLOSE;
      Door_State = D_CLOSE;
      ISR_Servo.setPosition(servoIndex, door_close_degree);
    }
  }

  // Store CLOSE -> Monitor Mode
  else if ( Store_State == S_CLOSE ) {

    // Ultrasonic - Generate Trigger
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // Calc Distance
    duration = pulseIn(ECHO_PIN, HIGH);
    Serial.print("Duration: ");
    Serial.println(duration);
    distance = duration * 0.017;  // distance = duration / 29 / 2

    // Print Distance (cm)
    Serial.print(distance);
    Serial.println("cm");

    // Monitor
    if (distance <= 5) {
      Serial.println("!!! Danger !!!");
      digitalWrite(SIG_PIN, HIGH);
      // tone(BUZZER_PIN , Sound);
      delay(300);
      // noTone(BUZZER_PIN);
    }
    else{
      digitalWrite(SIG_PIN, LOW);
      // noTone(BUZZER_PIN );
      delay(100);
    }
    
    // Stop Monitor Mode -> Change to Work Mode
    // Store -> OPEN
    if( digitalRead(SW_STORE) == 1) {
      digitalWrite(SIG_PIN, LOW);
      Store_State = S_OPEN;
      // noTone(BUZZER_PIN );
    }
  }

    if (!client.connected()) {
        iot_connect();
    }
    client.loop();
    if ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)) {
        publishData();
        lastPublishMillis = millis();
    }
}