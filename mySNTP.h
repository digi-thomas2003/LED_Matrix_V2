/********************************************************************

   SNTP function TomSoft

   This functions synchronise regularly with a NTP server
   NTP-Server: FritzBox  :-)
   used with the TimeLib

   Daylight Saving Time (DST) rule configuration is done within the function summertime_EU

   C:\Users\Administrator\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\2.3.0\tools\sdk\include\sntp.h

   - initNTP() must be called one time in the setup of the main sketch
   - getLocalTime() can be called regularly, e.g. using setSyncProvider
   - to adjust DST use the summertime_EU function below, this is done for Germany in getLocalTime

 *******************************************************************/

extern "C" {
#include "sntp.h"
}

char NTP_server[] = "fritz.box";

void initNTP() {
  sntp_stop();
  sntp_setservername(0, NTP_server);
  sntp_set_timezone(0);
  sntp_init();
  delay(100);
}

time_t updateNTP() {
	uint32_t timestamp = sntp_get_current_timestamp();
	// new part: checking if timestamp has a usefull value
	while (timestamp < 100000ul) {
		Serial.print("+");
		delay(10);
		timestamp = sntp_get_current_timestamp();
	}

	return timestamp;
}


boolean summertime_EU(int year, byte month, byte day, byte hour)
// European Daylight Savings Time calculation by "jurs" for German Arduino Forum
// input parameters: "normal time" for year, month, day, hour
// return value: returns true during Daylight Saving Time, false otherwise
{
	if (month < 3 || month > 10) return false; // keine Sommerzeit in Jan, Feb, Nov, Dez
	if (month > 3 && month < 10) return true;  // Sommerzeit in Apr, Mai, Jun, Jul, Aug, Sep
	if (month == 3 && (hour + 24 * day) >= (1 + 24 * (31 - (5 * year / 4 + 4) % 7)) || month == 10 && (hour + 24 * day) < (1 + 24 * (31 - (5 * year / 4 + 1) % 7)))
		return true;
	else
		return false;
}

// check for timezone and summertime (DST)
time_t getLocalTime(void) {
	time_t n = updateNTP();  // UTC in seconds
	int _year = year(n);
	byte _month = month(n);
	byte _day = day(n);
	byte _hour = hour(n);
	boolean dst = summertime_EU(_year, _month, _day, _hour);
	if (dst)  n += 7200;    // CEST Summertime in Central Europe --> UTC + 2 hours
	else      n += 3600;    // CET  Wintertime in Central Europe --> UTC + 1 hour

	return n;               // local time in seconds
}