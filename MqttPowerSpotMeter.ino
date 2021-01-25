// 
// Power usage display with current spot price using OLED and Neopixel. 
// Uses a NodeMCU board with wifi to recieve a single MQTT message on
// the form powerUsage;spotprice using ';' as a delimiter/IFS then 
// splits the message into two variables to visualise them.
// 

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>

// Following 4 are needed for SSD1306 oled display
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define PIXELPIN D3   // Datapin for neopixel
#define NUMPIXELS 24  // Number of neopixels 
#define DELAYVAL 500 // Time (in milliseconds) to pause between pixels

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)

//OLED display (Uses GPIO5 (D1) (SCL) and GPIO4 (D2) (SDA) as standard
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Neopixels / ws2812 compatible leds
Adafruit_NeoPixel pixels(NUMPIXELS, PIXELPIN, NEO_GRB + NEO_KHZ800);

// Update these with values suitable for your network.
const char* ssid = "Your-Wifi-Network";
const char* password = "wifi-passphrase";
const char* mqtt_server = "10.0.0.5"; // Your MQTT broker

// Set your variable for "nettleie" to your power company. 
const float nettleie = 42.61;               // BKK nettleie variabel del, Price in øre.
const char* powerPriceTopic = "strompris";  // Your MQTT topic where you publish "powerUsage;spotpris"
const char* delimiter = ";";

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];


void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  char incomming[length];
  
  for (int i=0; i<length; i++){
    incomming[i] = (char)payload[i];  
  }

  // Split incomming message into Power and spot price with delimiter
  // First token (Power)
  char* incToken = strtok(incomming,";");
  char* incommingPower = incToken;

  // Second token (Spot price)
  incToken = strtok(NULL,delimiter);
  char* incommingSpot = incToken;

  // Convert into easier to manage data types.
  long power = atol(incommingPower);
  float kronepris = atof(incommingSpot);
  
  int pris = round((kronepris*100)); // Go from Krone to øre.

  // Calculate price per hour based on current power usage
  float timepris = (((kronepris*100) + nettleie)*power)/100000;

  // Number of LEDS which will light up, round to nearest kW.
  int powerInt = (int)(round((float)(power/1000.0)));

  // Light up Neopixel, colours suits my normal power usage.
  // Green is OK ( power < 5kW )
  // Yellow is hmm ( 5kW < power < 10kW )
  // Red gets expensive fast ( Power > 10kW )
  pixels.clear();
  for(int i=0;i<powerInt;i++) {
    if (i<=4) {
      pixels.setPixelColor(i, pixels.Color(0,50,0));
    } else if ((i >= 5) && (i <= 9)) {
      pixels.setPixelColor(i, pixels.Color(50,50,0));
    } else if (i >= 10) {
      pixels.setPixelColor(i, pixels.Color(50,0,0));  
    }
  }
  pixels.show(); // Light up pixels

  // OLED display, 3 lines with medium/large text. 
  //  --------
  // | Power  |
  // | Spot   |
  // | perHour|
  //  --------
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,10);

  display.print(power);
  display.println(" W");
  
  display.print(pris);
  display.println(" 0re");  // Fake a norwegian Ø, haven't gotten Adafruit_gfx to display non ascii chars.

  display.print(timepris);
  display.println(" kr/t");
  
  display.display();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // ... and resubscribe
      client.subscribe(powerPriceTopic); // Topic where you publish your meter values
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  Serial.begin(115200);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x64 (Default address) can also be 0x3D
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  display.display();
  delay(300); // Wait and show adafruit splashscreen, they provided a nice library
  // Clear the buffer
  display.clearDisplay();
  
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {

   if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
