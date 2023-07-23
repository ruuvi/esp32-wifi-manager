
This is modification of *esp32-wifi-manager*, which you can find here: https://github.com/tonyp7/esp32-wifi-manager

The original component has been rewritten quite extensively in order to best integrate it with "Ruuvi Gateway" https://github.com/ruuvi/ruuvi.gateway_esp.c.git

---

# What is esp32-wifi-manager?

*esp32-wifi-manager* is an esp-idf component for ESP32 and ESP8266 that enables easy management of wifi networks through a web application.

*esp32-wifi-manager* is **lightweight** (8KB of task stack in total) and barely uses any CPU power through a completely event driven architecture. It's an all in one wifi scanner, http server & dns daemon living in the least amount of RAM possible.

For real time constrained applications, *esp32-wifi-manager* can live entirely on PRO CPU, leaving the entire APP CPU untouched for your own needs.

*esp32-wifi-manager* will automatically attempt to re-connect to a previously saved network on boot, and it will start its own wifi access point through which you can manage wifi networks if a saved network cannot be found and/or if the connection is lost.

*esp32-wifi-manager* is an esp-idf project that compiles successfully with the esp-idf 3.2 release. You can simply copy the project and start adding your own code to it.

# License
*esp32-wifi-manager* is MIT licensed. As such, it can be included in any project, commercial or not, as long as you retain original copyright. Please make sure to read the license file.
