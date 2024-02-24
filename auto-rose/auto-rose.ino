//#include <Arduino.h>

#undef ENABLE_WIFI
#define ENABLE_OSC
#undef ENABLE_HTTP
#define ENABLE_SERVO

#define LED_NEOPIXEL
#define LED_NEOPXL8

#ifdef ENABLE_WIFI
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#ifdef ENABLE_OSC
#include <WiFiUdp.h>
#endif
#endif

#ifdef ENABLE_SERVO
#include <Servo.h>
#endif

#ifdef ENABLE_OSC
#include <OSCMessage.h>
#include <OSCBundle.h>
#endif

#ifdef LED_NEOPXL8
#include <Adafruit_NeoPXL8.h>
#else
#include <NeoPixelBusLg.h>
#endif

// put function declarations here:
#ifdef ENABLE_OSC
void osc_on_petal(OSCMessage &, int);
void osc_on_led(OSCMessage &, int);
void osc_on_led_strip(OSCMessage &, int);
#ifndef ENABLE_WIFI
#include <SLIPEncodedSerial.h>
SLIPEncodedSerial rs485(Serial1);
#endif
#endif

#ifdef ENABLE_HTTP
void server_on_connect();
void server_on_ledoff();
void server_on_ledon();
void server_on_ledcolor();
void server_on_styles();
void server_on_move_petal();
void server_on_not_found();
String construct_style_sheet();
String construct_response_html(uint8_t);
#endif

#ifdef ENABLE_SERVO
#define NUMBER_OF_SERVOS 2
int petal_pins[NUMBER_OF_SERVOS] = { PIN_NEOPIXEL7, PIN_NEOPIXEL6 };
Servo petals[NUMBER_OF_SERVOS];
#endif

#ifdef ENABLE_WIFI
const char* SSID = "vo-guest";
const char* SSID_ACCESS = "alwayswithmercy";

#ifdef ENABLE_HTTP
ESP8266WebServer server(80);
#endif

#if defined(ENABLE_OSC)
WiFiUDP UdpOSC;
const unsigned int localPort = 8801;        // local port to listen for UDP packets (here's where we send the packets)
#endif

#endif

#define NUM_PIXELS 24

#ifdef LED_NEOPIXEL
Adafruit_NeoPixel boardPixel(1, PIN_NEOPIXEL);
#endif

#ifdef LED_NEOPXL8
// Pixel strips [unused]
// 0 -> 0-23
// 1 -> 24-47
// 2 -> 48-71
// 3 -> 72-95
// 4 -> 96-119
// 5 -> 120-143
// 6 -> 144-167
#define PIXEL_OFFSET 0
#define PIXEL_STRIPS 1
int8_t PIXEL_PINS[8] = { PIN_NEOPIXEL0, PIN_NEOPIXEL1, -1, -1, -1, -1, -1, -1 };
Adafruit_NeoPXL8HDR pixels(NUM_PIXELS, PIXEL_PINS, NEO_GRBW);
//Adafruit_NeoPixel pixels(NUM_PIXELS, PIN_NEOPIXEL0, NEO_GRBW);
typedef struct RgbwColor {
  uint8_t R;
  uint8_t G;
  uint8_t B;
  uint8_t W;
} RgbwColor_t;
RgbwColor_t *currentColor = NULL;
#else
#define PIXEL_OFFSET 0
NeoPixelBusLg<NeoGrbwFeature, NeoSk6812Method> pixels(NUM_PIXELS, 23);
RgbColor red(255, 0, 0);
RgbColor green(0, 255, 0);
RgbColor blue(0, 0, 255);
RgbColor black(0, 0, 0);
RgbColor white(255, 255, 255);
RgbColor *currentColor = NULL;
#endif
uint16_t currentPixel = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

#if defined(ENABLE_OSC) && !defined(ENABLE_WIFI)
  Serial.println(F("Configuring RS485 input"));
  //Serial2.setPinout(24, 25);
  //Serial1.begin(19200);
  rs485.begin(19200);
#endif

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

#ifdef LED_NEOPIXEL
  boardPixel.begin();
  boardPixel.setPixelColor(1, 225, 180, 40);
  boardPixel.show();
#endif

#ifdef ENABLE_WIFI
  // Configure WIFI
  Serial.print("Connecting to: ");
  Serial.println(SSID);

  WiFi.hostname("auto-rose-control");
  WiFi.begin(SSID, SSID_ACCESS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
#else
  Serial.println("WiFi Disabled");
#endif

#ifdef ENABLE_HTTP
  // Setup Web Server Access
  server.on("/", server_on_connect);
  server.on("/style.css", server_on_styles);
  server.on("/ledon", server_on_ledon);
  server.on("/ledoff", server_on_ledoff);
  server.on("/ledcolor", server_on_ledcolor);
  server.on("/petal", server_on_move_petal);
  server.onNotFound(server_on_not_found);
  server.begin(80);
#else
  Serial.println("HTTP Server Disabled");
#endif

#ifdef ENABLE_SERVO
  for (int idx = 0; idx < NUMBER_OF_SERVOS; idx++) {
    int channel = petals[idx].attach(petal_pins[idx], 500, 2500);
    if (channel == 0) {
      Serial.printf("Failed to assign servo channel for servo [%d]\n", idx);
    }
    else {
      Serial.print(F("Channel: "));
      Serial.println(channel);
      Serial.print(F("Servo "));
      if (petals[idx].attached()) { Serial.println(F("attached")); } else { Serial.println(F("NOT attached")); }
      int petalCurrentPos = petals[idx].read();
      Serial.print(F("Current Servo Position: "));
      Serial.println(petalCurrentPos);
      Serial.println(F("Moving to position 0"));
      petals[idx].write(0);
      delay(500);
      petalCurrentPos = petals[idx].read();
      Serial.print(F("Current Servo Position: "));
      Serial.println(petalCurrentPos);
      delay(100);
      Serial.println(F("Moving to position 180"));
      petals[idx].write(180);
      delay(1000);
      petalCurrentPos = petals[idx].read();
      Serial.print(F("Current Servo Position: "));
      Serial.println(petalCurrentPos);
      delay(100);
      petals[idx].write(90);
      delay(1000);
      petals[idx].detach();
    }
  }
#endif

#ifdef ENABLE_OSC
  #ifdef ENABLE_WIFI
    // Configure OSC endpoint
    UdpOSC.begin(localPort);
    Serial.print("Listening for OSC messages on port: ");
    Serial.println(localPort);
  #else
    Serial.println("OSC Enabled (Serial)");
  #endif
#else
  Serial.println("OSC Disabled");
#endif

  // Configure LEDs
  delay(1000);
  Serial.println("Configuring LED strip(s)");
  #ifdef LED_NEOPXL8
  pixels.begin(true, 4, true, pio1);
  pixels.setBrightness(240, 2.6);
  pixels.show();

  //pixels.fill(pixels.Color(255, 0, 0));
  for (uint16_t i = PIXEL_OFFSET; i < PIXEL_OFFSET + (PIXEL_STRIPS * NUM_PIXELS); i++) {
    pixels.setPixelColor(i, 255, 0, 0);
  }
  pixels.show();
  delay(1000);
  for (uint16_t i = PIXEL_OFFSET; i < PIXEL_OFFSET + (PIXEL_STRIPS * NUM_PIXELS); i++) {
    pixels.setPixelColor(i, 0, 255, 0);
  }
  pixels.show();
  delay(1000);
  for (uint16_t i = PIXEL_OFFSET; i < PIXEL_OFFSET + (PIXEL_STRIPS * NUM_PIXELS); i++) {
    pixels.setPixelColor(i, 0, 0, 255);
  }
  pixels.show();
  delay(1000);
  for (uint16_t i = PIXEL_OFFSET; i < PIXEL_OFFSET + (PIXEL_STRIPS * NUM_PIXELS); i++) {
    pixels.setPixelColor(i, 0, 0, 0, 255);
  }
  pixels.show();
  delay(1000);
  #else
  pixels.Begin();
  pixels.SetLuminance(128);
  pixels.Show();
  #endif
}

uint8_t LEDstate = HIGH;

#ifdef LED_NEOPXL8
uint8_t linear_blend(const uint8_t left, const uint8_t right, float progress)
{
    return left + ((static_cast<int16_t>(right) - left) * progress);
}
/*uint8_t linear_blend(const uint8_t left, const uint8_t right, uint8_t progress)
{
    return left + (((static_cast<int32_t>(right) - left) * static_cast<int32_t>(progress) + 1) >> 8);
}*/
void DrawGradient(const uint8_t* start, const uint8_t* finish, uint16_t startIndex, uint16_t finishIndex)
{
    uint16_t delta = finishIndex - startIndex;
    
    for (uint16_t index = startIndex; index < finishIndex; index++)
    {
        float progress = static_cast<float>(index - startIndex) / delta;
        uint8_t r = linear_blend(start[0], finish[0], progress);
        uint8_t g = linear_blend(start[1], finish[1], progress);
        uint8_t b = linear_blend(start[2], finish[2], progress);
        pixels.setPixelColor(index, r, g, b);
    }
}
#else
void DrawGradient(RgbColor startColor, 
        RgbColor finishColor, 
        uint16_t startIndex, 
        uint16_t finishIndex)
{
    uint16_t delta = finishIndex - startIndex;
    
    for (uint16_t index = startIndex; index < finishIndex; index++)
    {
        float progress = static_cast<float>(index - startIndex) / delta;
        RgbColor color = RgbColor::LinearBlend(startColor, finishColor, progress);
        pixels.SetPixelColor(index, color);
    }
}
#endif

#ifdef ENABLE_OSC
OSCErrorCode error;
#endif

// On RP2040, the refresh() function is called in a tight loop on the
// second core (via the loop1() function). The first core is then 100%
// free for animation logic in the regular loop() function.
void loop1() {
  pixels.refresh();
}

void loop() {
#ifdef ENABLE_HTTP
  server.handleClient();
#endif

#ifdef ENABLE_OSC
  OSCBundle bundle;

  #ifdef ENABLE_WIFI
  int size = UdpOSC.parsePacket();
  #else
  int size = 1;
  #endif

  if (size > 0) {
    #if ENABLE_WIFI
    while (size--) {
      bundle.fill(UdpOSC.read());
    }
    #else
    /*bool gotEOT = false;
    while (Serial1.available()) {
      int c = Serial1.read();
      switch(c)
      {
        case 192: // EOT
          if (gotEOT) {
            // this is the end, get route the bundle
            break;
          }
          gotEOT = true;
          break;

        default:
          if (gotEOT) {
            // we're in packet, fill the bundle
            bundle.fill(c);
          }
          break;
      }
    }*/    
    while(!rs485.endofPacket()) {
      if((size = rs485.available()) > 0) {
        while(size--) {
            bundle.fill(rs485.read());
        }
      }
    }
    #endif

    if (!bundle.hasError()) {
      Serial.println(F("Routing OSC Bundle"));
      bundle.route("/strip", osc_on_led_strip);
      bundle.route("/petal", osc_on_petal);
      bundle.route("/led", osc_on_led);
    } else {
      error = bundle.getError();
      Serial.print(F("OSC receive error: "));
      Serial.println(error);
    }
  }
#else

#endif

  // put your main code here, to run repeatedly:
  digitalWrite(LED_BUILTIN, LEDstate);
  #ifdef LED_NEOPIXEL
  if (LEDstate) {
    boardPixel.setPixelColor(1,0,0,0);
    boardPixel.show();
  }
  else {
    boardPixel.setPixelColor(1, 220, 180, 40);
    boardPixel.show();
  }
  #endif

  if (currentColor)
  {
    if (currentPixel >= (PIXEL_OFFSET + (NUM_PIXELS * PIXEL_STRIPS))) { currentPixel = PIXEL_OFFSET; }
    #ifdef LED_NEOPXL8
    pixels.setPixelColor(currentPixel++, currentColor->R, currentColor->G, currentColor->B, currentColor->W);
    #else
    pixels.SetPixelColor(currentPixel++, *currentColor);
    #endif
  }
  else
  {
    #ifdef LED_NEOPXL8
    uint16_t half = NUM_PIXELS / 2;
    uint8_t startColor[4] = {0, 255, 0, 0};
    uint8_t endColor[4] = {0, 0, 0, 0};
    DrawGradient(&startColor[0], &endColor[0], PIXEL_OFFSET, NUM_PIXELS*PIXEL_STRIPS);
    #else
    uint16_t half = pixels.PixelCount() / 2;
    DrawGradient(green, black, 0, half - 1);
    #endif
  }

  #ifdef LED_NEOPXL8
  pixels.show();
  #else
  pixels.Show();
  #endif
}

#ifdef ENABLE_OSC
// put function definitions here:
void osc_on_petal(OSCMessage &msg, int addrOffset) {
  int petalNumber = msg.isInt(0) ? msg.getInt(0) : 0;
  int petalPosition = msg.isInt(1) ? min(180, max(msg.getInt(1), 0)) : 0;

  petalNumber = min((NUMBER_OF_SERVOS - 1), max(0, petalNumber));
  petalPosition = min(180, max(0, petalPosition));

  Serial.print(F("Moving petal "));
  Serial.print(petalNumber);
  Serial.print(F(" to position "));
  Serial.println(petalPosition);

  if (petals[petalNumber].attach(petal_pins[petalNumber], 500, 2500))
  {
    int currentPosition = petals[petalNumber].read();
    petals[petalNumber].write(petalPosition);
    //while (petals[petalNumber].read() != petalPosition) {
    //  delay(100);
    //}
    delay(1500);
    // Wait a 10ms for each degree moved + 100ms baseline
    // delay(10 * abs(currentPosition - petalPosition) + 100);
    petals[petalNumber].detach();
  }
}

void osc_on_led(OSCMessage &msg, int addrOffset) {
  int ledstate = msg.isInt(0) ? msg.getInt(0) : -1;
  if (ledstate < 0) {
    Serial.println(F("Invalid LED State Received"));
    return;
  }

  LEDstate = ledstate > 0 ? LOW : HIGH;
}

void osc_on_led_strip(OSCMessage &msg, int addrOffset) {
  int pixelOffset = msg.isInt(0) ? msg.getInt(0) : 0;
  int pixelCount = msg.isInt(1) ? msg.getInt(1) : 1;
  uint8_t red = msg.isInt(2) ? (uint8_t)msg.getInt(2) : 0;
  uint8_t green = msg.isInt(3) ? (uint8_t)msg.getInt(3) : 0;
  uint8_t blue = msg.isInt(4) ? (uint8_t)msg.getInt(4) : 0;
  uint8_t white = msg.isInt(5) ? (uint8_t)msg.getInt(5) : 0;

  if (!red && !green && !blue && !white) {
    if (currentColor != NULL) {
      delete currentColor;
      currentColor = NULL;
    }
  }
  else if (currentColor == NULL) {
    currentColor = new RgbwColor_t { red, green, blue, white };
  } else {
    currentColor->B = blue;
    currentColor->G = green;
    currentColor->R = red;
    currentColor->W = white;
  }
}
#endif

#ifdef ENABLE_HTTP
void server_on_connect() {
  Serial.println("Sending HTML Response...");
  server.send(200, "text/html", construct_response_html(LEDstate));
}

void server_on_not_found() {
  Serial.print("Bad Request: ");
  Serial.println(server.uri());
  server.send(404, "text/plain", "Not Found");
}

void server_on_styles() {
  Serial.println("Sending CSS...");
  server.send(200, "text/css", construct_style_sheet());
}

void server_on_ledon() {
  Serial.println("LED Status: ON");
  LEDstate = LOW;
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "Moved");
}

void server_on_ledoff() {
  Serial.println("LED Status: OFF");
  LEDstate = HIGH;
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "Moved");
}

void server_on_ledcolor() {
  Serial.println("LED Color:");
  uint8_t rledColor = min(255, max((int)server.arg("r").toInt(), 0));
  uint8_t gledColor = min(255, max((int)server.arg("g").toInt(), 0));
  uint8_t bledColor = min(255, max((int)server.arg("b").toInt(), 0));

  RgbColor *tmp = currentColor;
  currentColor = new RgbColor(rledColor, gledColor, bledColor);
  if (tmp) { delete tmp; }

  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "Moved");
}

void server_on_move_petal() {
  int petalNumber = server.arg("p").toInt();
  int petalPosition = min(180, max((int)server.arg("a").toInt(), 0));
  Serial.print("Moving petal ");
  Serial.print(petalNumber);
  Serial.print(" to position ");
  Serial.print(petalPosition);

  samplePetal.write(petalPosition);
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "Moved");
}

String construct_style_sheet() {
  String ptr = 
    "html { font-family: sans-serif; display: inline-block; margin: 0px auto; text-align: center;}\n"
    "body{margin-top: 50px;}\n"
    "h1 {color: #444444;margin: 50px auto 30px;}\n"
    "h3 {color: #444444;margin-bottom: 50px;}\n"
    ".button {display: block;width: 80px;background-color: #1abc9c;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n"
    ".button-inline { display: inline-block; }"
    ".button-on {background-color: #1abc9c;}\n"
    ".button-on:active {background-color: #16a085;}\n"
    ".button-off {background-color: #34495e;}\n"
    ".button-off:active {background-color: #2c3e50;}\n"
    "p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  return ptr;
}

String construct_response_html(uint8_t ledstate) {
  String ptr = 
  "<!DOCTYPE html>\n"
  "<html>\n"
    "\t<head>\n"
    "\t\t<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n"
    "\t\t<title>Auto Rose Control</title>\n"
    "\t\t<link rel=\"stylesheet\" type=\"text/css\" href=\"/style.css\" />\n"
    "\t</head>\n"
    "\t<body>\n"
    "\t<h1>Auto-Rose Control Server</h1>\n";
  
  String rnd(rand());

  if(ledstate == LOW) {
    ptr +="\t<p>LED Status: ON</p><a class=\"button button-off\" href=\"/ledoff?r=" + rnd + "\">TURN OFF</a>\n";
  } else {
    ptr +="\t<p>LED Status: OFF</p><a class=\"button button-on\" href=\"/ledon?r=" + rnd + "\">TURN ON</a>\n";
  }

  int petal0state = samplePetal.read();
  String petal0position(petal0state);
  String petal0_0_state = petal0state == 0 ? "button-off" : "button-on";
  String petal0_90_state = petal0state == 90 ? "button-off" : "button-on";
  String petal0_180_state = petal0state == 180 ? "button-off" : "button-on";
  ptr += "\t<br /><h2>Petals</h2>\n"
         "\t<p>First Petal</p>\n"
         "\t<p>Current State: " + petal0position + "&deg;</p>\n"
         "\t<p>\n"
         "\t<a class=\"button button-inline " + petal0_0_state + "\" href=\"/petal?p=0&a=0&r=" + rnd + "\">Move to 0&deg;</a>\n"
         "\t<a class=\"button button-inline " + petal0_90_state + "\" href=\"/petal?p=0&a=90&r=" + rnd + "\">Move to 90&deg;</a>\n"
         "\t<a class=\"button button-inline " + petal0_180_state + "\" href=\"/petal?p=0&a=180&r=" + rnd + "\">Move to 180&deg;</a>\n"
         "\t</p>\n";

  String rText(currentColor ? currentColor->R : -1);
  String gText(currentColor ? currentColor->G : -1);
  String bText(currentColor ? currentColor->B : -1);
  ptr += "\t<br /><h2>LEDs</h2>\n"
         "\t<p>Current State: (" + rText + "," + gText + "," + bText + ")</p>\n"
         "\t<p>\n"
         "\t<a class=\"button button-inline\" href=\"/ledcolor?r=255&g=0&b=0&r=" + rnd + "\">Red</a>\n"
         "\t<a class=\"button button-inline\" href=\"/ledcolor?r=0&g=255&b=0&r=" + rnd + "\">Green</a>\n"
         "\t<a class=\"button button-inline\" href=\"/ledcolor?r=0&g=0&b=255&r=" + rnd + "\">Blue</a>\n"
         "\t</p>\n";

  ptr +=
    "\t</body>\n"
    "</html>\n";

  return ptr;
}
#endif
