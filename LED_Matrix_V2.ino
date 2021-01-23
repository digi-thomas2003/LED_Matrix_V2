/*******************************************************************************************

LED-Matrix-Laufband Basisversion

Programmbeispiel zum Ansteuern von einem Dot Matrix Modul mit MAX7219 Treiber.
ESP 8266 Board: WeMos D1

Basis: Idee von Alf Müller
https://www.youtube.com/watch?time_continue=2&v=BMY3TZdeMGw
https://www.youtube.com/watch?v=YUtqLjs-alo
Anpassungen und Erweiterungen von mir

Anschlüsse:
DOT Matrix:       ESP8266 NodeMCU:
CLK       ->      D5 (SCK)
CS        ->      D6
DIN       ->      D7 (MOSI)
VCC       ->      5V+
GND       ->      GND-


Verwendete Libraries:
TimeLib:            http://www.arduino.cc/playground/Code/Time


V1.0  27.04.2019: Erste Version: Lauftext und Uhr
V2.0  28.04.2019: Verzicht auf Max7219 Library
V2.1  30.04.2019: Anpassungen am Zeichensatz, komplett neue Webpage
V2.2  02.05.2019: OTA und mDNS ergänzt
V2.3  12.05.2019: Smileys added to fonts
V2.4  28.05.2019: Change to WeMos D1
V2.5  29.05.2019: Added two buttons on webpage: display reset and ESP8266 reset
V2.6  18.01.2021: add random heart animation, passwords moved to Credentials
V2.7  23.01.2021: add button to webpage: show animation yes/no


*******************************************************************************************/

#define FIRMWARE "2.7"

/************************( include of the necessary libraries )************************/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>   

#include <TimeLib.h>
#include <Credentials.h>

#include "mySNTP.h"               // get the NTP from the web

/************************************( Wifi preferences )*********************************/
//const char* ssid = "xxx";
//const char* password = "xxx";                             // stored in Credentials
IPAddress ip(192, 168, 178, 220);                           // Static IP
IPAddress gateway(192, 168, 178, 1);
IPAddress dns(192, 168, 178, 1);
IPAddress subnet(255, 255, 255, 0);

#define HOSTNAME "LED-Matrix"

/******************************( initialising of the libraries )*****************************/
ESP8266WebServer server(80);      // start the webserver on port 80
								  // the server can be reached with http://192.168.178.220
								  // or http://led-matrix.local

/**************************( variables and defines for MAX7219 )*****************************************/
String spaces = "           ";
String LaufschriftDefault = spaces + "TomSoft grüßt Euch!" + spaces;  // default scrolling text
String LaufschriftText;                                               // for ascii-coded text
String LaufschriftWeb;                                                // for web input

byte wait = 50;                  // time (ms) for scrolling --> scrolling speed
byte helligkeit = 2;             // intensity, 0..15
int spacer = 1;                  // space between the chars
int width = 5 + spacer;          // width of a single char + spacer
bool showAnim = true;            // true = show animations and scrolling text, false = show only time and nothing else

#define ROTATE 90   // orientation of the matrices 0 / 90 / 270
#define NUM_MAX 4   // number of 8x8 matrices

#define DIN_PIN D7  // D7
#define CS_PIN  D6  // D3
#define CLK_PIN D5  // D5

#include "max7219.h"
#include "fonts.h"

#define MAX_DIGITS 16
byte dig[MAX_DIGITS] = { 0 };
byte digold[MAX_DIGITS] = { 0 };
byte digtrans[MAX_DIGITS] = { 0 };
int dx = 0;
int dy = 0;
byte del = 0;

/**************************( global variables for time and date )*****************************************/
boolean dots = 0;             // 1 = dots visible, 0 = dots invisible
unsigned long dotTime = 0;    // used for blinking dots
int h, m, s, w, mo, ye, d;
time_t t, restart;
const char* Wochentag[] = { "Fehler", "Sonntag", "Montag", "Dienstag","Mittwoch", "Donnerstag", "Freitag","Samstag" };
const char* Monate[] = { "Fehler", "Januar", "Februar", "M\xE4rz","April", "Mai", "Juni", "Juli", "August", "September", "Oktober", "November","Dezember" };
String Datum;
static byte c1;              // Last character buffer, used for utf8 decoding

/*****************************************( Setup )****************************************/
void setup() {
	Serial.begin(57600);
	Serial.println(F("\n\nTomSoft"));

	initMAX7219();
	sendCmdAll(CMD_SHUTDOWN, 1);
	sendCmdAll(CMD_INTENSITY, 0);

	// Connect to WiFi
	String hostname(HOSTNAME);
	WiFi.persistent(false);
	WiFi.disconnect();
	WiFi.hostname(hostname);
	WiFi.mode(WIFI_STA);
	WiFi.config(ip, gateway, subnet, dns);
	WiFi.begin(ssid, password);

	printStringWithShift("Starte", 15);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	delay(1000);
	// serial output of connection details
	Serial.print(F("\nConnected to "));
	Serial.println(ssid);
	Serial.print(F("IP address: "));
	Serial.println(WiFi.localIP());

	// Initialize NTP Client and get the time
	initNTP();
	delay(500);
	getLocalTime();
	delay(500);
	// Set the time provider to NTP
	setSyncProvider(getLocalTime);
	setSyncInterval(300);

	// start webserver
	setupWebServer();

	// start mDNS --> http://led-matrix.local
	MDNS.begin(HOSTNAME);
	MDNS.addService("http", "tcp", 80);

	// start OTA
	ArduinoOTA.setHostname((const char*)hostname.c_str());

	ArduinoOTA.onStart([]() {
		//Serial.println("Start updating "); 
		clr();
		printStringWithShift("Updating...", 20);
		delay(200);
		clr();
	});
	ArduinoOTA.onEnd([]() {
		//Serial.println("\nEnd");
		clr();
		printStringWithShift("Done!", 20);
		delay(200);
	});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		//Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
		byte led = map(progress, 0, total, 1, NUM_MAX * 8);
		setCol(led, B00000011);
		refreshAll();
	});

	ArduinoOTA.begin();

	randomSeed(analogRead(A0));         // for really random numbers

	restart = now();

	LaufschriftText = utf8ascii(LaufschriftDefault);

}

/*************************************( main loop )**************************************/
void loop() {
	server.handleClient();             // check if the HTML page was opened
	ArduinoOTA.handle();               // check if there is an OTA update

	t = now();

	if (showAnim) {

		if ((second(t) == 15 || second(t) == 45) && !del && dots) {       // check if second is 15 or 45
			printStringWithShift(LaufschriftText.c_str(), wait);
		}

		if ((second(t) == 30 && minute(t) % 2 == 0) && !del) {            // check if second is 30 and minute is even
			Datum = spaces + makeDate() + spaces;
			printStringWithShift(Datum.c_str(), wait);
		}

		if ((second(t) == 30 && minute(t) % 2 != 0) && !del) {            // check if second is 30 and minute is uneven
			showHeart(random(8));
		}
	}

	// check for blinking dots
	if (millis() < dotTime) {
		// we had an overflow...
		dotTime = millis();
	}
	if (millis() - dotTime > 500) {
		dotTime = millis();
		dots = !dots;
	}

	showAnimClock();
}

/*************************************( functions )****************************************/


/*************************************( Char functions )***********************************/
void showDigit(char ch, int col, const uint8_t* data) {
	if (dy < -8 || dy > 8) return;
	int len = pgm_read_byte(data);
	int w = pgm_read_byte(data + 1 + ch * len);
	col += dx;
	for (int i = 0; i < w; i++)
		if (col + i >= 0 && col + i < 8 * NUM_MAX) {
			byte v = pgm_read_byte(data + 1 + ch * len + 1 + i);
			if (!dy) scr[col + i] = v; else scr[col + i] |= dy > 0 ? v >> dy : v << -dy;
		}
}

// =======================================================================

void setCol(int col, byte v) {
	if (dy < -8 || dy > 8) return;
	col += dx;
	if (col >= 0 && col < 8 * NUM_MAX) {
		if (!dy) scr[col] = v;
		else scr[col] |= dy > 0 ? v >> dy : v << -dy;
	}
}

// =======================================================================

int showChar(char ch, const uint8_t* data) {
	int len = pgm_read_byte(data);
	int i;
	int w = pgm_read_byte(data + 1 + ch * len);
	for (i = 0; i < w; i++) {
		scr[NUM_MAX * 8 + i] = pgm_read_byte(data + 1 + ch * len + 1 + i);  // start at the first invisible column
	}
	scr[NUM_MAX * 8 + i] = 0;                                               // one column distance after the char to the next char
	return w;
}

// =======================================================================

void printCharWithShift(unsigned char c, int shiftDelay) {
	if (c < ' ') return;
	//c -= 32;
	int w = showChar(c, myFont2);
	for (int i = 0; i < w + 1; i++) {
		delay(shiftDelay);
		scrollLeft();
		refreshAll();
	}
}

// =======================================================================

void printStringWithShift(const char* s, int shiftDelay) {
	while (*s) {
		server.handleClient();             // check if the HTML page was opened
		ArduinoOTA.handle();
		printCharWithShift(*s, shiftDelay);
		s++;
	}
}

// =======================================================================

void showHeart(char art) {
	clr();
	showDigit(art, 11, hearts);
	refreshAll();
	for (byte n = 0; n < 4; n++) {
		for (int i = 0; i < 16; i = i + 3) {
			sendCmdAll(CMD_INTENSITY, i);
			delay(80);
		}
		for (int i = 15; i >= 0; i = i - 3) {
			sendCmdAll(CMD_INTENSITY, i);
			delay(80);
		}
	}
	sendCmdAll(CMD_INTENSITY, helligkeit);
}

// =======================================================================

void showSmileys() {
	//clr();
	for (byte n = 0; n < 4; n++) {
		for (byte i = 0; i < 3; i++) {
			clr();
			showDigit(i, 12, smiley);
			refreshAll();
			delay(100);
		}
		for (byte i = 2; i >= 0; i--) {
			clr();
			showDigit(i, 12, smiley);
			refreshAll();
			delay(100);
		}
	}
}


/*******************************************( Time functions )**************************/

String makeDate() {
	char cDate[30];
	sprintf(cDate, "%s, %02d.%s %04d ", Wochentag[weekday(t)], day(t), Monate[month(t)], year(t));
	return cDate;
}


void showSimpleClock() {
	h = hour(t);
	m = minute(t);
	s = second(t);
	dx = dy = 0;
	clr();
	showDigit(h / 10, 0, dig6x8);
	showDigit(h % 10, 8, dig6x8);
	showDigit(m / 10, 17, dig6x8);
	showDigit(m % 10, 25, dig6x8);
	showDigit(s / 10, 34, dig6x8);
	showDigit(s % 10, 42, dig6x8);
	setCol(15, dots ? B00100100 : 0);
	setCol(32, dots ? B00100100 : 0);
	refreshAll();
}

// =======================================================================

void showAnimClock() {
	h = hour(t);
	m = minute(t);
	s = second(t);
	byte digPos[6] = { 1, 8, 18, 25, 34, 42 };
	int digHt = 12;
	int num = 6;
	int i;
	if (del == 0) {
		del = digHt;
		for (i = 0; i < num; i++) digold[i] = dig[i];
		dig[0] = h / 10 ? h / 10 : 10;
		dig[1] = h % 10;
		dig[2] = m / 10;
		dig[3] = m % 10;
		dig[4] = s / 10;
		dig[5] = s % 10;
		for (i = 0; i < num; i++)  digtrans[i] = (dig[i] == digold[i]) ? 0 : digHt;
	}
	else
		del--;

	clr();
	for (i = 0; i < num; i++) {
		if (digtrans[i] == 0) {
			dy = 0;
			showDigit(dig[i], digPos[i], dig6x8);
		}
		else {
			dy = digHt - digtrans[i];
			showDigit(digold[i], digPos[i], dig6x8);
			dy = -digtrans[i];
			showDigit(dig[i], digPos[i], dig6x8);
			digtrans[i]--;
		}
	}
	dy = 0;
	setCol(15, dots ? B00100100 : B01000010);
	setCol(16, dots ? B01000010 : B00100100);
	//setCol(32, dots ? B00100100 : 0);   // for seconds on larger displays
	refreshAll();
	delay(30);
}

// =======================================================================

// Convert a single Character from UTF8 to Extended ASCII
// Return "0" if a byte has to be ignored, taken from http://playground.arduino.cc/Main/Utf8ascii
byte utf8ascii(byte ascii) {
	if (ascii < 128) {                                       // Standard ASCII-set 0..0x7F handling  
		c1 = 0;
		return(ascii);
	}

	// get previous input
	byte last = c1;                                           // get last char
	c1 = ascii;                                               // remember actual character

	switch (last) {                                           // conversion depending on first UTF8-character
	case 0xC2: return  (ascii);  break;
	case 0xC3: return  (ascii | 0xC0);  break;
	case 0x82: if (ascii == 0xAC) return(0x80);               // special case Euro-symbol
	}

	return  (0);                                              // otherwise: return zero, if character has to be ignored

}

// convert String object from UTF8 String to Extended ASCII
String utf8ascii(String s) {
	String r = "";
	char c;
	for (unsigned int i = 0; i < s.length(); i++) {
		c = utf8ascii(s.charAt(i));
		if (c != 0) r += c;
	}
	return r;
}

/******************************************************************************
  Webserver.
******************************************************************************/

void setupWebServer() {
	server.onNotFound(redirectHome);
	server.on("/", handleRoot);
	server.on("/commitSettings", handleCommitSettings);
	server.on("/handleReset", handleReset);
	server.on("/handleDisplay", handleDisplay);
	server.begin();
}

void redirectHome() {
	// Send them back to the Root Directory
	server.sendHeader("Location", String("/"), true);
	server.sendHeader("Cache-Control", "no-cache, no-store");
	server.sendHeader("Pragma", "no-cache");
	server.sendHeader("Expires", "-1");
	server.send(302, "text/plain", "");
	server.client().stop();
	delay(1000);
}

void callRoot() {
	server.send(200, "text/html", "<!doctype html><html><head><script>window.onload=function(){window.location.replace('/');}</script></head></html>");
}

void callBack() {
	server.send(200, "text/html", "<!doctype html><html><head><script>window.onload=function(){if(navigator.userAgent.indexOf(\"Firefox\")!=-1){window.location.replace('/');}else{window.history.back();}}</script></head></html>");
}

// Page 404.
void handleNotFound() {
	server.send(404, "text/plain", "404 - File Not Found.");
}

// Page /.
void handleRoot() {
	String message = "<!doctype html>";
	message += "<html>";
	message += "<head>";
	message += "<title>" + String(HOSTNAME) + " Settings</title>";
	message += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
	message += "<meta http-equiv=\"refresh\" content=\"60\" charset=\"UTF-8\">";
	message += "<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/font-awesome/4.7.0/css/font-awesome.min.css\">";
	message += "<style>";
	message += "body{background-color:#FFFFFF;text-align:center;color:#333333;font-family:Sans-serif;font-size:16px;}";
	message += "button{background-color:#1FA3EC;text-align:center;color:#FFFFFF;width:200px;padding:10px;border:5px solid #FFFFFF;font-size:24px;border-radius:10px;}";
	message += "input[type=submit]{background-color:#1FA3EC;text-align:center;color:#FFFFFF;width:200px;padding:12px;border:5px solid #FFFFFF;font-size:20px;border-radius:10px;}";
	message += "table{border-collapse:collapse;margin:0px auto;} td{padding:12px;border-bottom:1px solid #ddd;} tr:first-child{border-top:1px solid #ddd;} td:first-child{text-align:right;} td:last-child{text-align:left;}";
	message += "select{font-size:16px;}";
	message += "</style>";
	message += "</head>";
	message += "<body>";
	message += "<h1>" + String(HOSTNAME) + " Settings</h1>";
	message += "<span style=\"font-size:12px;\">";
	message += "<br>The LED-Matrix-Uhr was <i class=\"fa fa-code\"></i> with <i class=\"fa fa-heart\"></i> by <a href=\"http://www.arlitt.de\">TomSoft</a>.";
	message += "<br><i class=\"fa fa-paper-plane\"></i> Firmware: " + String(FIRMWARE);
	time_t tempEspTime = now();
	char currentDate[30];
	sprintf(currentDate, "%s, der %02d.%02d.%04d", Wochentag[weekday(tempEspTime)], day(tempEspTime), month(tempEspTime), year(tempEspTime));
	char currentTime[7];
	sprintf(currentTime, "%02d:%02d", hour(tempEspTime), minute(tempEspTime));
	char restartDate[11];
	sprintf(restartDate, "%02d.%02d.%04d", day(restart), month(restart), year(restart));
	char restartTime[9];
	sprintf(restartTime, "%02d:%02d:%02d", hour(restart), minute(restart), second(restart));
	message += "<br><br>Heute ist " + String(currentDate);
	message += "<br>aktuelle Zeit: " + String(currentTime);
	message += "<br><br>Letzter Neustart: " + String(restartDate) + ", " + String(restartTime);
	message += "<br>l&auml;uft seit " + String(int((tempEspTime - restart) / 86400)) + " Tagen, " + String(hour(tempEspTime - restart)) + " Stunden und ";
	if (minute(tempEspTime - restart) < 10) message += "0";
	message += String(minute(tempEspTime - restart)) + " Minuten";
	message += "<br>Grund des letzten Neustarts: " + ESP.getResetReason();
	message += "<br>Freier Speicher: " + String(ESP.getFreeHeap()) + " bytes";
	message += "</span>";
	message += "<form action=\"/commitSettings\">";
	message += "<table>";
	// ------------------------------------------------------------------------
	message += "<tr><td>";
	message += "Helligkeit:";
	message += "</td><td>";
	message += "<select name=\"helligkeit\">";
	message += "<option value=\"0\"";
	if (helligkeit == 0) message += " selected";
	message += ">0</option>";
	message += "<option value=\"2\"";
	if (helligkeit == 2) message += " selected";
	message += ">2</option>";
	message += "<option value=\"4\"";
	if (helligkeit == 4) message += " selected";
	message += ">4</option>";
	message += "<option value=\"6\"";
	if (helligkeit == 6) message += " selected";
	message += ">6</option>";
	message += "<option value=\"10\"";
	if (helligkeit == 10) message += " selected";
	message += ">10</option>";
	message += "<option value=\"15\"";
	if (helligkeit == 15) message += " selected";
	message += ">15</option>";
	message += "</select> Helligkeit.";
	message += "</td></tr>";
	// ------------------------------------------------------------------------
	message += "<tr><td>";
	message += "Geschwindigkeit:";
	message += "</td><td>";
	message += "<select name=\"geschwindigkeit\">";
	message += "<option value=\"30\"";
	if (wait == 30) message += " selected";
	message += ">30</option>";
	message += "<option value=\"50\"";
	if (wait == 50) message += " selected";
	message += ">50</option>";
	message += "<option value=\"70\"";
	if (wait == 70) message += " selected";
	message += ">70</option>";
	message += "</select> Millisekunden.";
	message += "</td></tr>";
	// ------------------------------------------------------------------------
	message += "<tr><td>";
	message += "Animationen/Lauftext an/aus:";
	message += "</td><td>";
	message += "<input type=\"radio\" name=\"showanim\" value=\"1\"";
	if (showAnim) message += " checked";
	message += "> an ";
	message += "<input type=\"radio\" name=\"showanim\" value=\"0\"";
	if (!showAnim) message += " checked";
	message += "> aus";
	message += "</td></tr>";
	// ------------------------------------------------------------------------
	message += "<tr><td>";
	message += "Lauftext:";
	message += "</td><td>";
	message += "<input type=\"text\" name=\"lauftext\" size=\"30\" maxlength=\"30\"";
	if (LaufschriftWeb.length() > 0) message += " value=\"" + LaufschriftWeb + "\"";
	message += ">";
	message += "</td></tr>";
	// ------------------------------------------------------------------------
	message += "</table>";
	message += "<br><input type=\"submit\" value=\"save\">";
	message += "</form>";
	message += "<button title=\"Reset Display\" onclick=\"window.location.href='/handleDisplay'\"><i class=\"fa fa-refresh\"></i></button>";
	message += "<button title=\"Reset ESP\" onclick=\"window.location.href='/handleReset'\"><i class=\"fa fa-microchip\"></i></button>";
	message += "</body>";
	message += "</html>";
	server.send(200, "text/html", message);
}

void handleCommitSettings() {
	helligkeit = server.arg("helligkeit").toInt();
	sendCmdAll(CMD_INTENSITY, helligkeit);
	wait = server.arg("geschwindigkeit").toInt();
	server.arg("showanim") == "0" ? showAnim = 0 : showAnim = 1;
	LaufschriftWeb = server.arg("lauftext");
	LaufschriftText = spaces + utf8ascii(LaufschriftWeb) + spaces;
	callRoot();
}

void handleReset() {
	server.send(200, "text/plain", "OK.");
	ESP.restart();
}

void handleDisplay() {
	server.send(200, "text/plain", "OK.");
	initMAX7219();
	sendCmdAll(CMD_SHUTDOWN, 1);
	sendCmdAll(CMD_INTENSITY, 0);
	callRoot();
}
