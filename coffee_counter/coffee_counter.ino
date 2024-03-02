#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiGeneric.h>
#include <WiFiMulti.h>
#include <WiFiSTA.h>
#include <WiFiScan.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoOTA.h>

# define INPUT_PIN_1 15
# define INPUT_PIN_2 16

//# define OUTPUT_PIN 14

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

int buttonState1;
int buttonState2;
int lastButtonState1 = LOW;
int lastButtonState2 = LOW;

unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 100;

const char* ssid = "..."; // TODO
const char* password = "..."; // TODO
const char* mqtt_server = "..."; // TODO

IPAddress local_IP(123, 456, 789, 111); // TODO
IPAddress gateway(123, 456, 789, 1); // TODO
IPAddress subnet(255, 255, 255, 0);

const char* person1 = "name"; // Name of person 1 for MQTT topic
const char* person2 = "name"; // Name of person 2 for MQTT topic
const char* fw_ver = "0.3.1";
int count_total = 0;
int count_person1 = 0;
int count_person2 = 0;
int count_both = 0;

uint64_t chipid = ESP.getEfuseMac();

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0; // For keep alive signal
char msg[50];
String displayText = ""; // for storing the last text on display

void setup_wifi() {
  Serial.begin(115200);
  Serial.println("Booting");


  // Start to connect to wifi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  if (WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("STA Failed to configure.");
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  display.println("WLAN connected");
  display.println(WiFi.localIP());
  display.display();

  ArduinoOTA.begin();
}

void setup_display() {
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(1000); 
  display.clearDisplay();
  display.setTextSize(1); // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
}

void printTextToDisplay(String text) {
  display.clearDisplay();
  display.setTextSize(4);
  display.setCursor(0, 0);
  display.println(text);
  display.display();
}

void printFeedbackToDisplay(String name) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("Coffee");
  display.println(name);
  display.display();
}

void printStatisticToDisplay(int total, int both, int c_person1, int c_person2) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("total: ");
  display.println(total);
  display.setTextSize(1);
  display.print(person2.toUpperCase() + " today: ");
  display.println(c_person2);
  display.print(person1.toUpperCase() + " today: ");
  display.println(c_person1);
  display.display();
}

void callback(char* topic, byte* message, unsigned int length) {
  //Serial.print("Message arrived on topic: ");
  //Serial.print(topic);
  
  String messageTemp;

  if (String(topic) == "iot/coffee/count") {
    for (int i = 0; i < length; i++) {
        messageTemp += (char)message[i];
    }
        
    if (messageTemp.toInt() != count_total) {
      Serial.println("Refresh display");
      count_total = messageTemp.toInt();
      printStatisticToDisplay(count_total, count_both, count_person1, count_person2);
    }
  }

  if (String(topic) == "iot/coffee/count/today") {
    for (int i = 0; i < length; i++) {
        messageTemp += (char)message[i];
    }
        
    if (messageTemp.toInt() != count_both) {
      Serial.println("Refresh display");
      count_both = messageTemp.toInt();
      printStatisticToDisplay(count_total, count_both, count_person1, count_person2);
    }
  }

  if (String(topic) == "iot/coffee/count/today/" + person1) {
    for (int i = 0; i < length; i++) {
        messageTemp += (char)message[i];
    }
        
    if (messageTemp.toInt() != count_person1) {
      Serial.println("Refresh display");
      count_person1 = messageTemp.toInt();
      printStatisticToDisplay(count_total, count_both, count_person1, count_person2);
    }
  }

  if (String(topic) == "iot/coffee/count/today/" + person2) {
    for (int i = 0; i < length; i++) {
        messageTemp += (char)message[i];
    }
        
    if (messageTemp.toInt() != count_person2) {
      Serial.println("Refresh display");
      count_person2 = messageTemp.toInt();
      printStatisticToDisplay(count_total, count_both, count_person1, count_person2);
    }
  }

  Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32")) {
      Serial.println("connected");
      display.println("MQTT connected");
      display.display();
      client.subscribe("iot/coffee/count");
      client.subscribe("iot/coffee/count/today/" + person1);
      client.subscribe("iot/coffee/count/today/" + person2);
    } else {
      Serial.print("failed rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds.");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println(chipid);

  setup_display();
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  pinMode(INPUT_PIN_1, INPUT_PULLUP); // Button
  pinMode(INPUT_PIN_2, INPUT_PULLUP); // Button
  //pinMode(OUTPUT_PIN, OUTPUT); // LED
}

void handleButton1() {
  int reading = digitalRead(INPUT_PIN_1);

  if (reading != lastButtonState1) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState1) {
      buttonState1 = reading;
      if (reading == LOW) {
          client.publish("iot/coffee/drink", person1);

          printFeedbackToDisplay(person1);
          delay(2000);

          printStatisticToDisplay(count_total, count_both, count_person1, count_person2);
        }
      }
    }

  lastButtonState1 = reading;
}

void handleButton2() {
  int reading = digitalRead(INPUT_PIN_2);

  if (reading != lastButtonState2) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState2) {
      buttonState2 = reading;
      if (reading == LOW) {
          client.publish("iot/coffee/drink", person2);
          
          printFeedbackToDisplay(person2);
          delay(2000);

          printStatisticToDisplay(count_total, count_both, count_person1, count_person2);
        }
      }
    }

  lastButtonState2 = reading;
}



void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  handleButton1();
  handleButton2();
  
  unsigned long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;
    client.publish("iot/coffee/alive", "alive");
    client.publish("iot/coffee/fw_ver", fw_ver);
  }

  ArduinoOTA.handle();
}
