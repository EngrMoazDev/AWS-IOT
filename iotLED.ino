/*
  Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
  Permission is hereby granted, free of charge, to any person obtaining a copy of this
  software and associated documentation files (the "Software"), to deal in the Software
  without restriction, including without limitation the rights to use, copy, modify,
  merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
  INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
  PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/


#include "secrets.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"
#include <string>
// The MQTT topics that this device should publish/subscribe
#define AWS_IOT_PUBLISH_TOPIC   "$aws/things/ESP32WROOM/shadow/update"
#define AWS_IOT_SUBSCRIBE_TOPIC "$aws/things/ESP32WROOM/shadow/update/delta"

int msgReceived = 0;
String rcvdPayload;
char sndPayloadOff[512];
unsigned long interval = 50000;
WiFiClientSecure net = WiFiClientSecure();
MQTTClient client = MQTTClient(256);

void connectAWS()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("");
  Serial.println("###################### Starting Execution ########################");
  Serial.println("Connecting to Wi-Fi");
  unsigned long previousMillis = millis();
  while (WiFi.status() != WL_CONNECTED ){
    unsigned long currentMillis = millis();
    delay(500);
    Serial.print(".");
    if (currentMillis - previousMillis >=interval){
      Serial.print("Restarting");
      delay(2000);
      ESP.restart();
    }
  }

  Serial.println("Wi-Fi Connected");
  
  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.begin(AWS_IOT_ENDPOINT, 8883, net);

  // Create a message handler
  client.onMessage(messageHandler);

  Serial.println("Connecting to AWS IOT");
  previousMillis = millis();
  while (!client.connect(THINGNAME)) {
    unsigned long currentMillis = millis();
    Serial.print(".");
    delay(100);
    if (currentMillis - previousMillis >=interval){
      Serial.print("Restarting ESP");
      delay(300);
      ESP.restart();
    }
  }

  if(!client.connected()){
    Serial.println("AWS IoT Timeout!");
    Serial.print("Restarting ESP");
    delay(300);
    ESP.restart();
  }

  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

  Serial.println("AWS IoT Connected!");
}

void messageHandler(String &topic, String &payload) {
  if (payload.indexOf("status")!=-1) {
    // not found
    msgReceived = 1;
    rcvdPayload = payload;
  } else {
    // found
    msgReceived = 0;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(13, OUTPUT);

  sprintf(sndPayloadOff,"{\"state\": { \"reported\": { \"status\": \"off\" ,\"temp\": \"0\" ,\"humidity\": \"0\" } }}");
  
  connectAWS();
  
  Serial.println("Turning OFF LED, Setting Humidity and Temperature to 0");
  client.publish(AWS_IOT_PUBLISH_TOPIC, sndPayloadOff);
  digitalWrite(13, LOW);
  Serial.println("##############################################");
}

void loop() {
   if(msgReceived == 1)
    {
//      This code will run whenever a message is received on the SUBSCRIBE_TOPIC_NAME Topic
        delay(100);
        msgReceived = 0;
        Serial.print("Received Message:");
        Serial.println(rcvdPayload);
        StaticJsonDocument<200> sensor_doc;
        DeserializationError error_sensor = deserializeJson(sensor_doc, rcvdPayload);
        const char *sensor = sensor_doc["state"]["status"];
 
        Serial.print("AWS Says:");
        Serial.println(sensor); 
        
        if(strcmp(sensor, "on") == 0)
        {
         Serial.println("Turning LED On");
         digitalWrite(13, HIGH);
         String payload = "{\"state\": { \"reported\": { \"status\": \"on\" } }}";
         client.publish(AWS_IOT_PUBLISH_TOPIC, payload);
        }
        else 
        {
         Serial.println("Turning LED Off");
         digitalWrite(13, LOW);
         String payload = "{\"state\": { \"reported\": { \"status\": \"off\" } }}";
         client.publish(AWS_IOT_PUBLISH_TOPIC, payload);
        }
      Serial.println("##############################################");
    }
    if (Serial.available()) { // if there is data comming
        String command = Serial.readStringUntil('\n'); // read string until newline character
        int pos = command.indexOf(':');
        String key = command.substring(0, pos);
        String value = command.substring(pos+1);
        if (key == "temp") {
          String payload = "{\"state\": { \"reported\": { \"temp\": \""+value+"\" } }}" ;
          client.publish(AWS_IOT_PUBLISH_TOPIC, payload);
          Serial.println("Tempertaure Published");
        } else if (key == "humidity") {
          String payload = "{\"state\": { \"reported\": { \"humidity\": \""+value+"\" } }}";
          client.publish(AWS_IOT_PUBLISH_TOPIC, payload);
          Serial.println("Himidity Published");
        }
     }
     if (WiFi.status() != WL_CONNECTED ){
       Serial.print("Restarting");
       delay(2000);
       ESP.restart();
     }
     
  client.loop();
}
