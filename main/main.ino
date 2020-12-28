// v1.0.0 rewrote the entire code
#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <BlynkSimpleEsp8266.h>
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
//#include <SimpleTimer.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson


#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
//#define BLYNK_DEBUG

//*************************************************
//*                   mein kram                   *
//*************************************************
#define BAUD 115200                 //serial print speed
#define SSID "Modellhaussteuerung"  //netzwerkname der einrichtungsoberfläche
#define PASSWORD "789456123"        //passwort der einrichtungsoberfläche
BlynkTimer timer; // Create a Timer object called "timer"!
#include <Adafruit_NeoPixel.h>

#define LED_PIN D3     //GPIO pin on which the Neopixels are connected //original value was D3
#define NUM_LEDS 6 //amount of LEDs connected to the controller
#define AnalogInJitter 10 //defines how much the meausred analog val has to be bigger/smaller than the set val to switch leds on or off
int brightness = 255; //initial brightness value for the leds (0-255)
int val_V0;     //mode selector
int val_V2;     //on/off button
int val_V11;    //flicker propability
int val_V12;    //effect delay value
int val_V20;    //trigger value for auto mode
bool lightLevel;    //state of the ambient Light sensor
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);


int n = 0;
float addValue[NUM_LEDS * 3];
float setValue[NUM_LEDS * 3];
int cycles = 1;
long firstPixelHue = 0;


//*************************************************
//*                 ende mein kram                *
//*************************************************


//define your default values here, if there are different values in config.json, they are overwritten.
char blynk_token[34] = "YOUR_BLYNK_TOKEN";

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(BAUD);
  Serial.println();

  //clean FS, for testing
  //SPIFFS.format();
  
  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          //strcpy(mqtt_server, json["mqtt_server"]);
          //strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(blynk_token, json["blynk_token"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read


  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  //WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  //WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 33);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //add all your parameters here
  //wifiManager.addParameter(&custom_mqtt_server);
  //wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_blynk_token);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(SSID, PASSWORD)) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  //strcpy(mqtt_server, custom_mqtt_server.getValue());
  //strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(blynk_token, custom_blynk_token.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    //json["mqtt_server"] = mqtt_server;
    //json["mqtt_port"] = mqtt_port;
    json["blynk_token"] = blynk_token;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  //Serial.println("local ip");
  //Serial.println(WiFi.localIP());

  Blynk.config(blynk_token);
  bool result = Blynk.connect();

  if (result != true)
  {
    Serial.println("BLYNK Connection Fail");
    Serial.println(blynk_token);
    wifiManager.resetSettings();
    ESP.reset();
    delay (5000);
  }
  else
  {
    Serial.println("BLYNK Connected");
  }
  strip.begin(); // INITIALIZE NeoPixels
  timer.setInterval(1L, checkModes); //  Here you set interval and which function to call
  timer.setInterval(250L, printAnalogval); //  Here you set interval and which function to call
  randomSeed(analogRead(0));
  strip.clear();
}

void printAnalogval() {
  int val = analogRead(A0);
  Blynk.virtualWrite(V21, val);
  if (val <= (val_V20 - AnalogInJitter)) lightLevel = 0;
  else if ((val_V20 + AnalogInJitter ) <= val) lightLevel = 1;
}

void checkModes() {
  switch (val_V0) {
    case (1):
      SimulateFire();
      break;
    case (2):
      SimulateFire();
      break;
    case (3):
      return;
    case (4):
      rainbow(10);
      break;
    case (5):
      rainbowPlus(10);
      break;
  }
}
void SimulateFire ()
{
  //if auto-mode is active and its bright enough, turn off leds
  if (val_V0 == 1 & lightLevel == 1) {
    strip.clear();
    strip.show();
    return;
  }

  if (n == cycles) n = 0;
  if (n == 0) {
    cycles = random(20, 100);
    byte LightValue[NUM_LEDS * 3];
    for (int i = 0; i < NUM_LEDS; i++)
    { // For each pixel...
      LightValue[i * 3] = random(220, 255); // 250
      LightValue[i * 3 + 1] = random(40, 60); // 50
      LightValue[i * 3 + 2] = 0;
    }
    // Switch some lights darker
    byte LightsOff  = random(0, 100);
    if (LightsOff < val_V11) LightsOff = 1;
    else LightsOff = 0;
    for (int i = 0; i < LightsOff; i++)
    {
      byte Selected = random(NUM_LEDS);
      LightValue[Selected * 3] = random(20, 100);
      LightValue[Selected * 3 + 1] = random(20, 10);
      LightValue[Selected * 3 + 2] = 0;
    }
    for (int i = 0; i < NUM_LEDS; i++)
    { // For each pixel...
      addValue[i * 3] = (LightValue[i * 3] - setValue[i * 3]) / cycles;
      addValue[i * 3 + 1] = (LightValue[i * 3 + 1] - setValue[i * 3 + 1]) / cycles;
      addValue[i * 3 + 2] = (LightValue[i * 3 + 2] - setValue[i * 3 + 2]) / cycles;
    }
  }
  n++;
  for (int i = 0; i < NUM_LEDS; i++)
  { // For each pixel...
    setValue[i * 3] =  setValue[i * 3] + addValue[i * 3];
    setValue[i * 3 + 1] = setValue[i * 3 + 1] + addValue[i * 3 + 1];
    setValue[i * 3 + 2] = setValue[i * 3 + 2] + addValue[i * 3 + 2];
    strip.setPixelColor(i, strip.Color(int(setValue[i * 3]), int(setValue[i * 3 + 1]), int(setValue[i * 3 + 2])));
    //Serial.println(readPixelColor(i)[1]);
    strip.show();   // Send the updated pixel colors to the hardware.
  }
  //Serial.print(LightValue[0]); Serial.print(",");
  //Serial.print(addValue[0]); Serial.print(",");
  //Serial.println(setValue[0]);


}

void loop()
{
  Blynk.run();
  timer.run(); // BlynkTimer is working...
}

void rainbow(int wait) {
  // Hue of first pixel runs 5 complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to 5*65536. Adding 256 to firstPixelHue each time
  // means we'll make 5*65536/256 = 1280 passes through this outer loop:

  if (!(firstPixelHue < 5 * 65536)) firstPixelHue = 0;
  // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
  // optionally add saturation and value (brightness) (each 0 to 255).
  // Here we're using just the single-argument hue variant. The result
  // is passed through strip.gamma32() to provide 'truer' colors
  // before assigning to each pixel:
  for (int i = 0; i < strip.numPixels(); i++)
  {
    strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(firstPixelHue)));
  }

  strip.show(); // Update strip with new contents
  //firstPixelHue += 256;
  firstPixelHue += val_V12;
  delay(wait);  // Pause for a moment

}
void rainbowPlus(int wait) {
  // Hue of first pixel runs 5 complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to 5*65536. Adding 256 to firstPixelHue each time
  // means we'll make 5*65536/256 = 1280 passes through this outer loop:

  if (!(firstPixelHue < 5 * 65536)) firstPixelHue = 0;
  for (int i = 0; i < strip.numPixels(); i++) { // For each pixel in strip...
    // Offset pixel hue by an amount to make one full revolution of the
    // color wheel (range of 65536) along the length of the strip
    // (strip.numPixels() steps):
    int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
    // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
    // optionally add saturation and value (brightness) (each 0 to 255).
    // Here we're using just the single-argument hue variant. The result
    // is passed through strip.gamma32() to provide 'truer' colors
    // before assigning to each pixel:
    strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
  }
  strip.show(); // Update strip with new contents
  //firstPixelHue += 256;
  firstPixelHue += val_V12;
  delay(wait);  // Pause for a moment

}

BLYNK_WRITE(V0)
{
  val_V0 = param.asInt();
  if (val_V0 == 3) Blynk.syncVirtual(V10);
  else if (val_V0 == 2) loop();
}
BLYNK_WRITE(V1)
{
  int oldVal = brightness;
  brightness = param.asInt();
  if (!val_V2) brightness = 0;
  if (oldVal != brightness) {
    strip.setBrightness(brightness);
    strip.show();
    Blynk.syncVirtual(V10);
  }

}
BLYNK_WRITE(V2)
{
  val_V2 = param.asInt();
  Blynk.syncVirtual(V1);
}
BLYNK_WRITE(V10)
{
  if (val_V0 == 3) {
    int r = param[0].asInt();
    int g = param[1].asInt();
    int b = param[2].asInt();
    for (int i = 0; i < strip.numPixels(); i++)
    {
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
  }
}

BLYNK_WRITE(V11)
{
  val_V11 = param.asInt();
}
BLYNK_WRITE(V12)
{
  val_V12 = param.asInt();
}

BLYNK_WRITE(V20)
{
  val_V20 = param.asInt();
}

BLYNK_CONNECTED() {
  Blynk.syncAll();
}
