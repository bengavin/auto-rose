#include <Arduino.h>
#include <SLIPEncodedSerial.h>
#include <WiFi101.h>
#include <aWOT.h>
#include <OSCBundle.h>

const char* SSID = "vo-guest";
const char* SSID_ACCESS = "alwayswithmercy";

int status = WL_IDLE_STATUS;

WiFiServer server(80);
Application app;

SLIPEncodedSerial rs485(Serial1);

uint8_t LEDstate = HIGH;

void construct_style_sheet(Response &res) {
    P(cssContent) = "html { font-family: sans-serif; display: inline-block; margin: 0px auto; text-align: center;}\n"
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
    res.printP(cssContent);
}

void construct_response_html(Response &res, uint8_t ledstate) {
  Serial.println(F("Constructing Header..."));
  P(headerContent) = "<!DOCTYPE html>\n"
    "<html>\n"
    "\t<head>\n"
    "\t\t<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n"
    "\t\t<title>Auto Rose Control</title>\n"
    "\t\t<link rel=\"stylesheet\" type=\"text/css\" href=\"/style.css\" />\n"
    "\t</head>\n"
    "\t<body>\n"
    "\t<h1>Auto-Rose Control Server</h1>\n";
  res.printP(headerContent);

  String rnd(rand());
  res.flush();
  Serial.println(F("Sending LED control..."));
  if(ledstate == LOW) {
    res.print(F("\t<p>LED Status: ON</p><a class=\"button button-off\" href=\"/ledoff?r="));
    res.print(rnd);
    res.print(F("\">TURN OFF</a>\n"));
  } else {
    res.print(F("\t<p>LED Status: OFF</p><a class=\"button button-on\" href=\"/ledon?r="));
    res.print(rnd);
    res.print(F("\">TURN ON</a>\n"));
  }
  res.flush();

  Serial.println(F("Sending Petal control..."));
  int petal0state = -1; //samplePetal.read();
  String petal0position(petal0state);
  String petal0_0_state = petal0state == 0 ? "button-off" : "button-on";
  String petal0_90_state = petal0state == 90 ? "button-off" : "button-on";
  String petal0_180_state = petal0state == 180 ? "button-off" : "button-on";
  res.print(F("\t<br /><h2>Petals</h2>\n"
         "\t<p>First Petal</p>\n"
         "\t<p>Current State: "));
  res.print(petal0position);
  res.print(F("&deg;</p>\n"
         "\t<p>\n"
         "\t<a class=\"button button-inline "));
  res.print(petal0_0_state);
  res.print(F("\" href=\"/petal?p=0&a=0&r="));
  res.print(rnd);
  res.print(F("\">Move to 0&deg;</a>\n"
         "\t<a class=\"button button-inline "));
  res.print(petal0_90_state);
  res.print(F("\" href=\"/petal?p=0&a=90&r="));
  res.print(rnd);
  res.print(F("\">Move to 90&deg;</a>\n"
         "\t<a class=\"button button-inline "));
  res.print(petal0_180_state);
  res.print(F("\" href=\"/petal?p=0&a=180&r="));
  res.print(rnd);
  res.print(F("\">Move to 180&deg;</a>\n"
         "\t</p>\n"));
  res.flush();

  Serial.println(F("Sending LED color control..."));
//  String rText(currentColor ? currentColor->R : -1);
//  String gText(currentColor ? currentColor->G : -1);
//  String bText(currentColor ? currentColor->B : -1);
//         "\t<p>Current State: (" + rText + "," + gText + "," + bText + ")</p>\n"
  res.print(F("\t<br /><h2>LEDs</h2>\n"
         "\t<p>\n"
         "\t<a class=\"button button-inline\" href=\"/ledcolor?r=255&g=0&b=0&r="));
  res.print(rnd);
  res.print(F("\">Red</a>\n"
         "\t<a class=\"button button-inline\" href=\"/ledcolor?r=0&g=255&b=0&r="));
  res.print(rnd);
  res.print(F("\">Green</a>\n"
         "\t<a class=\"button button-inline\" href=\"/ledcolor?r=0&g=0&b=255&r="));
  res.print(rnd);
  res.print(F("\">Blue</a>\n"
         "\t</p>\n"));

  res.print(F("\t</body>\n"
    "</html>\n"));
  res.flush();
}

void heartBeatPrint()
{
  static int num = 1;

  Serial.print(F("H"));

  if (num == 80)
  {
    Serial.println();
    num = 1;
  }
  else if (num++ % 10 == 0)
  {
    Serial.print(F(" "));
  }
}

void check_status()
{
  static unsigned long checkstatus_timeout = 0;

#define STATUS_CHECK_INTERVAL     60000L

  // Send status report every STATUS_REPORT_INTERVAL (60) seconds: we don't need to send updates frequently if there is no status change.
  if ((millis() > checkstatus_timeout) || (checkstatus_timeout == 0))
  {
    heartBeatPrint();
    checkstatus_timeout = millis() + STATUS_CHECK_INTERVAL;
  }
}

void server_on_connect(Request &req, Response &res) {
  Serial.println("Sending HTML Response...");
  res.status(200);
  res.set("Content-Type", "text/html");
  construct_response_html(res, LEDstate);
}

void server_on_not_found(Request &req, Response &res) {
  Serial.print(F("Bad Request: "));
  Serial.println(req.path());
  res.sendStatus(404);
}

void server_on_styles(Request &req, Response &res) {
  Serial.println(F("Sending CSS..."));
  res.status(200);
  res.set("Content-Type", "text/css");
  construct_style_sheet(res);
}

void server_on_ledon(Request &req, Response &res) {
  Serial.println(F("LED Status: ON"));
  LEDstate = LOW;
  res.set("Location", "/");
  res.sendStatus(302);

  rs485.beginPacket();
  OSCBundle bundle;
  bundle.add("/led").add(1);
  bundle.send(rs485);
  rs485.endPacket();
  rs485.flush();
  Serial.println(F("Sent LED on packet"));
}

void server_on_ledoff(Request &req, Response &res) {
  Serial.println(F("LED Status: OFF"));
  LEDstate = HIGH;
  res.set("Location", "/");
  res.sendStatus(302);

  rs485.beginPacket();
  OSCBundle bundle;
  bundle.add("/led").add(0);
  bundle.send(rs485);
  rs485.endPacket();
  rs485.flush();
  Serial.println(F("Sent LED off packet"));
}

void server_on_ledcolor(Request &req, Response &res) {
  Serial.println(F("LED Color:"));
  char rVal[4];
  char gVal[4];
  char bVal[4];
  char wVal[4];
  bool hasrVal = req.query("r", rVal, 4);
  bool hasgVal = req.query("g", gVal, 4);
  bool hasbVal = req.query("b", bVal, 4);
  bool haswVal = req.query("w", wVal, 4);

  uint8_t rledColor = hasrVal ? min(255, max(atoi(&rVal[0]), 0)) : 0;
  uint8_t gledColor = hasgVal ? min(255, max(atoi(&gVal[0]), 0)) : 0;
  uint8_t bledColor = hasbVal ? min(255, max(atoi(&bVal[0]), 0)) : 0;
  uint8_t wledColor = haswVal ? min(255, max(atoi(&wVal[0]), 0)) : 0;

  res.set("Location", "/");
  res.sendStatus(302);

  Serial.printf("Got strip color: %d, %d, %d, %d\n", rledColor, gledColor, bledColor, wledColor);

  rs485.beginPacket();
  OSCBundle bundle;
  bundle.add("/strip")
        .add(0) // starting pixel
        .add(10) // how many
        .add(rledColor).add(gledColor).add(bledColor).add(wledColor);
  bundle.send(rs485);
  rs485.endPacket();
  rs485.flush();
}

void server_on_move_petal(Request &req, Response &res) {
  char petalNumberVal[10];
  char petalPositionVal[10];
  bool hasPetalNumber = req.query("p", petalNumberVal, 10);
  bool hasPetalPosition = req.query("a", petalPositionVal, 10);

  int petalNumber = hasPetalNumber ? atoi(&petalNumberVal[0]) : 1;
  int petalPosition = hasPetalPosition ? min(180, max(atoi(&petalPositionVal[0]), 0)) : 0;
  
  res.set("Location", "/");
  res.sendStatus(302);

  //samplePetal.write(petalPosition);
  Serial.print("Moving petal ");
  Serial.print(petalNumber);
  Serial.print(" to position ");
  Serial.println(petalPosition);

  rs485.beginPacket();
  OSCBundle bundle;
  bundle.add("/petal").add(petalNumber).add(petalPosition);
  bundle.send(rs485);
  rs485.endPacket();
  rs485.flush();

  Serial.println("Petal Message Sent");
}

void setup() {
  //Configure pins for Adafruit ATWINC1500 Feather
  WiFi.setPins(8,7,4,2);

  Serial.begin(19200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  
  rs485.begin(19200);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // Configure WIFI
  Serial.print(F("Connecting to: "));
  Serial.println(SSID);

  WiFi.hostname("auto-rose-control");
    // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Serial.print(F("Attempting to connect to SSID: "));
    Serial.println(SSID);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(SSID, SSID_ACCESS);

    // wait 10 seconds for connection:
    delay(10000);
  }

  Serial.println();
  Serial.println(F("Connected!"));
  Serial.print(F("IP address: "));
  IPAddress localIP = WiFi.localIP();
  Serial.println(localIP);

  // Setup Web Server Access
  Serial.println(F("Configuring HTTP Server..."));
  app.get("/style.css", &server_on_styles);
  app.get("/ledon", &server_on_ledon);
  app.get("/ledoff", &server_on_ledoff);
  app.get("/ledcolor", &server_on_ledcolor);
  app.get("/petal", &server_on_move_petal);
  app.get("/", &server_on_connect);
  app.notFound(&server_on_not_found);
  Serial.println(F("HTTP Server Configured."));
  server.begin();
  Serial.println(F("WiFi Server Started"));
}

void loop() {
  WiFiClient client = server.available();
  
  if (client.connected()) {
    Serial.println(F("Processing HTTP Request..."));
    app.process(&client);
    client.stop();
  }

  // put your main code here, to run repeatedly:
  digitalWrite(LED_BUILTIN, LEDstate);

  check_status();
}
