#include <FanController.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

// Sensor wire is plugged into port 2 on the Arduino.
// For a list of available pins on your board,
// please refer to: https://www.arduino.cc/en/Reference/AttachInterrupt
#define SENSOR_PIN_FAN 5 //D1

// Choose a threshold in milliseconds between readings.
// A smaller value will give more updated results,
// while a higher value will give more accurate and smooth readings
#define SENSOR_THRESHOLD 1000

// PWM pin (4th on 4 pin fans)
#define PWM_PIN_FAN 4 //D2

int fan_transistor = 0; //D3

const char* mqtt_sensor_name = "AutoStirrer";
const char* mqtt_sensor_name_subscriber = "AutoStirrer/Unit1/subscribe";
const char* mqtt_sensor_name_publisher = "AutoStirrer/Unit1/status";


// Initialize library
FanController fan(SENSOR_PIN_FAN, SENSOR_THRESHOLD, PWM_PIN_FAN);
WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi(){
  Serial.println();
  Serial.println();
  Serial.print("Connecting to (");
  Serial.print(ssid);   
  Serial.println(")");   

  /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.mode(WIFI_STA);
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

void setup_mqtt(){
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  connect_mqtt();
}

void connect_mqtt(){
  Serial.println("Reconnecting MQTT...");
  // Loop until we're reconnected
  while (!client.connected()) {
    if(client.connect(mqtt_sensor_name, mqtt_user, mqtt_password)){
      Serial.println();
      Serial.println();
      Serial.println("MQTT Connection Success");
      client.subscribe(mqtt_sensor_name_subscriber);
    }
    else{
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}  

void setup_fan(FanController &fan){
  fan.begin();
  set_fan_speed(0, fan);
  set_fan_transistor("OFF");
}


/*
   The setup function. We only start the library here
*/
void setup(void)
{
  // start serial port
  Serial.begin(9600);
  setup_wifi();
  // Start up the library
  setup_fan(fan);
  setup_mqtt();
}

void set_fan_speed(byte target, FanController &fan) {
  fan.setDutyCycle(target);
  delay(2000);
  Serial.print("Setting duty cycle: ");
  Serial.println(target, DEC);
  
}

void set_fan_transistor(char* status){
  if(status == "ON"){
    client.publish(mqtt_sensor_name_publisher, "ON" );
    digitalWrite(fan_transistor , HIGH);
  }
  else{
    client.publish(mqtt_sensor_name_publisher, "OFF" );
    digitalWrite(fan_transistor , LOW);
  }
}

void increment_fan_speed(int input, FanController &fan) {
  int dutyCycle = fan.getDutyCycle();
  
  // Constrain a 0-100 range
  // min 100, returns the minimum of the two numbers.
  // max 0, returns the maximum of the two numbers.
  int target = max(min(input, 100), 0);

  if (target == 0) {
    set_fan_speed(target, fan);
  }
  // example: target = 0, dutycycle = 100
  else if (target < dutyCycle) {
    while(target <= dutyCycle){
      if (dutyCycle % 5 == 0 && dutyCycle >= 0){
        set_fan_speed(dutyCycle, fan);
      }
      dutyCycle--;  
    }
  }
  // example: target = 100, dutycycle = 0
  else if (target > dutyCycle) {
    while(target >= dutyCycle){
      if (dutyCycle % 5 == 0 && dutyCycle <= 100){
        set_fan_speed(dutyCycle, fan);
      }  
      dutyCycle++;
    }
  }
  else {
    Serial.println("No Update to Duty Cycle.");
  }
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == mqtt_sensor_name_subscriber) {
    if(messageTemp == "ON"){
      set_fan_transistor("ON");
      increment_fan_speed(100, fan);
    }
    else{
      set_fan_transistor("OFF");
      increment_fan_speed(0, fan);
    }
  }
}

/*
   Main function, get and show the temperature
*/
void loop(void)
{
  if (!client.connected()) {
    connect_mqtt();
  }
  Serial.println("Looping...");
  client.loop();
  delay(5000);

} 
