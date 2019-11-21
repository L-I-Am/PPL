# Ping Pong Led Clock
This project is made for a combination of an Arduino and an ESP connected through UART.
The Led_example code is meant for the Arduino while the WiFiScan is coded for an ESP. 


Led_Example:
* Mostly used as a led-driver.
* Needs 10 seconds to start up (to give time to ESP to connect to Wi-Fi and the leds to power up)
* There are 3 different modes: clock, temperature and fire.
* Every TIME_CHECK_PERIOD (in this case 2) seconds, it will check in with the ESP through UART
* Update_serial() is the heart of this driver. The ESP will send the time everytime the Arduino polls for it. It will also send extra information if the webserver of the ESP got a POST-request. 

WifiScan:
* Used as a webserver and the real clock
* Fill in Wi-Fi SSID and password as you please, this can even be multiple SSID's and passwords to make it more flexible.
* It is recommended to set up a static ip on your router for the ESP, then the ip address of the website will always be the same.
* The webpage is split up because I wanted it to remember the previous arguments given
* HandleRoot() will handle http://192.168.1.X. This is the only webpage visible.
* HandlePost() will handle the post request of the previous webpage, this will just show the same webpage again with a banner saying it's been sent

Remarks:
* I know I could've used interrupt-driven UART instead of polling etc but the FastLED-library disables all interrupts while sending data to the leds, this resulted in packet-loss. Therefore I made it poll.
* This project could've been coded solely on an ESP but I only had an ESP01 which didn't have enough ports.
* The code is very chaotic and was never really meant for public. Sorry to freak you out over it
* Any bugs are always welcome reporting, it looks to be working fine but improvements are appreciated.
* Enjoy!
