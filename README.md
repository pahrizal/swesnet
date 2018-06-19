# SWESNET (Smart Wireless Electrical Switching Network)
Is an IoT device using ESP8266 to control / turn on or off electrical equipment from internet using MQTT protocol.

To perform initial configuration, you can access the internal webserver by:
1. Connect to SWESNET-<chip_id> Access point using your android phone with password to connect is ```swesnetwork```
2. Open your phone browser and then goto http://192.168.4.1
3. Follow instructions in your browser

## Hardware
Schematic Layout: https://goo.gl/1PwGa5

## Arduino dependencies
- ESP8266 Core Library
- ESP AsyncTCP
- AsyncMqttClient

## Android
You can use any MQTT apps on android, for example:
https://play.google.com/store/apps/details?id=net.routix.mqttdash&hl=en
