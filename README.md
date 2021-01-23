# LED_Matrix_V2
LED-Matrix, gesteuert von ESP8266 (Wemos D1)

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
