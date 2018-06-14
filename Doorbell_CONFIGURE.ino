
#include <SimpleTimer.h> //https://github.com/jfturcot/SimpleTimer
#include <PubSubClient.h> //https://github.com/knolleary/pubsubclient
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h> //https://github.com/esp8266/Arduino/tree/master/libraries/ArduinoOTA

//USER CONFIGURED SECTION START//
const char* ssid = "YOUR_WIRELESS_SSID";
const char* password = "YOUR_WIRELESS_PASSWORD";
const char* mqtt_server = "YOUR_MQTT_SERVER_ADDRESS";
const int mqtt_port = YOUR_MQTT_SERVER_PORT;
const char *mqtt_user = "YOUR_MQTT_USERNAME";
const char *mqtt_pass = "YOUR_MQTT_PASSWORD";
const char *mqtt_client_name = "Doorbell"; // Client connections can't have the same connection name
//USER CONFIGURED SECTION END//

WiFiClient espClient;
PubSubClient client(espClient);
SimpleTimer timer;

// Variables
bool alreadyTriggered = false;
const int doorBellPin = 16; //marked as D0 on the board
const int frontPin = 0; //marked as D3 on the board
const int silencePin = 2;  //marked as D4 on the board
int frontOldStatus = 1;
bool boot = true;

//Functions

void setup_wifi() {
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() 
{
  // Loop until we're reconnected
  int retries = 0;
  while (!client.connected()) {
    if(retries < 15)
    {
        Serial.print("Attempting MQTT connection...");
      // Attempt to connect
      if (client.connect(mqtt_client_name, mqtt_user, mqtt_pass, "LWT/DoorbellMCU", 1,1,"Offline")) 
      {
        client.publish("LWT/DoorbellMCU","Online", true);
        Serial.println("connected");
        // Once connected, publish an announcement...
        if(boot == true)
        {
          client.publish("checkIn/DoorbellMCU","Rebooted");
          boot = false;
        }
        if(boot == false)
        {
          client.publish("checkIn/DoorbellMCU","Reconnected"); 
        }
        // ... and resubscribe
        client.subscribe("doorbell/commands");
      } 
      else 
      {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        retries++;
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }
    if(retries > 14)
    {
    ESP.restart();
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) 
{
  Serial.print("Message arrived [");
  String newTopic = topic;
  Serial.print(topic);
  Serial.print("] ");
  payload[length] = '\0';
  String newPayload = String((char *)payload);
  Serial.println(newPayload);
  Serial.println();
  if (newTopic == "doorbell/commands") 
  {
    if (newPayload == "Silent Doorbell")
    {
      digitalWrite(silencePin, LOW);
      client.publish("state/doorbell","Silent Doorbell", true);
    }
    if (newPayload == "Audio Doorbell")
    {
      digitalWrite(silencePin, HIGH);
      client.publish("state/doorbell","Audio Doorbell", true);
    }
  }
}

void getDoorBell()
{
  if(digitalRead(doorBellPin) == 1 && alreadyTriggered == false)
  {
    client.publish("Door_Status", "Doorbell");
    alreadyTriggered = true;
    timer.setTimeout(6000, resetTrigger); 
  }
}

void resetTrigger()
{
  alreadyTriggered = false;
}

void getFrontState()
{
  int newStatus = digitalRead(frontPin);
  if(newStatus != frontOldStatus && newStatus == 0)
  {
    client.publish("doors/Front","Closed", true);
    frontOldStatus = newStatus;
  }
  if(newStatus != frontOldStatus && newStatus == 1)
  {
    client.publish("doors/Front","Open", true);
    frontOldStatus = newStatus;
  }
}

void checkIn()
{
  client.publish("checkIn/doorbellMCU","OK");
}

//Run once setup
void setup() {
  Serial.begin(115200);
  while(!Serial) {} // Wait

  // GPIO Pin Setup
  pinMode(frontPin, INPUT_PULLUP);
  pinMode(doorBellPin, INPUT_PULLDOWN_16);
  pinMode(silencePin, OUTPUT);
  
  setup_wifi();
  
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  ArduinoOTA.setHostname("doorbellMCU");
  ArduinoOTA.begin(); 
  
  timer.setInterval(120000, checkIn);
  timer.setInterval(500, getFrontState);
  timer.setInterval(200, getDoorBell);

}

void loop() 
{
  if (!client.connected()) 
  {
    reconnect();
  }
  client.loop();
  ArduinoOTA.handle();
  timer.run();
}


