# Ping Pong Led Clock

# Now updated to use one platform isntead of two!

This project is made for the instructable: https://www.instructables.com/Ping-Pong-Ball-LED-Clock/
The version I made has a different way of attaching the leds which results in the code not being compatible with the newer instructable but it could be easily ported to support both versions. The main difference is the sequence of the leds but all the logic built on top of it can still be used. If you need any help with this, don't hesitate to contact me on here or on reddit: u\L_I-Am

The newest update on this project uses only one ESP32. The only code you need to adapt is the Wi-Fi-SSID and PW.

Features:
* Uses a real-time clock which gets the time even without Wi-Fi connection (has to be connected once te set the time but will persist over power-cycles)
* has a web-interface at URL: clock.local    This can be used to adjust the mode, brightness, foreground color, background color, blinking, raindrop speed and raindrop amount.
* 4 different modes: Time, Temperature (uses a temperature sensor in the contraption), Fire (ambient fire), Raindrops










If for any reason, you still like to use the old code on Arduino Uno and ESP8266:

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

Any further questions or remarks can be sent to u/L_I-Am
